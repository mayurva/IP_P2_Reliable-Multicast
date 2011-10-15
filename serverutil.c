#include<stdio.h>
#include<stdint.h>

#include"p2mpserver.h"

int last_packet;

uint16_t compute_checksum(/*char *data*/)
{
}

int init_receiver(int argc, char *argv[])
{
        last_packet = 1;
}

int send_ack()
{
}

int compare_checksum(uint16_t checksum)
{
}

int udp_recv()
{
}

int add_to_buffer()
{
}

int is_in_sequence()
{
	return 0;
}

int is_in_recv_window()
{
}

int write_file()
{
}

int process_pkt()
{
        add_to_buffer();
        while (is_in_sequence())
                write_file();
}

int recv_data()
{
        int ret_val;
        uint16_t recv_checksum;
        udp_recv();
	recv_checksum = compute_checksum();
        if(!(compare_checksum(recv_checksum) && is_in_recv_window()))
                return 1;
        process_pkt();
        return 0;
}

