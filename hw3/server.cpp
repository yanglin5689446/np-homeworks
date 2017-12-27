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
#include "general.hpp"
using namespace std;

class Server{
public:
    bool debug;
    // callbacks
    ~Server() = default;
    Server(const int port, const int max_connection=10, bool debug=false) : 
        port(port), listener(0), backlog(max_connection), debug(debug){ 
        events = new epoll_event[max_connection];
    }
    void run() {
        // create server listener socket
        listener = create_socket(port);
        setup_epoll();
        // server loop
        while(1){
            // use epoll to find available fds
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
                    epoll_event event;
                    event.events = EPOLLIN;
                    event.data.fd = new_client;
                    if(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, new_client, &event) < 0)
                        perror("error on adding user to epoll monitor queue.\n");
                }
                else{
                    int client = events[i].data.fd;
                    if(events[i].events & EPOLLIN){
                        if(receiving.count(client))receive_file(client);
                        else handle_client(client);
                    }
                    else if(events[i].events & EPOLLOUT)send_file(client);
                }
            }
        }
        // close server
        close(listener);
        close(epoll_fd);
    }
    void sync_new_connection(int fd, string& client_name){
        for(auto& file_name: client_files[client_name]){
            string get_file = string("/put ") + file_name + ' ' + to_string(get_file_size(file_name)) + ' ' + client_name;
            non_block_write(fd, get_file.c_str(), get_file.length());
        }
    }
    void sync_peers(string& client_name, string& file_name){
        string get_file = string("/put ") + file_name + ' ' + to_string(get_file_size(file_name)) + ' ' + client_name;
        for(auto& peer: name_clients_map[client_name])
            non_block_write(peer, get_file.c_str(), get_file.length());
    }
    void handle_client(int client){
        // handle command fd
        int bytes_read = non_block_read(client, read_buffer);
        if(bytes_read == 0) {
            disconnect(client);
            return;
        }
        // tokenize
        vector<string> tokens;
        string token;
        stringstream tokenizer(read_buffer);
        while(tokenizer >> token)tokens.push_back(token);
        // command
        if(tokens[0] == "/exit") disconnect(client);
        else if(tokens[0] == "/put"){
            fd_file_map[client] = tokens[3];
            set<string> &files = client_files[tokens[3]];
            if(files.find(tokens[1]) == files.end()){
                files.insert(tokens[1]);
                receiving[client] = FileInfo(tokens[1], stoi(tokens[2]), FileInfo::out);
            }
            else {
                int n = 1;
                while(true){
                    string new_file_name = tokens[1] + '(' + to_string(n) + ')';
                    if(files.find(new_file_name) == files.end()){
                        files.insert(new_file_name);
                        receiving[client] = FileInfo(new_file_name, stoi(tokens[2]), FileInfo::out);
                        break;
                    }
                    n++;    
                }
            }
            if(tokens.size() > 4){
                int from = 4 + tokens[1].length() + tokens[2].length() + tokens[3].length() + 4;
                int data_size = bytes_read - from;
                receiving[client].file.write(read_buffer + from, data_size);
                receiving[client].current += data_size;
                cout << receiving[client].current << ' ' << receiving[client].size << endl;
                if(receiving[client].done() || !data_size){
                    if(!data_size){
                        fail_count[client] ++;
                        if(fail_count[client] < 3)return;
                        else fail_count[client] = 0;
                        perror("error while receiving file: client may end the connection.\n");
                    }
                    receiving[client].file.close();
                    if(receiving[client].done())sync_peers(fd_file_map[client], receiving[client].file_name);
                    receiving.erase(client);
                    clients.erase(client);
                    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client, NULL);
                    close(client);
                }
            }
        }
        else if(tokens[0] == "/name" && tokens.size() == 2){
            name_map[client] = tokens[1];
            // add client to same name clients set
            if(!name_clients_map.count(tokens[1]))name_clients_map[tokens[1]] = set<int>();
            name_clients_map[tokens[1]].insert(client);
            string response = string("Welcome to the dropbox-like server! : ") + name_map[client] + '\n';
            non_block_write(client, response.c_str(), response.length());
            sync_new_connection(client, tokens[1]);
        }
        else if(tokens[0] == "/get" && tokens.size() == 2){
            sending[client] = FileInfo(tokens[1], get_file_size(tokens[1]), FileInfo::in);
            // change client to write event;
            epoll_event event;
            event.events = EPOLLOUT;
            event.data.fd = client;
            if(epoll_ctl(epoll_fd, EPOLL_CTL_MOD, client, &event) < 0)
                perror("error on adding user to epoll monitor queue.\n");
        }
    }
    void receive_file(int fd){
        int data_size = non_block_read(fd, read_buffer);
        receiving[fd].file.write(read_buffer, data_size);
        receiving[fd].current += data_size;
        cout << receiving[fd].current << ' ' << receiving[fd].size << endl;
        if(receiving[fd].done() || !data_size){
            if(!data_size){
                perror("error while receiving file: client may end the connection.\n");
                fail_count[fd] ++;
                if(fail_count[fd] < 3)return;
                else fail_count[fd] = 0;
            }
            receiving[fd].file.close();
            if(receiving[fd].done())sync_peers(fd_file_map[fd], receiving[fd].file_name);
            receiving.erase(fd);
            clients.erase(fd);
            epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL);
            close(fd);
        }
    }
    void send_file(int fd){
        if(!sending[fd].file.eof()){
            memset(write_buffer, 0, sizeof(write_buffer));
            sending[fd].file.read(write_buffer, BUFFER_SIZE - 1);
            int data_size = sending[fd].file.gcount();
            non_block_write(fd, write_buffer, data_size);
        }
        else {
            sending[fd].file.close();
            sending.erase(fd);
            clients.erase(fd);
            epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL);
            close(fd);
        }
    }
    void disconnect(int client){
        if(name_map.count(client)){
            name_clients_map[name_map[client]].erase(client);
            name_map.erase(client);
        }
        clients.erase(client);
        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client, NULL);
        close(client);
    }
protected:
    // max connection 
    const int backlog; 
    const int port;
    int listener, epoll_fd;
    epoll_event* events;
    set<int> clients;
    char read_buffer[BUFFER_SIZE];
    char write_buffer[BUFFER_SIZE];
    map<int, int> fail_count;
    // fd to name map
    map<int, string> name_map;
    // client with same name map
    map<string, set<int>> name_clients_map;
    map<string, set<string>> client_files;
    // client sending which file
    map<int, FileInfo> sending;
    map<int, FileInfo> receiving;
    map<int, string> fd_file_map;

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
        epoll_event event;
        event.events = EPOLLIN;
        event.data.fd = listener;
        if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listener, &event) < 0){
            perror("error on registering epoll monitor event,\n");
            exit(1);
        }
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
