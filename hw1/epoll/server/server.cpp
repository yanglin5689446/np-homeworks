#include <stdio.h>  
#include <stdlib.h>  
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <string>
#include <sstream>
#include <vector>
#include <map>
#include <regex>
using namespace std;

#define SERVER_PORT 12345 
#define MAX_EVENTS  10
#define BUFFER_SIZE 1024

int buffer[BUFFER_SIZE];

bool set_nonblocking(int &sock){
    int opts;
    opts = fcntl(sock, F_GETFL);
    if(opts < 0){
        perror("error on getting sock staus.\n");
        return 0;
    }
    opts |= O_NONBLOCK;
    if(fcntl(sock, F_SETFL, opts) < 0){
        perror("error on setting sock non-blocking.\n");
        return 0;
    }
    return 1;
}

int create_socket(int port, int connection = 10){
    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);
    int sock = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if(sock < 0){
        perror("error on creating socket.\n");
        exit(1);
    }
    if(bind(sock, (sockaddr *)&server_addr, sizeof(sockaddr)) < 0){
        perror("error on binding.\n");
        exit(1);
    }
    if(listen(sock, connection) < 0){
        perror("error on listening.\n");
        exit(1);
    }
    return sock;
}

int main() {
    int server_fd = create_socket(SERVER_PORT);
    epoll_event event, events[MAX_EVENTS];
    sockaddr_in client_addr;
    socklen_t sin_size = sizeof(sockaddr_in);
    int new_client_fd, epoll_fd;

    // create epoll event monitor
    epoll_fd = epoll_create(MAX_EVENTS);
    if(epoll_fd < 0){
        perror("error on creating epoll fd.\n");
        exit(1);
    }

    // monitor epoll_fd event;
    event.events = EPOLLIN;
    event.data.fd = server_fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &event) < 0){
        perror("error on registering epoll monitor event,\n");
        exit(1);
    }
    printf("server is up.\n");
    while(true){
        printf("monitoring.\n");
        int nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        if(nfds < 0){
            perror("error on listening events.\n");
            exit(1);
        }
        new_client_fd = 0;
        printf("handling.\n");
        for(int i = 0 ;i < nfds ;i ++){
            // server listener new event
            if(events[i].data.fd == server_fd){
                // accept new connection 
                printf("new connection.\n");
                new_client_fd = accept(server_fd, (sockaddr *)&client_addr, &sin_size);
                if(new_client_fd < 0)perror("error on handling client.\n");
                set_nonblocking(new_client_fd);
                // add new client fd to monitor queue
                event.events = EPOLLIN | EPOLLET;
                event.data.fd = new_client_fd;
                if(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, new_client_fd, &event)){
                    perror("error on adding user to epoll monitor queue.\n");
                }
                printf("send hello to new client.\n");
                write(new_client_fd, "Hello world!", 12);
            }
            else{
                int client = events[i].data.fd;
                int n = read(client, buffer, sizeof(buffer));
                if(n <= 0){
                    printf("client connection closed.\n");
                    // close client, remove client from monitor queue
                    close(client);
                    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client, NULL);
                    continue;
                }
                write(client, "Hello world!", 12);
            }
        }
    }

    close(epoll_fd);
    close(server_fd);
}  
