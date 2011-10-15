#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<fcntl.h>
#include<math.h>
#include"p2mpclient.h"

int n;
int mss;
FILE *file;
int server_port;
segment *send_buffer;

// divide segment's data into 2 byte words
uint16_t split_buffer(char *data)
{

}  

uint16_t create_checksum(char *data)
{
	uint16_t padd=0; //in case data has odd no. of octets 
	uint16_t word16; //stores 16 bit words out of adjacent 8 bits
	uint32_t sum;	
	int i;	

	int len_udp = ceil((strlen(data)/sizeof(char))); //no. of octets
	uint16_t *buff = (uint16_t *)malloc(len_udp * sizeof(uint16_t));
	char padding;

	if(len_udp%2 == 0)  //padding = 1 if even no. of octets
		padding = 1;

	// Find out if the length of data is even or odd number. If odd,
	// add a padding byte = 0 at the end of packet
	if (padding&1==1){
		padd=1;
		buff[len_udp]=0; //len_udp is no. of octets 
	}
	
	//initialize sum to zero
	sum=0;
	
	// make 16 bit words out of every two adjacent 8 bit words and 
	// calculate the sum of all 16 bit words
	for (i=0; i<len_udp+padd; i=i+2)
	{
		word16 =((buff[i]<<8)&0xFF00)+(buff[i+1]&0xFF);
		sum = sum + (unsigned long)word16;
	}	

/*	// add the UDP pseudo header which contains the IP source and destinationn addresses
	for (i=0;i<4;i=i+2){
		word16 =((src_addr[i]<<8)&0xFF00)+(src_addr[i+1]&0xFF);
		sum=sum+word16;	
	}
	for (i=0;i<4;i=i+2){
		word16 =((dest_addr[i]<<8)&0xFF00)+(dest_addr[i+1]&0xFF);
		sum=sum+word16; 	
	}
	// the protocol number and the length of the UDP packet
	sum = sum + prot_udp + len_udp;
*/
	// keep only the last 16 bits of the 32 bit calculated sum and add the carries
    	while (sum>>16)
		sum = (sum & 0xFFFF)+(sum >> 16);
		
	// Take the one's complement of sum
	sum = ~sum;

	return ((uint16_t) sum);
}


// Read 1 MSS worth data from file and return the data - reads byte-by-byte
char * read_file()
{
    int j=0;
    
    char *temp_buf = (char *)malloc(mss*sizeof(char));
    char buff[1];

    for(j=0;j<mss;j++)
    {
   	if((fread(buff,sizeof(char),1,file))<0)
	{
		perror("\nRead error");
		exit(1);
	}
	temp_buf[j] = buff[0];
   }
   temp_buf[j] = '\0';
   return temp_buf;
}

// appends header to payload
void create_segments(uint32_t seg_num) 
{
	send_buffer[seg_num].seq_num = seg_num%MAX_SEQ;
	send_buffer[seg_num].pkt_type = 0x5555; //indicates data packet - 0101010101010101
//	send_buffer[seg_num].checksum = create_checksum(send_buffer[seg_num].data);
	send_buffer[seg_num].ack = NULL;
	 
}

//When the protocol starts, need to initialize sender buffer with N segments

void initialize_sender_buffer()
{
	int i=0;
	send_buffer = (segment *)malloc(n*sizeof(segment));
	for(i=0;i<n;i++){
		 send_buffer[i].data = (char *)malloc(mss*sizeof(char));
	       	 strcpy(send_buffer[i].data, read_file()); //copy 1 segment data into sender buffer
		 send_buffer[i].data[strlen(send_buffer[i].data)] = '\0';
		 create_segments((uint32_t) i);
	}
}

void print_segments()
{
	int i = 0;
	while(i<n)
	{
		printf("\nSegment %d's data : %s",i,send_buffer[i].data);
		i++;
	}
	printf("\nDone !!\n");
}

void rdt_send()
{
//	read_file();
//	create_segments();
	print_segments();
}



