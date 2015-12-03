/* A simple server in the internet domain using TCP
   The port number is passed as an argument 
   This version runs forever, forking off a separate 
   process for each connection
*/
#include <stdio.h>
#include <sys/types.h>   // definitions of a number of data types used in socket.h and netinet/in.h
#include <sys/socket.h>  // definitions of structures needed for sockets, e.g. sockaddr
#include <netinet/in.h>  // constants and structures needed for internet domain addresses, e.g. sockaddr_in
#include <stdlib.h>
#include <strings.h>
#include <sys/wait.h>	/* for the waitpid() system call */
#include <signal.h>	/* signal name macros, and the kill() prototype */
#include <string.h>
#include <time.h> 
#include <sys/stat.h>
#include <fcntl.h>

void sigchld_handler(int s)
{
    while(waitpid(-1, NULL, WNOHANG) > 0);
}

void dostuff(int); /* function prototype */
void error(char *msg)
{
    perror(msg);
    exit(1);
}

int main(int argc, char *argv[])
{
     int sockfd, newsockfd, portno, pid;
     socklen_t clilen;
     struct sockaddr_in serv_addr, cli_addr;
     struct sigaction sa;          // for signal SIGCHLD

     if (argc < 2) {
         fprintf(stderr,"ERROR, no port provided\n");
         exit(1);
     }
     sockfd = socket(AF_INET, SOCK_STREAM, 0);
     if (sockfd < 0) 
        error("ERROR opening socket");
     bzero((char *) &serv_addr, sizeof(serv_addr));
     portno = atoi(argv[1]);
     serv_addr.sin_family = AF_INET;
     serv_addr.sin_addr.s_addr = INADDR_ANY;
     serv_addr.sin_port = htons(portno);
     
     if (bind(sockfd, (struct sockaddr *) &serv_addr,
              sizeof(serv_addr)) < 0) 
              error("ERROR on binding");
     
     listen(sockfd,5);
     
     clilen = sizeof(cli_addr);
     
     /****** Kill Zombie Processes ******/
     sa.sa_handler = sigchld_handler; // reap all dead processes
     sigemptyset(&sa.sa_mask);
     sa.sa_flags = SA_RESTART;
     if (sigaction(SIGCHLD, &sa, NULL) == -1) {
         perror("sigaction");
         exit(1);
     }
     /*********************************/
     
     while (1) {
         newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
         
         if (newsockfd < 0) 
             error("ERROR on accept");
         
         pid = fork(); //create a new process
         if (pid < 0)
             error("ERROR on fork");
         
         if (pid == 0)  { // fork() returns a value of 0 to the child process
             close(sockfd);
             dostuff(newsockfd);
             exit(0);
         }
         else //returns the process ID of the child process to the parent
             close(newsockfd); // parent doesn't need this 
     } /* end of while */
     return 0; /* we never get here */
}

/******** DOSTUFF() *********************
 There is a separate instance of this function 
 for each connection.  It handles all communication
 once a connnection has been established.
 *****************************************/

typedef struct ResponseMessage
{
	char *status;
	char **headers;
	char *data;
} ResponseMessage;

static const int numHeaders = 10;
ResponseMessage* newMessage()
{
	int i;
	struct ResponseMessage *msg = (ResponseMessage*)malloc(sizeof(ResponseMessage*));
	msg->headers = (char**)malloc(sizeof(char**) * numHeaders);
	for(i = 0; i < numHeaders; i++)
	{
		msg->headers[i] = NULL;
		
	}
	msg->status = NULL;
	msg->data = NULL;
	
	return msg;
}

void sendMessage(int sock, ResponseMessage *msg, int fileSize)
{
	char **temp;
	int n;
	n = write(sock, msg->status, strlen(msg->status));
	if (n < 0) 
		error("ERROR writing to socket");
	n = write(sock, "\n", 1);

	for(temp = msg->headers; temp[0] != NULL; temp++)
	{
		n = write(sock, temp[0], strlen(temp[0]));
		if (n < 0) 
			error("ERROR writing to socket");
		n = write(sock, "\n", 1);
	}

	n = write(sock, "\n", 1);

	n = write(sock, msg->data, fileSize);
	if (n < 0) error("ERROR writing to socket");
}

void addHeaderLine(ResponseMessage *msg, char* header)
{
	int i;
	for(i = 0; i < numHeaders; i++)
	{
		if(msg->headers[i] == NULL)
			break;		
	} 
	msg->headers[i] = header;
}

/*
static char response[3000] = 
"HTTP/1.1 200 OK\n\
Connection: close\n\
Date: ";

static char response1[3000] = 
"\n\
Server: Custom server\n\
Last-Modified: \n\
Content-Type: text/html\n\
\n\
<html><head><title>Hello, World</title></head>\
<body><h1>Hello, World!</h1>   </body></html>";

*/


void dostuff (int sock)
{
   int n;
   char buffer[512];

	time_t rawtime;
	struct tm * timeinfo;
	char date[80];
	char lastMod[80];
	FILE* file;
	char *fileName;
	char *fileType;
	char sizeHeader[40];
	int fileSize;
char c;
	struct stat st;

	
