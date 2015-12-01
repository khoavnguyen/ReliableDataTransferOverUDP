#define DATASIZE 1024

typedef enum {REQ, ACK, DAT, FIN} FLAG;

struct Packet 
{
//header
	FLAG type;
	int seqNum;	
	int dataSize;
//data
	char data[DATASIZE];
} Packet;

#define PACKETSIZE sizeof(Packet)
#define HEADERSIZE (PACKETSIZE-DATASIZE) 
