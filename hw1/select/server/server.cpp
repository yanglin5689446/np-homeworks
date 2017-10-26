#include <stdio.h>  
#include <stdlib.h>  
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string>
#include <sstream>
#include <vector>
#include <map>
#include <regex>
#include "server.hpp"
using namespace std;

#define SERVER_PORT 12345 

const int EXIT = 0;
const int WHO = 1;
const int NAME = 2;
const int YELL = 3;
const int TELL = 4;
const int ERROR = 5;

class Server : public GenericSelectServer{
public:
    Server(int port, int max_connection = 10, bool debug=false) : GenericSelectServer(port, max_connection, debug) {};
private:
    map<int, string> fd_name_map;
    map<string, int> name_fd_map;
    void handle_new_connection(int client){
        fd_name_map[client] = string("anonymous");
        say_hello(client);
        brodcast(client, "[Server] Someone is coming!\n");
    }

    void handle_client(int client){
        char addr[INET_ADDRSTRLEN];
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        // get client address
        getpeername(client, (sockaddr *)&client_addr, &client_len);
        inet_ntop(AF_INET,&(client_addr.sin_addr), addr, INET_ADDRSTRLEN);
        // man processing
        vector<string> tokens;
        int status = process_input(client, tokens);
        // for change name function
        static string old_name;
        switch(status){
            case EXIT:
                handle_disconnect(client);
                break;
            case WHO:
                who(client);
                break;
            case NAME:
                old_name = fd_name_map[client];
                if(change_user_name(client, tokens[1]))
                    brodcast(client, "[Server] " + old_name + " is now known as " + fd_name_map[client] + ".\n");
                break;
            case YELL:
                brodcast(0, string("[Server] ") + fd_name_map[client] + " yell " + tokens[1] + '\n');
                break;
            case TELL:
                private_message(client, tokens[1], tokens[2]);
                break;
            case ERROR:
                write(client, "[Server] ERROR: Error command.\n", 32);
                break;
        }
    }
    void handle_disconnect(int client){
        static string client_name;
        brodcast(client, string("[Server] ") + fd_name_map[client] + " is offline.\n");
        client_name = fd_name_map[client];
        if(client_name != "anonymous")
           name_fd_map.erase(client_name); 
        fd_name_map.erase(client);
    }

    int process_input(int client, vector<string>& tokens){
        string token;
        printf("%s\n", r_buf[client].str().c_str());
        // tokenize
        while(r_buf[client] >> token)tokens.push_back(token);
        if(tokens[0] == "exit")return EXIT;
        else if(tokens[0] == "who" && tokens.size() == 1)return WHO;
        else if(tokens[0] == "name" && tokens.size() <= 2)return NAME;
        else if(tokens[0] == "yell" && tokens.size() == 2)return YELL;
        else if(tokens[0] == "tell" && tokens.size() == 3)return TELL;
        return ERROR;
    }

    void say_hello(int client){
        char addr[INET_ADDRSTRLEN];
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        // get client address
        getpeername(client, (sockaddr *)&client_addr, &client_len);
        inet_ntop(AF_INET,&(client_addr.sin_addr), addr, INET_ADDRSTRLEN);
        string message = string("[Server] Hello, anonymous! From: ") + addr + '/' + to_string(client_addr.sin_port) + '\n';
        write(client, message.c_str(), message.length());
    }
    void who(int client){
        char addr[INET_ADDRSTRLEN];
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        string message;
        for(auto &c: clients){
            // get client address
            getpeername(c, (sockaddr *)&client_addr, &client_len);
            inet_ntop(AF_INET,&(client_addr.sin_addr), addr, INET_ADDRSTRLEN);
            message = string("[Server] ") + fd_name_map[c]  + ' ' + addr + '/' + to_string(client_addr.sin_port) +
                    + (c == client? " ->me\n" : "\n");
            write(client, message.c_str(), message.length());
        }
    }
    bool change_user_name(int client, const string& new_name){
        static string response;
        // check if new name = anonymous
        if(new_name == "anonymous"){
            response = "[Server] ERROR: Username cannot be anonymous.\n";
            write(client, response.c_str(), response.length());
            return false;
        }
        // check new name unique 
        if(name_fd_map.find(new_name) != name_fd_map.end()){
            response = string("[Server] ERROR: ") + new_name + " has been used by others.\n";
            write(client, response.c_str(), response.length());
            return false;
        }
        // check new name validity
        if(new_name.length() < 2 || new_name.length() > 12 || !regex_match(new_name, regex("[a-zA-Z]*"))){
            response = "[Server] ERROR: Username can only consists of 2~12 English letters.\n";
            write(client, response.c_str(), response.length());
            return false;
        }
        fd_name_map[client] = new_name;
        name_fd_map[new_name] = client;
        response = string("[Server] You're now known as ") + new_name + ".\n";
        write(client, response.c_str(), response.length());
        return true;
    }

    void private_message(int client, const string& to, const string& message){
        static string response;
        // check client anonymous
        if(fd_name_map.find(client) == fd_name_map.end()){
            response = "[Server] ERROR: You are anonymous.\n";
            write(client, response.c_str(), response.length());
            return;
        }
        // check target user not anonymous
        if(to == "anonymous"){
            response = "[Server] ERROR: The client to which you sent is anonymous.\n";
            write(client, response.c_str(), response.length());
            return;
        }
        // check target user exist 
        if(clients.find(name_fd_map[to]) == clients.end()){
            response = "[Server] ERROR: The receiver doesn't exist.\n";
            write(client, response.c_str(), response.length());
            return;
        }
        response = string("[Server] ") + fd_name_map[client] + " tell you " + message + '\n';
        write(name_fd_map[to], response.c_str(), response.length());
        response = "[Server] SUCCESS: Your message has been sent.\n";
        write(client, response.c_str(), response.length());
    }

    void brodcast(int exclude, string message){
        for(auto& client: clients)
            if(client != listener && client != exclude)
                write(client, message.c_str(), message.length());
    }
}; 

int main() {
    Server server(SERVER_PORT, 10, true);
    server.run();
}  
