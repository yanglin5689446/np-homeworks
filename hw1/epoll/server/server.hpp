
#ifndef SERVER_H
#define SERVER_H

#include <stdio.h>  
#include <stdlib.h>  
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <set>
#include <map>
#include <functional>
using namespace std;

#define BUFFER_SIZE 1024

class GenericSelectServer{
public:
    bool debug;
    // callbacks
    ~GenericSelectServer() = default;
    GenericSelectServer(const int port, const int max_connection=10, bool debug=true) : 
                        port(port), listener(0), backlog(max_connection), debug(debug){ 
        FD_ZERO(&all_fds);
        FD_ZERO(&available);
    }
    void run() {
        // create server listener socket
        listener = create_socket(port);
        max_fd = listener + 1;

        // append listener fd to all_fds set
        FD_SET(listener, &all_fds);

        // server loop
        while(1){
            // use select() to find available fds
            available = all_fds;
            if(select(max_fd, &available, NULL, NULL, NULL) == -1){
                perror("Error: select() failed.");
                exit(1);
            }
            // handle new connection
            int new_client = 0;
            if(FD_ISSET(listener, &available)){
                if(debug)printf("new connection\n");
                // new connection
                new_client = new_connection();
                // set fds
                FD_SET(new_client, &all_fds);
                if(new_client >= max_fd)max_fd = new_client + 1;
                FD_CLR(listener, &available);
                clients.insert(new_client);
                // allocate new buffer
                w_buf[new_client] = stringstream();
                r_buf[new_client] = stringstream();
                handle_new_connection(new_client);
            }
            // handle existing connection
            if(debug)printf("handle existing connection\n");
            for(auto& client: clients){
                if(FD_ISSET(client, &available)){
                    // read input
                    int n = read(client, buffer, BUFFER_SIZE);
                    // clear read buffer
                    r_buf[client].str(buffer);
                    r_buf[client].clear();
                    memset(buffer, 0, sizeof(buffer));
                    // disconnection
                    if(n <= 0){
                        if(debug)printf("handle disconnection %d\n", client);
                        handle_disconnect(client);
                        // remove client from set, remove r/w buffer
                        clients.erase(client);
                        w_buf.erase(client);
                        r_buf.erase(client);
                        FD_CLR(client, &all_fds);
                        FD_CLR(client, &available);
                        close(client);
                    }
                    else{
                        if(debug)printf("handle client %d\n", client);
                        handle_client(client);
                    }
                }
            }
            if(debug)printf("write to all clients\n");
            for(auto& client: clients){
                if(FD_ISSET(client, &available) || client == new_client){
                    write(client, w_buf[client].str().c_str(), w_buf[client].str().length());
                    // clear write buffer
                    w_buf[client].str("");
                    w_buf[client].clear();
                }
            }
            if(debug)printf("finish select cycle\n");
        }
        // close server
        close(listener);
    }
protected:
    // max connection 
    const int backlog; 
    const int port;
    fd_set all_fds, available; 
    int listener;
    int max_fd;
    set<int> clients;
    char buffer[BUFFER_SIZE];
    map<int, stringstream> w_buf;
    map<int, stringstream> r_buf;

    int create_socket(int port){
        int sock;
        struct sockaddr_in server_addr;
        char addr[INET_ADDRSTRLEN];
        int reuse = 1;
       
        printf("Creating server socket...\n");
        sock = socket(AF_INET, SOCK_STREAM, 0);
        if(sock < 0){
            perror("Error: Create server socket failed.\n");
            exit(1);
        } 

        // clear padding bit in case something went wrong
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = INADDR_ANY;
        server_addr.sin_port = htons(port);

        // enable sock reuse, in case the "address already in use" error
        setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

        printf("Binding...\n");
        if(bind(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0){
            fprintf(stderr, "Error: Bind failed.\n");
            exit(1);
        }

        inet_ntop(AF_INET,&(server_addr.sin_addr), addr, INET_ADDRSTRLEN);
        printf("Listening on %s:%d ...\n", addr, port);
        if(listen(sock, backlog) < 0){
            fprintf(stderr, "Error: Listen failed.\n");
            exit(1);
        }
        printf("Server is up.\n");
        return sock; 
    }

    int new_connection(){
        static struct sockaddr_in client_addr;
        socklen_t addrlen = sizeof(client_addr);
        int new_client = accept(listener, (struct sockaddr*)&client_addr, &addrlen);
        if(new_client == -1)
            perror("Error: establish new connection failed.");
        return new_client;
    }
    virtual void handle_client(int){} 
    virtual void handle_new_connection(int){}
    virtual void handle_disconnect(int){};
};

#endif
