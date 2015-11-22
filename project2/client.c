/*
** client.c -- a datagram "client" demo
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


int main(int argc, char *argv[])
{
	int sockfd;
	struct addrinfo hints, *servinfo, *p;
	int rv;
	int numbytes;
	char *serverIP, *portnumber, *filename;
/*
	if (argc != 4) {
		fprintf(stderr,"usage: client serverIP portnumber filename\n");
		exit(1);
	}
	serverIP = argv[1];
	portnumber = argv[2];
	filename = argv[3];
*/
	serverIP = "127.0.0.1";
	portnumber = "4444";
	filename = "test.jpg";

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;

	if ((rv = getaddrinfo(serverIP, portnumber, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

	// loop through all the results and make a socket
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
				p->ai_protocol)) == -1) {
			perror("client: socket");
			continue;
		}

		break;
	}

	if (p == NULL) {
		fprintf(stderr, "client: failed to create socket\n");
		return 2;
	}

	struct Packet reqPacket;
	reqPacket.type = REQ;
	reqPacket.seqNum = 0;
	reqPacket.dataSize = strlen(filename);
	strcpy(reqPacket.data, filename);

	if ((numbytes = sendto(sockfd, &reqPacket, sizeof(reqPacket), 0,
			 p->ai_addr, p->ai_addrlen)) == -1) {
		perror("client: sendto");
		exit(1);
	}

	printf("--> sent %d bytes to %s\n", numbytes, serverIP);
	printf("message is %d bytes: \"%s\"\n", reqPacket.dataSize, reqPacket.data);

	
	
	FILE *file = fopen("receivedFile", "wb");

	if (file==NULL)
	{
		printf("Error creating file\n");
		exit(1);
	}
	
	struct Packet dataPacket, ackPacket;
	ackPacket.type = ACK;
	ackPacket.seqNum = 0;
	ackPacket.dataSize = 0;
	ackPacket.data[0] = '\0';
	while(1)
	{
		printf("client: waiting for server...\n");
		if ((numbytes = recvfrom(sockfd, &dataPacket, sizeof(dataPacket) , 0,
			p->ai_addr, &p->ai_addrlen)) == -1) {
			perror("recvfrom");
			exit(1);
		}
		if(dataPacket.type == FIN)
		{
			printf("<-- received FIN from %s, \t\tSEQ #%d\n", serverIP, dataPacket.seqNum);
			break;
		}

		printf("<-- received %d bytes from %s, \tSEQ #%d\n", dataPacket.dataSize, serverIP, dataPacket.seqNum );
		
//		printf("client: packet contains \"%s\", %d bytes\n", dataPacket.data, dataPacket.dataSize);

		fwrite(dataPacket.data, sizeof(char), dataPacket.dataSize, file);
	
		ackPacket.seqNum = dataPacket.seqNum + 1;
		if ((numbytes = sendto(sockfd, &ackPacket, sizeof(ackPacket), 0,
			 p->ai_addr, p->ai_addrlen)) == -1) {
		perror("client: sendto");
		exit(1);
	}
	}


	fclose(file);
	

	freeaddrinfo(servinfo);
	close(sockfd);

	return 0;
}
