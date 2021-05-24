#include "PROTO_DIST_QUEUE.h"

//Gets pkg from main_server, creates child and
//runs exec_server_protocol_child for child and
//runs exec_server_protocol_father for father
void exec_server_protocol(int socket);

//Gets job to do
void consumeJob(Msg *pkg);

//Creates socket to connect directly to client. This is
//necesary for an interactive job
int socketPortConnect(uint16_t port, uint32_t address);

//Sends signal of job being executed to main_server, so
//main_server can change job state to finished
void exec_server_protocol_father(Msg *pkg, int socket_main);

// execute getIterJob() or getNoInterJob() to run execl()
void exec_server_protocol_child(Msg *pkg);

//Sets FDs end chdir() for iter job
int setFDsIter(Msg *pkg);

//Sets FDs end chdir() for no iter job
void setFDsNoIter(Msg *pkg);

//Father waits until child is finished or signaled
void fatherJobRoutine(JobID job_id,int socket_main);

//Creates child to execute job
void execJob(Msg *pkg, int socket_main);