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
/* HTTP Response Message Format */
typedef struct ResponseMessage
{
	char *status;
	char **headers;
	char *data;
} ResponseMessage;

/*
 *	Generate a blank HTTP response message
 */
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

/*
 *	Free the allocated message after it has been sent.
 */
void deleteMessage(ResponseMessage *msg)
{
	free(msg->headers);
	free(msg->data);
	free(msg);
}

/*
 *	Send an HTTP response to the client from the server after 
 *	formatting the message. Write the status code, headers, and 
 *	data content to the client socket.
 */
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

	if(msg->data != NULL && fileSize != 0)
		n = write(sock, msg->data, fileSize);
	if (n < 0) error("ERROR writing to socket");
}

/*
 * 	Add header line to HTTP response message.
 */
void addHeaderLine(ResponseMessage *msg, char* header)
{
	int i;
	for(i = 0; i < numHeaders - 1; i++)
	{
		if(msg->headers[i] == NULL)
			break;		
	} 
	if(i < numHeaders - 1)		//last element must be NULL
		msg->headers[i] = header;
}

///////////////////
static char rootResponse[200] = "<html><head><title>Something went wrong!</title></head>\
<body><h1>You need to specify a file to download.</h1></body></html>";

static char fileNotFoundResponse[200] = "<html><head><title>Something went wrong!</title></head>\
<body><h1>File cannot be found on server.</h1></body></html>";

static char wrongFileTypeResponse[200] = "<html><head><title>Something went wrong!</title></head>\
<body><h1>File type not supported.</h1></body></html>";

static char internalErrorResponse[200] = "<html><head><title>Something went wrong!</title></head>\
<body><h1>Internal error on server side.</h1></body></html>";

///////////////////
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
	char *httpVer;
	char sizeHeader[40];
	int fileSize;
	char c;
	struct stat st;

	////
	ResponseMessage *msg = newMessage();

	msg->status = "HTTP/1.1 200 OK";
	addHeaderLine(msg, "Connection: close");
	
////////	
   bzero(buffer,512);
   n = read(sock,buffer,511);
   if (n < 0) 
		error("ERROR reading from socket");
   printf("HTTP Request Message: \n%s\n",buffer);
////////


	/* Parse file name */
	fileName = strchr(buffer, '/');
	++fileName;			//++ to go over /
	if(fileName[0] == ' ')
	{
		msg->status = "HTTP/1.1 400 Bad Request";
		msg->data = rootResponse;
		sendMessage(sock, msg, strlen(msg->data));
		exit(0);
	}

	/* Parse file type */
	fileName = strtok(fileName, " ");	
	fileType = strchr(fileName, '.');
	++fileType;			//++ to go over .	
	
	/* Read file stats */
	if (stat(fileName, &st) == -1) 
	{
		printf("%s\n", fileName);
		perror("stat");

		msg->status = "HTTP/1.1 404 Not Found";
		msg->data = fileNotFoundResponse;
		sendMessage(sock, msg, strlen(msg->data));

		exit(0);
   	}

	/* Content-Type Header: check file type */
	if(!strcmp(fileType, "html"))
		addHeaderLine(msg, "Content-Type: text/html");
	else if(!strcmp(fileType, "jpg"))
		addHeaderLine(msg, "Content-Type: image/jpg");
	else if(!strcmp(fileType, "jpeg"))
		addHeaderLine(msg, "Content-Type: image/jpeg");
	else if(!strcmp(fileType, "gif"))
		addHeaderLine(msg, "Content-Type: image/gif");
	else
	{
		msg->status = "HTTP/1.1 400 Bad Request";
		msg->data = wrongFileTypeResponse;
		sendMessage(sock, msg, strlen(msg->data));
		exit(0);
	}

	/* Prepare header files */
	time (&rawtime); 
	timeinfo = gmtime (&rawtime);	
	strftime(date, 80, "Date: %a, %d %b %Y %X GMT", timeinfo);
	addHeaderLine(msg, date);

	addHeaderLine(msg, "Server: Custom server");

	timeinfo = gmtime (&st.st_mtime);	
	strftime(lastMod, 80, "Last-Modified: %a, %d %b %Y %X GMT", timeinfo);
	addHeaderLine(msg, lastMod);
	
	fileSize = (int)st.st_size;
	snprintf(sizeHeader, 40, "Content-Length: %d", fileSize);
	addHeaderLine(msg, sizeHeader);

	/* Read and send file data content */
	msg->data = (char*)malloc(sizeof(char) * (fileSize + 1));
	if(msg->data == NULL)
	{
		printf("Error malloc.\n");
		msg->data = internalErrorResponse;
		sendMessage(sock, msg, strlen(msg->data));
		exit(0);
	}
	
	file = fopen(fileName, "r");
	if (file != NULL) 
	{
		size_t newLen = fread(msg->data, sizeof(char), fileSize, file);
		if (newLen == 0)
		{
		    fputs("Error reading file", stderr);
			 msg->data = internalErrorResponse;
			 sendMessage(sock, msg, strlen(msg->data));
			 exit(0);
		}
		else
		   msg->data[newLen] = '\0'; 		

		fclose(file);
	}
	
	/* Send HTTP Response to the Client */
	sendMessage(sock, msg, fileSize);
	deleteMessage(msg);
}
