/*
** server.c -- a datagram sockets "server" demo
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "packet.h"
#include <sys/stat.h>
#include <math.h> 
#include <time.h>

#define MAXBUFLEN 100


void prepareDataPacket(struct Packet *dataPacket, int nextSeqNum, FILE *file, int packetCount, int fileSize)
{
	dataPacket->type = DAT;
	dataPacket->seqNum = nextSeqNum;
	dataPacket->dataSize = nextSeqNum == packetCount ?
				fileSize % DATASIZE : DATASIZE;
	fseek(file, (nextSeqNum - 1) * DATASIZE, SEEK_SET);
	size_t newLen = fread(dataPacket->data, sizeof(char), dataPacket->dataSize, file);
	if (newLen == 0)
	{
		fputs("Error reading file", stderr);
		exit(0);
	}
	else
	dataPacket->data[newLen] = '\0'; 		

}
// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(int argc, char *argv[])
{
	int sockfd;
	struct addrinfo hints, *servinfo, *p;
	int rv;
	int numbytes;
	struct sockaddr_storage their_addr;
	socklen_t addr_len;
	char s[INET6_ADDRSTRLEN];
	char *portNumber, *windowSize, *probLoss, *probCorr;
	
	int CWND;
	float pLoss, pCorr;

	if (argc != 5) {
		fprintf(stderr,"usage: server portNumber windowSize probLoss probCorr\n");
		exit(1);
	}
	portNumber = argv[1];
	windowSize = argv[2];
	probLoss = argv[3];
	probCorr = argv[4];

	/*
	portNumber = "4444";
	windowSize = "4";
	probLoss = "0.5";
	probCorr = "0.1";
	*/

	CWND = atoi(windowSize);
	pLoss = atof(probLoss);
	pCorr = atof(probCorr);
	
	if(CWND < 1 || CWND > 50)
	{
		fprintf(stderr,"usage: CWND should be between 1 and 50\n");
		exit(1);
	}

	if(pLoss < 0 || pLoss > 1)
	{
		fprintf(stderr,"usage: probLoss should be between 0 and 1\n");
		exit(1);
	}

	if(pCorr < 0 || pCorr > 1)
	{
		fprintf(stderr,"usage: probCorr should be between 0 and 1\n");
		exit(1);
	}

///////////////////////
///////////////////////
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC; // set to AF_INET to force IPv4
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_flags = AI_PASSIVE; // use my IP

	if ((rv = getaddrinfo(NULL, portNumber, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

	// loop through all the results and bind to the first we can
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
				p->ai_protocol)) == -1) {
			perror("server: socket");
			continue;
		}

		if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			perror("server: bind");
			continue;
		}

		break;
	}

	if (p == NULL) {
		fprintf(stderr, "server: failed to bind socket\n");
		return 2;
	}

	freeaddrinfo(servinfo);

	printf("server: waiting for client...\n");
///////////////////////////////
///////////////////////////////
	struct Packet reqPacket;
	addr_len = sizeof their_addr;
	if ((numbytes = recvfrom(sockfd, &reqPacket, sizeof(reqPacket) , 0,
		(struct sockaddr *)&their_addr, &addr_len)) == -1) {
		perror("recvfrom");
		exit(1);
	}

	const char *clientIP = inet_ntop(their_addr.ss_family,
			get_in_addr((struct sockaddr *)&their_addr),
			s, sizeof s);
	printf("server: got request packet from %s\n", clientIP);
	printf("server: packet is %d bytes long\n", numbytes);
	
	printf("server: request message is \"%s\", %d bytes\n", 
		reqPacket.data, reqPacket.dataSize);

	char *fileName = reqPacket.data;
	struct stat st;



	
	struct Packet dataPacket, ackPacket, finPacket;

	if (stat(fileName, &st) == -1) 
	{
		printf("File not found: %s\n", fileName);
		perror("stat");

		finPacket.type = FIN;
		finPacket.seqNum = 0;
		strcpy(finPacket.data, "File not found.");
		finPacket.dataSize = strlen(finPacket.data);
	
		if ((numbytes = sendto(sockfd, &finPacket, sizeof(finPacket), 0,
			(struct sockaddr *)&their_addr, addr_len)) == -1) {
				perror("server: sendto");
				exit(1);
			}
		printf("--> sent FIN to %s\n", clientIP);
		exit(0);
   }

	FILE *file = fopen(fileName, "rb");
	
	if(file == NULL)
	{
		printf("Cannot open file\n");
		
		finPacket.type = FIN;
		finPacket.seqNum = 0;
		strcpy(finPacket.data, "Cannot open file.");
		finPacket.dataSize = strlen(finPacket.data);
	
		if ((numbytes = sendto(sockfd, &finPacket, sizeof(finPacket), 0,
			(struct sockaddr *)&their_addr, addr_len)) == -1) {
				perror("server: sendto");
				exit(1);
			}
		printf("--> sent FIN to %s\n", clientIP);
		exit(0);
	}
	int base = 1;
	int inFlight = 0;
	int nextSeqNum = 1;

	fd_set readSet;	

	
	int packetCount = (int)ceil((float)st.st_size / DATASIZE);
	srand(time(NULL));
	while(1)
	{
		if(base > packetCount)	///zzzzzzzzzz
		{
	//		printf("base: %d, packetCount: %d\n", base, packetCount);
			printf("File transfer completed.\n");
			break;
		}
		int i, temp = nextSeqNum;

		for(i = 0; i < base + CWND - temp && nextSeqNum <= packetCount; i++)
		{
			prepareDataPacket(&dataPacket, nextSeqNum, file, packetCount, st.st_size);
			if ((numbytes = sendto(sockfd, &dataPacket, sizeof(dataPacket), 0,
						 (struct sockaddr *)&their_addr, addr_len)) == -1) {
					perror("server: sendto");
					exit(1);
			}

			printf("--> sent %d bytes to %s, \tSEQ #%d \n", dataPacket.dataSize, clientIP, dataPacket.seqNum);
	//		printf("message %d bytes to \"%s\"\n", dataPacket.dataSize, dataPacket.data);

			nextSeqNum++;
			inFlight++;
		}

//		printf("server: waiting for ACKs...\n");

		FD_ZERO(&readSet);
		FD_SET(sockfd, &readSet);
		struct timeval timeout = {1, 0}; //Timeout in 1 second
		int res = select(sockfd + 1, &readSet, NULL, NULL, &timeout);
		if(res < 0)
		{
			perror("select()");
		//	exit(1);
		}
		else if (res == 0)
		{
			printf("<-- Time out for ACK #%d\n", base + 1);
			nextSeqNum = base;
		}
		else
		{
			if ((numbytes = recvfrom(sockfd, &ackPacket, sizeof(ackPacket) , 0,
				(struct sockaddr *)&their_addr, &addr_len)) == -1) {
				perror("recvfrom");
				exit(1);
			}

			float loss = (float)(rand() % 101) / 100;
			float corrupt = (float)(rand() % 101) / 100;
			if(pLoss > 0 && loss <= pLoss)
				printf("<-- ACK #%d received, discarded as loss, base %d\n", ackPacket.seqNum, base);
			else
			{
				if(pCorr > 0 && corrupt <= pCorr)
					printf("<-- ACK #%d received, discarded as corruption, base %d\n", ackPacket.seqNum, base);
				else
				{
					printf("<-- received ACK #%d, base ", ackPacket.seqNum);
					if(ackPacket.seqNum > base)
					{	
						printf("changed from %d to %d\n", base, ackPacket.seqNum);
						base = ackPacket.seqNum;
					}
					else
						printf("%d unchanged\n", base);
				}
			}
		}
	}
////////////////////
////////////////////
	
	
	finPacket.type = FIN;
	finPacket.seqNum = nextSeqNum;
	strcpy(finPacket.data, "File sent.");
	finPacket.dataSize = strlen(finPacket.data);
	
	if ((numbytes = sendto(sockfd, &finPacket, sizeof(finPacket), 0,
		(struct sockaddr *)&their_addr, addr_len)) == -1) {
			perror("server: sendto");
			exit(1);
		}
	printf("--> sent FIN to %s\n", clientIP);

	while(1)
	{
		if ((numbytes = recvfrom(sockfd, &ackPacket, sizeof(ackPacket) , 0,
			(struct sockaddr *)&their_addr, &addr_len)) == -1) {
				perror("recvfrom");
				exit(1);
			}
		if(ackPacket.type == FIN)
		{
			printf("<-- received FIN from %s\n", clientIP);
			printf("FIN message is \"%s\"\n", ackPacket.data);
			printf("server closing down connection\n");
			break;
		}
		else
		{
				printf("<-- received something other than FIN from %s\n", clientIP);
		}
	}
	fclose(file);
	close(sockfd);

	return 0;
}

