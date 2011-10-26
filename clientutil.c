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


//below seq number related variables need mutex_seq_num
int oldest_unacked;   //denotes the no. of oldest unacknowledged segment in buffer
int next_seq_num;     //to populate seq no. field in seg hdr 

int end_of_task;
pthread_mutex_t mutex_timer = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_ack = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_seq_num = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_end_of_task = PTHREAD_MUTEX_INITIALIZER;

/*void tokenize(char *buf)
{
        char *a,*b,*c,*d;
        a = strtok_r(buf,"\n",&d);
        printf("\n\nSeq num %u\n",(uint32_t)atoi(a));

        b = strtok_r(NULL,"\n",&d);
        printf("Checksum %hu\n",(uint16_t)atoi(b));

        c = strtok_r(NULL,"\n",&d);
        printf("packet type %hu\n",(uint16_t)atoi(c));

        printf("data len is %u\t Data is: %s\n\n",(unsigned int)strlen(d),d);

}*/


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
	//strcpy(buf,"");
	buf[0]='\0';
	seg_index = seg_index%n;
	
	sprintf(buf,"%u\n%u\n%u\n%s",send_buffer[seg_index].seq_num,send_buffer[seg_index].checksum,send_buffer[seg_index].pkt_type,send_buffer[seg_index].data);
	len = strlen(buf);
	printf("\n***********Bytes Sent: %d \tdata length is %d\t seq num is %d\t Buffer index is: %d\n\n",len,(int)strlen(send_buffer[seg_index].data),send_buffer[seg_index].seq_num,seg_index);
	
	//Clearing Seg_Index's Data part
//	printf("Buf sent::: %s\n\n",buf);

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
    
    //char *temp_buf = (char *)calloc(mss,sizeof(char));
//    char temp_buf[MAXLEN];
    char buff[1];

    for(j=0;j<mss;j++)
    {
	if(fread(buff,sizeof(char),1,file)==0) //means end of file is reached
	{
		temp_buf[j] = '\0';
		fclose(file);
		file = NULL;
		printf("***@@@@@@Closing the File \n\n");
		return;
	//	return temp_buf;
		
	}
	temp_buf[j] = buff[0];
   }
   temp_buf[j] = '\0';
//   return temp_buf;
   
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

	printf("Checksum is %x\n",(uint16_t)sum);
	return ((uint16_t) sum);
}

// appends header to payload
void create_segment(uint32_t seg_num,uint16_t pkt_type)
{
	int i;
	send_buffer[(seg_num)%n].seq_num = seg_num;
	printf("Creating Segment for Sequence No: %d at Index %d\n",seg_num,seg_num%n);
	send_buffer[(seg_num)%n].pkt_type = pkt_type; 
	send_buffer[(seg_num)%n].checksum = create_checksum(send_buffer[(seg_num)%n].data);
	//for(i=0;i<no_of_receivers;i++)
	//	ack_buffer[(seg_num+1)%(n+1)][i] = 0;
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


/*
void initialize_sender_buffer()
{
	int i=0;
	send_buffer = (segment *)malloc(n*sizeof(segment));
	for(i=0;i<n;i++){
		//send_buffer[i].data = (char *)malloc(mss*sizeof(char));
		//strcpy(send_buffer[i].data, read_file()); //copy 1 segment data into sender buffer
		//send_buffer[i].data[strlen(send_buffer[i].data)] = '\0';
		//create_segments((uint32_t) i);
		
		send_buffer[i].seq_num = - 1;
	}
	printf("iterated %d times\n",i);
}
*/

/*int second_dup_ack(int current,int *ret)
{
	int i,flag = 0;
	for(i=0;i<no_of_receivers;i++){
		if(send_buffer[current % n].ack[i] == 3)  //Second Duplicate, third overall
			ret[i] = i;
			flag =1;
	}
	return flag;
}*/

/*void update_retransmit_arr(int current,int second_duplicates[])
{
	//do we Clear the retransmit array first?
	int i;
	for(i = 0;i < no_of_receivers; i++){
		send_buffer[current % n].retransmit[i] = -1;
	}
	for(i = 0;i < no_of_receivers; i++){
		if(second_duplicates[i] != -1){
			send_buffer[current % n].retransmit[i] = second_duplicates[i];
		}
	}
	//Updates the retransmit array
}*/

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
//	if(recvd_seq_num >= oldest_unacked-1)
//	{
		printf("Updating ack array.. \n");
		for(i=0;i<no_of_receivers;i++)
		{
			//printf("Server_addr: %s\t Recv_addr: %s\n",server_addr[i].ip_addr,recv_addr.ip_addr);
			if(strcmp(server_addr[i].ip_addr,recv_addr.ip_addr)==0)
			{
				if(ack_buffer[i].ack_seq_num == recvd_seq_num)
				{
					ack_buffer[i].no_of_acks++;
					if(ack_buffer[i].no_of_acks>=3)
					{
						ack_buffer[i].no_of_acks=1; //reset the counter 
						printf("This is second duplicate ack\n");
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
//	}
	//printf("Returning from Update Ack\n");
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
	printf("Waiting for ack\n");

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
	//printf("@@@@ACK Type Set: %x \t for Seq Num: %d in Index %d",send_buffer[recvd_seq_num%n].pkt_type,recvd_seq_num,recvd_seq_num%n);	

	strcpy(recv_addr.ip_addr,recv_addr_arr);
	printf("Received ack for segment num %d and from server %s\n",recvd_seq_num,recv_addr_arr);
//	strcpy(ack_from,recv_addr);
	return recvd_seq_num;		
}

/*int recvd_all_ack(int sn)
{
	int i;
	sn = sn % n;
	for(i=0;i<no_of_receivers;i++)
		if(ack_buffer[(sn+1)%(n+1)].ack[i] == 0)
		//if(send_buffer[sn].ack[i] == 0)
			return FALSE;
	return TRUE;
}

void slide_window()
{
	int i;
//	printf("Sliding the window forward\n");
	do
	{
		printf("Window slided for 1 packet\n");
		for(i=0;i<no_of_receivers;i++)
			send_buffer[oldest_unacked%n].ack[i] = 0;
		
		oldest_unacked++;
	}while(recvd_all_ack(oldest_unacked));
	printf("Timer will start for segment %d now\n",oldest_unacked);
	pthread_mutex_lock(&mutex_timer);
		start_timer = oldest_unacked;
	pthread_mutex_unlock(&mutex_timer);
}*/

int min_acked_seq_num()
{
	int i;
	int min = ack_buffer[0].ack_seq_num;
	for(i=1;i<no_of_receivers;i++)
		if(ack_buffer[i].ack_seq_num < min)
			min = ack_buffer[i].ack_seq_num;
	return min;
}

/*int get_max(int a,int b)
{	return (a>=b?a:b);	}*/

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
//			int second_duplicates[5]; //to retrieve the recvr indexes from ack array for current packet for which we have 2nd duplicates
//			for(i = 0;i< 5;i++) second_duplicates[i] = -1;
//				pthread_mutex_lock(&mutex_ack);
//					retval = second_dup_ack(curr_seq_num,second_duplicates);
//				pthread_mutex_unlock(&mutex_ack);
		
		
		if(ret_val == TRUE)	//double dup ack
		{
			printf("Resending segment %d due to double dup ack\n",recvd_seq_num+1);
			for(i=0;i<no_of_receivers;i++)
			{
				pthread_mutex_lock(&mutex_ack);
				if(ack_buffer[i].ack_seq_num <= recvd_seq_num)
				{
					//send_buffer[oldest_unacked %n].retransmit[i]=0;
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
	//Wait for timer to timeout.
	//is_ack_received_from_all() - (With ACK Mutex)
		//exit
	//Else, find retransmission array, - (With RETRANSMIT Mutex)
		//exit
	int sn = timer_seq_num%n;
	int i;
	printf("Timeout!!! resending segment %d to all un_ack receivers\n",timer_seq_num);
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

/*void start_timer_thread(pthread_t timer_thread)
{
	pthread_create(&timer_thread,NULL,timeout_process,NULL);

}*/

int is_buffer_avail()
{
	pthread_mutex_lock(&mutex_seq_num);
//	printf("Next Seq Num: %d\t Oldest Unacked: %d\t",next_seq_num,oldest_unacked);
		if(next_seq_num-oldest_unacked+1<n && file!=NULL){
			printf("Buffer is available for seq_num %d as oldest unack is %d\n",next_seq_num,oldest_unacked);
			 pthread_mutex_unlock(&mutex_seq_num);
			return TRUE;
		}
	pthread_mutex_unlock(&mutex_seq_num);
	return FALSE;
}

int is_pkt_earliest_transmitted()
{
	printf("In earliest Trans....\n");
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
//	pthread_t timer_thread;
	//printf("In RDT_SEND!\n");
	while(1)
	{
		//printf("Checking for Buff...\n");
		if(reached_end()==TRUE)
		{
			printf("\nExiting sender thread in client ... \n\n");
			//free(send_buffer);
			pthread_exit(NULL);
		}
		if(is_buffer_avail())	
		{
			next_seq_num=(next_seq_num+1)%MAX_SEQ;
			printf("Buffer Available: Getting 1 MSS from File!\n");
			//printf("Next Seq Num is: %d\n",next_seq_num);
			char tmp[MAXLEN]; 
			read_file(tmp);
			
			strcpy(send_buffer[next_seq_num%n].data,tmp); //copy 1 segment data into sender buffer
                       		
		//	printf("New Data Read: %s\n",send_buffer[next_seq_num%n].data);
                        send_buffer[next_seq_num%n].data[strlen(send_buffer[next_seq_num%n].data)] = '\0';

//	                printf("Data at %d\t is: %s",next_seq_num%n,send_buffer[next_seq_num%n].data);

                       	if(strlen(tmp)!=mss)			
			{
				printf("\nGot the last packet from file .... \n\n");
				create_segment(next_seq_num,0x5500); //denotes the last packet
			}
			else
				create_segment(next_seq_num,0x5555); //denotes normal data packet		
			
			//free(tmp);
			for(j=0;j<no_of_receivers;j++)
				udt_send(next_seq_num,j);
			printf("Out of UDT_SEND..\n");
			if(is_pkt_earliest_transmitted())
			{
				printf("Packet %d is earliest transmitted so need to start timer",next_seq_num);
				pthread_mutex_lock(&mutex_timer);
					start_timer = oldest_unacked;
					//start_timer_thread(timer_thread);
				pthread_mutex_unlock(&mutex_timer);
			}

			printf("Next Seq Num in Send Thread is: %d\n",next_seq_num);
		}
		//printf("Lock in Timer..\n");		
		pthread_mutex_lock(&mutex_timer);
			if(start_timer != -1) //this means timer should start now
			{
				printf("Started timer for segment %d\n",start_timer);
				timer_seq_num = start_timer;
				start_timer = -1;

		//		if(timer_seq_num == oldest_unacked)
					setup_timer();
			}
		pthread_mutex_unlock(&mutex_timer);
		//printf("Unlock from Timer..\n");
		/*pthread_mutex_lock(&mutex_retransmit);
			for(i=0;i<no_of_receivers;i++)
			{
				if(send_buffer[oldest_unacked % n].retransmit[i] == 1)
				{
					send_buffer[oldest_unacked %n].retransmit[i]=0;
					udt_send(oldest_unacked,i);
					
				}
			}
			//Timer for Oldest Unacked
		pthread_mutex_unlock(&mutex_retransmit);*/
	}
}

int init_sender(int argc,char *argv[])
{
	int i;

	oldest_unacked = 0;
	next_seq_num = -1;
 	end_of_task = 0;	
	//pthread_t sender_thread;
//	pthread_t timer_thread;
	//pthread_t recv_thread;
	recvd_pkt_type = -1;

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

	//populate_public_ip();
//	populate_server_ip(argc);
	
	no_of_receivers = argc - 5;
	printf("No of Receivers: %d\n",no_of_receivers);
	populate_public_ip();
	server_addr = (host_info*)malloc(sizeof(host_info)*(no_of_receivers));	
	
	for(i=argc-5;i>=1;i--){
		strcpy(server_addr[i-1].ip_addr,argv[i]);
	}

	//printf("no of receivers: %d\n",no_of_receivers);
	for(i=0;i<no_of_receivers;i++)
	{
		printf("Receiver %d, ip addr %s\n",i,server_addr[i].ip_addr);
	}


	if ((soc = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
	        printf("Error creating socket\n");
        	exit(-1);
	}

//	initialize_sender_buffer();	

	//printf("Initializing Send Buffer! \n");
	send_buffer = (segment *)malloc(n*sizeof(segment)); //allocate space for sender buffer 

	ack_buffer = (ack*)malloc(no_of_receivers*sizeof(ack));
	for(i=0;i<no_of_receivers;i++)
	{
		ack_buffer[i].ack_seq_num = -1;
		ack_buffer[i].no_of_acks = 0;
	}

	//printf("Initializing Retransmit Array! \n");
	//retransmit = (int *)malloc(no_of_receivers*sizeof(int));	
	signal(SIGALRM,process_timeout);
	printf("After Process Timeout!\n\n");
	//int j;
/*	for(i=0;i<n;i++){
		send_buffer[i].retransmit = (int *)malloc(no_of_receivers*sizeof(int));
		for(j=0;j<no_of_receivers;j++){
					send_buffer[i].retransmit[j]=-1;
		}
	}*/

	
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

