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
#include <pthread.h>
#include"p2mpclient.h"
#include<sys/time.h>
#include<signal.h>

typedef struct ack_{
	int ack_seq_num;
	int no_of_acks;
}ack;

int n;
int mss;
FILE *file;
int server_port;
segment *send_buffer;	//this needs mutex.. probably only ack array inside it.. mutex_ack
int no_of_receivers;
int soc;
host_info client_addr;
host_info *server_addr;
host_info recv_addr;
struct sockaddr_in sender_addr;
int recvd_pkt_type;
ack *ack_buffer;

//below time related variables will need mutex_timer
int start_timer;
int timer_seq_num;
double TIMEOUT;

//below seq number related variables need mutex_seq_num
int oldest_unacked;   //denotes the no. of oldest unacknowledged segment in buffer
int next_seq_num;     //to populate seq no. field in seg hdr 

int end_of_task;
pthread_mutex_t mutex_timer = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_ack = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_seq_num = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_end_of_task = PTHREAD_MUTEX_INITIALIZER;


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
	their_addr.sin_port = htons(server_addr[server_index].portnum);
	their_addr.sin_addr = *((struct in_addr *)he->h_addr);
	buf[0]='\0';
	seg_index = seg_index%n;
	
	sprintf(buf,"%u\n%u\n%u\n%s",send_buffer[seg_index].seq_num,send_buffer[seg_index].checksum,send_buffer[seg_index].pkt_type,send_buffer[seg_index].data);
	len = strlen(buf);
	//HIDEprintf("\n***********Bytes Sent: %d \tdata length is %d\t seq num is %d\t Buffer index is: %d\n\n",len,(int)strlen(send_buffer[seg_index].data),send_buffer[seg_index].seq_num,seg_index);
	

	if (sendto(soc,buf, len, 0, (struct sockaddr *)&their_addr, sizeof (their_addr)) == -1) {
       		printf("Error in sending");
       		exit(-1);
	}	

	fflush(stdout);
}

// Read 1 MSS worth data from file and return the data - reads byte-by-byte
void  read_file(char temp_buf[])
{
    int j=0;
    char buff[1];
    for(j=0;j<mss;j++)
    {
	if(fread(buff,sizeof(char),1,file)==0) //means end of file is reached
	{
		temp_buf[j] = '\0';
		fclose(file);
		file = NULL;
		//HIDEprintf("***@@@@@@Closing the File \n\n");
		return;
	}
	temp_buf[j] = buff[0];
   }
   temp_buf[j] = '\0';
}

uint16_t create_checksum(char *data)
{
	uint16_t padd=0; //in case data has odd no. of octets
	uint16_t word16; //stores 16 bit words out of adjacent 8 bits
	uint32_t sum;
	int i;

	int len_udp = strlen(data)+strlen(data)%2; //no. of octets
	sum=0;

	// make 16 bit words out of every two adjacent 8 bit words and
	// calculate the sum of all 16 bit words

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

	//HIDEprintf("Checksum is %x\n",(uint16_t)sum);
	return ((uint16_t) sum);
}

// appends header to payload
void create_segment(uint32_t seg_num,uint16_t pkt_type)
{
	int i;
	send_buffer[(seg_num)%n].seq_num = seg_num;
	//HIDEprintf("Creating Segment for Sequence No: %d at Index %d\n",seg_num,seg_num%n);
	send_buffer[(seg_num)%n].pkt_type = pkt_type; 
	send_buffer[(seg_num)%n].checksum = create_checksum(send_buffer[(seg_num)%n].data);
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

int is_in_between(int prev, int next, int key)
{
	if ( prev <= next ) {
		if ( prev <= key && key < next ) 
			return 1;
	}
	else {
		if ( (prev < key && key > next) || ( prev > key && key < next ) ) 
			return 1;
	}

	return 0;
}

int update_ack_arr(int recvd_seq_num)
{
	int i,j;
	int is_sec_dup_ack = FALSE;
		//HIDEprintf("Updating ack array.. \n");
		for(i=0;i<no_of_receivers;i++)
		{
			if(strcmp(server_addr[i].ip_addr,recv_addr.ip_addr)==0 && server_addr[i].portnum == recv_addr.portnum) //both IP and Port to be compared now
			{
				if(ack_buffer[i].ack_seq_num == recvd_seq_num)
				{
					ack_buffer[i].no_of_acks++;
					if(ack_buffer[i].no_of_acks>=3)
					{
						ack_buffer[i].no_of_acks=1; //reset the counter 
						//HIDEprintf("This is second duplicate ack\n");
						is_sec_dup_ack = TRUE;
					}
				}
				else if(recvd_seq_num > ack_buffer[i].ack_seq_num && recvd_seq_num < oldest_unacked+n)
				{
					ack_buffer[i].no_of_acks=1;
					ack_buffer[i].ack_seq_num = recvd_seq_num;
				}
				break;
			}
			
		}
	return is_sec_dup_ack;
}

uint32_t wait_for_ack()
{
	
	char buf[MAXLEN];
        int numbytes,i;

	char *a,*b,*c;
	uint32_t recvd_seq_num;

	char recv_addr_arr[16] = {0}; //to store IP address of receiver from data from socket 
	
        int addr_len = sizeof (sender_addr);
        strcpy(buf,"");
	//HIDEprintf("Waiting for ack\n");

	numbytes=recvfrom(soc, buf, MAXLEN , 0,(struct sockaddr *)&sender_addr, &addr_len);
        if(numbytes == -1) {
                printf("Error in receiving\n");
                exit(-1);
        }
	
	inet_ntop(AF_INET, &sender_addr.sin_addr.s_addr, recv_addr_arr, sizeof recv_addr_arr); //extract IP address from sender_addr

	a=strtok(buf,"\n");
	b = strtok(NULL,"\n");
	c = strtok(NULL,"\n");
	recvd_seq_num = (uint32_t)atoi(a);

	recvd_pkt_type = (uint16_t)atoi(c);

	strcpy(recv_addr.ip_addr,recv_addr_arr);
	recv_addr.portnum = (int)ntohs(sender_addr.sin_port);
	//HIDEprintf("Received ack for segment num %d and from server %s WITH Port %d\n",recvd_seq_num,recv_addr_arr,recv_addr.portnum);
	return recvd_seq_num;		
}


int min_acked_seq_num()
{
	int i;
	int min = ack_buffer[0].ack_seq_num;
	for(i=1;i<no_of_receivers;i++)
		if(ack_buffer[i].ack_seq_num < min)
			min = ack_buffer[i].ack_seq_num;
	return min;
}


void *recv_ack(void *ptr)
{
	int ret_val,recvd_seq_num,i;
	
	while(1)
	{
		char ack_from[16] = {0};
		recvd_seq_num = wait_for_ack();
		pthread_mutex_lock(&mutex_ack);
			ret_val = update_ack_arr(recvd_seq_num);		
		pthread_mutex_unlock(&mutex_ack);
		
		
		if(ret_val == TRUE)	//double dup ack
		{
			//printf("Resending segment %d due to double dup ack\n",recvd_seq_num+1);
			for(i=0;i<no_of_receivers;i++)
			{
				pthread_mutex_lock(&mutex_ack);
				if(ack_buffer[i].ack_seq_num <= recvd_seq_num)
				{
					udt_send((recvd_seq_num+1)%n,i);	
					if(recvd_seq_num+1 == oldest_unacked)
						start_timer = oldest_unacked;
				}
				pthread_mutex_unlock(&mutex_ack);
			}

		}
		else if(min_acked_seq_num() >= oldest_unacked)
		{
			pthread_mutex_lock(&mutex_seq_num);
				oldest_unacked = min_acked_seq_num()+1;
			pthread_mutex_unlock(&mutex_seq_num);


			pthread_mutex_lock(&mutex_timer);
				start_timer = oldest_unacked;
			pthread_mutex_unlock(&mutex_timer);

			if(recvd_pkt_type==0x00AA){ //indicates 0x00AA in decimal
				printf("\n\nIn receive thread of client ... setting end_of_task variable and exiting ... \n\n");
				//LOCK
				pthread_mutex_lock(&mutex_end_of_task);
				end_of_task = 1;	
				//UNLOCK
				pthread_mutex_unlock(&mutex_end_of_task);
				pthread_exit(NULL);
			}	
		}	
	}
}

void process_timeout()
{
	int sn = timer_seq_num%n;
	int i;
	//printf("Timeout!!! resending segment %d to all un_ack receivers\n",timer_seq_num);
	printf("Timeout, Sequence Number: %d\n",timer_seq_num);
	pthread_mutex_lock(&mutex_ack);
		for(i=0;i<no_of_receivers;i++)
			if(ack_buffer[i].ack_seq_num < timer_seq_num)
				udt_send(sn,i);
	pthread_mutex_unlock(&mutex_ack);

	pthread_mutex_lock(&mutex_timer);
	//TODO:not sure if we need mutex for oldest_unack here
	//printf("Resetting Timer..\n");
		start_timer = oldest_unacked;
	pthread_mutex_unlock(&mutex_timer);
	

}


int is_buffer_avail()
{
	pthread_mutex_lock(&mutex_seq_num);
		if(next_seq_num-oldest_unacked+1<n && file!=NULL){
			//HIDEprintf("Buffer is available for seq_num %d as oldest unack is %d\n",next_seq_num,oldest_unacked);
			 pthread_mutex_unlock(&mutex_seq_num);
			return TRUE;
		}
	pthread_mutex_unlock(&mutex_seq_num);
	return FALSE;
}

int is_pkt_earliest_transmitted()
{
	//HIDEprintf("In earliest Trans....\n");
	pthread_mutex_lock(&mutex_seq_num);
		if(oldest_unacked == next_seq_num){
			 pthread_mutex_unlock(&mutex_seq_num);
			return TRUE;
		}
	pthread_mutex_unlock(&mutex_seq_num);
	return FALSE;
}

void setup_timer()
{
	int interval = TIMEOUT * 1000; //convert millisecond in microseconds 
	//printf("TIMEOUT INTerval: %d Microseconds\n",interval);
	struct itimerval oldtime,newtime;
	newtime.it_interval.tv_sec = newtime.it_interval.tv_usec = 0;
	newtime.it_value.tv_sec = interval/1000000;
	newtime.it_value.tv_usec = interval%1000000;
	setitimer(ITIMER_REAL,&newtime,&oldtime);
}

int reached_end(){
	//LOCK
	pthread_mutex_lock(&mutex_end_of_task);
	if(end_of_task == 1){
		//UNLOCK
		pthread_mutex_unlock(&mutex_end_of_task);
		return TRUE;
	}
	//UNLOCK
	pthread_mutex_unlock(&mutex_end_of_task);
	return FALSE;
}

void * rdt_send(void *ptr)
{
	int i,j;
	while(1)
	{
		if(reached_end()==TRUE)
		{
			printf("\nExiting sender thread in client ... \n\n");
			//free(send_buffer);
			pthread_exit(NULL);
		}
		if(is_buffer_avail())	
		{
			next_seq_num=(next_seq_num+1)%MAX_SEQ;
			//HIDEprintf("Buffer Available: Getting 1 MSS from File!\n");
			//printf("Next Seq Num is: %d\n",next_seq_num);
			char tmp[MAXLEN]; 
			read_file(tmp);
			
			strcpy(send_buffer[next_seq_num%n].data,tmp); //copy 1 segment data into sender buffer
                       		
		//HIDE	printf("New Data Read: %s\n",send_buffer[next_seq_num%n].data);
                        send_buffer[next_seq_num%n].data[strlen(send_buffer[next_seq_num%n].data)] = '\0';

//HIDE	                printf("Data at %d\t is: %s",next_seq_num%n,send_buffer[next_seq_num%n].data);

                       	if(strlen(tmp)!=mss)			
			{
				printf("\nGot the last packet from file .... \n\n");
				create_segment(next_seq_num,0x5500); //denotes the last packet
			}
			else
				create_segment(next_seq_num,0x5555); //denotes normal data packet		
			
			for(j=0;j<no_of_receivers;j++)
				udt_send(next_seq_num,j);
			//HIDEprintf("Out of UDT_SEND..\n");
			if(is_pkt_earliest_transmitted())
			{
				//HIDEprintf("Packet %d is earliest transmitted so need to start timer",next_seq_num);
				pthread_mutex_lock(&mutex_timer);
					start_timer = oldest_unacked;
					//start_timer_thread(timer_thread);
				pthread_mutex_unlock(&mutex_timer);
			}

			//HIDEprintf("Next Seq Num in Send Thread is: %d\n",next_seq_num);
		}
		//printf("Lock in Timer..\n");		
		pthread_mutex_lock(&mutex_timer);
			if(start_timer != -1) //this means timer should start now
			{
				//HIDEprintf("Started timer for segment %d\n",start_timer);
				timer_seq_num = start_timer;
				start_timer = -1;

					setup_timer();
			}
		pthread_mutex_unlock(&mutex_timer);
	}
}

int init_sender(int argc,char *argv[])
{
	int i;

	oldest_unacked = 0;
	next_seq_num = -1;
 	end_of_task = 0;	
	recvd_pkt_type = -1;

	if(argc==1)
	{
		printf("Incorrect command line agruments\n");
		exit(-1);
	}

	TIMEOUT = atof(argv[argc-1]);
	printf("TIMEOUT set to: %lf\n",TIMEOUT);
	mss = atoi(argv[argc-2]);
	printf("MSS is:%d\n",mss);
	n = atoi(argv[argc-3]);
	printf("Window size:%d\n",n);
	if(!(file = fopen(argv[argc-4],"rb")))
	{
		printf("File opening failed\n");
		exit(-1);
	}
	
	no_of_receivers = ceil((argc - 5)/2);
	printf("No of Receivers: %d\n",no_of_receivers);
	populate_public_ip();
	server_addr = (host_info*)malloc(sizeof(host_info)*(no_of_receivers));	
	

	//HIDEprintf("Argc-5: %s\n",argv[argc-5]);
	int j=no_of_receivers-1;
	for(i = argc-5;i>=1;i=i-2){
		server_addr[j].portnum = atoi(argv[i]);		
		strcpy(server_addr[j--].ip_addr,argv[i-1]);
	}
	for(i=0;i<no_of_receivers;i++)
	{
		printf("Receiver %d, ip addr %s, portnum: %d\n",i,server_addr[i].ip_addr,server_addr[i].portnum);
	}


	if ((soc = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
	        printf("Error creating socket\n");
        	exit(-1);
	}


	send_buffer = (segment *)malloc(n*sizeof(segment)); //allocate space for sender buffer 

	ack_buffer = (ack*)malloc(no_of_receivers*sizeof(ack));
	for(i=0;i<no_of_receivers;i++)
	{
		ack_buffer[i].ack_seq_num = -1;
		ack_buffer[i].no_of_acks = 0;
	}

	signal(SIGALRM,process_timeout);
	//HIDEprintf("After Process Timeout!\n\n");

	
}



