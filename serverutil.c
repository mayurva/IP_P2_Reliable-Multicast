#include<stdio.h>
#include<stdint.h>
#include<stdlib.h>
#include<string.h>
#include"p2mpserver.h"

int last_packet;
int n;
FILE *file;
int server_port;
float p;
segment *recv_buffer;
segment curr_pkt;
int buf_counter;
uint32_t next_seg_seq_num;

uint16_t compute_checksum(/*char *data*/)
{
}

int init_recv_window()
{
	recv_buffer = (segment*)malloc(n*sizeof(segment));
	next_seg_seq_num = 0;	
	buf_counter = 0;
}

int init_receiver(int argc, char *argv[])
{
        if(argc<=1 || argc>5)
        {
                printf("Incorrect command line agruments\n");
                exit(-1);
        }

        n = atoi(argv[3]);
        printf("Window size:%d\n",n);
        if(!(file = fopen(argv[2],"wb")))
        {
                printf("File opening failed\n");
                exit(-1);
        }
        server_port = atoi(argv[1]);
        printf("Server port is: %d\n",server_port);

	init_recv_window();
        last_packet = 1;
}

int send_ack()
{
}

int compare_checksum(uint16_t checksum)
{
	if(curr_pkt.checksum == checksum)
		return TRUE;
	return FALSE;
}

int udp_recv()
{
	//read data through tcp port
	//assign it to curr_pkt
}

int add_to_buffer(int index)
{
	recv_buffer[index].seq_num = curr_pkt.seq_num;
	recv_buffer[index].data = (char*)malloc(strlen(curr_pkt.data));
	strcpy(recv_buffer[index].data,curr_pkt.data);
	return TRUE;
}

int is_in_sequence()
{
	return FALSE;
}

int is_in_recv_window()
{
	if(curr_pkt.seq_num<next_seg_seq_num)
		return -1;
	else if(curr_pkt.seq_num<next_seg_seq_num+n)
		return curr_pkt.seq_num-next_seg_seq_num+1;
	return 0;
}

int write_file()
{
}

int process_pkt()
{
        while (is_in_sequence())
                write_file();
}

int recv_data()
{
        int ret_val;
        uint16_t recv_checksum;
	udp_recv();
	recv_checksum = compute_checksum(curr_pkt.data);
        if(!(compare_checksum(recv_checksum)))
                return FALSE;
	ret_val = is_in_recv_window();
	if(ret_val == -1)
		return TRUE;
	else if(ret_val == 0)
		return FALSE;
	add_to_buffer(ret_val-1);
        process_pkt();
        return TRUE;
}

