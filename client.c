#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <net/if.h>
#include <sys/ioctl.h>

#include "client.h"
#include "PROTO_DIST_QUEUE.h"

void error(const char *msg)
{
    perror(msg);
    exit(0);
}

int main(int argc, char *argv[])
{
    int sockfd;
    Msg pkg;
    char *command;
    char buffer[256];
    if (argc < 3) {
       fprintf(stderr,"usage %s hostname port \n", argv[0]);
       exit(0);
    }

    sockfd = socketCreate_connect(argv[1], argv[2]);
    
    command = buffer;
    int flag_stop = 0;
    // int err;

    while( !flag_stop ){
        printf("Insert command: \n");
        bzero(buffer,256);
        scanf("%s",buffer);
        getchar();

        if ( strcmp(command, "submit") == 0 ){

            getArgsSubmit(&pkg, sockfd);
            client_protocol(sockfd);

        } else if ( strcmp(command, "submiti") == 0 ){

            int new_sock = getArgsSubmitInt(&pkg, sockfd);
            client_protocol(sockfd);
            client_accept(new_sock);


        }else if( strcmp(command, "statejob") == 0 ){
            bzero(buffer,256);
            scanf("%s",buffer);
            getchar();
            JobState(&pkg, buffer,sockfd);
            client_protocol(sockfd);
        

        }else if( strcmp(command, "statequeue") == 0 ){
            bzero(buffer,256);
            scanf("%s",buffer);
            getchar();
            QueueState(&pkg, buffer,sockfd);
            client_protocol(sockfd);
        
        }else if( strcmp(command, "unsubmit") == 0 ){
            bzero(buffer,256);
            scanf("%s",buffer);
            getchar();
            unsubmit(&pkg, buffer,sockfd);
            client_protocol(sockfd);

        } else if ( strcmp(command, "stop") == 0 ){

            setClosedConnection(&pkg);
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

//num_args name dir arg1 arg2 ...
void getArgsSubmit(Msg *pkg, int sockfd){
    char buff[SZ_PATH];
    bzero(buff,SZ_PATH);
    char *line = NULL;
    size_t line_size = 0;

    if ( getline(&line, &line_size,stdin) == -1 ){
        perror("Error getline()");
        return;
    }

    if (getcwd(buff,sizeof(buff)) == NULL){
        perror("Error getcwd()");
        return;
    }

    setFileExecNoInter_CLIENT(pkg, line, buff , 0);
    free(line);

    if (sendMsg(sockfd,pkg) != 1){
        perror("ERROR on sending msg");
    }

}        

//num_args name dir arg1 arg2 ...
int getArgsSubmitInt(Msg *pkg, int sockfd){
    char buff[SZ_PATH];
    char *line = NULL;
    size_t line_size = 0;
    int val = 1;
    struct sockaddr_in serv_addr;
    socklen_t socklen = sizeof(struct sockaddr_in);
    struct ifreq ifr;

    if ( getline(&line, &line_size,stdin) == -1 ){
        perror("Error getline()");
        return -1;
    } 
    if (getcwd(buff,sizeof(buff)) == NULL){
        perror("Error getcwd()");
        return -1;
    }

    int newsockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (newsockfd < 0) 
        perror("ERROR opening socket");
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = 0;
    setsockopt(newsockfd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));
    if (bind(newsockfd, (struct sockaddr *) &serv_addr,
            sizeof(serv_addr)) < 0) 
            perror("ERROR on binding");
    listen(newsockfd,5);

    ifr.ifr_addr.sa_family = AF_INET;
    strncpy(ifr.ifr_name, "en0", IFNAMSIZ-1);

    ioctl(newsockfd, SIOCGIFADDR, &ifr);
    getsockname(newsockfd, (struct sockaddr *) &serv_addr, &socklen);
    printf("My address: %s x: %d \n", inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr), ntohs(serv_addr.sin_port));
    setFileExecInter_CLIENT(pkg, line, buff ,((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr,serv_addr.sin_port, 0);
    free(line);

    if (sendMsg(sockfd,pkg) != 1){
        perror("ERROR on sending msg");
    }

    return newsockfd;

}     

void client_accept(int sock){
    int newsockfd;
    struct sockaddr_in cli_addr;
    socklen_t clilen = sizeof(cli_addr);

    newsockfd = accept(sock,(struct sockaddr *) &cli_addr, &clilen);
    if (newsockfd < 0) {
        perror("ERROR on accept");
        return;
    }

    interaction_2(newsockfd);

    close(newsockfd);

}

void client_protocol(int socket){
    Msg pkg;
    int err;

    if( (err = recvMsg(socket, &pkg)) == 1 ) {

        switch( pkg.hdr.type ) {
            case TYPE_SUB_RESP:
                jobSubmittedResponse(&pkg);
                break;
            case TYPE_JOBSTATE_RESP:
                jobStateResponse(&pkg);
                break;
            case TYPE_QUESTATE_RESP:
                queueStateResponse(&pkg);
                break;
            case TYPE_UNSUB_RESP:
                unsubmitResponse(&pkg);
                break;
            case TYPE_BAD_PARAMS:
                badParamsResponse();
                break;
            default:
                perror("Bad type msg attending client");
        }
    }else{
        if (err == 0 ){
            printf("Socket closed. \n");
        }else{
            perror("ERROR on recvMsg");
        }
    }

}

void jobSubmittedResponse(Msg *pkg){
    JobID job_id = getJobID(pkg);
    printf("Job submitted -> JobID: %d \n", job_id.job_id);
}

void jobStateResponse(Msg *pkg){
    Job_State job_state = getJobState(pkg);
    printf("JobID %d - State: %d - Exit_status: %d \n", job_state.jobid.job_id,job_state.state, job_state.exit_value);
}

void badParamsResponse(){
    printf("Bad parameters sent \n");
}

void queueStateResponse(Msg *pkg){
    Queue_State queue_state = getQueueState(pkg);
    printf("QueueID %d - State: %d\n", queue_state.queueid.queue_id,queue_state.state);
}

void unsubmitResponse(Msg *pkg){
    printf("JobID %d unsubmited\n", pkg->payload.jobid.job_id);
}
void JobState(Msg *pkg, char *to_jobid, int sockfd){
    JobID jobid;
    jobid.job_id = atoi(to_jobid);
    setJobState_CLIENT(pkg, jobid);
    if (sendMsg(sockfd,pkg) != 1){
        perror("ERROR on sending msg");
    }
    
}

void QueueState(Msg *pkg, char *to_queue_idx, int sockfd){
    QueueID queue_id;
    queue_id.queue_id = atoi(to_queue_idx);
    setQueueState_CLIENT(pkg, queue_id);
    if (sendMsg(sockfd,pkg) != 1){
        perror("ERROR on sending msg");
    }
}

int socketCreate_connect(char* addr, char* port){
    int sockfd, portno;
    struct sockaddr_in serv_addr;
    struct hostent *server;

    portno = atoi(port);
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) 
        error("ERROR opening socket");
    server = gethostbyname(addr);
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
    if (connect(sockfd,(struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0) 
        error("ERROR connecting");

    return sockfd;
}

void unsubmit(Msg *pkg, char *to_jobid, int sockfd){
    JobID jobid;
    jobid.job_id = atoi(to_jobid);
    setUnsubmit_CLIENT(pkg, jobid);
    if (sendMsg(sockfd,pkg) != 1){
        perror("ERROR on sending msg");
    }

}

/*
// void interaction_1(int sockfd){
//     int n;
//     char buffer[256];
//     bzero(buffer,256);

//     n = write(sockfd,buffer,255);
//     if (n < 0) 
//          error("ERROR reading from socket");

//     n = read(sockfd,buffer,255);
//     if (n < 0) 
//          error("ERROR reading from socket");
//     printf("%s\n",buffer);

//     bzero(buffer,256);
//     fgets(buffer,255,stdin);

//     n = write(sockfd,buffer,255);
//     if (n < 0) 
//          error("ERROR reading from socket");

//     n = read(sockfd,buffer,255);
//     if (n < 0) 
//         error("ERROR reading from socket");
//     printf("%s \n",buffer);

// }
*/

void interaction_2(int sockfd){
    fd_set rfds;
    char buff[256];
    int n;

    while(1){
        FD_ZERO(&rfds);
        FD_SET(STDIN_FILENO, &rfds);
        FD_SET(sockfd, &rfds);

        int ret = select(sockfd+1, &rfds,NULL,NULL,NULL);
        if ( ret == -1 ){
            perror("select_client()");
            return;
        }

        if ( FD_ISSET(sockfd, &rfds) ){
            n = read(sockfd, buff, sizeof(buff));
            if (n <= 0 ){
                printf("Reading sockfd %d \n",n);
                return;
            } else {
                n = write(STDOUT_FILENO, buff, n);
                if (n <= 0 ){
                    printf("Writing sockfd %d \n",n);
                    return;
                }
            }
        } else if ( FD_ISSET(STDIN_FILENO,&rfds)){
            n = read(STDIN_FILENO, buff, sizeof(buff));
            if (n <= 0 ){
                printf("Reading from stdin %d \n",n);
                return;
            } else {
                n = write(sockfd, buff, n);
                if (n <= 0 ){
                    printf("Writing on sockfd %d \n",n);
                    return;
                }
            }
        } else {
            assert(0);
        }

    }


}
