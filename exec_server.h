#include "PROTO_DIST_QUEUE.h"

//Gets pkg from main_server, creates child and
//runs exec_server_protocol_child for child and
//runs exec_server_protocol_father for father
//It returns 1 if everything was ok
int exec_server_protocol(int socket);

//Gets job to do
void consumeJob(const Msg *pkg);

//Creates socket to connect directly to client. This is
//necesary for an interactive job
int socketPortConnect(uint16_t port, uint32_t address);

//Sends signal of job being executed to main_server, so
//main_server can change job state to finished
void exec_server_protocol_father(const Msg *pkg, int socket_main);

// execute getIterJob() or getNoInterJob() to run execl()
void exec_server_protocol_child(const Msg *pkg);

//Sets FDs end chdir() for iter job. It returns new socket created
int setFDsIter(const Msg *pkg);

//Sets FDs end chdir() for no iter job
void setFDsNoIter(const Msg *pkg);

//Father waits until child is finished or signaled
void fatherJobRoutine(uint16_t job_id,int socket_main);

//Creates child to execute job. Returns 1 if everthing was OK
int execJob(const Msg *pkg, int socket_main);

//Reports error on fork()
void error_fork(const Msg *pkg, int socket_main);

//Creates pkg and sends it 
//returns 0 if there was a problem. Another case, 1.
int sendAck(int socket);

//Executes sendAck and exec_server_protocol
//returns 0 if there was a problem. Another case, 1.
int ackRoutine(int socket);

//Returns 0 if connection was closed correctly.
int closeConnecMain(int sockfd);

//Generates a job request and executes
//exec_server_protocol(). If everything was OK,
//returns 1
int jobRequest(int sockfd);