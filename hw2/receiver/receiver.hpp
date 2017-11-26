
#ifndef RECEIVER_H
#define RECEIVER_H

#include <stdio.h>  
#include <stdlib.h>  
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <set>
#include <functional>
#include <fcntl.h>
#include <iostream>
#include <fstream>
#include "../udp_file.hpp"
using namespace std;

#define BUFFER_SIZE 2048

namespace Select{
class Receiver{
public:
    bool debug;
    // callbacks
    ~Receiver() = default;
    Receiver(const int port, bool debug=false) : 
        port(port), bind_fd(0), debug(debug){ 
    }
    void run() {
        // create receiver bind_fd socket
        bind_fd = create_socket(port);
        ofstream file;
        Header header;
        // receiver loop
        while(1){
            receive_packet(bind_fd);
            memcpy(&header, buffer, sizeof(header));
            if(header.offset == -1)file.open(buffer + sizeof(header), ofstream::out | ofstream::binary);
            else{
                const char *data = buffer + sizeof(header);
                file << data << flush; 
                if(header.fin)file.close();
            }
            // tell sender we received data
            send_ack(bind_fd);
        }
        // close receiver
        close(bind_fd);
    }
protected:
    // max connection 
    sockaddr_in addr_peer;
    const int port;
    int bind_fd;
    set<int> clients;
    char buffer[MAX_BUFFER_SIZE];

    void receive_packet(int bind_fd){
        socklen_t cli_len = sizeof(addr_peer);
        int n = recvfrom(bind_fd, buffer ,sizeof(buffer), 0, (sockaddr*)&addr_peer, &cli_len); 
        if(n <= 0){
            cout << "unexpected error." << endl;
            exit(1);
        }
        // trailing '\0'
        buffer[n] = 0;
    }
    void send_ack(int bind_fd){
        socklen_t cli_len = sizeof(addr_peer);
        sendto(bind_fd, buffer, sizeof(buffer), 0, (sockaddr*)&addr_peer, cli_len);
    }

    int create_socket(int port){
        int sock;
        struct sockaddr_in receiver_addr;
        char addr[INET_ADDRSTRLEN];
        int reuse = 1;
       
        printf("Creating receiver socket...\n");
        sock = socket(AF_INET, SOCK_DGRAM, 0);
        if(sock < 0){
            perror("Error: Create receiver socket failed.\n");
            exit(1);
        } 

        // clear padding bit in case something went wrong
        memset(&receiver_addr, 0, sizeof(receiver_addr));
        receiver_addr.sin_family = AF_INET;
        receiver_addr.sin_addr.s_addr = INADDR_ANY;
        receiver_addr.sin_port = htons(port);

        // enable sock reuse, in case the "address already in use" error
        setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

        printf("Binding...\n");
        if(bind(sock, (struct sockaddr*)&receiver_addr, sizeof(receiver_addr)) < 0){
            fprintf(stderr, "Error: Bind failed.\n");
            exit(1);
        }

        inet_ntop(AF_INET,&(receiver_addr.sin_addr), addr, INET_ADDRSTRLEN);
        printf("Bindinng on %s:%d ...\n", addr, port);
        printf("Receiver is up.\n");
        return sock; 
    }
};
}

#endif
