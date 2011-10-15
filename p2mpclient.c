#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<fcntl.h>
#include<stdint.h>

#include"p2mpclient.h"	

int main(int argc, char *argv[])
{
	init_sender(argc,argv);
	initialize_sender_buffer();
	rdt_send();
}
	
