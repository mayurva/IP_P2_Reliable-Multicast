#ifndef P2MPSERVER
#define P2MPSERVER
#define MAXLEN 5000
#define FALSE 0
#define TRUE 1

typedef struct segment_t{
        uint32_t seq_num;
        uint16_t checksum;
        uint16_t pkt_type;
        char *data;
} segment;

extern segment *recv_buffer;
extern uint32_t next_seg_seq_num;
extern int last_packet;
extern int n;
extern FILE *file;
extern int server_port;
extern float p;
#endif
