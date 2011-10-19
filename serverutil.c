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

segment curr_pkt,*recv_buffer;
int curr_pkt_seq_num;
//segment *recv_ack_buffer;
int flag;

int last_packet;
int n;
FILE *file;
int server_port;
float p;
//segment curr_pkt,*recv_buffer;
//segment curr_pkt;
int buf_counter;
uint32_t next_seg_seq_num;
int soc,send_soc;
struct sockaddr_in sender_addr;



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
	printf("Allocated %d segments of Total length: %d\n",n,(int)(n*sizeof(segment)));
	next_seg_seq_num = 0;	
	buf_counter = 0; //not used`
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
	my_addr.sin_port = htons(server_port);// short, network byte order
	my_addr.sin_addr.s_addr = INADDR_ANY; // automatically fill with my IP
	memset(my_addr.sin_zero, '\0', sizeof my_addr.sin_zero);

	if (bind(soc, (struct sockaddr *)&my_addr, sizeof my_addr) == -1) {
		printf("\n bind failed");
		exit(-1);
	}
	
	init_recv_window();
	last_packet = 1; //not used
}


int udt_send(int seg_index)
{
//        struct sockaddr_in their_addr; // connector's address information
        struct hostent *he;
        int numbytes;
        int addr_len;
        int len;
        char buf[MAXLEN];

        strcpy(buf,"");

	//SEND THE ACK For Current Packet (seg_index)
	//Right now overrighting the recv_buffer's pkt_type to indicate that it is now an ACK Packet (Logically) ..

	recv_buffer[seg_index].pkt_type = 0xAAAA;  //indicates ACK packet - 1010101010101010
        sprintf(buf,"%d\n%d\n%d\n",recv_buffer[seg_index].seq_num,0,recv_buffer[seg_index].pkt_type);
        
/*
	//socket to send
        if ((send_soc = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
                printf("Error creating socket\n");
                exit(-1);
        }
*/

        len = strlen(buf);
        printf("\n\n***********ACK Bytes Sent: %d",len);
//      if (sendto(send_soc,buf, len, 0, (struct sockaddr *)&sender_addr, sizeof (sender_addr)) == -1) {
	if (sendto(soc,buf, len, 0, (struct sockaddr *)&sender_addr, sizeof (sender_addr)) == -1) {
                printf("Error in sending");
                exit(-1);
        }

}


//Sending ack for the curr_pkt_seq_num  (wrapped around to get correct 'recv_buffer' index)  MSS in 'recv_buffer'
int send_ack(int seg_index)
{
		int index = seg_index%n;
		printf("\nSent ACK For Index: %d",index);
		udt_send(index);

}

int compare_checksum(uint16_t checksum)
{
	if(curr_pkt.checksum == checksum)
		return TRUE;
	return FALSE;
}


int add_to_buffer(int index)
{
        printf("Index is %d\n",index);
	
	
        recv_buffer[index].seq_num = curr_pkt.seq_num;

	//ARRIVED INDICATES current packet is arrived
	recv_buffer[index].arrived = TRUE;

/*        if((recv_buffer[index].data = (char*)malloc(strlen(curr_pkt.data)*sizeof(char)))==NULL){
                perror("CANNOT ALLOCATE MEMORY :( : Add to Buffer \n");
                exit(1);
        }*/
        strcpy(recv_buffer[index].data,curr_pkt.data);
        //strcat(recv_buffer[index].data,"\0"); 

        printf("Length of data in packet with seq_num=%d DATA: %d\n",curr_pkt.seq_num,(int)strlen(curr_pkt.data));
	curr_pkt_seq_num = curr_pkt.seq_num;
	if(flag)
	{
	        //free(curr_pkt.data);
		curr_pkt.data == NULL;
		flag = 0;
		//printf("freed!!!\n");
	}
        return TRUE;
}

int udt_recv()
{
	char buf[MAXLEN];
	int numbytes,data_length;
	char *a, *b, *c, *d, *e,*f;
	flag = 0; 
//	struct sockaddr_in their_addr; // connector's address information
	int addr_len = sizeof (sender_addr);
	strcpy(buf,"");
	numbytes=recvfrom(soc, buf, MAXLEN , 0,(struct sockaddr *)&sender_addr, &addr_len);
	if(numbytes == -1) {
		printf(" Error in receiving\n");
		exit(-1);
	}
	printf("\n\n***********Bytes Received: %d\n",numbytes);	
//	printf("the received string is%s\n",buf);
//	buf[strlen(buf)] = '\0';
	a = strtok_r(buf,"\n",&d);
	curr_pkt.seq_num = (uint32_t)atoi(a);

	b = strtok_r(NULL,"\n",&d);
	curr_pkt.checksum = (uint16_t)atoi(b);

	c = strtok_r(NULL,"\n",&d);
	curr_pkt.pkt_type = (uint16_t)atoi(c);

/*	if((curr_pkt.data = malloc(strlen(d)*sizeof(char))) == NULL)
	{
		printf("Error allocating memory\n");
		exit(-1);
	}*/
	strcpy(curr_pkt.data,d);
	flag = 1;
/*	f = strtok(NULL,"\n"); //'f' contains the data_length passed from sender
	d = strtok(NULL,"\0"); //This is not allowed, since d is not a null terminated string now.
//	strcat(d,'\0');
//	e = strtok(a,":");
//	e = strtok(NULL,":");
	e = strtok(b,":");
	e = strtok(NULL,":");
	e = strtok(c,":");
	e = strtok(NULL,":");
	
	e = strtok(f,":");
	e = strtok(NULL,":");
	data_length = (uint16_t)atoi(e);

	fflush(stdout);
	d[data_length] = '\0'; //delimit the data string	*/

	printf("seq_num: %d, checksum: %x, packet type: %x\n",curr_pkt.seq_num,curr_pkt.checksum,curr_pkt.pkt_type);
/*	if((curr_pkt.data = (char*)malloc(strlen(d)*sizeof(char)))==NULL)
	{
		perror("CANNOT ALLOCATE MEMORY :( For Curr Packet Data\n");
		exit(1);
	}
	strcpy(curr_pkt.data,d);
	strcat(curr_pkt.data,"\0");*/
	printf("For received data.. length is %d\n data is %s\n",(int)strlen(d),d);
	printf("For copied data.. length is %d\n data is %s\n\n",(int)strlen(curr_pkt.data),curr_pkt.data);
	strcpy(d,"");	
}


int is_next_expected()
{
	if(curr_pkt.seq_num == next_seg_seq_num) //In Sequence TRUE
		return TRUE;
	else
		return FALSE;
}

int is_gap_filled()
{
	if(recv_buffer[(curr_pkt.seq_num%n)-1].seq_num == curr_pkt.seq_num-1 && recv_buffer[(curr_pkt.seq_num%n)+1].seq_num == curr_pkt.seq_num+1)
		return TRUE;
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

//writing 1 MSS Data into file .. called after checksum comparison
int write_file(char *temp_buf)
{
    	int j=0,numbytes;
	char buff[1];
	printf("\nWritten! %d bytes",(int)strlen(temp_buf));
	if((numbytes=fwrite(temp_buf,strlen(temp_buf),1,file))<0)
	{
		perror("\nWrite error");
		exit(1);
	}
	return numbytes;
}

int process_pkt()
{
	if(is_next_expected()){
		return 1; //
	}
	else{  //Curr Pkt is not in sequence
		if((is_gap_filled() && !is_next_expected()) || (!is_gap_filled() && !is_next_expected())) //Gap is filled but the packet is out of sequence
		{
			//Buffer the curr_pkt
			return 2;
			
		}
	}
	return 0;
       // while (is_in_sequence())
		//printf("Received packet is:\nSeq Number:%d\nChecksum:%d\npacket Type:%d\ndata: %s\n",curr_pkt.seq_num,curr_pkt.checksum,curr_pkt.pkt_type,curr_pkt.data);
                //write_file();
}

void slide_window()
{
	next_seg_seq_num++;
}

int recv_data()
{
        int ret_val,process_ret;
        uint16_t recv_checksum;

	//Got a new packet
	udt_recv();

	recv_checksum = compute_checksum(curr_pkt.data);
	if(!(compare_checksum(recv_checksum)))
	{
		printf("Checksum mismatch\n");
                return FALSE; //Discard and no ACK Sent
	}

	ret_val = is_in_recv_window();
        if(ret_val == -1)
                return TRUE;
        else if(ret_val == 0)
                return FALSE;

	process_ret = process_pkt();
	if(process_ret == 1) //Add to Buffer, slide the window, send ACK
	{
		add_to_buffer(ret_val-1);
	
		int i;	
		for(i = next_seg_seq_num%n;recv_buffer[i].arrived == TRUE;i=(i+1)%n){
			slide_window();
			write_file(recv_buffer[i].data); //when the window slides, write the corresponding segment to file
			recv_buffer[i].arrived = FALSE;
			//FLUSH i'th index in recv_buffer
		}
		//send ACK for curr_pkt.seq_num
		send_ack(next_seg_seq_num-1);
		
	}
	else if(process_ret == 2) //Add to Buffer, send ACK for most recent in-sequence packet
	{
		add_to_buffer(ret_val-1);
		//send ACK for most in-sequence packet ... next_seg_seq_num
		send_ack(next_seg_seq_num-1);
	}

	//Write this 1 MSS to file
//	write_file(curr_pkt.data);
        return TRUE;
}

