/* GBN_client.c */
/* Programmed by Ye Wang */
/* December 2, 2015 */

/* Version of udp_client.c that is nonblocking */

#include <stdio.h>          /* for standard I/O functions */
#include <stdlib.h>         /* for exit */
#include <string.h>         /* for memset, memcpy, and strlen */
#include <netdb.h>          /* for struct hostent and gethostbyname */
#include <sys/socket.h>     /* for socket, sendto, and recvfrom */
#include <netinet/in.h>     /* for sockaddr_in */
#include <unistd.h>         /* for close */
#include <fcntl.h>          /* for fcntl */
#include <sys/time.h>   //for timer
#include <math.h>       //for pow()

# define Packet struct  packet  //for struct usage
#define STRING_SIZE 1024

typedef enum { false, true } bool;
//check if timeout happened or not
bool CheckTimeout(struct timeval Retime);
//read a line from input file
int Read_Line(FILE *fp,char* buf);
//set the time of retransmission
void setRetransmitTime(struct timeval timer, struct timeval timeout,struct timeval *Retime);


int main(void) {
    int sock_client;  /* Socket used by client */
    struct sockaddr_in client_addr;  /* Internet address structure that
                                      stores client address */
    unsigned short client_port;  /* Port number used by client (local port) */
    struct sockaddr_in server_addr;  /* Internet address structure that
                                      stores server address */
    struct hostent * server_hp;      /* Structure to store server's IP
                                      address */
    char server_hostname[STRING_SIZE]="127.0.0.1"; /* Server's hostname */
    unsigned short server_port=12345;  /* Port number used by server (remote port) */
    char sentence[STRING_SIZE];  /* send message */
    char modifiedSentence[STRING_SIZE]; /* receive message */
    unsigned int msg_len;  /* length of message */
    int bytes_sent, bytes_recd; /* number of bytes sent or received */
    int fcntl_flags; /* flags used by the fcntl function to set socket
                      for non-blocking operation */
    
    //config parameters
    int Window_size=1;//1-8
    int Time_out=10;//1-10
    char input_file[STRING_SIZE];
    
    printf("Enter hostname of server: ");
    scanf("%s", server_hostname);
    printf("Please input a number between 1 and 8 as the Window size:\n");
    scanf("%d",&Window_size);
    printf("Please input a number between 1 and 10 as the Timeout(10^n usec):\n");
    scanf("%d",&Time_out);
    printf("Please input the name of input file:\n");
    scanf("%s",input_file);
    
    //initialize other parameters
    short Count=0;//0-80
    short Sequence_number=0;//0-15
    int Window_begin=0;
    int Window_end=Window_begin+Window_size-1;
    unsigned short ACK_num=0;
    //store unacked packets
    struct  packet{
        unsigned short Count;
        unsigned short Sequence_number;
        char Data[80];
    };
    Packet Send_buffer[16];
    memset(&Send_buffer, 0, sizeof (Send_buffer));
    
    //set timeout
    struct timeval timer;
    struct timeval timeout;
    struct timeval Retime;
    int t=0;
    int value=1;
    if(Time_out>=6){
	for(t=0;t<Time_out-6;t++)
		value*=10;
    	timeout.tv_sec=value;
    	timeout.tv_usec=0;
    }else{
	for(t=1;t<Time_out;t++)
		value*=10;
    	timeout.tv_usec=value;
    	timeout.tv_sec=0;
    } 
    int initial_packets=0;
    int total_bytes=0;
    int retransmit_packets=0;
    int total_packets=0;
    int total_ACKs=0;
    int timeout_times=0;
    //store network byte
    int Net_Count=0;
    int Net_Seq=0;
    char buf[80];
    int i=-1;
    
    //open file to read
    FILE *fp;
    fp = fopen(input_file, "r");
    
    /* open a socket */
    
    if ((sock_client = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
        perror("Client: can't open datagram socket\n");
        exit(1);
    }
    /* initialize client address information */
    
    client_port = 0;   /* This allows choice of any available local port */
    
    /* clear client address structure and initialize with client address */
    memset(&client_addr, 0, sizeof(client_addr));
    client_addr.sin_family = AF_INET;
    client_addr.sin_addr.s_addr = htonl(INADDR_ANY); /* This allows choice of
                                                      any host interface, if more than one
                                                      are present */
    client_addr.sin_port = htons(client_port);
    
    /* bind the socket to the local client port */
    
    if (bind(sock_client, (struct sockaddr *) &client_addr,
             sizeof (client_addr)) < 0) {
        perror("Client: can't bind to local address\n");
        close(sock_client);
        exit(1);
    }
    /* make socket non-blocking */
    fcntl_flags = fcntl(sock_client, F_GETFL, 0);
    fcntl(sock_client, F_SETFL, fcntl_flags | O_NONBLOCK);
    
    /* initialize server address information */
    if ((server_hp = gethostbyname(server_hostname)) == NULL) {
        perror("Client: invalid server hostname\n");
        close(sock_client);
        exit(1);
    }
    /* Clear server address structure and initialize with server address */
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    memcpy((char *)&server_addr.sin_addr, server_hp->h_addr,
           server_hp->h_length);
    server_addr.sin_port = htons(server_port);
    
    
    /* Start transmission with GBN  */
    //send the packets within the first window size
    while(i++<Window_size-1){
        //read a line from input file
        if(Read_Line(fp,buf)){
            //put packet into send buffer
            //length of packet should be 4+count
            msg_len = strlen(buf) + 4;
            Send_buffer[i].Count=msg_len-4;
            Send_buffer[i].Sequence_number=Sequence_number;
            Sequence_number++;
            strcpy(Send_buffer[i].Data,buf);
            //transmit packet
            Net_Count=htons(Send_buffer[i].Count);
            Net_Seq=htons(Send_buffer[i].Sequence_number);
            //clear sentence
            memset(&sentence, 0, sizeof(sentence));
            memcpy(sentence,&Net_Count,2);
            memcpy(&sentence[2],&Net_Seq,2);
            memcpy(&sentence[4],Send_buffer[i].Data,Send_buffer[i].Count);
            /* send message */
            printf("Packet %u transmitted with %u data bytes\n",Send_buffer[i].Sequence_number,Send_buffer[i].Count);
            initial_packets++;
            total_packets++;
            total_bytes+=Send_buffer[i].Count;
            
            bytes_sent = sendto(sock_client,sentence, msg_len, 0,
                                (struct sockaddr *) &server_addr, sizeof (server_addr));
            //set time out for the first iteration
            if (i==0) {
                gettimeofday(&timer,NULL);
                setRetransmitTime(timer,timeout,&Retime);
            }
        }
    }
    printf("\n");
    do {  /* loop required because socket is nonblocking */
        //check time out
        if(!CheckTimeout(Retime)){
            //receive ack from server
            bytes_recd = recvfrom(sock_client, &ACK_num, STRING_SIZE, 0,
                                  (struct sockaddr *) 0, (int *) 0);
            if (bytes_recd>0) {
                //get ack number
                ACK_num=ntohs(ACK_num);
                printf("ACK %u received\n",ACK_num);
                total_ACKs++;
                //move window if ack number is in  send window
                if ((ACK_num<=Window_end&&ACK_num>=Window_begin)||(Window_end<Window_begin&&(Window_begin<=ACK_num||Window_end>=ACK_num))) {
                    gettimeofday(&timer,NULL);
                    setRetransmitTime(timer,timeout,&Retime);
                    int j,n;
                    n=((ACK_num+16)-Window_begin)%16+1;
                    if(n>Window_size||n<=0){
                        printf("error\n");
                        return 0;
                    }
                    Window_begin=(ACK_num+1)%16;
                    //move window  forward
                    for(j=0;j<n;j++){
                        
                        if(Read_Line(fp,buf)){
                            msg_len = strlen(buf) + 4;
                            Send_buffer[Sequence_number].Count=msg_len-4;
                            Send_buffer[Sequence_number].Sequence_number=Sequence_number;
                            strcpy(Send_buffer[Sequence_number].Data,buf);
                            
                            Net_Count=htons(Send_buffer[Sequence_number].Count);
                            Net_Seq=htons(Send_buffer[Sequence_number].Sequence_number);
                            
                            printf("Packet %u transmitted with %u data bytes\n",Send_buffer[Sequence_number].Sequence_number,Send_buffer[Sequence_number].Count);
                            
                            initial_packets++;
                            total_packets++;
                            total_bytes+=Send_buffer[Sequence_number].Count;
                            //clear sentence
                            memset(&sentence, 0, sizeof(sentence));
                            memcpy(sentence,&Net_Count,2);
                            memcpy(&sentence[2],&Net_Seq,2);
                            memcpy(&sentence[4],Send_buffer[Sequence_number].Data,Send_buffer[Sequence_number].Count);
                            //set window end number and next untransmitted packet number
                            Window_end=Sequence_number;
                            Sequence_number=(Sequence_number+1)%16;
                            
                            /* send message */
                            bytes_sent = sendto(sock_client,sentence, msg_len, 0,
                                                (struct sockaddr *) &server_addr, sizeof (server_addr));
                        }
                    }
                }
                printf("\n");
            }
        }
        //if timeout happened, retransmit the packets in send buffer
        else{
            int i,t,end;
            printf("Timeout expired for packet numbered %d,%d\n",Window_begin,Window_end);
            timeout_times++;
            if(Window_begin>Window_end){
                end=Window_end+16;
            }else{
                end=Window_end;
            }
            //retransmit the packets in send buffer
            for(i=Window_begin;i<=end;i++){
                t=i%16;
                msg_len=Send_buffer[t].Count+4;
                Net_Count=htons(Send_buffer[t].Count);
                Net_Seq=htons(Send_buffer[t].Sequence_number);
                memset(&sentence, 0, sizeof(sentence));
                memcpy(sentence,&Net_Count,2);
                memcpy(&sentence[2],&Net_Seq,2);
                memcpy(&sentence[4],Send_buffer[t].Data,Send_buffer[t].Count);
                
                printf("Packet %u retransmitted with %u data bytes\n",Send_buffer[t].Sequence_number,Send_buffer[t].Count);
                
                retransmit_packets++;
                total_packets++;
                
                /* send message */
                bytes_sent = sendto(sock_client,sentence, msg_len, 0,
                                    (struct sockaddr *) &server_addr, sizeof (server_addr));
                if (i==Window_begin) {
                    gettimeofday(&timer,NULL);
                    setRetransmitTime(timer,timeout,&Retime);
                }
            }
            printf("\n");
        }
    }
    while (Window_begin!=(Window_end+1)%16);        //all acks received, stop loop
    //clean send buffer
    memset(&Send_buffer, 0, sizeof (Send_buffer));
    
    /* send EOT message */
    Net_Count=htons(Send_buffer[0].Count);
    Net_Seq=htons(Sequence_number);
    memset(&sentence, 0, sizeof(sentence));
    memcpy(sentence,&Net_Count,2);
    memcpy(&sentence[2],&Net_Seq,2);
    printf("End of Transmission Packet with sequence number %u transmitted with %u data bytes!\n",Sequence_number,Send_buffer[0].Count);
    bytes_sent = sendto(sock_client,sentence, 4, 0,
                        (struct sockaddr *) &server_addr, sizeof (server_addr));
    
    /* close the socket and file */
    fclose(fp);//关闭文件。
    close (sock_client);
    printf("Transmission is finished! \n\n");
    //print summary information
    printf("Number of data packets transmitted (initial transmission only): %d\n",initial_packets);
    printf("Total number of data bytes transmitted (this should be the sum of the count fields of all transmitted packets when transmitted for the first time only): %d\n",total_bytes);
    printf("Total number of retransmissions: %d\n",retransmit_packets);
    printf("Total number of data packets transmitted (initial transmissions plus retransmissions): %d\n",total_packets);
    printf("Number of ACKs received: %d\n",total_ACKs);
    printf("Count of how many times timeout expired: %d\n",timeout_times);
    
    return 0;
    
}
//read a line from input file
int Read_Line(FILE *fp,char * buf){
    
    if(fgets(buf, 80,fp)){
        return 1;
    }else{
        return 0;
    }
}
//set the time of retransmission
void setRetransmitTime(struct timeval timer, struct timeval timeout,struct timeval *Retime){
    Retime->tv_usec=(timer.tv_usec+timeout.tv_usec)%1000000;
    Retime->tv_sec=timer.tv_sec+timeout.tv_sec+(timer.tv_usec+timeout.tv_usec)/1000000;
    
}
//check if timeout happened or not
bool CheckTimeout(struct timeval Retime){
    struct timeval tv;
    gettimeofday(&tv,NULL);
    if(tv.tv_sec<Retime.tv_sec){
        return false;
    }else if(tv.tv_sec>Retime.tv_sec){
        return true;
    }else{
        if (tv.tv_usec<Retime.tv_usec) {
            return false;
        }
        else{
            return true;
        }
    }
}
