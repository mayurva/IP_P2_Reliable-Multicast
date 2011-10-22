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

//below time related variables will need mutex_timer
int start_timer;
int timer_seq_num;

//int *retransmit; //stores index of receivers to whom segment has to be retransmitted

//below seq number related variables need mutex_seq_num
int oldest_unacked;   //denotes the no. of oldest unacknowledged segment in buffer
int next_seq_num;     //to populate seq no. field in seg hdr 

pthread_mutex_t mutex_timer = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_ack = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_seq_num = PTHREAD_MUTEX_INITIALIZER;


void tokenize(char *buf)
{
        char *a,*b,*c,*d;
        a = strtok_r(buf,"\n",&d);
        printf("\n\nSeq num %u\n",(uint32_t)atoi(a));

        b = strtok_r(NULL,"\n",&d);
        printf("Checksum %hu\n",(uint16_t)atoi(b));

        c = strtok_r(NULL,"\n",&d);
        printf("packet type %hu\n",(uint16_t)atoi(c));

        printf("data len is %u\t Data is: %s\n\n",(unsigned int)strlen(d),d);


}


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
//	printf("Seg Index is: %d\n",seg_index);
	
	sprintf(buf,"%u\n%u\n%u\n%s",send_buffer[seg_index].seq_num,send_buffer[seg_index].checksum,send_buffer[seg_index].pkt_type,send_buffer[seg_index].data);
	len = strlen(buf);
	printf("\n***********Bytes Sent: %d \tdata length is %d\t seq num is %d\t Buffer index is: %d\t DATA IS: %s\n\n",len,(int)strlen(send_buffer[seg_index].data),send_buffer[seg_index].seq_num,seg_index,send_buffer[seg_index].data);
	
	//Clearing Seg_Index's Data part
	printf("Buf sent::: %s\n\n",buf);
	strcpy(send_buffer[seg_index].data,"");

	if (sendto(soc,buf, len, 0, (struct sockaddr *)&their_addr, sizeof (their_addr)) == -1) {
       		printf("Error in sending");
       		exit(-1);
	}	

	//Testing for Buf
	if(len!=19)
	{
		printf("$$$ Tokenizing: \n");	
		tokenize(buf);
	}
	fflush(stdout);
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
	//	exit(1);
	

		temp_buf[j] = '~';
		temp_buf[j+1] = '\0';
		fclose(file);
		return temp_buf;
		//return '~'; //Indicate the end of file.
		
	}
	temp_buf[j] = buff[0];
   }
   temp_buf[j] = '\0';
 //  printf("returning!\n");
   return temp_buf;
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
void create_segment(uint32_t seg_num)
{
	int i;
	send_buffer[(seg_num)%n].seq_num = seg_num;
	printf("Creating Segment for Sequence No: %d at Index %d\n",seg_num,seg_num%n);
	send_buffer[(seg_num)%n].pkt_type = 0x5555; //indicates data packet - 0101010101010101
	send_buffer[(seg_num)%n].checksum = create_checksum(send_buffer[(seg_num)%n].data);
	send_buffer[(seg_num)%n].ack = (int *)malloc(no_of_receivers*sizeof(int));
	for(i=0;i<no_of_receivers;i++)
		send_buffer[seg_num%n].ack[i] = 0;
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

int second_dup_ack(int current,int *ret)
{
	int i,flag = 0;
	for(i=0;i<no_of_receivers;i++){
		if(send_buffer[current % n].ack[i] == 3)  //Second Duplicate, third overall
			ret[i] = i;
			flag =1;
	}
	return flag;
}

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

int update_ack_arr(int recvd_seq_num)
{
	int i,j;
	int is_sec_dup_ack = FALSE;
	if(recvd_seq_num >= oldest_unacked)
	{
		printf("Updating ack array.. \n");
		for(i=0;i<no_of_receivers;i++)
		{
			//printf("Server_addr: %s\t Recv_addr: %s\n",server_addr[i].ip_addr,recv_addr.ip_addr);
			if(strcmp(server_addr[i].ip_addr,recv_addr.ip_addr)==0)
			{
				send_buffer[(recvd_seq_num)%n].ack[i]+=1;
				printf("ACK[%d] for Packet: %d is: %d\n",i,recvd_seq_num,send_buffer[recvd_seq_num%n].ack[i]);
				if(send_buffer[(recvd_seq_num)%n].ack[i] >=3) //this is the second dup ack
				{
					send_buffer[(recvd_seq_num)%n].ack[i]=1; //reset the counter 
					printf("This is second duplicate ack\n");
					is_sec_dup_ack = TRUE;
				}
				break;
			}
			
		}
	}
	printf("Returning from Update Ack\n");
	return is_sec_dup_ack;
}

uint32_t wait_for_ack()
{
	
	char buf[MAXLEN];
        int numbytes,i;

	char *a;
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
		
	recvd_seq_num = (uint32_t)atoi(a);
	strcpy(recv_addr.ip_addr,recv_addr_arr);
	printf("Received ack for segment num %d and from server %s\n",recvd_seq_num,recv_addr_arr);
//	strcpy(ack_from,recv_addr);
	return recvd_seq_num;		
}

int recvd_all_ack(int sn)
{
	int i;
	sn = sn % n;
	for(i=0;i<no_of_receivers;i++)
		if(send_buffer[sn].ack[i] == 0)
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
//			int second_duplicates[5]; //to retrieve the recvr indexes from ack array for current packet for which we have 2nd duplicates
//			for(i = 0;i< 5;i++) second_duplicates[i] = -1;
//				pthread_mutex_lock(&mutex_ack);
//					retval = second_dup_ack(curr_seq_num,second_duplicates);
//				pthread_mutex_unlock(&mutex_ack);
		
		
		if(ret_val == 1)	//double dup ack
		{
			printf("Resending segments due to double dup ack\n");
			for(i=0;i<no_of_receivers;i++)
			{
				pthread_mutex_lock(&mutex_ack);
				if(send_buffer[(recvd_seq_num+1)%n].ack[i] == 0)
				{
					//send_buffer[oldest_unacked %n].retransmit[i]=0;
					udt_send((recvd_seq_num+1)%n,i);
					
				}
				pthread_mutex_unlock(&mutex_ack);
			}
			//Timer for Oldest Unacked

//			pthread_mutex_lock(&mutex_retransmit);
//				update_retransmit_arr(curr_seq_num+1,second_duplicates); //do we need to have per packet retransmit array?.. also, we have to send 'FOLLOWING' packet. So what to send?
//			pthread_mutex_unlock(&mutex_retransmit);
		}
		else if(recvd_all_ack(recvd_seq_num) && recvd_seq_num == oldest_unacked) //need to slide window
		{
			//Update curr_seq_num's ack array at the correct index using the ack_from which has IP_Address of the current receiver
			pthread_mutex_lock(&mutex_seq_num);
				printf("Sliding Window!...\n");
				slide_window();
			pthread_mutex_unlock(&mutex_seq_num);
//			pthread_mutex_lock(&mutex_ack);
//			pthread_mutex_unlock(&mutex_ack);
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
	printf("Timeout!!! resending segment %d to all an_ack receivers",timer_seq_num);
	pthread_mutex_lock(&mutex_ack);
		for(i=0;i<no_of_receivers;i++)
			if(send_buffer[sn].ack[i] == 0)
				udt_send(sn,i);
	pthread_mutex_unlock(&mutex_ack);

	pthread_mutex_lock(&mutex_timer);
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
		if(next_seq_num-oldest_unacked<n){
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

void * rdt_send(void *ptr)
{
	int i,j;
//	pthread_t timer_thread;
	//printf("In RDT_SEND!\n");
	while(1)
	{
		//printf("Checking for Buff...\n");
		if(is_buffer_avail())	
		{
			printf("Buffer Available: Getting 1 MSS from File!\n");
			//printf("Next Seq Num is: %d\n",next_seq_num);
			char *tmp = read_file();
			if(strstr(tmp,"~") == NULL){ 
				
				strcpy(send_buffer[next_seq_num%n].data,tmp); //copy 1 segment data into sender buffer
				printf("Data at %d\t is: %s",next_seq_num%n,send_buffer[next_seq_num%n].data);
			}
			else //Condition says that EOF has reached, break from loop. TODO : Last segment containing EOF has pkt loss right now.
			{	
				printf("Breaking!\n");
				break;
			}
			printf("New Data Read: %s\n",send_buffer[next_seq_num%n].data);
                	send_buffer[next_seq_num%n].data[strlen(send_buffer[next_seq_num%n].data)] = '\0';
			create_segment(next_seq_num);
			for(j=0;j<no_of_receivers;j++)
				udt_send(next_seq_num,j);
			printf("Out of UDT_SEND..\n");
			if(is_pkt_earliest_transmitted())
			{
				printf("Packet %d is earliest transmitted so need to start timer",next_seq_num);
				pthread_mutex_lock(&mutex_timer);
					start_timer = next_seq_num;
					//start_timer_thread(timer_thread);
				pthread_mutex_unlock(&mutex_timer);
			}

			next_seq_num=(next_seq_num+1)%MAX_SEQ;
			printf("Next Seq Num in Send Thread is: %d\n",next_seq_num);
		}
		//printf("Lock in Timer..\n");		
		pthread_mutex_lock(&mutex_timer);
			if(start_timer != -1) //this means timer should start now
			{
				printf("Started timer for segment %d\n",start_timer);
				timer_seq_num = start_timer;
				start_timer = -1;
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
	next_seq_num = 0;
	
	//pthread_t sender_thread;
//	pthread_t timer_thread;
	//pthread_t recv_thread;

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

