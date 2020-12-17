/* EE542 LAB 3: Reliable file transfer */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <netdb.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/time.h>
#include <stdbool.h>
#include <time.h>


/* packet is divided as Seq# and Data as follows:
 * Seq #     +          Data
 * 8 bytes   +       1464 bytes
 * This means the max file size supported is 1 GB
 * */



int main(int argc, char *argv[]) {
    
    if (argc < 5) {
        printf("Wrong Commands");
        exit(0);
    }

    int tcpPort;
    tcpPort = atoi(argv[3]);
    int sockfd, n;
    struct sockaddr_in serveraddr;
    struct hostent *server;
    bzero((char *) &serveraddr, sizeof(serveraddr));
    
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Error TCP Socket");
        exit(1);
    }
    if ((server = gethostbyname(argv[2])) == NULL) {
        perror("Error Host Doesnt Exist");
        exit(1);
    }
    

    serveraddr.sin_family = AF_INET;
    bcopy((char *) server->h_addr, (char *) &serveraddr.sin_addr.s_addr, server->h_length);
    serveraddr.sin_port = htons(tcpPort);
    //Connect to TCP socket
    if ((connect(sockfd, (struct sockaddr *) &serveraddr, sizeof(serveraddr))) < 0) {
        perror("TCP Connection Failed");
    }

    int udpPort = atoi(argv[4]);
    int sockUDP;
    int nackCounter = 0;
    struct sockaddr_in serveradd;
    const int sockBuffer = 32*1024*1024;
    if ((sockUDP = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Error UDP Socket");
    }
    

    if (setsockopt(sockUDP, SOL_SOCKET, SO_SNDBUF, &sockBuffer, sizeof sockBuffer) < 0) {
        perror("Buffer Error");
    }
    
    if (setsockopt(sockUDP, SOL_SOCKET, SO_RCVBUF, &sockBuffer, sizeof sockBuffer) < 0) {
        perror("Buffer Error");
    }
    
    serveradd.sin_family = AF_INET;
    bcopy((char *) server->h_addr, (char *) &serveradd.sin_addr, server->h_length);
    serveradd.sin_port = htons(udpPort);
    
    ///////////////////////////////////////////////////////////////////////////////////////////////////
    const int PacketPayLoadLength = 1464;
    const int PacketHeaderLength =  8;
    const int PacketLength = PacketPayLoadLength + PacketHeaderLength ;


    char tempBuff[100];
    char fileName[100];
    long long int fileSize;
    int fd;
    struct stat fileStat;
    char *fileMMAP;
    

    sprintf(fileName,"%s",argv[1]);
    if ((fd = open(fileName, O_RDONLY)) == -1) {
        perror("Can't open File");
    }
    

    stat(fileName, &fileStat);
    fileSize=fileStat.st_size;
    bzero(tempBuff,100);
    sprintf(tempBuff, "%lld", fileSize);
    
// file is mapped to the virtual memory area for quick access
    fileMMAP = (char*)mmap(0, fileSize, PROT_READ, MAP_SHARED, fd, 0);
    

    time_t t1 = time(NULL);
    struct tm tm = *localtime(&t1);
    printf("%d-%d-%d %d:%d:%d\n", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
    

    n = write(sockfd,fileName,strlen(fileName));

    n = write(sockfd,tempBuff,strlen(tempBuff));

    printf("Transfer Just Started\n");
    printf("file Name: %s\n",fileName);
    


    int PacketNumber=0;
    int TotalPackets=0;

    TotalPackets = (fileSize/PacketPayLoadLength)+1;
    

    for(PacketNumber; PacketNumber<TotalPackets; PacketNumber++) {

        char packet[PacketLength];
        char packetNumberString[PacketHeaderLength];
        sprintf(packetNumberString,"%d",PacketNumber);
        memcpy(packet,packetNumberString,PacketHeaderLength);
        memcpy(packet+PacketHeaderLength,&fileMMAP[PacketNumber*PacketPayLoadLength],PacketPayLoadLength);
        
        n = sendto(sockUDP,packet,sizeof(packet),0,(struct sockaddr *)&serveradd,sizeof(serveradd));
        if (n<0){
            perror("Error in sending packets");
        }
    }

    int packetsDropped[2000];
    socklen_t addr_size =sizeof(serveradd);
    while(1) {
       // printf("1\n");
        bzero(packetsDropped, 2000);
        recvfrom(sockUDP, packetsDropped, sizeof(packetsDropped), 0, (struct sockaddr *) &serveradd,
                 &addr_size);
        if (packetsDropped[0] != -1) {
            char data[PacketPayLoadLength];
            char packet[PacketLength];
            char packetNumberString[PacketHeaderLength];
            
            for (int i = 0; i < 2000; i++) {
                if (packetsDropped[i] == -2) {
                    break;
                }
                if (packetsDropped[i] != TotalPackets) {
                    sprintf(packetNumberString, "%d", packetsDropped[i]);
                    memcpy(packet, packetNumberString, PacketHeaderLength);
                    memcpy(packet + PacketHeaderLength, &fileMMAP[packetsDropped[i] * PacketPayLoadLength],
                           PacketPayLoadLength);
                    sendto(sockUDP, packet, sizeof(packet), 0, (struct sockaddr *) &serveradd,
                           sizeof(serveradd));
                } else {
                   // printf("3\n");
                    memcpy(data, &fileMMAP[packetsDropped[i] * PacketPayLoadLength],
                           fileSize - ((TotalPackets - 1) * PacketPayLoadLength));
                    sprintf(packetNumberString, "%d", packetsDropped[i]);
                    memcpy(packet, packetNumberString, PacketHeaderLength);
                    memcpy(packet + PacketHeaderLength, data, sizeof(data));
                    sendto(sockUDP, packet, sizeof(packet), 0, (struct sockaddr *) &serveradd,
                           sizeof(serveradd));
                }
            }
           // printf("2\n");
        }
        else {
            time_t t2 = time(NULL);
            struct tm tm = *localtime(&t2);
            printf("%d-%d-%d %d:%d:%d\n", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min,
                   tm.tm_sec);
            double diff_t = difftime(t2, t1);
            printf("throughput: %fMbps\n", (fileSize * 8 / 1024 / 1024) / diff_t);
            exit(0);
        }
    }
    
    
}

