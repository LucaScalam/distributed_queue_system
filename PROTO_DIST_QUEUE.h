#ifndef PROTO_DIST_QUEUE_H_
#define PROTO_DIST_QUEUE_H_ 1

#include <stdint.h>
#include <assert.h>
#include <string.h>
#include <arpa/inet.h>

#define SZ_PATH 256
#define MAX_ARGS 20

typedef enum {
    VER_1 = 0x8001,
} Version;

typedef enum {
    TYPE_SUB_NO_INTER,                  //cli to main_server
    TYPE_SUB_INTERACTIVE,               //cli to main_server
    TYPE_SUB_RESP,                      //main_server to cli
    TYPE_JOBSTATE,                      //cli to main_server    
    TYPE_JOBSTATE_RESP,                 //main_server to cli
    TYPE_QUESTATE,                      //cli to main_server
    TYPE_QUESTATE_RESP,                 //main_server to cli
    TYPE_UNSUB,                         //cli to main_server
    TYPE_UNSUB_RESP,                    //main_server to cli
    TYPE_SENDJOB_NOINTER,               //main_server to exec_server
    TYPE_SENDJOB_INTER,                 //main_server to exec_server
    TYPE_JOB_REQ,                       //exec_server to main_server
    TYPE_JOB_DONE,                      //exec_server to main_server
    TYPE_JOB_RUN,                       //exec_server to main_server
    TYPE_JOB_SIG,                       //exec_server to main_server
    TYPE_CLIENT_CLOSED,                 //cli to main_server
    TYPE_SERVER_CLOSED,                 //exec_server to main_server
    TYPE_BAD_PARAMS                     //main_server to cli
} Type;

typedef struct __attribute__((__packed__)) 
{
    uint16_t sz8;
    uint16_t version;
    uint8_t  type;
} Header;

typedef struct __attribute__((__packed__)) 
{
    char exec_file[SZ_PATH * MAX_ARGS];
    char working_dir[SZ_PATH];
} File_Exec;

typedef struct __attribute__((__packed__)) 
{
    uint16_t job_id;
} JobID;

typedef struct __attribute__((__packed__)) 
{
    uint16_t queue_id;

} QueueID;

typedef enum {
    STATE_WAIT,        
    STATE_RUN,         
    STATE_FINISHED,     
    STATE_UNSUBMITED,
    STATE_SIGNALED,
    // STATE_WAIT_INTER,        
    // STATE_RUN_INTER,         
    // STATE_FINISHED_INTER,     
    // STATE_UNSUBMITED_INTER,
    // STATE_SIGNALED_INTER,
} JOB_STATE;

typedef enum {
    STATE_AVAIL,        
    STATE_FULL,         
} QUEUE_STATE;

typedef struct __attribute__((__packed__)) 
{
    uint8_t state;
    JobID jobid;
    uint8_t exit_value;
} Job_State;

typedef struct __attribute__((__packed__)) 
{
    uint8_t state;
    QueueID queueid;
} Queue_State;

typedef struct __attribute__((__packed__)) 
{
    JobID jobid;
    File_Exec file_exec;
} Job_Exec;

typedef struct __attribute__((__packed__)) 
{
    JobID jobid;
    uint16_t port;
    uint32_t ipaddr;
    File_Exec file_exec;
} Job_Exec_Addr;

typedef struct __attribute__((__packed__))
{
    Header hdr;
    union __attribute__((__packed__)) {
        JobID jobid;
        QueueID queueid;
        Job_State job_state;
        Queue_State queue_state;
        Job_Exec job_exec;
        Job_Exec_Addr job_exec_addr;
    } payload;
} Msg;

//inline is for simple functions
inline static uint16_t getVersion(const Header *hdr)
{
    return ntohs(hdr->version);
}

inline static uint16_t getSize8(const Header *hdr)
{
    return ntohs(hdr->sz8);
}

inline static void setFileExecNoInter(Msg *msg, char file_ex[], char working_dir[], uint16_t job_id)
{   
    msg->hdr.version = htons(VER_1);
    msg->hdr.type = TYPE_SENDJOB_NOINTER;
    msg->hdr.sz8 = htons(sizeof(Header) + sizeof(JobID) + sizeof(File_Exec));
    strncpy(msg->payload.job_exec.file_exec.exec_file,file_ex,SZ_PATH*MAX_ARGS);
    strncpy(msg->payload.job_exec.file_exec.working_dir,working_dir,SZ_PATH);
    msg->payload.job_exec.jobid.job_id = job_id;
}

inline static void setFileExecInter(Msg *msg, char file_ex[], char working_dir[],struct  in_addr addr,in_port_t port, uint16_t job_id)
{   
    msg->hdr.version = htons(VER_1);
    msg->hdr.type = TYPE_SENDJOB_INTER;
    msg->hdr.sz8 = htons(sizeof(Header) + sizeof(JobID) + sizeof(uint32_t) + sizeof(uint16_t) + sizeof(File_Exec));
    strncpy(msg->payload.job_exec_addr.file_exec.exec_file,file_ex,SZ_PATH*MAX_ARGS);
    strncpy(msg->payload.job_exec_addr.file_exec.working_dir,working_dir,SZ_PATH);
    msg->payload.job_exec_addr.jobid.job_id = job_id;
    msg->payload.job_exec_addr.ipaddr = (uint32_t) addr.s_addr;
    msg->payload.job_exec_addr.port = (uint16_t)port;
}

inline static void setFileExecNoInter_CLIENT(Msg *msg, char file_ex[], char working_dir[], uint16_t job_id)
{   
    msg->hdr.version = htons(VER_1);
    msg->hdr.type = TYPE_SUB_NO_INTER;
    msg->hdr.sz8 = htons(sizeof(Header) + sizeof(JobID) + sizeof(File_Exec));
    strncpy(msg->payload.job_exec.file_exec.exec_file,file_ex, SZ_PATH*MAX_ARGS);
    strncpy(msg->payload.job_exec.file_exec.working_dir,working_dir, SZ_PATH);
    msg->payload.job_exec.jobid.job_id = job_id;
}

inline static void setFileExecInter_CLIENT(Msg *msg, char file_ex[], char working_dir[],struct  in_addr addr,in_port_t port, uint16_t job_id)
{   
    msg->hdr.version = htons(VER_1);
    msg->hdr.type = TYPE_SUB_INTERACTIVE;
    msg->hdr.sz8 = htons(sizeof(Header) + sizeof(JobID) + sizeof(uint32_t) + sizeof(uint16_t) + sizeof(File_Exec));
    strncpy(msg->payload.job_exec_addr.file_exec.exec_file,file_ex,SZ_PATH*MAX_ARGS);
    strncpy(msg->payload.job_exec_addr.file_exec.working_dir,working_dir,SZ_PATH);
    msg->payload.job_exec_addr.jobid.job_id = job_id;
    msg->payload.job_exec_addr.ipaddr = (uint32_t) addr.s_addr;
    msg->payload.job_exec_addr.port = (uint16_t)port;

}

inline static void setJobState(Msg *msg, Job_State job_state)
{   
    msg->hdr.version = htons(VER_1);
    msg->hdr.type = TYPE_JOBSTATE_RESP;
    msg->hdr.sz8 = htons(sizeof(Header) + sizeof(Job_State));
    msg->payload.job_state.jobid = job_state.jobid;
    msg->payload.job_state.state = job_state.state;
    msg->payload.job_state.exit_value = job_state.exit_value;
}

inline static void setQueueState(Msg *msg, Queue_State queue_state)
{   
    msg->hdr.version = htons(VER_1);
    msg->hdr.type = TYPE_QUESTATE_RESP;
    msg->hdr.sz8 = htons(sizeof(Header) + sizeof(Queue_State));
    msg->payload.queue_state.queueid = queue_state.queueid;
    msg->payload.queue_state.state = queue_state.state;
}

inline static void setUnsubmit(Msg *msg, JobID jobid)
{   
    msg->hdr.version = htons(VER_1);
    msg->hdr.type = TYPE_UNSUB_RESP;
    msg->payload.jobid = jobid;
    msg->hdr.sz8 = htons(sizeof(Header) + sizeof(JobID));
}

inline static void setBadParams(Msg *msg)
{   
    msg->hdr.version = htons(VER_1);
    msg->hdr.type = TYPE_BAD_PARAMS;
    msg->hdr.sz8 = htons(sizeof(Header));
}

inline static JobID getJobID(Msg *pkg)
{   
    return pkg->payload.jobid;
}

inline static Job_State getJobState(Msg *pkg){
    return pkg->payload.job_state;
}

inline static Queue_State getQueueState(Msg *pkg){
    return pkg->payload.queue_state;
}

/***** CLIENT *****/

inline static void setClosedConnection(Msg *msg)
{   
    msg->hdr.version = htons(VER_1);
    msg->hdr.type = TYPE_CLIENT_CLOSED;
    msg->hdr.sz8 = htons(sizeof(Header));
}

inline static void setJobID(Msg *msg, JobID jobid)
{   
    msg->hdr.version = htons(VER_1);
    msg->hdr.type = TYPE_SUB_RESP;
    msg->payload.jobid = jobid;
    msg->hdr.sz8 = htons(sizeof(Header) + sizeof(JobID));
}

inline static void setJobState_CLIENT(Msg *msg, JobID jobid)
{   
    msg->hdr.version = htons(VER_1);
    msg->hdr.type = TYPE_JOBSTATE;
    msg->payload.jobid = jobid;
    msg->hdr.sz8 = htons(sizeof(Header) + sizeof(JobID));
}

inline static void setQueueState_CLIENT(Msg *msg, QueueID queueid)
{   
    msg->hdr.version = htons(VER_1);
    msg->hdr.type = TYPE_QUESTATE;
    msg->payload.queueid = queueid;
    msg->hdr.sz8 = htons(sizeof(Header) + sizeof(QueueID));
}

inline static void setUnsubmit_CLIENT(Msg *msg, JobID jobid)
{   
    msg->hdr.version = htons(VER_1);
    msg->hdr.type = TYPE_UNSUB;
    msg->payload.jobid = jobid;
    msg->hdr.sz8 = htons(sizeof(Header) + sizeof(JobID));
}

void printType(const Msg *m);
int sendMsg(int sockfd, const Msg *msg);
int recvMsg(int sockfd, Msg *msg);

/***** exec_server *****/

inline static void setClosedConnection_ExecServer(Msg *msg)
{   
    msg->hdr.version = htons(VER_1);
    msg->hdr.type = TYPE_SERVER_CLOSED;
    msg->hdr.sz8 = htons(sizeof(Header));
}

inline static void setJobReq(Msg *msg)
{   
    msg->hdr.version = htons(VER_1);
    msg->hdr.type = TYPE_JOB_REQ;
    msg->hdr.sz8 = htons(sizeof(Header));
}

inline static void setJobRun(Msg *msg, JobID jobid)
{   
    msg->hdr.version = htons(VER_1);
    msg->hdr.type = TYPE_JOB_RUN;
    msg->hdr.sz8 = htons(sizeof(Header) + sizeof(Job_State));
    msg->payload.job_state.jobid.job_id = jobid.job_id;
    msg->payload.job_state.state = STATE_RUN;
    msg->payload.job_state.exit_value = 0;
}

inline static void setJobDone(Msg *msg, JobID jobid, uint8_t e_status)
{   
    msg->hdr.version = htons(VER_1);
    msg->hdr.type = TYPE_JOB_DONE;
    msg->hdr.sz8 = htons(sizeof(Header) + sizeof(Job_State));
    msg->payload.job_state.jobid.job_id = jobid.job_id;
    msg->payload.job_state.state = STATE_FINISHED;
    msg->payload.job_state.exit_value = e_status;
}

inline static void setJobSignaled(Msg *msg, JobID jobid, uint8_t e_status)
{   
    msg->hdr.version = htons(VER_1);
    msg->hdr.type = TYPE_JOB_SIG;
    msg->hdr.sz8 = htons(sizeof(Header) + sizeof(Job_State));
    msg->payload.job_state.jobid.job_id = jobid.job_id;
    msg->payload.job_state.state = STATE_SIGNALED;
    msg->payload.job_state.exit_value = e_status;
}

#endif // PROTO_DIST_QUEUE_H_