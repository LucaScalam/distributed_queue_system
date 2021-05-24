#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <net/if.h>
#include <sys/ioctl.h>
#include <assert.h>

#include "exec_server.h"
#include "main_server.h"
#include "PROTO_DIST_QUEUE.h"

int main(int argc, char *argv[]){
    int sockfd, portno;
    struct sockaddr_in serv_addr;
    struct hostent *server;
    Msg pkg;
    char *command;
    char buffer[256];
    if (argc < 3) {
       fprintf(stderr,"usage %s hostname port \n", argv[0]);
       exit(0);
    }
    portno = atoi(argv[2]);
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) 
        perror("ERROR opening socket");
    server = gethostbyname(argv[1]);
    if (server == NULL) {
        fprintf(stderr,"ERROR, no such host \n");
        exit(0);
    }
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, 
         (char *)&serv_addr.sin_addr.s_addr,
         server->h_length);
    serv_addr.sin_port = htons(portno);
    if (connect(sockfd,(struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0){
        perror("ERROR connecting");
        return -1;
    }

    command = buffer;
    int flag_stop = 0;

    while( !flag_stop ){
        printf("Insert command: \n");
        bzero(buffer,256);
        scanf("%s",buffer);
        getchar();

        if ( strcmp(command, "job") == 0 ){

            setJobReq(&pkg);
            if (sendMsg(sockfd,&pkg) != 1){
                perror("ERROR on sending setJobReq");
            }
            exec_server_protocol(sockfd);

        } else if ( strcmp(command, "stop") == 0 ){

            setClosedConnection_ExecServer(&pkg);
            if (sendMsg(sockfd,&pkg) != 1){
                perror("ERROR on sending msg");
            }
            flag_stop = 1;


        }else{
            printf("Bad command, try again \n");
        }

    }


    close(sockfd);
    return 0;

}

void exec_server_protocol(int socket){
    Msg pkg;
    int err;

    if( (err = recvMsg(socket, &pkg)) == 1 ) {
        switch( pkg.hdr.type ) {
            case TYPE_SENDJOB_NOINTER:
            case TYPE_SENDJOB_INTER:
                execJob(&pkg, socket);
                break;
            default:
                perror("Bad type msg attending exec_server");
                break;
        }
        
    }else{
        if (err == 0 ){
            printf("Socket closed. \n");
        }else{
            perror("ERROR on recvMsg - exec_server");
        }
    }

}

void execJob(Msg *pkg, int socket_main){
    pid_t pid;

    pid = fork();
    switch( pid ) {
        case 0:
            exec_server_protocol_child(pkg);
            exit(0);
        case -1:
            perror("error fork()");
            return;
        default:
            exec_server_protocol_father(pkg, socket_main);
            break;  
    }

}

void exec_server_protocol_child(Msg *pkg){

            consumeJob(pkg);

}

void exec_server_protocol_father(Msg *pkg, int socket_main){
    switch( pkg->hdr.type ) {
        case TYPE_SENDJOB_NOINTER:
            fatherJobRoutine(pkg->payload.job_exec.jobid, socket_main);
            break;
        case TYPE_SENDJOB_INTER:
            fatherJobRoutine(pkg->payload.job_exec_addr.jobid, socket_main);
            break;
        default:
            break;
    }
}

void consumeJob(Msg *pkg){
    int ret, i, sock_client;
    char *line;
    char *args[MAX_ARGS] = { NULL };
    char *auxline;
    char *saveptr;
    
    if ( pkg->hdr.type == TYPE_SENDJOB_NOINTER ){
        setFDsNoIter(pkg);
        line = pkg->payload.job_exec.file_exec.exec_file;
    } else if ( pkg->hdr.type == TYPE_SENDJOB_INTER ){
        sock_client = setFDsIter(pkg);
        line = pkg->payload.job_exec_addr.file_exec.exec_file;
    } else {
        assert(0);
    }

    auxline = line;
    i = 0;
    while( (args[i] = strtok_r(auxline, " \n", &saveptr)) ) {
        if( auxline )
            auxline = NULL;
        ++i;
        if( i == MAX_ARGS ) {
            args[MAX_ARGS - 1] = NULL;
            break;
        }
    }

    if( (ret = execvp(args[0], args)) < 0 ){
        printf("ret %d\n",ret);
        perror("problem on execvp(): ");
        return;
    }

    // close(out);
    // close(err);
    // close(devNull);
    // close(sock_client);
    
    while( i != 0 ){
        free(args[i-1]);
        i--;
    }
}

void setFDsNoIter(Msg *pkg){
    char fname[128];

    sprintf(fname, "cout-%d.log", pkg->payload.job_exec.jobid.job_id);
    int out = open(fname, O_WRONLY|O_CREAT|O_APPEND, 0600);
    if (-1 == out) { 
        perror("opening cout.log"); 
        return; 
    }

    sprintf(fname, "cerr-%d.log", pkg->payload.job_exec.jobid.job_id);
    int err = open(fname , O_WRONLY|O_CREAT|O_APPEND, 0600);
    if (-1 == err) { 
        perror("opening cerr.log"); 
        return; 
    }

    int devNull = open("/dev/null",0);
    if (-1 == devNull) { 
        perror("opening devNull"); 
        return; 
    }

    if ( dup2(out, STDOUT_FILENO) == -1 ) { 
        perror("cannot redirect stdout"); 
        return; 
    }
    if ( dup2(err, STDERR_FILENO) == -1) {
        perror("cannot redirect stderr"); 
        return; 
    }

    if ( dup2(devNull, STDIN_FILENO) == -1 ) { 
        perror("cannot redirect stdin"); 
        return; 
    }

    if ( chdir(pkg->payload.job_exec.file_exec.working_dir) == -1 ){ 
        perror("cannot change dir"); 
        return; 
    }
}

int setFDsIter(Msg *pkg){
    int sock_client;

    sock_client = socketPortConnect(pkg->payload.job_exec_addr.port, pkg->payload.job_exec_addr.ipaddr);

    printf("Connected to client... \n");

    if ( dup2(sock_client, STDOUT_FILENO) == -1 ) { 
        perror("cannot redirect stdout"); 
        return -1; 
    }
    if ( dup2(sock_client, STDERR_FILENO) == -1) {
        perror("cannot redirect stderr"); 
        return -1; 
    }

    if ( dup2(sock_client, STDIN_FILENO) == -1 ) { 
        perror("cannot redirect stdin"); 
        return -1; 
    }

    if ( chdir(pkg->payload.job_exec_addr.file_exec.working_dir) == -1 ){ 
        perror("cannot change dir"); 
        return -1; 
    }

    return sock_client;

}

int socketPortConnect(uint16_t port, uint32_t address){
    int sockfd;
    struct sockaddr_in serv_addr;
    struct hostent *server;
    struct in_addr addr;
    addr.s_addr = (in_addr_t) address;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) 
        perror("ERROR opening socket");
    
    printf("Client address: %s x: %d \n", inet_ntoa(addr), ntohs((in_port_t)port));
    server = gethostbyaddr(&addr, sizeof(addr), AF_INET);
    if (server == NULL) {
        fprintf(stderr,"ERROR, no such host \n");
        exit(0);
    }
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, 
         (char *)&serv_addr.sin_addr.s_addr,
         server->h_length);
    serv_addr.sin_port = port;
    if (connect(sockfd,(struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0){
        perror("ERROR connecting");
        return -1;
    }

    return sockfd;
}

void fatherJobRoutine(JobID job_id,int socket_main){
    Msg pkg_to_send_1, pkg_to_send_2;
    pid_t pid;
    int wstatus;
    int err = 0;

    
    setJobRun(&pkg_to_send_1, job_id);
    if ( (err = sendMsg(socket_main,&pkg_to_send_1)) == -1 ){
        perror("ERROR sending RunMsg");
    }

    pid = wait(&wstatus);
    if ( pid == -1 ){
        perror("ERROR on wait()");
    }

    if ( WIFEXITED(wstatus) ){
        setJobDone(&pkg_to_send_2, job_id, WEXITSTATUS(wstatus));
    }else if ( WIFSIGNALED(wstatus) ){
        setJobSignaled(&pkg_to_send_2, job_id, WTERMSIG(wstatus));
    } else {
        assert(0);
    }

    err = 0;
    if ( (err = sendMsg(socket_main,&pkg_to_send_2)) == -1 ){
        perror("ERROR sending DoneMsg");
    }

}