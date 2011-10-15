#include<stdio.h>
#include<stdlib.h>

#include"p2mpclient.h"

int main(int argc, char *argv[])
{
	int i;
	if(argc==1)
	{
		printf("Incorrect command line agruments\n");
		exit(-1);
	}
	
	mss = atoi(argv[argc-1]);
	printf("MSS is:%d\n",mss);
	n = atoi(argv[argc-2]);
	printf("Window size:%d\n",n);
	if(file = open(argv[argc-3]) == -1)
	{
		printf("File opening failed\n");
		exit(-1);
	}
	server_port = atoi(argv[argc-4]);
	printf("Server port is: %d\n",server_port);

	/*Add the code to process server IP addrs entered from command line
	for(i=argc-5;i>=1;i--){
	}*/

	rdt_send();
}
	
