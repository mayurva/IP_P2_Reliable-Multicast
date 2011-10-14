#ifndef P2MPCLIENT
#define P2MPCLIENT
typedef struct segment_t {
	uint32_t seq_num;
	uint16_t checksum;
	uint16_t pkt_type;
	char *data;
	int *ack;
} segment;
extern int mss;
extern int N;
extern char *file_name;
#endif
