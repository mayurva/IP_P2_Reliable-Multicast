#include<stdio.h>
#include<stdint.h>

#include"p2mpserver.h"

int main(int argc,char *argv[])
{
	int ret_val;
	init_receiver(argc,argv);
	printf("Receiver started\n");
	do
	{
		ret_val = recv_data();
		if(!ret_val)	send_ack();
	}while(!last_packet);
	return 0;
}
