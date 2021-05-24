#include "PROTO_DIST_QUEUE.h"


//Gets arguments from stdin and 
//creates pkg "setFileExecNoInter_CLIENT" to send
//to main_server
void getArgsSubmit(Msg *pkg, int sockfd);

//Gets arguments from stdin, creates socket to connect with exec_server 
//and creates pkg "setFileExecInter_CLIENT" to send
//to main_server
int getArgsSubmitInt(Msg *pkg, int sockfd);

//Gets response pkg from main_server
void client_protocol(int socket);

//Gets response from submitted job
void jobSubmittedResponse(Msg *pkg);

//Gets response from job state
void jobStateResponse(Msg *pkg);

//Gets response from queue state
void queueStateResponse(Msg *pkg);

//Gets response from unsubmited job
void unsubmitResponse(Msg *pkg);

//Parameters from msg sent were wrong
void badParamsResponse();

//blocks the client with accept() until
//exec_server connects throught socket "sock"
void client_accept(int sock);

//Create new socket to be connected to exec_server
//for interactive job
int socketCreate_connect(char* addr, char* port);

//Creates and sends pkg with job state request
void JobState(Msg *pkg, char *to_jobid, int sockfd);

//Creates and sends pkg with queue state request
void QueueState(Msg *pkg, char *to_queue_idx, int sockfd);

//Creates and sends pkg with jobid to unsubmit
void unsubmit(Msg *pkg, char *to_jobid, int sockfd);

//function 1 to test iter jobs
void interaction_1(int sockfd);

//function 2 to test iter jobs
void interaction_2(int sockfd);