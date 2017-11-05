
#ifndef SERVER_H
#define SERVER_H

#include <stdio.h>  
#include <stdlib.h>  
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <set>
#include <map>
#include <functional>
#include <fcntl.h>
using namespace std;

#define BUFFER_SIZE 2048

namespace select_server{
    class Server{
    public:
        bool debug;
        // callbacks
        ~Server() = default;
        Server(const int port, const int max_connection=10, bool debug=true) : 
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
                    r_buf[new_client] = stringstream();
                    new_connection_handler(*this, new_client);
                }
                // handle existing connection
                if(debug)printf("handle existing connection\n");
                for(auto& client: clients){
                    if(FD_ISSET(client, &available)){
                        // read input
                        memset(buffer, 0, sizeof(buffer));
                        int n = read(client, buffer, BUFFER_SIZE);
                        // clear read buffer
                        r_buf[client].str(buffer);
                        r_buf[client].clear();
                        memset(buffer, 0, sizeof(buffer));
                        // disconnection
                        if(n <= 0){
                            if(debug)printf("handle disconnection %d\n", client);
                            disconnection_handler(*this, client);
                            // remove client from set, remove r/w buffer
                            clients.erase(client);
                            r_buf.erase(client);
                            FD_CLR(client, &all_fds);
                            FD_CLR(client, &available);
                            close(client);
                        }
                        else{
                            if(debug)printf("handle client %d\n", client);
                            client_handler(*this, client);
                        }
                    }
                }
                if(debug)printf("finish select cycle\n");
            }
            // close server
            close(listener);
        }
        void registerClientHandler(function<void(Server&, int)> f){client_handler = f;}
        void registerNewConnectionHandler(function<void(Server&, int)> f){new_connection_handler = f;}
        void registerDisconnectionHandler(function<void(Server&, int)> f){disconnection_handler = f;}
        void forEachClient(function<void(Server&, int)> callback, int exclude = 0){
            for(auto& client : clients)
                if(client != exclude)
                    callback(*this, client);
        } 
        void send(int client, string message){
            write(client, message.c_str(), message.length());
        }
        string getClientInput(int client){
            return r_buf[client].str();
        }
        void disconnect(int client){
            disconnection_handler(*this, client);
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
        map<int, stringstream> r_buf;
        function<void(Server&, int)> client_handler;
        function<void(Server&, int)> new_connection_handler;
        function<void(Server&, int)> disconnection_handler;

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
    };
}

namespace epoll_server {
    class Server{
    public:
        bool debug;
        // callbacks
        ~Server() = default;
        Server(const int port, const int max_connection=10, bool debug=true) : 
            port(port), listener(0), backlog(max_connection), debug(debug){ 
            events = new epoll_event[max_connection];
        }
        void run() {
            // create server listener socket
            listener = create_socket(port);
            setup_epoll();
            // server loop
            while(1){
                // use select() to find available fds
                int nfds = epoll_wait(epoll_fd, events, backlog, -1);
                if(nfds < 0){
                    perror("error on listening events.\n");
                    exit(1);
                }
                int new_client = 0;
                if(debug)printf("handle existing connection\n");
                for(int i = 0 ;i < nfds ;i ++){
                    // server listener new event
                    if(events[i].data.fd == listener){
                        // accept new connection 
                        new_client = new_connection(); 
                        // insert new client to clients set;
                        clients.insert(new_client);
                        // initialize read buffer
                        r_buf[new_client] = stringstream();
                        set_nonblocking(new_client);
                        // add new client fd to monitor queue
                        event.events = EPOLLIN | EPOLLET;
                        event.data.fd = new_client;
                        if(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, new_client, &event) < 0){
                            perror("error on adding user to epoll monitor queue.\n");
                        }
                        // call handler
                        new_connection_handler(*this, new_client);
                    }
                    else{
                        int client = events[i].data.fd;
                        memset(buffer, 0, sizeof(buffer));
                        int n = read(client, buffer, sizeof(buffer));
                        r_buf[client].str(buffer);
                        r_buf[client].clear();
                        if(n <= 0){
                            // close client, remove client from monitor queue
                            disconnection_handler(*this, client);
                            clients.erase(client);
                            r_buf.erase(client);
                            epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client, NULL);
                            close(client);
                        }
                        else{
                            client_handler(*this, client);
                        }
                    }
                }
            }
            // close server
            close(listener);
            close(epoll_fd);
        }
        void registerClientHandler(function<void(Server&, int)> f){client_handler = f;}
        void registerNewConnectionHandler(function<void(Server&, int)> f){new_connection_handler = f;}
        void registerDisconnectionHandler(function<void(Server&, int)> f){disconnection_handler = f;}
        void forEachClient(function<void(Server&, int)> callback, int exclude = 0){
            for(auto& client : clients)
                if(client != exclude)
                    callback(*this, client);
        } 
        void send(int client, string message){
            write(client, message.c_str(), message.length());
        }
        string getClientInput(int client){
            return r_buf[client].str();
        }
        void disconnect(int client){
            disconnection_handler(*this, client);
        }
    protected:
        // max connection 
        const int backlog; 
        const int port;
        int listener, epoll_fd;
        epoll_event event;
        epoll_event* events;
        set<int> clients;
        char buffer[BUFFER_SIZE];
        map<int, stringstream> r_buf;
        function<void(Server&, int)> client_handler;
        function<void(Server&, int)> new_connection_handler;
        function<void(Server&, int)> disconnection_handler;

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

        void setup_epoll(){
            // create epoll event monitor
            epoll_fd = epoll_create(backlog);
            if(epoll_fd < 0){
                perror("error on creating epoll fd.\n");
                exit(1);
            }
            // monitor epoll_fd event;
            event.events = EPOLLIN;
            event.data.fd = listener;
            if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listener, &event) < 0){
                perror("error on registering epoll monitor event,\n");
                exit(1);
            }
        }
        bool set_nonblocking(int &sock){
            int opts;
            opts = fcntl(sock, F_GETFL);
            if(opts < 0){
                perror("error on getting sock staus.\n");
                return false;
            }
            opts |= O_NONBLOCK;
            if(fcntl(sock, F_SETFL, opts) < 0){
                perror("error on setting sock non-blocking.\n");
                return false;
            }
            return true;
        }
    };
}

#endif
