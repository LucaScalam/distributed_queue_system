#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <pthread.h>
#include <assert.h>
#include <time.h>

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

        if ( strcmp(buff, "stop") == 0 ){
            break;
        } else if ( strcmp(buff, "t") == 0 ){

            printf("Total of jobs: %d \n",main_server.history_putidx);

        } else if ( strcmp(buff, "jobs") == 0 ){

            printHistoryJobs(&main_server);

        } else if ( strcmp(buff, "q") == 0 ){
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
    q->state = STATE_AVAIL;
    q->queue_mtx = queue_mtx;
    q->get_condv = get_condv;
    q->put_condv = put_condv;
    q->id = q_id;

    return q;
}

void QueueDestroy(Queue_t * q){
    int i;
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

    for ( i = 0; i < q->put_idx - q->get_idx; i++ ){
        free(q->jobs[(q->get_idx + i) % MAX_JOBS_QUEUE]);
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
            
            switch( getType(&pkg.hdr) ) {
                case TYPE_SUB_NO_INTER:
                    submitNoInter( &pkg, main_server, thread_arg->sockfd );
                    break;
                case TYPE_SUB_INTERACTIVE:
                    submitInter( &pkg, main_server, thread_arg->sockfd );
                    break;
                case TYPE_JOBSTATE:
                    jobState( getJobID(&pkg), main_server, thread_arg->sockfd );
                    break;
                case TYPE_QUESTATE:
                    queueState( getQueueID(&pkg), main_server, thread_arg->sockfd );
                    break;
                case TYPE_UNSUB:
                    unsubmit( getJobID(&pkg), main_server, thread_arg->sockfd );
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
void submitNoInter(const Msg *pkg, MainServer_t *main_server, int sock){ 
    const File_Exec *file_exec = getExec_fileExec(pkg);

    Job_t *new_job = malloc(sizeof(Job_t));
    strncpy(new_job->exec_file, file_exec->exec_file, SZ_PATH*NUM_PATH);
    strncpy(new_job->working_dir, file_exec->working_dir, SZ_PATH);
    new_job->port = 0;
    new_job->ipaddr = 0;

    newJob(new_job, main_server, sock);
}

//Submits interactive job
void submitInter(const Msg *pkg, MainServer_t *main_server, int sock){
    const File_Exec *file_exec = getExecAddr_fileExec(pkg);

    Job_t *new_job = malloc(sizeof(Job_t));
    strncpy(new_job->exec_file, file_exec->exec_file, SZ_PATH*NUM_PATH);
    strncpy(new_job->working_dir, file_exec->working_dir, SZ_PATH);
    new_job->port = getExecAddr_port(pkg);
    new_job->ipaddr = getExecAddr_ipadd(pkg);
    newJob(new_job, main_server, sock);

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

void newJob(Job_t *new_job, MainServer_t *main_server, int sock){
    uint16_t queue_idx;
    uint16_t new_jobid;
    JobHistory_t *job_history = malloc(sizeof(JobHistory_t));
    
    //Gets queue idx
    pthread_mutex_lock(&main_server->main_server_mtx);
    queue_idx = main_server->queue2put++ % NUM_QUEUE_MAX;    
    pthread_mutex_unlock(&main_server->main_server_mtx);
    Queue_t *queue = main_server->queue[queue_idx % NUM_QUEUE_MAX];

    setParamsNewJobHistory(job_history, queue_idx,new_job->ipaddr);

    //Gets new job id
    pthread_mutex_lock(&main_server->job_hist_mtx);
    job_history->id = main_server->history_putidx++;
    new_jobid = job_history->id;
    if ( job_history->id >= JOB_HISTORY_MAX ) {
        free(main_server->job_array[job_history->id % JOB_HISTORY_MAX]);
    }
    main_server->job_array[job_history->id % JOB_HISTORY_MAX] = job_history;
    pthread_mutex_unlock(&main_server->job_hist_mtx);

    submittedResponse(new_jobid, sock);

    new_job->id = new_jobid;
    putJob(queue, new_job);

    
}

void setParamsNewJobHistory(JobHistory_t *job_history, uint16_t queue_idx, uint32_t ipaddr){
    job_history->queue_idx = queue_idx;
    job_history->ipaddr = ipaddr;
    job_history->state = STATE_WAIT;
    job_history->startProcTime = 0;
    job_history->endProcTime = 0;
    time(&job_history->submitTime);
}

void putJob(Queue_t *queue, Job_t *new_job){
    pthread_mutex_lock(&queue->queue_mtx);
    while ( (queue->put_idx - queue->get_idx) == MAX_JOBS_QUEUE ) {
        pthread_cond_wait(&queue->put_condv, &queue->queue_mtx);
    }
    if ( (queue->put_idx - queue->get_idx) == MAX_JOBS_QUEUE - 1 ){
        queue->state = STATE_FULL;
    }
    
    queue->jobs[queue->put_idx++ % MAX_JOBS_QUEUE] = new_job;
    pthread_cond_signal(&queue->get_condv);    
    pthread_mutex_unlock(&queue->queue_mtx);
}

void jobState(uint16_t jobid, MainServer_t *main_server, int sockfd){
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
        job_state.jobid.job_id = jobid;
        setJobState(&pkg,job_state);
    }
    int err = 0;
    if ( (err = sendMsg(sockfd,&pkg)) == -1 ){
        perror("ERROR sending msg");
    }
}


void queueState(QueueID queueid, MainServer_t *main_server, int sockfd){
    Msg pkg;
    Queue_State queue_state;
    if ( queueid.queue_id >= NUM_QUEUE_MAX ){
        setBadParams(&pkg);
    } else {
        pthread_mutex_lock(&main_server->queue[queueid.queue_id]->queue_mtx);
        queue_state.state = main_server->queue[queueid.queue_id]->state;
        pthread_mutex_unlock(&main_server->queue[queueid.queue_id]->queue_mtx);
        queue_state.queueid = queueid;
        setQueueState(&pkg,queue_state);
    }

    int err = 0;
    if ( (err = sendMsg(sockfd,&pkg)) == -1 ){
        perror("ERROR sending msg");
    }
}

void unsubmit(uint16_t jobid, MainServer_t *main_server, int sockfd){
    Msg pkg;
    int history_idx;
    uint8_t q_idx;

    pthread_mutex_lock(&main_server->job_hist_mtx);
    history_idx = findJob(jobid, main_server);

    if ( history_idx < 0  || main_server->job_array[history_idx]->state != STATE_WAIT){
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

    setUnsubmit(&pkg,jobid);
    int err = 0;
    if ( (err = sendMsg(sockfd,&pkg)) == -1 ){
        perror("ERROR sending msg");
    }


}

void removeJob(uint16_t jobid, Queue_t *queue){
    int idx;
    pthread_mutex_lock(&queue->queue_mtx);

    if( (queue->put_idx - queue->get_idx) != 0 ){

        idx = queue->get_idx % MAX_JOBS_QUEUE;
        while (idx < queue->put_idx % MAX_JOBS_QUEUE){
            if ( queue->jobs[idx]->id == jobid ){
                free(queue->jobs[idx]);
                break;
            }
            idx++;
        }
        while ( idx < (queue->put_idx % MAX_JOBS_QUEUE - 1)){
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

int findJob(uint16_t jobid, MainServer_t *main_server){
    int i = 0;

    while( main_server->job_array[i] != NULL ){
        if ( main_server->job_array[i]->id == jobid ){
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

            switch( getType(&pkg.hdr) ) {
                case TYPE_JOB_REQ:
                    flag = sendJob(server_thread_arg);
                    break;
                case TYPE_JOB_RUN:
                case TYPE_JOB_DONE:
                case TYPE_JOB_SIG:
                    changeStateJob(&pkg,main_server);
                    break;
                case TYPE_SERVER_CLOSED:
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
    printf("Connection to server closed ----- \n");
    close(server_thread_arg->sockfd);
    free(server_thread_arg);
    pthread_exit(NULL);

}


int sendJob(ThreadArg_t *server_thread_arg){
    Msg pkg;
    uint32_t queue_idx;
    int err = 0;
    int ch_server;

    pthread_mutex_lock(&server_thread_arg->main_server->main_server_mtx);
    queue_idx = server_thread_arg->main_server->queue2get++ % NUM_QUEUE_MAX;
    pthread_mutex_unlock(&server_thread_arg->main_server->main_server_mtx);

    Queue_t *queue = server_thread_arg->main_server->queue[queue_idx % NUM_QUEUE_MAX];
    
    pthread_mutex_lock(&queue->queue_mtx);
    while ( (queue->put_idx - queue->get_idx) == 0 ) {
        pthread_cond_wait(&queue->get_condv, &queue->queue_mtx);
    }
    if ( queue->state == STATE_FULL ){
        queue->state = STATE_AVAIL;
    }
    Job_t *job = queue->jobs[queue->get_idx++ % MAX_JOBS_QUEUE];

    pthread_cond_signal(&queue->put_condv);
    pthread_mutex_unlock(&queue->queue_mtx);

    ch_server = checkServerStatus(server_thread_arg->sockfd);
    if ( ch_server == 0 ){
        putJobBack(server_thread_arg->main_server,job);
        return 0;
    }

    if ( job->ipaddr == 0 ){
        setFileExecNoInter(&pkg, job->exec_file, job->working_dir,job->id);
    } else {
        struct  in_addr addr;
        addr.s_addr = (in_addr_t)job->ipaddr;
        setFileExecInter(&pkg, job->exec_file, job->working_dir, addr, job->port, job->id);
    }

    err = sendMsg(server_thread_arg->sockfd,&pkg);
    if ( err == -1  || err == 0 ){
        perror("ERROR sending msg");
        putJobBack(server_thread_arg->main_server,job);
        return 0;
    } else {
        free(job);
    }

    return 1;
}

int checkServerStatus(int sockfd){
    Msg pkg;
    int err;
    fd_set rfds;
    struct timeval tv;

    setCheckExecServer(&pkg);

    err = sendMsg(sockfd,&pkg);
    if ( err == -1 ){
        perror("ERROR sending msg");
        return 0;
    } else if ( err == 0 ){
        perror("socket closed");
        return 0;
    }

    FD_ZERO(&rfds);
    FD_SET(sockfd, &rfds);
    tv.tv_sec = 5;
    tv.tv_usec = 0;
    

    int ret = select(sockfd+1, &rfds,NULL,NULL,&tv);
    if ( ret == -1 ){
        perror("select_exec_server");
        return 0;
    }

    if ( FD_ISSET(sockfd, &rfds) ){
    
        err = recvMsg(sockfd, &pkg);
        if ( err == -1 ){
            perror("ERROR recv msg");
            return 0;
        } else if ( err == 0 ){
            perror("socket closed");
            return 0;
        }

        if ( getType(&pkg.hdr) != TYPE_CH_EXEC_SER_ACK ){
            return 0;
        } else {
            return 1;
        }
    } else{
        printf("No ACK response from exec_server \n");
        return 0;
    }

}

void putJobBack(MainServer_t* main_server,Job_t *job){
    int queue_idx, history_idx;
    Queue_t *queue;
    //Gets queue idx and puts job into the queue again
    pthread_mutex_lock(&main_server->main_server_mtx);
    queue_idx = main_server->queue2put++ % NUM_QUEUE_MAX;    
    pthread_mutex_unlock(&main_server->main_server_mtx);
    queue = main_server->queue[queue_idx % NUM_QUEUE_MAX];
    putJob(queue, job);
    pthread_mutex_lock(&main_server->job_hist_mtx);
    history_idx = findJob(job->id, main_server);

    if( history_idx < 0 || history_idx >= JOB_HISTORY_MAX){
        pthread_mutex_unlock(&main_server->job_hist_mtx);
        free(job);
        return;
    } 

    main_server->job_array[history_idx]->queue_idx = queue_idx;
    pthread_mutex_unlock(&main_server->job_hist_mtx);

}


void changeStateJob( const Msg *pkg, MainServer_t *main_server){
    uint16_t job_id = getJobState_ID(pkg);
    uint8_t state = getJobState_State(pkg);
    uint8_t exit_value = getJobState_ExitValue(pkg);

    int hist_idx;
    pthread_mutex_lock(&main_server->job_hist_mtx);
    hist_idx = findJob(job_id, main_server);
    if ( hist_idx >= 0 && hist_idx < JOB_HISTORY_MAX ){
        JobHistory_t *job =  main_server->job_array[hist_idx];
        job->state = state;
        job->exit_status = exit_value;
        switch (state)
        {
            case STATE_FINISHED:
            case STATE_SIGNALED:
            case STATE_ERROR:
                time(&job->endProcTime);
                break;
            case STATE_RUN:
                time(&job->startProcTime);
                break;
            default:
                assert(0);
                break;
        }
    }
    pthread_mutex_unlock(&main_server->job_hist_mtx);
}

void printHistoryJobs(MainServer_t* main_server){
    int i, idx;
    printf(" -----------\n");
    printf(" HistoryJobs\n");
    pthread_mutex_lock(&main_server->job_hist_mtx);
    if ( main_server->history_putidx >= JOB_HISTORY_MAX ){
        idx = JOB_HISTORY_MAX;
    } else {
        idx = main_server->history_putidx;
    }
    for (i = 0; i < idx; i++){
        printf("idx: %d\n",i);
        printf("JobID: %d \n", main_server->job_array[i]->id);
        printf("Queue: %d \n", main_server->job_array[i]->queue_idx);
        printf("State: %d \n", main_server->job_array[i]->state);
        printf("Exit status: %d \n", main_server->job_array[i]->exit_status);
        printf("Submit time: %ld \n", main_server->job_array[i]->submitTime);
        printf("Start proc. time: %ld \n", main_server->job_array[i]->startProcTime);
        printf("End proc. time: %ld \n", main_server->job_array[i]->endProcTime);
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
    printf("size: %d\n", queue->put_idx - queue->get_idx);
    for (i = 0; i < queue->put_idx - queue->get_idx; i++){
        printf("JobID: %d \n", queue->jobs[(i+queue->get_idx) % MAX_JOBS_QUEUE]->id);
        printf("File: %s", queue->jobs[(i+queue->get_idx) % MAX_JOBS_QUEUE]->exec_file);
        printf("Dir: %s\n", queue->jobs[(i+queue->get_idx) % MAX_JOBS_QUEUE]->working_dir);
        printf(" - - - - - \n");
    }
    pthread_mutex_unlock(&queue->queue_mtx);
    printf(" -----------\n");
}





