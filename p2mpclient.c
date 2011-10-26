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
	init_sender(argc,argv);

	if(pthread_create(&sender,NULL,rdt_send,NULL)!=0){
                perror("Cannot create sender_thread!\n");
                exit(1);
        }
        
	if(pthread_create(&receiver,NULL,recv_ack,NULL)!=0){
                perror("Cannot create recv_thread!\n");
                exit(1);
        }

        if(pthread_join(sender,NULL)!=0)
        {
                perror("Cannot Join sender_thread!\n");
                exit(1);
        }

       if(pthread_join(receiver,NULL)!=0)
        {
               perror("Cannot Join recv_thread!\n");
                exit(1);
        }

}
	
