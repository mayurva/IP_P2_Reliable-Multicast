#ifndef P2MPCLIENT
#define P2MPCLIENT
#include<stdint.h>
typedef struct segment_t{
	uint32_t seq_num;
	uint16_t checksum;
	uint16_t pkt_type;
	char *data;
	int *ack;
} segment;
extern int mss;
extern int n;
extern int file;
extern int server_port;
#endif
