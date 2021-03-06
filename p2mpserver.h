#ifndef P2MPSERVER
#define P2MPSERVER
#define MAXLEN 2000
#define FALSE 0
#define TRUE 1
#pragma pack(1)
typedef struct segment_t{
        uint32_t seq_num;
        uint16_t checksum;
        uint16_t pkt_type;
	char arrived;
        char data[MAXLEN];
} segment;
#pragma pack(0)

extern segment *recv_buffer;
extern uint32_t next_seg_seq_num;
extern int last_packet;
extern int n;
extern FILE *file;
extern int server_port;
extern float p;
#endif
