#include<stdio.h>
#include<stdlib.h>
int n;
int mss;
char *filename;
int main(int argc, char *argv[])
{
	if(argc==1)
	{
		printf("Incorrect command line agruments\n");
		exit(-1);
	}
	int i = argc;
	if (argc == 3) //this will change later
	{
		mss = atoi(argv[i-1]);
		printf("MSS is:%d\n",mss);
		n = atoi(argv[i-2]);
		printf("Window size:%d\n",n);
	}
}
	
