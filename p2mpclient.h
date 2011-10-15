#ifndef P2MPCLIENT
#define P2MPCLIENT

#include<stdint.h>
#include<unistd.h>

#define MAX_SEQ 4294967295

typedef struct segment_t{
	uint32_t seq_num;
	uint16_t checksum;
	uint16_t pkt_type;
	char *data;
	int *ack;
} segment;

/*
typedef struct send_buffer_t{
	segment *window;
} send_buffer;
*/


extern segment *send_buffer; //sender side buffer = array of segments
//extern send_buffer sbuf; 
extern int mss;
extern int n;
extern FILE *file;
extern int server_port;

#endif
