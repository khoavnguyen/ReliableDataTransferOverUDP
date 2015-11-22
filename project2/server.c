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

#define MAXBUFLEN 100


void prepareDataPacket(struct Packet *dataPacket, int currSeqNum, FILE *file, int packetCount, int fileSize)
{
	dataPacket->type = DAT;
	dataPacket->seqNum = currSeqNum;
	dataPacket->dataSize = currSeqNum == packetCount ?
				fileSize % DATASIZE : DATASIZE;
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
	char *portnumber;
	
	int CWND;
/*
	if (argc != 3) {
		fprintf(stderr,"usage: server portnumber windowsize\n");
		exit(1);
	}
	portnumber = argv[1];
	CWND = argv[2];
*/
	portnumber = "4444";
	CWND = 3;


///////////////////////
///////////////////////
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC; // set to AF_INET to force IPv4
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_flags = AI_PASSIVE; // use my IP

	if ((rv = getaddrinfo(NULL, portnumber, &hints, &servinfo)) != 0) {
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

	if (stat(fileName, &st) == -1) 
	{
		printf("%s\n", fileName);
		perror("stat");

		exit(0);
   }

	int packetCount = (int)ceil((float)st.st_size / DATASIZE);
	struct Packet dataPacket, ackPacket, finPacket;
	FILE *file = fopen(fileName, "r");
	
	if(file == NULL)
	{
		printf("Cannot open file\n");
		//send FIN
		exit(0);
	}
	int base = 1;
	int inFlight = 0;
	int currSeqNum = 1;
	while(1)
	{
		if(base > packetCount)	///zzzzzzzzzz
		{
			printf("base: %d, packetCount: %d\n", base, packetCount);
			break;
		}
		int i, temp = currSeqNum;

		for(i = 0; i < base + CWND - temp && currSeqNum <= packetCount; i++)
		{
			prepareDataPacket(&dataPacket, currSeqNum, file, packetCount, st.st_size);
			if ((numbytes = sendto(sockfd, &dataPacket, sizeof(dataPacket), 0,
						 (struct sockaddr *)&their_addr, addr_len)) == -1) {
					perror("server: sendto");
					exit(1);
			}

			printf("--> sent %d bytes to %s, \tSEQ #%d \n", dataPacket.dataSize, clientIP, dataPacket.seqNum);
	//		printf("message %d bytes to \"%s\"\n", dataPacket.dataSize, dataPacket.data);

			currSeqNum++;
			inFlight++;
		}

		printf("server: waiting for ACKs...\n");

		if ((numbytes = recvfrom(sockfd, &ackPacket, sizeof(ackPacket) , 0,
			(struct sockaddr *)&their_addr, &addr_len)) == -1) {
			perror("recvfrom");
			exit(1);
		}
		printf("<-- received ACK #%d, base %d\n", ackPacket.seqNum, base);
		if(ackPacket.seqNum >= base)
			base = ackPacket.seqNum;
	}
////////////////////
////////////////////
	fclose(file);
	
	finPacket.type = FIN;
	finPacket.seqNum = currSeqNum;
	finPacket.dataSize = 0;
	finPacket.data[0] = '\0';

	if ((numbytes = sendto(sockfd, &finPacket, sizeof(finPacket), 0,
						 (struct sockaddr *)&their_addr, addr_len)) == -1) {
					perror("server: sendto");
					exit(1);
				}
	printf("--> sent FIN to %s, \t\tSEQ #%d\n", clientIP, finPacket.seqNum);
	close(sockfd);

	return 0;
}
