#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <assert.h>

#include "main_server.h"
#include "PROTO_DIST_QUEUE.h"


int main(int argc, char *argv[]){
    int socket_server, socket_client;
    char buff[256];
    int cmd;
    pthread_attr_t attr;
    pthread_t thread_client, thread_exec_server;
    MainServer_t main_server;
    ThreadArg_t arg_thread_server, arg_thread_client;

    if (argc < 2) {
        fprintf(stderr,"ERROR, no port provided\n");
        exit(1);
    }
    
    socket_client = socketCreate(argv[1]);
    socket_server = socketCreate(argv[2]);
    MainServerInit(&main_server);
    arg_thread_client.main_server = &main_server;
    arg_thread_server.main_server = &main_server;
    arg_thread_client.sockfd = socket_client;
    arg_thread_server.sockfd = socket_server;
    
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    int rc = pthread_create(&thread_client,&attr,threadNewClients,(void *)&arg_thread_client);
    if( rc ) {
        perror("pthread_create()");
        exit(-1);
    }

    rc = pthread_create(&thread_exec_server,&attr,threadNewExecServ,(void *)&arg_thread_server);
    if( rc ) {
        perror("pthread_create()");
        exit(-1);
    }

    while(1){
        printf("Insert command: \n");
        bzero(buff,256);
        scanf("%s",buff);
        getchar();

        if (buff[0] == 's'){
            break;
        } else if ( buff[0] == 't' ){

            printf("Total of jobs: %d \n",main_server.history_putidx);

        } else if ( buff[0] == 'j' ){

            printHistoryJobs(&main_server);

        } else if ( buff[0] == 'q' ){
            bzero(buff,256);
            scanf("%s",buff);
            cmd = atoi(buff);
            if ( cmd < 0 || cmd >= NUM_QUEUE_MAX ){
                printf("Bad command, try again \n");
            } else {
                printQueueJobs(main_server.queue[cmd]);
            }

        } else {

            printf("Bad command, try again \n");

        }
    }

    close(socket_server);
    close(socket_client);

    MainServerDestroy(&main_server,thread_client,thread_exec_server);

    pthread_attr_destroy(&attr);
    pthread_exit(NULL);
    return 0;
}
//Creates new socket
int socketCreate(char* argv){
    int val = 1;
    int portno;
    struct sockaddr_in serv_addr;
    //TCP
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) 
        perror("ERROR opening socket");
    bzero((char *) &serv_addr, sizeof(serv_addr));
    portno = atoi(argv);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));
    if (bind(sockfd, (struct sockaddr *) &serv_addr,
            sizeof(serv_addr)) < 0) 
            perror("ERROR on binding");
    listen(sockfd,5);
    return sockfd;
}

void MainServerInit(MainServer_t* main_server){
    pthread_mutex_t master_mtx;
    pthread_mutex_t job_hist_mtx;
    uint16_t i = 0;
    
    int mx = pthread_mutex_init(&master_mtx, NULL);
    if( mx ) {
        char buff[64];
        strerror_r(mx,buff, sizeof(buff));
        printf("Problem in pthread_mutex_init()1: %s \n", buff);
        exit(-1);
    }
    mx = pthread_mutex_init(&job_hist_mtx, NULL);
    if( mx ) {
        char buff[64];
        strerror_r(mx,buff, sizeof(buff));
        printf("Problem in pthread_mutex_init()2: %s \n", buff);
        exit(-1);
    }
    main_server->queue2get = 0;
    main_server->queue2put = 0;
    main_server->history_putidx = 0;
    main_server->main_server_mtx = master_mtx;
    main_server->job_hist_mtx = job_hist_mtx;

    for (i = 0; i < NUM_QUEUE_MAX; i++){
        main_server->queue[i] = QueueInit(i);
    }
    for (i = 0; i < JOB_HISTORY_MAX; i++){
        main_server->job_array[i] = NULL;
    }
    

}

void MainServerDestroy(MainServer_t* main_server, pthread_t thread_client, pthread_t thread_exec_server){
    uint16_t i = 0;
    int limit;

    int mx = pthread_mutex_destroy(&main_server->main_server_mtx);
    if( mx ) {
        char buff[64];
        strerror_r(mx,buff, sizeof(buff));
        printf("Problem in pthread_mutex_destroy()1: %s \n", buff);
    }

    mx = pthread_mutex_destroy(&main_server->job_hist_mtx);
    if( mx ) {
        char buff[64];
        strerror_r(mx,buff, sizeof(buff));
        printf("Problem in pthread_mutex_destroy()2: %s \n", buff);
    }        
    for (i = 0; i < NUM_QUEUE_MAX; i++){
        QueueDestroy(main_server->queue[i]);
    }    
    
    if ( main_server->history_putidx < JOB_HISTORY_MAX ) {
        limit = main_server->history_putidx;
    } else {
        limit = JOB_HISTORY_MAX;
    }
    for (i = 0; i < limit; i++){
        free(main_server->job_array[i]);
    }    

}


//Creates queue to store Jobs
Queue_t *QueueInit(uint16_t q_id){
    pthread_mutex_t queue_mtx;
    pthread_cond_t get_condv;
    pthread_cond_t put_condv;
    Queue_t *q = malloc(sizeof(Queue_t));

    int mx = pthread_mutex_init(&queue_mtx, NULL);
    if( mx ) {
        printf("ERROR; return code from pthread_mutex_init() is %d\n", mx);
        exit(-1);
    }    
    int condv = pthread_cond_init(&get_condv,NULL);
    if( condv ) {
        printf("ERROR; return code from pthread_cond_init() is %d\n", condv);
        exit(-1);
    }
    condv = pthread_cond_init(&put_condv,NULL);
    if( condv ) {
        printf("ERROR; return code from pthread_cond_init() is %d\n", condv);
        exit(-1);
    }

    q->get_idx = 0;
    q->put_idx = 0;
    q->q_size = QUEUE_SZ;
    q->state = STATE_AVAIL;
    q->queue_mtx = queue_mtx;
    q->get_condv = get_condv;
    q->put_condv = put_condv;
    q->id = q_id;

    return q;
}

void QueueDestroy(Queue_t * q){
    int mx = pthread_mutex_destroy(&q->queue_mtx);
    if( mx ) {
        printf("ERROR; return code from pthread_mutex_destroy() is %d\n", mx);
        exit(-1);
    }   

    int condv = pthread_cond_destroy(&q->put_condv);
    if( condv ) {
        printf("ERROR; return code from pthread_cond_destroy() is %d\n", condv);
        exit(-1);
    }
    condv = pthread_cond_destroy(&q->get_condv);
    if( condv ) {
        printf("ERROR; return code from pthread_cond_destroy() is %d\n", condv);
        exit(-1);
    }

    free(q);
}

//Creates new conexion to client
void *threadNewClients(void *thr_arg){
    ThreadArg_t *thread_arg = (ThreadArg_t*)thr_arg;
    MainServer_t *main_server = thread_arg->main_server;
    struct sockaddr_in cli_addr;
    pthread_t thread;
    socklen_t clilen = sizeof(cli_addr);
    pthread_attr_t attr;
    int newsockfd;

    printf("client_accept_thread created \n");

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    while(1){
        newsockfd = accept(thread_arg->sockfd,(struct sockaddr *) &cli_addr, &clilen);
        if (newsockfd < 0){
            printf("EXEC_CLIENT_th exiting \n");
            break;
        }
        ThreadArg_t *arg_thread_client = malloc(sizeof(ThreadArg_t));
        
        arg_thread_client->main_server = main_server;
        arg_thread_client->sockfd = newsockfd;

        int rc = pthread_create(&thread,&attr,attendClient,(void *)arg_thread_client);
        if( rc ) {
            char buff[64];
            strerror_r(rc,buff, sizeof(buff));
            printf("Problem creating client thread: %s \n", buff);
            close(newsockfd);
        }
    }
    pthread_attr_destroy(&attr);
    pthread_exit(NULL);

}

//Creates new conexion to client
void *attendClient(void *thr_arg){
    ThreadArg_t *thread_arg = (ThreadArg_t*)thr_arg;
    MainServer_t *main_server = thread_arg->main_server;
    Msg pkg;
    int err;
    int flag = 1;
    while( flag ){
        if( (err = recvMsg(thread_arg->sockfd, &pkg)) == 1 ) {
            
            switch( pkg.hdr.type ) {
                case TYPE_SUB_NO_INTER:
                    submitNoInter(&pkg.payload.job_exec, main_server, thread_arg->sockfd);
                    break;
                case TYPE_SUB_INTERACTIVE:
                    submitInter(&pkg.payload.job_exec_addr, main_server, thread_arg->sockfd);
                    break;
                case TYPE_JOBSTATE:
                    jobState(&pkg.payload.jobid, main_server, thread_arg->sockfd);
                    break;
                case TYPE_QUESTATE:
                    queueState(&pkg.payload.queueid, main_server, thread_arg->sockfd);
                    break;
                case TYPE_UNSUB:
                    unsubmit(&pkg.payload.jobid, main_server, thread_arg->sockfd);
                    break;
                case TYPE_CLIENT_CLOSED:
                    printf("Connection to client closed ----- \n");
                    flag = 0;
                    break;
                default:
                    perror("Bad type msg attending client");
            }
        }else{
            if (err == 0 ){
                printf("Socket closed. \n");
            }else{
                perror("ERROR on recvMsg - attendClient()");
            }
            break;
        }
    }
    close(thread_arg->sockfd);
    free(thread_arg);
    pthread_exit(NULL);

}


//Submits No interactive job
void submitNoInter(Job_Exec *job_exec, MainServer_t *main_server, int sock){ 
    File_Exec *file_exec = &job_exec->file_exec;

    Job_t *new_job = malloc(sizeof(Job_t));
    strncpy(new_job->exec_file, file_exec->exec_file, SZ_PATH * 4);
    strncpy(new_job->working_dir, file_exec->working_dir, SZ_PATH);
    new_job->port = 0;
    new_job->ipaddr = 0;

    putJob(new_job, main_server, sock);
}

//Submits interactive job
void submitInter(Job_Exec_Addr *job_exec_ad, MainServer_t *main_server, int sock){
    File_Exec *file_exec = &job_exec_ad->file_exec;

    Job_t *new_job = malloc(sizeof(Job_t));
    strncpy(new_job->exec_file, file_exec->exec_file, SZ_PATH*4);
    strncpy(new_job->working_dir, file_exec->working_dir, SZ_PATH);
    new_job->port = job_exec_ad->port;
    new_job->ipaddr = job_exec_ad->ipaddr;

    putJob(new_job, main_server, sock);

}

void submittedResponse(uint16_t jobid, int sock){
    Msg pkg;
    JobID jobID;
    jobID.job_id = jobid;
    setJobID(&pkg, jobID);

    int err = 0;
    if ( (err = sendMsg(sock,&pkg)) == -1 ){
        perror("ERROR sending msg");
    }
}

void putJob(Job_t *new_job, MainServer_t *main_server, int sock){
    uint16_t queue_idx;
    uint16_t new_jobid;
    JobHistory_t *job_history = malloc(sizeof(JobHistory_t));
    

    //Gets queue idx
    pthread_mutex_lock(&main_server->main_server_mtx);
    queue_idx = main_server->queue2put++ % NUM_QUEUE_MAX;    
    Queue_t *queue = main_server->queue[queue_idx % NUM_QUEUE_MAX];
    pthread_mutex_unlock(&main_server->main_server_mtx);

    //Gets new job id
    pthread_mutex_lock(&main_server->job_hist_mtx);
    new_jobid = main_server->history_putidx++;
    pthread_mutex_unlock(&main_server->job_hist_mtx);
    new_job->id = new_jobid;

    job_history->queue_idx = queue_idx;
    job_history->id = new_jobid;
    job_history->ipaddr = new_job->ipaddr;
    job_history->state = STATE_WAIT;

    pthread_mutex_lock(&queue->queue_mtx);
    while ( (queue->put_idx - queue->get_idx) == queue->q_size ) {
        queue->state = STATE_FULL;
        pthread_cond_wait(&queue->put_condv, &queue->queue_mtx);
    }
    queue->jobs[queue->put_idx++ % queue->q_size] = new_job;
    pthread_cond_signal(&queue->get_condv);
    pthread_mutex_unlock(&queue->queue_mtx);

    pthread_mutex_lock(&main_server->job_hist_mtx);
    main_server->job_array[job_history->id % JOB_HISTORY_MAX] = job_history;
    pthread_mutex_unlock(&main_server->job_hist_mtx);

    submittedResponse(new_job->id, sock);
}

void jobState(JobID *jobid, MainServer_t *main_server, int sockfd){
    Msg pkg;
    Job_State job_state;
    int idx;

    pthread_mutex_lock(&main_server->job_hist_mtx);
    idx = findJob(jobid, main_server);
    if (idx < 0 || idx >= JOB_HISTORY_MAX ){
        pthread_mutex_unlock(&main_server->job_hist_mtx);
        setBadParams(&pkg);
    } else {

        job_state.state = main_server->job_array[idx]->state;
        job_state.exit_value = main_server->job_array[idx]->exit_status;
        pthread_mutex_unlock(&main_server->job_hist_mtx);
        job_state.jobid = *jobid;
        setJobState(&pkg,job_state);
    }
    int err = 0;
    if ( (err = sendMsg(sockfd,&pkg)) == -1 ){
        perror("ERROR sending msg");
    }
}


void queueState(const QueueID *queueid, MainServer_t *main_server, int sockfd){
    Msg pkg;
    Queue_State queue_state;
    if ( queueid->queue_id >= NUM_QUEUE_MAX ){
        setBadParams(&pkg);
    } else {
        // pthread_mutex_lock(&main_server->main_server_mtx);
        pthread_mutex_lock(&main_server->queue[queueid->queue_id]->queue_mtx);
        queue_state.state = main_server->queue[queueid->queue_id]->state;
        pthread_mutex_unlock(&main_server->queue[queueid->queue_id]->queue_mtx);
        // pthread_mutex_unlock(&main_server->main_server_mtx);
        queue_state.queueid = *queueid;
        setQueueState(&pkg,queue_state);
    }

    int err = 0;
    if ( (err = sendMsg(sockfd,&pkg)) == -1 ){
        perror("ERROR sending msg");
    }
}

void unsubmit(JobID *jobid, MainServer_t *main_server, int sockfd){
    Msg pkg;
    int history_idx;
    uint8_t q_idx;

    pthread_mutex_lock(&main_server->job_hist_mtx);
    history_idx = findJob(jobid, main_server);

    if ( history_idx < 0 ){
        pthread_mutex_unlock(&main_server->job_hist_mtx);
        setBadParams(&pkg);
        int err = 0;
        if ( (err = sendMsg(sockfd,&pkg)) == -1 ){
            perror("ERROR sending msg");
        }

        return;
    }

    q_idx = main_server->job_array[history_idx]->queue_idx;
    pthread_mutex_unlock(&main_server->job_hist_mtx);

    pthread_mutex_lock(&main_server->main_server_mtx);
    Queue_t *queue = main_server->queue[q_idx];
    pthread_mutex_unlock(&main_server->main_server_mtx);

    removeJob(jobid,queue);
    
    pthread_mutex_lock(&main_server->job_hist_mtx);
    history_idx = findJob(jobid, main_server);
    main_server->job_array[history_idx]->state = STATE_UNSUBMITED;
    pthread_mutex_unlock(&main_server->job_hist_mtx);

    setUnsubmit(&pkg,*jobid);
    int err = 0;
    if ( (err = sendMsg(sockfd,&pkg)) == -1 ){
        perror("ERROR sending msg");
    }


}

void removeJob(const JobID *jobid, Queue_t *queue){
    int idx;
    pthread_mutex_lock(&queue->queue_mtx);

    if( (queue->put_idx - queue->get_idx) != 0 ){

        idx = queue->get_idx % queue->q_size;
        while (idx < queue->put_idx % queue->q_size){
            if ( queue->jobs[idx]->id == jobid->job_id){
                break;
            }
            idx++;
        }
        while ( idx < (queue->put_idx % queue->q_size - 1)){
            queue->jobs[idx] = queue->jobs[idx+1];
            idx++;
        }
        queue->put_idx--;
        if ( queue->state == STATE_FULL ){
            queue->state = STATE_AVAIL;
        }
        pthread_cond_signal(&queue->put_condv);
    }

    pthread_mutex_unlock(&queue->queue_mtx);

}

int findJob(JobID *jobid, MainServer_t *main_server){
    int i = 0;

    while( main_server->job_array[i] != NULL ){
        if ( main_server->job_array[i]->id == jobid->job_id ){
            return i;
        }
        i++;
        if ( i == JOB_HISTORY_MAX ){
            break;
        }
    }

    return -1;

}

//Creates new conexion to server
void *threadNewExecServ(void *thr_arg){
    ThreadArg_t *thread_arg = (ThreadArg_t*)thr_arg;
    MainServer_t *main_server = thread_arg->main_server;
    struct sockaddr_in cli_addr;
    pthread_t thread;
    socklen_t clilen = sizeof(cli_addr);
    pthread_attr_t attr;
    int newsockfd;

    printf("server_accept_thread created \n");

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    while(1){
        newsockfd = accept(thread_arg->sockfd,(struct sockaddr *) &cli_addr, &clilen);
        if (newsockfd < 0){ 
            printf("EXEC_SERVER_th exiting \n");
            break;
        }
        ThreadArg_t *arg_thread_server = malloc(sizeof(ThreadArg_t));
        arg_thread_server->main_server = main_server;
        arg_thread_server->sockfd = newsockfd;

        int rc = pthread_create(&thread,&attr,attendExecServ,(void *)arg_thread_server);
        if( rc ) {
            char buff[64];
            strerror_r(rc,buff, sizeof(buff));
            printf("Problem creating server thread: %s \n", buff);
            close(newsockfd);
        }
    }
    pthread_attr_destroy(&attr);
    pthread_exit(NULL);

}

void *attendExecServ(void *server_args){
    ThreadArg_t *server_thread_arg = (ThreadArg_t *)server_args;
    MainServer_t *main_server = server_thread_arg->main_server;
    Msg pkg;
    int err;
    int flag = 1;

    while( flag ){
        if( (err = recvMsg(server_thread_arg->sockfd, &pkg)) == 1 ) {

            switch( pkg.hdr.type ) {
                case TYPE_JOB_REQ:
                    sendJob(server_thread_arg);
                    break;
                case TYPE_JOB_RUN:
                case TYPE_JOB_DONE:
                case TYPE_JOB_SIG:
                    changeStateJob(&pkg.payload.job_state, main_server);
                    break;
                case TYPE_SERVER_CLOSED:
                    printf("Connection to server closed ----- \n");
                    flag = 0;
                    break;
                default:
                    perror("Bad type msg");
            }
        }else{
            if (err == 0 ){
                printf("Socket closed. \n");
            }else{
                perror("ERROR on recvMsg - attendExecServer ----");
            }
            break;
        }

    }
    close(server_thread_arg->sockfd);
    free(server_thread_arg);
    pthread_exit(NULL);

}


void sendJob(ThreadArg_t *server_thread_arg){
    Msg pkg;
    uint32_t queue_idx;

    pthread_mutex_lock(&server_thread_arg->main_server->main_server_mtx);
    queue_idx = server_thread_arg->main_server->queue2get++ % QUEUE_SZ;
    pthread_mutex_unlock(&server_thread_arg->main_server->main_server_mtx);

    Queue_t *queue = server_thread_arg->main_server->queue[queue_idx % NUM_QUEUE_MAX];
    
    pthread_mutex_lock(&queue->queue_mtx);
    while ( (queue->put_idx - queue->get_idx) == 0 ) {
        queue->state = STATE_AVAIL;
        pthread_cond_wait(&queue->get_condv, &queue->queue_mtx);
    }
    Job_t *job = queue->jobs[queue->get_idx++ % queue->q_size];
    pthread_cond_signal(&queue->put_condv);
    pthread_mutex_unlock(&queue->queue_mtx);

    if ( job->ipaddr == 0 ){
        setFileExecNoInter(&pkg, job->exec_file, job->working_dir,job->id);
    } else {
        struct  in_addr addr;
        addr.s_addr = (in_addr_t)job->ipaddr;
        setFileExecInter(&pkg, job->exec_file, job->working_dir, addr, job->port, job->id);
    }

    int err = 0;
    if ( (err = sendMsg(server_thread_arg->sockfd,&pkg)) == -1 ){
        perror("ERROR sending msg");
    }


}

// void updateStateJob(Job_State jstate, MainServer_t *main_server){
//     changeStateJob(&jstate, main_server);

// }


void changeStateJob(Job_State *jstate, MainServer_t *main_server){
    int hist_idx;
    pthread_mutex_lock(&main_server->job_hist_mtx);
    hist_idx = findJob(&jstate->jobid, main_server);
    if ( hist_idx >= 0 && hist_idx < JOB_HISTORY_MAX ){
        JobHistory_t *job =  main_server->job_array[hist_idx];
        job->state = jstate->state;
        job->exit_status = jstate->exit_value;
    }

    pthread_mutex_unlock(&main_server->job_hist_mtx);

}

void printHistoryJobs(MainServer_t* main_server){
    int i;
    printf(" -----------\n");
    printf(" HistoryJobs\n");
    pthread_mutex_lock(&main_server->job_hist_mtx);
    
    for (i = 0; i < main_server->history_putidx % JOB_HISTORY_MAX; i++){
        printf("JobID: %d \n", main_server->job_array[i]->id);
        printf("Queue: %d \n", main_server->job_array[i]->queue_idx);
        printf("State: %d \n", main_server->job_array[i]->state);
        printf(" - - - - - \n");
    }
    pthread_mutex_unlock(&main_server->job_hist_mtx);
    printf(" -----------\n");
}

void printQueueJobs(Queue_t* queue){
    int i;
    printf(" -----------\n");
    printf(" Jobs Queue\n");
    pthread_mutex_lock(&queue->queue_mtx);
    for (i = 0; i < queue->put_idx - queue->get_idx; i++){
        
        printf("JobID: %d \n", queue->jobs[i + queue->get_idx % queue->q_size]->id);
        printf("File: %s", queue->jobs[i + queue->get_idx % queue->q_size]->exec_file);
        printf("Dir: %s\n", queue->jobs[i + queue->get_idx % queue->q_size]->working_dir);
        printf(" - - - - - \n");
    }
    pthread_mutex_unlock(&queue->queue_mtx);
    printf(" -----------\n");
}










/************************/
/* Functions for select */
/************************/


int searchMax(SocketStock_t *stock){
    int max = stock->fds_container_server[0];
    for(int i = 1; i<stock->count_socket_server; i++){
        if (stock->fds_container_server[i]>max){
            max = stock->fds_container_server[i];
        }
    }
    return max;
}

// void cleanStock(SocketStock_t *stock){
void cleanStock(int *fds_container[], int *count_socket){
    for (int i = 0; i<*count_socket; i++){
        if (*fds_container[i] == -1){
            for (int j = i; j<(*count_socket)-1; j++){
                *fds_container[j] = *fds_container[j+1];
            }
            (*count_socket)--;
            cleanStock(fds_container,count_socket);
            break;
        }
    }
}

void upStock(SocketStock_t *stock, int new_sockfd, int type){
    if (type == 0){
        stock->fds_container_server[stock->count_socket_server++] = new_sockfd;
    }else{
        stock->fds_container_client[stock->count_socket_client++] = new_sockfd;
    }
    
}

void showStock(SocketStock_t stock){
    printf("Stock of fds_server-> ");
    for(int i = 0; i<stock.count_socket_server;i++){
        printf("%d ",stock.fds_container_server[i]);
    }
    printf("|| \n");
    printf("Stock of fds_client-> ");
    for(int i = 0; i<stock.count_socket_client;i++){
        printf("%d ",stock.fds_container_client[i]);
    }
    printf("|| \n");

}