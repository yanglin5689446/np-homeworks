#include <stdio.h>  
#include <stdlib.h>  
#include <unistd.h>
#include <string.h>
#include <string>
#include <sstream>
#include <fstream>
#include <iostream>
#include <vector>
#include <map>
#include <set>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
using namespace std;

#define BUFFER_SIZE 16384

enum: int{
    INITIAL,
    IDLE,
    SLEEPING,
    UPLOADING,
};

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
            for(int i = 0 ;i < nfds ;i ++){
                // server listener new event
                if(events[i].data.fd == listener){
                    // accept new connection 
                    new_client = new_connection(); 
                    // insert new client to clients set;
                    clients.insert(new_client);
                    set_nonblocking(new_client);
                    // add new client fd to monitor queue
                    event.events = EPOLLIN;
                    event.data.fd = new_client;
                    if(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, new_client, &event) < 0){
                        perror("error on adding user to epoll monitor queue.\n");
                    }
                }
                else{
                    int client = events[i].data.fd;
                    if(events[i].events & EPOLLIN){
                        int n = read_client(client);
                        if(n == 0) disconnect(client);
                        else handle_client(client, n);
                    }
                    //else if(events[i].events & EPOLLOUT)
                }
            }
        }
        // close server
        close(listener);
        close(epoll_fd);
    }
    void handle_client(int client, int bytes_read){
        // split input to tokens
        cout << bytes_read << endl;
        if(status_map[client] == INITIAL){
            string input = read_buffer;
            name_map[client] = input;
            // add client to same name clients set
            if(!name_clients_map.count(input))name_clients_map[input] = set<int>();
            name_clients_map[input].insert(client);
            
            status_map[client] = IDLE;
            send_client(client, string("Welcome to the dropbox-like server! : ") + name_map[client] + '\n');
        }
        else if(status_map[client] == IDLE){
            // tokenize
            vector<string> tokens;
            string response;
            string token;
            stringstream tokenizer(read_buffer);
            while(tokenizer >> token)tokens.push_back(token);

            if(tokens[0] == "/exit") disconnect(client);
            else if(tokens[0] == "/put" && tokens.size() == 3){
                status_map[client] = UPLOADING;
                file_map[tokens[1]] = fstream(tokens[1], fstream::out | fstream::binary);
                progress[tokens[1]] = make_pair(0, stoi(tokens[2]));
                memcpy(write_buffer, read_buffer, bytes_read);
                for(auto& peer: name_clients_map[name_map[client]]){
                    if(status_map[peer] != SLEEPING && peer != client){
                        // TODO: slowdown I/O speed
                    }
                }
            }
        }
        else if(status_map[client] == UPLOADING){
            for(auto& peer: name_clients_map[name_map[client]]){
                if(status_map[peer] != SLEEPING && peer != client){
                    // TODO: slowdown I/O speed
                }
            }

            stringstream tokenizer(read_buffer);
            string file_name;
            tokenizer >> file_name;
            
            file_map[file_name].write(read_buffer + file_name.length() + 1, bytes_read - file_name.length() - 1);
            progress[file_name].first += bytes_read - file_name.length() - 1;
            cout << "progress: " << 100.0 * progress[file_name].first/progress[file_name].second << '%' << flush << endl;
            if(progress[file_name].first == progress[file_name].second){
                file_map[file_name].close();
                status_map[client] = IDLE;
            }
        }
    }
    void disconnection(int client){
        name_clients_map[name_map[client]].erase(client);
        name_map.erase(client);
        status_map[client] = INITIAL;
    }
    void send_client(int client, string message){
        write(client, message.c_str(), message.length());
    }
    int read_client(int client){
        memset(read_buffer, 0, sizeof(read_buffer));
        int n = 0, nread = 0;
        while((nread = read(client, read_buffer + n, BUFFER_SIZE - n - 1)) > 0 )n += nread;
        return n;
    }
    void disconnect(int client){
        disconnection(client);
        cout << name_map[client] << " exit." << endl;
        clients.erase(client);
        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client, NULL);
        close(client);
    }
protected:
    // max connection 
    const int backlog; 
    const int port;
    int listener, epoll_fd;
    epoll_event event;
    epoll_event* events;
    set<int> clients;
    char read_buffer[BUFFER_SIZE];
    char write_buffer[BUFFER_SIZE];
    map<int, string> name_map;
    map<int, int> status_map;
    map<string, fstream> file_map;
    map<string, set<int>> name_clients_map;
    map<string, pair<int, int>> progress;

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



int main(int argc, char **argv) {
    if(argc != 2){
        cout << "Usage: server [port]" << endl;
        exit(1);
    }
    Server server(stoi(argv[1]), 20, true);
    server.run();
}  
