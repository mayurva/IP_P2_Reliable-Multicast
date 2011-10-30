#include<stdio.h>
#include<stdint.h>

#include"p2mpserver.h"

int main(int argc,char *argv[])
{
	int ret_val;
	init_receiver(argc,argv);
	printf("Receiver started\n");
//	 pthread_create(&thread_id,NULL,handle_messages,(void *)&args);
	do
	{
		ret_val = recv_data();
	}while(1);
	cleanup();
	return 0;
}
