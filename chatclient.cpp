#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

#include <iostream>

#include <fcntl.h>
#include <sys/select.h>


#define BUFSIZ 1024
#define MAX_CLIENTS 50
#define delimitter  " "




bool stdinHasData(){

    // create a file descriptor set
    fd_set read_fds{};

    // initialize the fd_set to 0
    FD_ZERO(&read_fds);

    // set the fd_set to target file descriptor 0 (STDIN)
    FD_SET(STDIN_FILENO, &read_fds);

    // Set non-blocking mode on stdin
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);

    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;

    // pselect the number of file descriptors that are ready, since
    // we're only passing in 1 file descriptor, it will return either
    // a 0 if STDIN isn't ready, or a 1 if it is.
    return select(STDIN_FILENO + 1, &read_fds, NULL, NULL, &timeout);;
}


void* writerThread(void *arg){
    std::string input,filename, command;
    // std::string delimitter = " ";
    std::getline(std::cin, input);

    const int PORT = atoi(input.c_str());

    printf("Port is %d", PORT);

    int sock = 0, valread, client_fd, pos;
    char buffer[BUFSIZ] = { 0 };
    char stdin_buffer[BUFSIZ] = { 0 };
    
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("\n Writer socket creation error \n");
        pthread_exit(NULL);
    }
    
    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
 
    // Convert IPv4 and IPv6 addresses from text to binary
    // form
    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr)
        <= 0) {
        printf(
            "\nInvalid address/ Address not supported \n");
        pthread_exit(NULL);
    }
 
    // Connect to the port  
    if ((client_fd = connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr))) < 0) {
        perror("\nConnection Failed \n");
        pthread_exit(NULL);
    }

    while (1){

        if (stdinHasData()){
            memset(stdin_buffer, 0, BUFSIZ);
            ssize_t num_read = read(STDIN_FILENO, stdin_buffer, BUFSIZ);
            if (num_read == 1){
                continue;
            }
            
            stdin_buffer[num_read-1] = '\0';
            input = std::string(stdin_buffer);
            printf("char read - %ld\n", num_read);
            
        }
        else{
            // printf(".\n");
            fflush(stdout);
            sleep(1);
            continue;
        }

        // getline(std::cin, input);

        send(sock, input.c_str(), input.length(), 0);
        // printf("[.] - Command message sent\n");
        // valread = read(sock, buffer, BUFSIZ);
        // printf("%s\n", buffer);

        pos = input.find(delimitter);
        if(pos == std::string::npos){
            continue;
        }

        command = input.substr(0, input.find(delimitter));
        filename = input.substr(input.find(delimitter)+1, input.size());

        
        // If the input message contains transfer
        if(command == "transfer"){

            // Open the file
            FILE* fp = fopen(filename.c_str(), "rb");
            if(fp == NULL){
                printf("Error - Couldn't open the file - %s\n", filename.c_str());
                continue;
            }

            // Seek to the end of file to get the total size
            fseek(fp, 0 , SEEK_END);
            long fileSize = ftell(fp);
            long remainDataSize = fileSize;
            long lengthRead;
            const char* fileSizeToSend = std::to_string(fileSize).c_str();
            send(sock, fileSizeToSend, strlen(fileSizeToSend), 0);
            printf("File size sent - %ld\n", fileSize);

            // Seek back to the begining to start sending the actual file
            fseek(fp, 0 , SEEK_SET);
            printf("\n - Strarting to transfer the file\n");
            while( (remainDataSize > 0) && (lengthRead = fread(buffer, 1, sizeof(buffer), fp))>0 ){
                send(sock, buffer, lengthRead, 0);
                remainDataSize -= lengthRead;
            }
            printf("\n - Successfully transferred the file \n");
            fclose(fp);

        }
        
    }

}


int main(int argc, char const* argv[])
{

    pthread_t tid;

    if( pthread_create(&tid, NULL, writerThread, NULL) != 0){
        printf("Failed to create thread\n");
    }
    
    const int PORT = atoi(argv[1]);
    
    int server_fd, clientSock;
    struct sockaddr_in address;
    int addrlen = sizeof(address);

 
    // Creating socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("socket failed");
        return -1;
    }

    // printf("Socket file descriptor created\n");
 
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);
 
    // attaching socket to the port
    bind(server_fd, (struct sockaddr*)&address, addrlen);
    
    if (listen(server_fd, 3) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }
    printf("Listening on port %d\n", PORT);
    

    clientSock = accept(server_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen);
    if(clientSock < 0){
        perror("Error accepting request \n");
    }
    char buffer[BUFSIZ] = { 0 };

    while(1){
        memset(buffer, 0, BUFSIZ);
        read(clientSock, buffer, BUFSIZ);  // Receives the message
        // printf("Command received - %s\n", buffer);
        // send(sock, ack, strlen(ack), 0);
        // printf("Command ack sent\n");
        
        std::string input, filename, command;

        input = buffer;
        printf("[x]: %s\n", buffer);

        int pos = input.find(delimitter);
        if(pos == std::string::npos){
            continue;
        }

        command = input.substr(0, input.find(delimitter));
        filename = input.substr(input.find(delimitter)+1, input.size());

        // If the input message contains transfer
        if(command == "transfer"){

            std::string newFilename = "new" + filename;
            memset(buffer, 0, BUFSIZ);
            read(clientSock, buffer, BUFSIZ);

            printf("file size is  %s\n", buffer);
            // send(sock, ack, strlen(ack), 0);
            // printf("File size ack sent\n");

            long fileSize = atol(buffer);
            long remainDataSize = fileSize;
            long lengthRead;

            FILE* upload_fp = fopen(newFilename.c_str(), "wb");
            if(upload_fp == NULL){
                printf("something went wrong!! couldn't open the file");
                continue;
            }

            printf("\nStrarting to receive the file\n");
            while ((remainDataSize > 0) && ((lengthRead = recv(clientSock, buffer, BUFSIZ, 0)) > 0))
            {
                    fwrite(buffer, sizeof(char), lengthRead, upload_fp);
                    remainDataSize -= lengthRead;
                    // fprintf(stdout, "Receive %ld bytes and we hope :- %ld bytes\n", lengthRead, remainDataSize);
            }
            printf("\nFile received successfully\n");
            fclose(upload_fp);
        }
    
    }

    // closing the listening socket
    shutdown(server_fd, SHUT_RDWR);
    return 0;
}