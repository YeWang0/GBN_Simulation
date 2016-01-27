/* GBN_server.c */
/* Programmed by Ye Wang */
/* December 2, 2015 */

#include <ctype.h>          /* for toupper */
#include <stdio.h>          /* for standard I/O functions */
#include <stdlib.h>         /* for exit */
#include <string.h>         /* for memset */
#include <sys/socket.h>     /* for socket, sendto, and recvfrom */
#include <netinet/in.h>     /* for sockaddr_in */
#include <unistd.h>         /* for close */
#include <sys/time.h>
# define Packet struct  packet
#define STRING_SIZE 1024

/* SERV_UDP_PORT is the port number on which the server listens for
 incoming messages from clients. You should change this to a different
 number to prevent conflicts with others in the class. */

#define SERV_UDP_PORT 12345
static int seed=1;//for random number generation
double GetRand();//for random number generation
double GetRand2();//for random number generation
int simulate(float Packet_loss_rate,float Packet_delay);//simulate different packet receive conditions
int ACKsimulate(float ACK_loss_rate);//simulate different ACK send conditions
void Write_Line(FILE *fp,char * buf);//write a line into output file

int main(void) {
    //config parameters
    int Window_size=1;
    int Packet_delay=0;
    float Packet_loss_rate=0;
    float ACK_loss_rate=0;
    char output_file[STRING_SIZE];
    printf("Please input a number between 1 and 8 as the Window size:\n");
    scanf("%d",&Window_size);
    printf("Please input a number between 0 and 1 as the Packet loss rate:\n");
    scanf("%f",&Packet_loss_rate);
    printf("Please input a number  to indicate packet delay:\n 0. No delay 1. Delay\n");
    scanf("%d",&Packet_delay);
    printf("Please input a number between 0 and 1 as the ACK loss rate:\n");
    scanf("%f",&ACK_loss_rate);
    printf("Please input the name of output file:\n");
    scanf("%s",output_file);
    
    //open output file
    FILE *fp;
    if((fp=fopen(output_file,"wb+"))==NULL){
        printf("Cannot open file strike any key exit!");
        exit(1);
    }
    //store data converted from network byte to host byte
    int Host_Count=0;
    int Host_Seq=0;
    //initialize statistic numbers
    int received_packets=0;
    int received_bytes=0;
    int duplicate_packets=0;
    int lost_packets=0;
    int total_packets=0;
    int transmitted_ACKs=0;
    int dropped_ACKs=0;
    int total_ACKs=0;
    //initial other parameters
    unsigned short Expected_seqnum=0;//store expected sequence number
    unsigned short Received_seqnum=0;//store sequence number in a packet received
    unsigned short ACK_num=0;//store ack sent to client
    unsigned short Count;//store
    char Data[80];//store data field in a packet received
    
    //socket related parameters
    int sock_server;  /* Socket on which server listens to clients */
    
    struct sockaddr_in server_addr;  /* Internet address structure that
                                      stores server address */
    unsigned short server_port;  /* Port number used by server (local port) */
    
    struct sockaddr_in client_addr;  /* Internet address structure that
                                      stores client address */
    unsigned int client_addr_len;  /* Length of client address structure */
    
    char sentence[STRING_SIZE];  /* receive message */
    char modifiedSentence[STRING_SIZE]; /* send message */
    unsigned int msg_len;  /* length of message */
    int bytes_sent, bytes_recd; /* number of bytes sent or received */
    unsigned int i;  /* temporary loop variable */
    /* open a socket */
    if ((sock_server = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
        perror("Server: can't open datagram socket\n");
        exit(1);
    }
    
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl (INADDR_ANY);  /* This allows choice of
                                                        any host interface, if more than one
                                                        are present */
    server_port = SERV_UDP_PORT; /* Server will listen on this port */
    server_addr.sin_port = htons(server_port);
    /* bind the socket to the local server port */
    
    if (bind(sock_server, (struct sockaddr *) &server_addr,
             sizeof (server_addr)) < 0) {
        perror("Server: can't bind to local address\n");
        close(sock_server);
        exit(1);
    }
    
    /* wait for incoming messages in an indefinite loop */
    
    printf("Waiting for incoming messages on port %hu\n\n",
           server_port);
    
    client_addr_len = sizeof (client_addr);
    
    //receive packets from client
    for (;;) {
        //clear sentence which is used to store received message
        memset(&sentence, 0, sizeof(sentence));
        //receive message
        bytes_recd = recvfrom(sock_server, &sentence, STRING_SIZE, 0,
                              (struct sockaddr *) &client_addr, &client_addr_len);
        //receive packet
        if (bytes_recd!=0) {
            // get information in the packet received
            memcpy(&Host_Seq,&sentence[2],2);
            Received_seqnum=ntohs(Host_Seq);
            
            memcpy(&Host_Count,sentence,2);
            Count=ntohs(Host_Count);
            //close connection if count = 0
            if (Count==0) {
                printf("End of Transmission Packet with sequence number %u received with %u data bytes\n\n",Received_seqnum,Count);
                //close
                fclose(fp);
                printf("Number of data packets received successfully (without loss and not including duplicates) : %d\n",received_packets);
                printf("Total number of data bytes received which are delivered to user (this should be the sum of the count fields of all packets received successfully without loss and without duplicates) : %d\n",received_bytes);
                printf("Total number of duplicate data packets received (without loss) : %d\n",duplicate_packets);
                printf("Number of data packets received but dropped due to loss : %d\n",lost_packets);
                printf("Total number of data packets received (including those that were successful, those lost, and duplicates) : %d\n",total_packets);
                printf("Number of ACKs transmitted without loss : %d\n",transmitted_ACKs);
                printf("Number of ACKs generated but dropped due to loss : %d\n",dropped_ACKs);
                printf("Total number of ACKs generated (with and without loss) :%d\n",total_ACKs);
                
                return 0;
            }
            //use simulat() to decide packet loss and delay
            if(simulate(Packet_loss_rate,Packet_delay)){
                if (Received_seqnum==Expected_seqnum) {
                    //accept packet
                    printf("Packet %u received with %u data bytes\n",Received_seqnum,Count);
                    //get data
                    memset(&Data,0,sizeof(Data));
                    memcpy(&Data,&sentence[4],Count);
                    //write data into output file
                    Write_Line(fp,Data);
                    printf("Packet %d delivered to user\n",Received_seqnum);
                    received_packets++;
                    total_packets++;
                    received_bytes+=Count;
                    
                    ACK_num=htons(Expected_seqnum);
                    //send ACK
                    if (ACKsimulate(ACK_loss_rate)) {
                        bytes_sent = sendto(sock_server, &ACK_num, 2, 0,
                                            (struct sockaddr*) &client_addr, client_addr_len);
                        printf("ACK %u transmitted\n",Received_seqnum);
                        transmitted_ACKs++;
                        total_ACKs++;
                    }
                    //simulate ACK lose
                    else{
                        printf("ACK %u lost\n",Received_seqnum);
                        dropped_ACKs++;
                        total_ACKs++;
                    }
                    //update expected sequence number
                    Expected_seqnum=(Expected_seqnum+1)%16;
                    
                }else{
                    //not expected sequence number
                    printf("Duplicate packet %u received with %u data bytes\n",Received_seqnum,Count);
                    duplicate_packets++;
                    total_packets++;
                    //transmit  ACK with the newest accepted packet sequence number
                    ACK_num=htons((Expected_seqnum+15)%16);
                    if (ACKsimulate(ACK_loss_rate)) {
                        bytes_sent = sendto(sock_server, &ACK_num, 2, 0,
                                            (struct sockaddr*) &client_addr, client_addr_len);
                        printf("ACK %u transmitted\n",ntohs(ACK_num));
                        transmitted_ACKs++;
                        total_ACKs++;
                    }
                    else{
                        //simulate ACK lose
                        printf("ACK %u lost\n",ntohs(ACK_num));
                        dropped_ACKs++;
                        total_ACKs++;
                    }
                }
            }else{
                //simulate packet lose
                printf("Packet %u lost\n",Received_seqnum);
                lost_packets++;
                total_packets++;
            }
            printf("\n");
        }
    }
}
double GetRand()
{
    struct timeval tv;
    gettimeofday(&tv,NULL);
    srand(tv.tv_usec%17+seed++);
    return((rand()% 1000) / 1000.0);
}
double GetRand2()
{
    struct timeval tv;
    gettimeofday(&tv,NULL);
    srand(tv.tv_usec%11+tv.tv_usec%3+seed++);
    return((rand()% 10000) / 10000.0);
}
int simulate(   float Packet_loss_rate,float Packet_delay)
{
    float a=GetRand();
    if (a <Packet_loss_rate){
        //drop packet
        return 0;
    }else{
        if (Packet_delay==0) {
            return 1;
        }
        else{
            //delay
            a=GetRand2();
            int i,j;
            j=(int)(a*100000000);
            for (i=0;i<j ; i++) ;
//            printf("\nDelayed %f %d\n",a,j);
            return 1;
        }
    }
}
int ACKsimulate(float ACK_loss_rate)
{
    float a=GetRand2();
    if (a <ACK_loss_rate){
        //ack lost
        return 0;
    }else{
        return 1;
    }
}
void Write_Line(FILE *fp,char * buf){
    fputs(buf, fp);
}
