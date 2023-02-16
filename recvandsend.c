#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <netpacket/packet.h>
#include<arpa/inet.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include<linux/if_ether.h>
#define PORT 8888
#define MAX_PACKET_SIZE 65536

//************QUEUE of character array**********************
#define QUEUE_SIZE 5
#define MAX_STR_LEN 100
pthread_cond_t not_full, not_empty;
pthread_mutex_t lock;

typedef struct {
    int head;
    int tail;
    char queue[QUEUE_SIZE][MAX_STR_LEN];
} Queue;
struct thread_args{
       int first;
       Queue *second;
       int third;
};


//Functions
Queue* createQueue() {
    Queue* q = (Queue*)malloc(sizeof(Queue));
    q->head = 0;
    q->tail = 0;
    return q;
}

// int isQueueFull(Queue* q) {
//     return ((q->tail + 1) % QUEUE_SIZE == q->head);
// }

// int isQueueEmpty(Queue* q) {
//     return (q->head == q->tail);
// }

void enqueue(Queue* q, unsigned char* str,int header_len) {
    pthread_mutex_lock(&lock);
    while ((q->tail + 1) % QUEUE_SIZE == q->head) {
        pthread_cond_wait(&not_full, &lock);
    }
    
    //strcpy(q->queue[q->tail], str);
    memcpy(q->queue[q->tail],str,header_len);
     
    printf("packet %d enqueued!\n",q->tail);
    q->tail = (q->tail + 1) % QUEUE_SIZE;
    
    pthread_cond_signal(&not_empty);
    pthread_mutex_unlock(&lock);
}

unsigned char* dequeue(Queue* q) {
    pthread_mutex_lock(&lock);
    while(q->head == q->tail){
        pthread_cond_wait(&not_empty,&lock);
    }
    //unsigned char* str = (unsigned char *) malloc(header_len);
    //printf("$$$$: %s",q->queue[q->head]);
    unsigned char* str = q->queue[q->head];
    //memcpy(str,q->queue[q->head],header_len);
    //debug
    //printf("\nDEBUG:");
//    for (int i = 0; i < header_len; i++) {
//            if(i!=0 && i%16==0)
//              printf("\n");
//            printf("%02x ", str[i]);
//    }
    printf("\npacket %d dequeued!\n",q->head);
    
    q->head = (q->head + 1) % QUEUE_SIZE;
    
    pthread_cond_signal(&not_full);
    pthread_mutex_unlock(&lock);
    return str;
}
//***********End of Queue implementation********************************************


// Thread 1 function to receive packets using raw sockets
void* thread_1(void* arg) {
    //int sock_fd = *(int*)arg;
    struct thread_args *args = (struct thread_args *)arg;
    int sock_fd = args->first;
    Queue* q = args->second; 
    int* header_len = &args->third;
    printf("thread1\n");
    unsigned char *packet = (unsigned char *) malloc(MAX_PACKET_SIZE);
    memset(packet,0,65536);
    //Receive packet
    while (1) {
        //char packet[MAX_PACKET_SIZE];
        

        int packet_size = recvfrom(sock_fd, packet, MAX_PACKET_SIZE, 0, NULL, NULL);
        *header_len = packet_size;
        if (packet_size < 0) {
            perror("Error receiving packet");
            continue;
        }

        // Pass the received packet to Thread 2
        enqueue(q,packet,*header_len);
        //Display received packet
        //struct ethhdr *eth = (struct ethhdr *)packet;
        struct ether_header *eth_header = (struct ether_header *)packet;
        unsigned char *data = packet ;
        //sizeof(struct ether_header);
        int data_len = packet_size - sizeof(struct ether_header);

        // Print the source and destination MAC addresses
        printf("Source MAC: %02x:%02x:%02x:%02x:%02x:%02x\n",
                eth_header->ether_shost[0], eth_header->ether_shost[1],
                eth_header->ether_shost[2], eth_header->ether_shost[3],
                eth_header->ether_shost[4], eth_header->ether_shost[5]);
        printf("Destination MAC: %02x:%02x:%02x:%02x:%02x:%02x\n",
                eth_header->ether_dhost[0], eth_header->ether_dhost[1],
                eth_header->ether_dhost[2], eth_header->ether_dhost[3],
                eth_header->ether_dhost[4], eth_header->ether_dhost[5]);
        printf("Protocol: 0x%04x\n", ntohs(eth_header->ether_type));
        //printf("Data: %s\n", data);
        //Extracting data
        printf("DATA:\n");
        for(int i=0;i<data_len;i++)
        {
          if(i!=0 && i%16==0)
          printf("\n");
          printf("%.2X ",data[i]);
        }
        printf("\n packet size: %d",*header_len);
        printf("\n****************************enqueued***************************\n");
    }
    return NULL;
}

// Thread 2 function to modify and send packets
void* thread_2(void* arg) {
    sleep(2);
    //int sock_fd = *(int*)arg;
    struct thread_args *args = (struct thread_args *)arg;
    int sock_fd = args->first;
    Queue* q = args->second;
    int* header_len = &args->third;
    while (1) {
        // Receive packet from Thread 1
        unsigned char *packet;
         //unsigned char *packet = (unsigned char *) malloc(*header_len);
         //memset(packet,0,*header_len);
         //memcpy(packet,dequeue(q,*header_len),*header_len);
        packet=dequeue(q);
        // Modify the packet
        // ...

        // Send the modified packet back to the client
        // ...
        //Before sending
        printf("\nEthernet packet:\n");
        for (int i = 0; i < *header_len; i++) {
            if(i!=0 && i%16==0)
              printf("\n");
            printf("%02x ", packet[i]);
        }
        printf("\n packet size: %d",*header_len);
        int sent =0;
        int size = *header_len;
        if((sent = write(sock_fd,packet,size)) != size)
            printf("\ncould only send %d bytes of packet of length %d \n",sent,size);
         else{
            printf("\npacket send!!!");
        }
        printf("\n*********************dequeue over*******************\n");
    }
    
    return NULL;
}

int main() {

    
    pthread_mutex_init(&lock, NULL);
    pthread_cond_init(&not_full, NULL);
    pthread_cond_init(&not_empty, NULL);
    int sock_fd;
    //struct sockaddr_in server_addr;

    // Create raw socket
    sock_fd =  socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if (sock_fd < 0) {
        perror("Error creating socket");
        exit(-1);
    }

    // Bind the socket to a specific interface
    struct sockaddr_ll sll;
    struct ifreq ifr;
    bzero(&sll,sizeof(sll));
    bzero(&ifr,sizeof(ifr));

    //first get interface index
    strncpy(ifr.ifr_name, "enp96s0f1", IFNAMSIZ);
    if (ioctl(sock_fd, SIOCGIFINDEX, &ifr) < 0) {
        perror("ioctl");
        exit(1);
    }
    //bind socket to interface
    sll.sll_family = AF_PACKET;
    sll.sll_ifindex = ifr.ifr_ifindex;
    sll.sll_protocol = htons(ETH_P_ALL);

    if (bind(sock_fd, (struct sockaddr*)&sll, sizeof(sll)) < 0) {
        perror("Error binding socket");
        exit(-1);
    }
    printf("Binding done!!!\n");
    // Create Thread 1 and Thread 2
    pthread_t thread1, thread2;
    Queue* q = createQueue();
    
    struct thread_args *args = malloc (sizeof (struct thread_args));
    args->first = sock_fd;
    args->second = q;
    int header_len;
    args->third= header_len;
    pthread_create(&thread1, NULL, thread_1, args);
    pthread_create(&thread2, NULL, thread_2, args);

    // Wait for threads to finish
    pthread_join(thread1, NULL);
    pthread_join(thread2, NULL);

    return 0;
}