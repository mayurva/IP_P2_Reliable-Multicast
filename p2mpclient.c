#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<fcntl.h>
#include<stdint.h>
#include<pthread.h>
#include"p2mpclient.h"	

int main(int argc, char *argv[])
{
	pthread_t sender,receiver;
        struct timeval start, end;

	init_sender(argc,argv);

	gettimeofday(&start,NULL);
	if(pthread_create(&sender,NULL,rdt_send,NULL)!=0){
                perror("Cannot create sender_thread!\n");
                exit(1);
        }
        
	if(pthread_create(&receiver,NULL,recv_ack,NULL)!=0){
                perror("Cannot create recv_thread!\n");
                exit(1);
        }

	if(pthread_join(receiver,NULL)!=0)
        {
               perror("Cannot Join recv_thread!\n");
                exit(1);
        }
	else printf("Receiver Exited\n");

        if(pthread_join(sender,NULL)!=0)
        {
                perror("Cannot Join sender_thread!\n");
                exit(1);
        }

	
	gettimeofday(&end, NULL);
	printf("Time taken for Data Transfer (Microseconds): %15ld\n",end.tv_usec-start.tv_usec + (end.tv_sec-start.tv_sec)*1000000);
	cleanup();

}
	
