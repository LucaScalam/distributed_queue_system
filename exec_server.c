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
#include <sys/wait.h>

#include "exec_server.h"
#include "main_server.h"
#include "PROTO_DIST_QUEUE.h"

int main(int argc, char *argv[]){
    int sockfd, portno;
    struct sockaddr_in serv_addr;
    struct hostent *server;
    char *command;
    char buffer[256];
    int flag = 1;
    
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

    while( flag ){
        printf("Insert command: \n");
        bzero(buffer,256);
        scanf("%s",buffer);
        getchar();

        if ( strcmp(command, "job") == 0 ){
            
            flag = jobRequest(sockfd);

        } else if ( strcmp(command, "stop") == 0 ){

            flag = closeConnecMain(sockfd);

        }else{
            printf("Bad command, try again \n");
        }

    } 

    close(sockfd);
    return 0;

}

int jobRequest(int sockfd){
    Msg pkg;
    int err;
    setJobReq(&pkg);
    err = sendMsg(sockfd,&pkg);
    if ( err == -1 ){
        perror("ERROR on sending setJobReq");
        return 0;
    } else if ( err == 0 ){
        perror("Socket closed");
        return 0;
    } else {
        return exec_server_protocol(sockfd);
    }

}

int closeConnecMain(int sockfd){
    Msg pkg;
    setClosedConnection_ExecServer(&pkg);
    if (sendMsg(sockfd,&pkg) != 1){
        perror("ERROR on sending msg");
    }
    return 0;
}
    
int exec_server_protocol(int socket){
    Msg pkg;
    int err,out;

    if( (err = recvMsg(socket, &pkg)) == 1 ) {
        switch( getType(&pkg.hdr) ) {
            case TYPE_SENDJOB_NOINTER:
            case TYPE_SENDJOB_INTER:
                out = execJob(&pkg, socket);
                break;
            case TYPE_CH_EXEC_SER:
                out = ackRoutine(socket);
                break;
            default:
                perror("Bad type msg attending exec_server");
                out = 0;
                break;
        }
        
    } else {
        if (err == 0 ){
            printf("Socket closed. \n");
            out = 0;
        } else {
            perror("ERROR on recvMsg - exec_server");
            out = 0;
        }
    }

    return out;
}

int ackRoutine(int socket){
    int ack_result;
    ack_result = sendAck(socket);

    if ( ack_result == 0 ){
        return 0;
    } else {
        return exec_server_protocol(socket);
    }
}

int sendAck(int socket){
    Msg pkg;
    int err;
    setCheckExecServerAck(&pkg);
    printf("to sleep \n");
    sleep(6);
    printf("up \n");
    err = sendMsg(socket,&pkg);
    if ( err == -1 ){
        perror("ERROR sending ACK");
        return 0;
    } else if ( err == 0 ){
        perror("socket closed sending ACK");
        return 0;
    }

    return 1;

}

int execJob(const Msg *pkg, int socket_main){
    pid_t pid;

    pid = fork();
    switch( pid ) {
        case 0:
            exec_server_protocol_child(pkg);
            exit(40);
        case -1:
            perror("error fork()");
            error_fork(pkg,socket_main);
            return 1;
            break;
        default:
            exec_server_protocol_father(pkg, socket_main);
            return 1;
            break;  
    }

    return 1;
}

void exec_server_protocol_child(const Msg *pkg){

            consumeJob(pkg);

}

void exec_server_protocol_father(const Msg *pkg, int socket_main){
    switch( getType(&pkg->hdr) ) {
        case TYPE_SENDJOB_NOINTER:
            fatherJobRoutine(getJobID_NoIter(pkg), socket_main);
            break;
        case TYPE_SENDJOB_INTER:

            fatherJobRoutine(getJobID_Iter(pkg), socket_main);
            break;
        default:
            break;
    }

}

void error_fork(const Msg *pkg, int socket_main){
    Msg new_pkg;
    int err = 0;

    switch( getType(&pkg->hdr) ) {
        case TYPE_SENDJOB_NOINTER:
            setJobDone(&new_pkg, getJobID_NoIter(pkg), 41);
            break;
        case TYPE_SENDJOB_INTER:
            setJobDone(&new_pkg, getJobID_Iter(pkg), 41);
            break;
        default:
            break;
    }
    

    err = 0;
    if ( (err = sendMsg(socket_main,&new_pkg)) == -1 ){
        perror("ERROR sending DoneMsg");
    }
}

void consumeJob(const Msg *pkg){
    int ret, i, sock_client;
    const char *line;
    char *args[ARG_MAX] = { NULL };
    char *auxline;
    char *saveptr;
    char line_cpy[SZ_PATH * NUM_PATH];
    
    if ( getType(&pkg->hdr) == TYPE_SENDJOB_NOINTER ){
        setFDsNoIter(pkg);
        line = getExec_fileExec_execfile(pkg);
    } else if ( getType(&pkg->hdr) == TYPE_SENDJOB_INTER ){
        sock_client = setFDsIter(pkg);
        line = getExecAddr_fileExec_execfile(pkg);
    } else {
        assert(0);
    }
    strncpy(line_cpy,line,SZ_PATH * NUM_PATH);
    auxline = line_cpy;
    i = 0;
    while( (args[i] = strtok_r(auxline, " \n", &saveptr)) ) {
        if( auxline )
            auxline = NULL;
        ++i;
        if( i == ARG_MAX ) {
            args[ARG_MAX - 1] = NULL;
            break;
        }
    }

    if( (ret = execvp(args[0], args)) < 0 ){
        printf("ret %d\n",ret);
        perror("problem on execvp(): ");
        return;
    }

    close(sock_client);
    
    while( i != 0 ){
        free(args[i-1]);
        i--;
    }
}

void setFDsNoIter(const Msg *pkg){
    char fname[128];

    sprintf(fname, "cout-%d.log", getJobID_NoIter(pkg));
    int out = open(fname, O_WRONLY|O_CREAT|O_APPEND, 0600);
    if (-1 == out) { 
        perror("opening cout.log"); 
        return; 
    }

    sprintf(fname, "cerr-%d.log", getJobID_NoIter(pkg));
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

    if ( chdir(getExec_fileExec_wdir(pkg)) == -1 ){ 
        perror("cannot change dir"); 
        return; 
    }
}

int setFDsIter(const Msg *pkg){
    int sock_client;

    sock_client = socketPortConnect(getExecAddr_port(pkg), getExecAddr_ipadd(pkg));

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

    if ( chdir(getExecAddr_fileExec_wdir(pkg)) == -1 ){ 
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

void fatherJobRoutine(uint16_t job_id,int socket_main){
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



