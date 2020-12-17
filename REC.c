/* EE542 LAB 3: Reliable file transfer */


#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/time.h>
#include <libgen.h>


/* packet is divided as Seq# and Data as follows:
 * Seq #     +          Data
 * 8 bytes   +       1464 bytes
 * This means the max file size supported is 1 GB
 * */

int main(int argc, char *argv[])
{
    //////////////////////////// Get input from User //////////////////////////////////////////////
    long long int fileSize; //received file size
    
    if (argc < 3) {
        printf("Wrong Commands");
        exit(1);
    }
    
    int tcpPort = atoi(argv[1]);
    int udpPort = atoi(argv[2]);
    
    ////////////////////////// Getting File info using TCP ////////////////////////////////////////
    //setup TCP Socket
    int socketFD;
    int newSocketFD;
    int errorHolder;
    struct sockaddr_in serverAddr;
    int addrSize = sizeof(serverAddr);
    bzero((char *) &serverAddr, sizeof(serverAddr));
    
    //Create TCP Socket
    if ((socketFD = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        perror("tcp socket error");
        exit(1);
    }
    
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(tcpPort);
    
    if ((bind(socketFD, (struct sockaddr *) &serverAddr, sizeof(serverAddr))) < 0){
        perror("tcp bind error");
        exit(1);
    }
    
    listen(socketFD,5);
    printf("Run Client Code \n");
    
    if ((newSocketFD = accept(socketFD,(struct sockaddr *) &serverAddr, (socklen_t*)&addrSize))< 0){
        perror("error in accepting connection");
        exit(1);
    }

    int sockBufferSize = 33554432;
    int udpSocket;
    struct sockaddr_in serverUdpAddr;
    struct sockaddr_in clientUdpAddr;
    
    if ((udpSocket= socket(AF_INET, SOCK_DGRAM, 0)) < 0){
        perror("udp socket error");
    }
    
    if (setsockopt(udpSocket, SOL_SOCKET, SO_SNDBUF, &sockBufferSize, sizeof sockBufferSize)<0){ //send buffer
        perror("buff size error");
    }
    if (setsockopt(udpSocket, SOL_SOCKET, SO_RCVBUF, &sockBufferSize, sizeof sockBufferSize)<0){ //receive buffer
        perror("buff size error");
    }
    bzero(&serverUdpAddr,sizeof(serverUdpAddr));
    serverUdpAddr.sin_family=AF_INET;
    serverUdpAddr.sin_addr.s_addr=INADDR_ANY; 
    serverUdpAddr.sin_port=htons(udpPort);

    if ((bind(udpSocket, (struct sockaddr *) &serverUdpAddr, sizeof(serverUdpAddr))) < 0){
        perror("udp bind error");
        exit(1);
    }

    
    //Get file info and setup file for writing
    
    char gettingFileNameBuffer[100];
    char fileName[100];
    char* plainFileName;
    int fileDescriptor;
    char *fileDataMap;
    
//printf("TCP OK\n");

    bzero(gettingFileNameBuffer,100);
    errorHolder = read(newSocketFD,gettingFileNameBuffer,100);
//printf("TCP OK 1\n");
    printf("File Name: %s\n",gettingFileNameBuffer);
//printf("TCP OK 2\n");
    plainFileName=basename(gettingFileNameBuffer);
    sprintf(fileName,"%s",plainFileName);
    bzero(gettingFileNameBuffer,100);
    errorHolder = read(newSocketFD,gettingFileNameBuffer,100);
    fileSize=atoi(gettingFileNameBuffer);

//printf("TCP OK 3\n");

    if ((fileDescriptor = open(fileName, O_RDWR | O_CREAT | O_TRUNC, (mode_t)0600)) == -1) {
        perror("ERRER OPENNING THE FILE");
    }
    if ((lseek(fileDescriptor, fileSize-1, SEEK_SET)) == -1) {
        perror("ERROR repositioning read/write file offset");
    }
    if ((write(fileDescriptor, "", 1)) != 1) {
        perror("ERROR WRITTING TO FILE");
    }
    fileDataMap = (char*)mmap(0, fileSize, PROT_READ | PROT_WRITE, MAP_SHARED, fileDescriptor, 0);
    if (fileDataMap == MAP_FAILED) {
        perror("ERROR CREATING A MEM MAP");
    }

    //printf("TCP OK\n");
    close(socketFD);
    close(newSocketFD);

    ///////////////////// Recieving Data Packets ////////////////////////////////////////////
   
    int packetPayloadSize = 1464;
    int packetNumberSize =8;
    int packetSize = packetNumberSize + packetPayloadSize;
    char packet[packetSize]; 
    char data[packetPayloadSize]; 
    long long int fileSizeSoFar=0; 
    long int numberOfPackets=(fileSize/packetPayloadSize)+1; 
    bool packetRecieved[5000000]={}; 
    char packetNumberString[packetNumberSize]; 
    int packetNo; 
    int droppedPackets[2000]; 
    
    struct timespec timeout;
    fd_set readfds;
    timeout.tv_sec = 0;
    timeout.tv_nsec = 50000000;  //Retransmission timeout is set to 50ms here
    while(1) {
        FD_ZERO(&readfds);
        FD_SET(udpSocket, &readfds);
        int sret = pselect(udpSocket+1, &readfds, NULL, NULL, &timeout, NULL); 
        
        if(sret != 0 ){ //Normal operation
		//printf("waiting");
            errorHolder = recvfrom(udpSocket,packet,packetSize,0,(struct sockaddr *)&clientUdpAddr, (socklen_t*)&addrSize);
		//printf("recev pct");
            memcpy(packetNumberString,packet,packetNumberSize); 
            packetNo=atoi(packetNumberString);
            
            if(packetRecieved[packetNo]==false){ 
                packetRecieved[packetNo]=true; 
                memcpy(data,packet+packetNumberSize,sizeof(packet)-packetNumberSize); 
                
                if (packetNo < numberOfPackets-1){
                    memcpy(&fileDataMap[packetNo*packetPayloadSize],data,sizeof(data)); 
                    fileSizeSoFar= fileSizeSoFar + sizeof(data); 
                }
                else{
                    memcpy(&fileDataMap[packetNo*packetPayloadSize],data,fileSize - ((packetNo)*packetPayloadSize)); 
                    fileSizeSoFar = fileSizeSoFar + (fileSize-((packetNo)*packetPayloadSize));
                }
                if(fileSizeSoFar>=fileSize) {
                    printf("Received\n");
                    droppedPackets[0] = -1; 
                    for (int k=0;k<4;k=k+1){ 
                        errorHolder = sendto(udpSocket,droppedPackets,sizeof(droppedPackets),0,(struct sockaddr *)&clientUdpAddr, addrSize); 
                    }
                    exit(1);
                }
            }
        }
        else { //Operation after timeout 
            int limit =0; 
            bool noMoreMissingPackets = false;
            
            for(int i=0;i<numberOfPackets;i=i+1) {
                if((limit==1999) && (packetRecieved[i]==false)) {
                    droppedPackets[limit] = i;
                    limit=0;
                    for (int k=0;k<2;k=k+1){ 
                        errorHolder = sendto(udpSocket,droppedPackets,sizeof(droppedPackets),0,(struct sockaddr *)&clientUdpAddr, addrSize);
                    }
                    break;
                } else if(packetRecieved[i]==false) {
                    droppedPackets[limit] = i;
                    limit = limit+1;
                    
                }
                if ((i==numberOfPackets-1) && (limit != 2000)){
                    noMoreMissingPackets = true;
                }
            }
            if(noMoreMissingPackets == true)
            {
                droppedPackets[limit] = -2;
                for (int k=0;k<2;k=k+1){
                    errorHolder = sendto(udpSocket,droppedPackets,sizeof(droppedPackets),0,(struct sockaddr *)&clientUdpAddr, addrSize);
                }
            }
        }
    }
    close(udpSocket);
    close(newSocketFD);
    return 0;
    exit(0);
}

