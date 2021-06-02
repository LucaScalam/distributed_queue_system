#include "PROTO_DIST_QUEUE.h"


#define MAX_JOBS_QUEUE 15     //Number max of jobs for each queue
#define JOB_HISTORY_MAX 10       //Number of jobs in the history array
#define NUM_QUEUE_MAX 1         //Number of queues


typedef struct {
    uint16_t id;
    uint32_t ipaddr;
    uint8_t state;
    uint8_t exit_status;
    uint8_t queue_idx;
    time_t submitTime;
    time_t startProcTime;
    time_t endProcTime;
} JobHistory_t;


typedef struct {
    uint16_t id;
    uint16_t port;
    uint32_t ipaddr;
    char exec_file[SZ_PATH*NUM_PATH];
    char working_dir[SZ_PATH];
} Job_t;

typedef struct {
    pthread_mutex_t queue_mtx;
    pthread_cond_t get_condv;
    pthread_cond_t put_condv;
    Job_t *jobs[MAX_JOBS_QUEUE];
    unsigned put_idx;
    unsigned get_idx;
    uint16_t id;
    uint8_t state;
} Queue_t;

typedef struct {
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
void submitNoInter(const Msg *pkg, MainServer_t *main_server, int sock);

//Submits interactive job
void submitInter(const Msg *pkg, MainServer_t *main_server, int sock);

//Function used by submitIter and submitNoIter to create job_history
//and calls putJob
void newJob(Job_t *new_job, MainServer_t *main_server, int sock);

//Function to put a jub into queue
void putJob(Queue_t *queue, Job_t *new_job);

//Sends pkg with job state
void jobState(uint16_t jobid, MainServer_t *main_server, int sockfd);

//sends pkg with queue state
void queueState(QueueID queueid, MainServer_t *main_server, int sockfd);

//Removes job from queue and sends pkg with queue state
void unsubmit(uint16_t jobid, MainServer_t *main_server, int sockfd);

//Removes job from queue
void removeJob(uint16_t jobid, Queue_t *queue);

//Sends job to exec_server. Returns 1 if OK.
//Returns 0 in other case.
int sendJob(ThreadArg_t *server_thread_arg);

//Changes state of Job
void changeStateJob( const Msg *pkg, MainServer_t *main_server);

//Shows jobs info in main_server
void printHistoryJobs(MainServer_t* main_server);

//Shows jobs info in queue
void printQueueJobs(Queue_t* queue);

//Sends submitted response to client
void submittedResponse(uint16_t jobid, int sock);

// returns idx of job_id from main_server->job_array
// if no job is found, returns -1
// Important: main_server->job_hist_mtx needs to be locked
// before the calling and it will need to be unlocked after the calling
int findJob(uint16_t jobid, MainServer_t *main_server);

//Sends check msg to exec_server and waits for ack
//If everything was OK, returns 1.
//Another case, returns 0;
int checkServerStatus(int sockfd);

//Puts job back into queue, calling putJob()
void putJobBack(MainServer_t* main_server,Job_t *job);

//Setter for params of a new JobHistory_t
void setParamsNewJobHistory(JobHistory_t *job_history, uint16_t queue_idx, uint32_t ipaddr);
