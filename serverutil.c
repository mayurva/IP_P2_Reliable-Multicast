#include<stdio.h>
#include<stdint.h>
#include<stdlib.h>
#include<string.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <ifaddrs.h>
#include <net/if.h>

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
int soc;

uint16_t compute_checksum(char *data)
{
	uint16_t padd=0; //in case data has odd no. of octets
uint16_t word16; //stores 16 bit words out of adjacent 8 bits
uint32_t sum;
int i;

int len_udp = strlen(data)+strlen(data)%2; //no. of octets
sum=0;

for (i=0; i<len_udp; i=i+2)
{
        word16 =((data[i]<<8)&0xFF00)+(data[i+1]&0xFF);
        sum = sum + (unsigned long)word16;
}

// keep only the last 16 bits of the 32 bit calculated sum and add the carries
     while (sum>>16)
sum = (sum & 0xFFFF)+(sum >> 16);

// Take the one's complement of sum
sum = ~sum;

printf("Checksum is %x\n",(uint16_t)sum);
return ((uint16_t) sum);

}

int init_recv_window()
{
	recv_buffer = (segment*)malloc(n*sizeof(segment));
	printf("Allocated %d segments\n",n);
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

	struct sockaddr_in my_addr;    // my address information

	if ((soc = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
        	printf("\n error creating socket");
 	        exit(-1);
	}

	my_addr.sin_family = AF_INET;         // host byte order
	my_addr.sin_port = htons(server_port);     // short, network byte order
	my_addr.sin_addr.s_addr = INADDR_ANY; // automatically fill with my IP
	memset(my_addr.sin_zero, '\0', sizeof my_addr.sin_zero);

	if (bind(soc, (struct sockaddr *)&my_addr, sizeof my_addr) == -1) {
		printf("\n bind failed");
		exit(-1);
	}
	
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
	char buf[MAXLEN];
	int numbytes,data_length;
	char *a, *b, *c, *d, *e,*f;
	struct sockaddr_in their_addr; // connector's address information
	int addr_len = sizeof (their_addr);
	strcpy(buf,"");
	numbytes=recvfrom(soc, buf, MAXLEN , 0,(struct sockaddr *)&their_addr, &addr_len);
	if(numbytes == -1) {
		printf(" Error in receiving\n");
		exit(-1);
	}
	
//	printf("the received string is%s\n",buf);
	a = strtok(buf,"\n");
	b = strtok(NULL,"\n");
	c = strtok(NULL,"\n");
	f = strtok(NULL,"\n"); //'f' contains the data_length passed from sender
	d = strtok(NULL,"\0"); //This is not allowed, since d is not a null terminated string now.
//	strcat(d,'\0');
	e = strtok(a,":");
	e = strtok(NULL,":");
	curr_pkt.seq_num = (uint32_t)atoi(e);
	e = strtok(b,":");
	e = strtok(NULL,":");
	curr_pkt.checksum = (uint16_t)atoi(e);
	e = strtok(c,":");
	e = strtok(NULL,":");
	curr_pkt.pkt_type = (uint16_t)atoi(e);
	
	e = strtok(f,":");
	e = strtok(NULL,":");
	data_length = (uint16_t)atoi(e);

	d[data_length] = '\0'; //delimit the data string	

	printf("seq_num: %d, checksum %x, packet type %x\n data is %s data length is %d\n",curr_pkt.seq_num,curr_pkt.checksum,curr_pkt.pkt_type,d,(int)strlen(d));
	curr_pkt.data = (char*)malloc(strlen(d));
	strcpy(curr_pkt.data,d);
	strcat(curr_pkt.data,"\0");
	printf("Done with receive with data %s\n\n",curr_pkt.data);
}

int add_to_buffer(int index)
{
	printf("Index is %d\n",index);
	recv_buffer[index].seq_num = curr_pkt.seq_num;
	recv_buffer[index].data = (char*)malloc(strlen(curr_pkt.data));
	strcpy(recv_buffer[index].data,curr_pkt.data);
	free(curr_pkt.data);
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
       // while (is_in_sequence())
		//printf("Received packet is:\nSeq Number:%d\nChecksum:%d\npacket Type:%d\ndata: %s\n",curr_pkt.seq_num,curr_pkt.checksum,curr_pkt.pkt_type,curr_pkt.data);
                //write_file();
}

int recv_data()
{
        int ret_val;
        uint16_t recv_checksum;
	udp_recv();
	recv_checksum = compute_checksum(curr_pkt.data);
        if(!(compare_checksum(recv_checksum)))
	{
		printf("Checksum mismatch\n");
                return FALSE;
	}
	ret_val = is_in_recv_window();
	if(ret_val == -1)
		return TRUE;
	else if(ret_val == 0)
		return FALSE;
	add_to_buffer(ret_val-1);
        process_pkt();
        return TRUE;
}

