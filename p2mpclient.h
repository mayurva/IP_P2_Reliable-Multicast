#ifndef P2MPCLIENT
#define P2MPCLIENT
typedef struct segment_t {
	unsigned seq_num : 32;
	unsigned checksum : 16;
	unsigned pkt_type : 16;
	char *data;
	int *ack;
} segment;
extern int mss;
extern int N;
extern char *file_name;
#endif
