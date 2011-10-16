#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<fcntl.h>
#include<math.h>
#include<stdint.h>
#include<string.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <ifaddrs.h>
#include <net/if.h>

#include"p2mpclient.h"

int n;
int mss;
FILE *file;
int server_port;
segment *send_buffer;
int no_of_receivers;
int soc;
host_info client_addr;
host_info *server_addr;

int udt_send(int seg_index,int server_index)
{
	struct sockaddr_in their_addr; // connector's address information
	struct hostent *he;
	int numbytes;
	int addr_len;
	int len;
	char buf[MAXLEN];
    
	if ((he=gethostbyname(server_addr[server_index].ip_addr)) == NULL) {  // get the host info
		printf("\nHost not found %s\n",server_addr[server_index].ip_addr);
		exit(-1);
    	}

	their_addr.sin_family = AF_INET;     // host byte order
	their_addr.sin_port = htons(server_port); // short, network byte order
	their_addr.sin_addr = *((struct in_addr *)he->h_addr);
	strcpy(buf,"");
//	printf("\n\nData length = %d",(int)strlen(send_buffer[seg_index].data));
	sprintf(buf,"Seq Number:%d\nChecksum:%d\npacket Type:%d\nData Length:%d\n%s",send_buffer[seg_index].seq_num,send_buffer[seg_index].checksum,send_buffer[seg_index].pkt_type,(int)strlen(send_buffer[seg_index].data),send_buffer[seg_index].data);
	len = strlen(buf);
	printf("\n\n***********Bytes Sent: %d",len);
	if (sendto(soc,buf, len, 0, (struct sockaddr *)&their_addr, sizeof (their_addr)) == -1) {
       		printf("Error in sending");
       		exit(-1);
	}	

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

uint16_t create_checksum(char *data)
{
uint16_t padd=0; //in case data has odd no. of octets
uint16_t word16; //stores 16 bit words out of adjacent 8 bits
uint32_t sum;
int i;

int len_udp = strlen(data)+strlen(data)%2; //no. of octets
//uint16_t *buff = (uint16_t *)malloc(len_udp * sizeof(uint16_t));
//char padding;

//if(len_udp%2 == 0) //padding = 1 if even no. of octets
//padding = 1;

// Find out if the length of data is even or odd number. If odd,
// add a padding byte = 0 at the end of packet
//if (padding&1==1){
//padd=1;
//buff[len_udp]=0; //len_udp is no. of octets
//}

//initialize sum to zero
sum=0;

// make 16 bit words out of every two adjacent 8 bit words and
// calculate the sum of all 16 bit words
for (i=0; i<len_udp; i=i+2)
{
	word16 =((data[i]<<8)&0xFF00)+(data[i+1]&0xFF);
	sum = sum + (unsigned long)word16;
}

/* // add the UDP pseudo header which contains the IP source and destinationn addresses
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

printf("Checksum is %x\n",(uint16_t)sum);
return ((uint16_t) sum);
}

// appends header to payload
void create_segments(uint32_t seg_num)
{
	send_buffer[seg_num].seq_num = seg_num%MAX_SEQ;
	send_buffer[seg_num].pkt_type = 0x5555; //indicates data packet - 0101010101010101
	send_buffer[seg_num].checksum = create_checksum(send_buffer[seg_num].data);
	send_buffer[seg_num].ack = NULL;

}

void populate_public_ip()
{

	struct ifaddrs *myaddrs, *ifa;
	void *in_addr;
	char buf[64], intf[128];

	strcpy(client_addr.iface_name, "");

	if(getifaddrs(&myaddrs) != 0) {
		printf("getifaddrs failed! \n");
		exit(-1);
	}

	for (ifa = myaddrs; ifa != NULL; ifa = ifa->ifa_next) {

		if (ifa->ifa_addr == NULL)
			continue;

		if (!(ifa->ifa_flags & IFF_UP))
			continue;

		switch (ifa->ifa_addr->sa_family) {
        
			case AF_INET: { 
				struct sockaddr_in *s4 = (struct sockaddr_in *)ifa->ifa_addr;
				in_addr = &s4->sin_addr;
				break;
			}

			case AF_INET6: {
				struct sockaddr_in6 *s6 = (struct sockaddr_in6 *)ifa->ifa_addr;
				in_addr = &s6->sin6_addr;
				break;
			}

			default:
				continue;
		}

		if (inet_ntop(ifa->ifa_addr->sa_family, in_addr, buf, sizeof(buf))) {
			if ( ifa->ifa_addr->sa_family == AF_INET && strcmp(ifa->ifa_name, "lo")!=0 ) {
				printf("\nDo you want the sender to bind to %s interface?(y/n): ", ifa->ifa_name);
				scanf("%s", intf);
				if ( strcmp(intf, "n") == 0 )
					continue;
				sprintf(client_addr.ip_addr, "%s", buf);
				sprintf(client_addr.iface_name, "%s", ifa->ifa_name);
			
			}
		}
	}

	freeifaddrs(myaddrs);
	
	if ( strcmp(client_addr.iface_name, "") == 0 ) {
		printf("Either no Interface is up or you did not select any interface ..... \nclient Exiting .... \n\n");
		exit(0);
	}

	 printf("\n\nMy public interface and IP is:  %s %s\n\n", client_addr.iface_name, client_addr.ip_addr);

}



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
	printf("iterated %d times\n",i);
}
/*int populate_server_ip(int argc)
{


    char buf[100];
    int soc;
    struct sockaddr_in their_addr,recv_addr; // connector's address information
    struct hostent *he;
    int numbytes;
    int addr_len;
    
    if ((he=gethostbyname("192.168.4.4")) == NULL) {  // get the host info
        printf("\nHost not found");
        exit(-1);
    }

    

    their_addr.sin_family = AF_INET;     // host byte order
    their_addr.sin_port = htons(1234); // short, network byte order
    their_addr.sin_addr = *((struct in_addr *)he->h_addr);
      
    do
	{
		printf("\nEnter string to be transmitted: ");
		gets(buf);
		if (sendto(soc,buf,strlen(buf), 0,(struct sockaddr *)&their_addr, sizeof (their_addr)) == -1) {
        		printf("Error in sending");
        		exit(-1);
		}	

}*/

int init_sender(int argc,char *argv[])
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
	if(!(file = fopen(argv[argc-3],"rb")))
	{
		printf("File opening failed\n");
		exit(-1);
	}
	server_port = atoi(argv[argc-4]);
	printf("Server port is: %d\n",server_port);

	populate_public_ip();
//	populate_server_ip(argc);
	
	no_of_receivers = argc - 5;
	server_addr = (host_info*)malloc(sizeof(host_info)*(no_of_receivers));	
	for(i=argc-5;i>=1;i--){
		strcpy(server_addr[i-1].ip_addr,argv[i]);
	}

	printf("no of receivers: %d\n",no_of_receivers);
	for(i=0;i<no_of_receivers;i++)
	{
		printf("Receiver %d, ip addr %s\n",i,server_addr[i].ip_addr);
	}

	if ((soc = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
	        printf("Error creating socket\n");
        	exit(-1);
	}

	initialize_sender_buffer();	
}

// divide segment's data into 2 byte words
uint16_t split_buffer(char *data)
{

}





//When the protocol starts, need to initialize sender buffer with N segments


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
	int i,j;
// read_file();
// create_segments();
	print_segments();
	for(i=0;i<n;i++)
		for(j=0;j<no_of_receivers;j++)
			udt_send(i,j);
}
