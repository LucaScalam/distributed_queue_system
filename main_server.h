#include "PROTO_DIST_QUEUE.h"


#define MAX_JOBS_QUEUE 1024
#define JOB_HISTORY_MAX 256
#define MAX_SOCKET  100
#define NUM_QUEUE_MAX 2
#define QUEUE_SZ 20

typedef struct {
    int fds_container_server[MAX_SOCKET];
    int fds_container_client[MAX_SOCKET];    
    int count_socket_server;
    int count_socket_client;
}SocketStock_t;

typedef struct {
    uint16_t id;
    uint32_t ipaddr;
    uint8_t state;
    uint8_t exit_status;
    uint8_t queue_idx;
} JobHistory_t;


typedef struct {
    uint16_t id;
    uint16_t port;
    uint32_t ipaddr;
    char exec_file[SZ_PATH*4];
    char working_dir[SZ_PATH];
    // uint8_t state;
    // uint8_t exit_status;
    // uint8_t queue_idx;
} Job_t;

typedef struct {
    pthread_mutex_t queue_mtx;
    pthread_cond_t get_condv;
    pthread_cond_t put_condv;
    Job_t *jobs[MAX_JOBS_QUEUE];
    unsigned q_size;
    unsigned put_idx;
    unsigned get_idx;
    uint16_t id;
    uint8_t state;
} Queue_t;

typedef struct {
    // uint16_t jobID_counter;
    JobHistory_t *job_array[JOB_HISTORY_MAX];
    uint32_t history_putidx;
    pthread_mutex_t job_hist_mtx;
    Queue_t *queue[NUM_QUEUE_MAX];
    uint32_t queue2put;
    uint32_t queue2get;
    pthread_mutex_t main_server_mtx;
} MainServer_t;

typedef struct {
    MainServer_t *main_server;
    int sockfd;
} ThreadArg_t;

int searchMax(SocketStock_t *stock);

void cleanStock(int *fds_container[], int *count_socket);

void upStock(SocketStock_t *stock, int new_sockfd, int type);

void showStock(SocketStock_t stock);

//Creates socket in main_server
int socketCreate(char* argv);

//Inits main server
void MainServerInit(MainServer_t* main_server);

//Destroys main server
void MainServerDestroy(MainServer_t* main_server, pthread_t thread_client, pthread_t thread_exec_server);

//Creates queue to store Jobs
Queue_t *QueueInit(uint16_t q_id);

//Destroys queue
void QueueDestroy(Queue_t *q);

//Creates new conexion to client
void *threadNewClients(void *m_server);

//Attends Client msgs
void *attendClient(void *thr_arg);

//Creates new conexion to exec server
void *threadNewExecServ(void *m_server);

//Attends exec_server msg
void *attendExecServ(void *server_args);

//Submits No interactive job
void submitNoInter(Job_Exec *job_exec, MainServer_t *main_server, int sock);

//Submits interactive job
void submitInter(Job_Exec_Addr *job_exec_ad, MainServer_t *main_server, int sock);

//Function used by submitIter and submitNoIter to put job into queue
void putJob(Job_t *new_job, MainServer_t *main_server, int sock);

//Sends pkg with job state
void jobState(JobID *jobid, MainServer_t *main_server, int sockfd);

//sends pkg with queue state
void queueState(const QueueID *queueid, MainServer_t *main_server, int sockfd);

//Removes job from queue and sends pkg with queue state
void unsubmit(JobID *jobid, MainServer_t *main_server, int sockfd);

//Removes job from queue
void removeJob(const JobID *jobid, Queue_t *queue);

//Sends job to exec_server
void sendJob(ThreadArg_t *server_thread_arg);

//Changes state of Job
void changeStateJob(Job_State* jstate, MainServer_t *main_server);

//Shows jobs info in main_server
void printHistoryJobs(MainServer_t* main_server);

//Shows jobs info in queue
void printQueueJobs(Queue_t* queue);

//Sends submitted response to client
void submittedResponse(uint16_t jobid, int sock);

// //Updates state of job to STATE_DONE
// void updateStateJobDone(Job_State jstate, MainServer_t *main_server);

// //Updates state of job to STATE_RUN
// void updateStateJobRun(JobID jobid, MainServer_t *main_server);

// //Updates state of job to STATE_SIGNALED
// void updateStateJobSignal(JobID jobid, MainServer_t *main_server);

// void updateStateJob(Job_State jstate, MainServer_t *main_server);

// returns idx of job_id from main_server->job_array
// if no job is found, returns -1
// Important: main_server->job_hist_mtx needs to be locked
// before the calling and it will need to be unlocked after the calling
int findJob(JobID *jobid, MainServer_t *main_server);
