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
#include <iostream>
#include <vector>
#include <map>
#include <regex>
#include "server.hpp"
using namespace std;
using namespace epoll_server;

#define SERVER_PORT 12345 

const int EXIT = 0;
const int WHO = 1;
const int NAME = 2;
const int YELL = 3;
const int TELL = 4;
const int ERROR = 5;

map<int, string> fd_name_map;
map<string, int> name_fd_map;

string getClientLocation(int client){
    static char addr[INET_ADDRSTRLEN];
    static struct sockaddr_in client_addr;
    static socklen_t client_len = sizeof(client_addr);
    getpeername(client, (sockaddr *)&client_addr, &client_len);
    inet_ntop(AF_INET, &(client_addr.sin_addr), addr, INET_ADDRSTRLEN);
    return string(addr) + '/' + to_string(client_addr.sin_port);
}

int processInput(const string& input, vector<string>& tokens){
    string token;
    stringstream buffer(input);
    // tokenize
    while(buffer >> token)tokens.push_back(token);
    if(!tokens.size())return ERROR;
    else if(tokens[0] == "exit")return EXIT;
    else if(tokens[0] == "who" && tokens.size() == 1)return WHO;
    else if(tokens[0] == "name" && tokens.size() <= 2)return NAME;
    else if(tokens[0] == "yell" && tokens.size() == 2)return YELL;
    else if(tokens[0] == "tell" && tokens.size() == 3)return TELL;
    return ERROR;
}

void brodcast(Server& server, string message, int exclude = 0){
    server.forEachClient([&, message](Server& server, int client){
        server.send(client, message);
    }, exclude);
}

string helloMessage(int client){
    // get client address
    return string("[Server] Hello, anonymous! From: ") + getClientLocation(client) + '\n';
}


void handleNewConnection(Server& server, int client){
    fd_name_map[client] = string("anonymous");
    server.send(client, helloMessage(client));
    brodcast(server, "[Server] Someone is coming!\n", client);
}
void handleClient(Server& server, int client){
    vector<string> tokens;
    // split input to tokens
    int status = processInput(server.getClientInput(client), tokens);
    string response;
    // for change name function
    switch(status){
        case EXIT:
            cout << "exit" << endl;
            server.disconnect(client);
            break;
        case WHO:
            server.forEachClient([&, response](Server& server, int c) mutable {
                response = string("[Server] ") + fd_name_map[c] + ' '; 
                response += getClientLocation(c) + (c == client? " ->me\n" : "\n");
                server.send(client, response);
            });
            break;
        case NAME:
            // tokens[1]: new name
            // check if new name is anonymous
            if(tokens[1] == "anonymous")
                server.send(client, "[Server] ERROR: Username cannot be anonymous.\n");
            // check new name unique 
            else if(name_fd_map[tokens[1]] != 0)
                server.send(client, string("[Server] ERROR: ") + tokens[1] + " has been used by others.\n");
            // check new name validity
            else if(tokens[1].length() < 2 || tokens[1].length() > 12 || !regex_match(tokens[1], regex("[a-zA-Z]*")))
                server.send(client, "[Server] ERROR: Username can only consists of 2~12 English letters.\n");
            else{
                server.send(client, string("[Server] You're now known as ") + tokens[1] + ".\n");
                brodcast(server, "[Server] " + fd_name_map[client] + " is now known as " + tokens[1] + ".\n", client);
                // change name-fd mapping
                fd_name_map[client] = tokens[1];
                name_fd_map[tokens[1]] = client;
            }
            break;
        case YELL:
            brodcast(server, string("[Server] ") + fd_name_map[client] + " yell " + tokens[1] + '\n');
            break;
        case TELL:
            // tokens[1] : receiver name, tokens[2]: message
            // check client anonymous
            if(fd_name_map[client] == "anonymouse"){
                response = "[Server] ERROR: You are anonymous.\n";
                server.send(client, response);
            }
            // check target user not anonymous
            else if(tokens[1] == "anonymous"){
                response = "[Server] ERROR: The client to which you sent is anonymous.\n";
                server.send(client, response);
            }
            // check target user exist 
            else if(name_fd_map[tokens[1]] == 0){
                response = "[Server] ERROR: The receiver doesn't exist.\n";
                server.send(client, response);
            }
            else{
                // response to sender
                response = "[Server] SUCCESS: Your message has been sent.\n";
                server.send(client, response);
                // resoinse to receiver
                response = string("[Server] ") + fd_name_map[client] + " tell you " + tokens[2] + '\n';
                server.send(name_fd_map[tokens[1]], response);
            }
            break;
        case ERROR:
            server.send(client, "[Server] ERROR: Error command.\n");
            break;
    }
}

void handleDisconnection(Server& server, int client){
    static string client_name;
    brodcast(server,string("[Server] ") + fd_name_map[client] + " is offline.\n", client);
    client_name = fd_name_map[client];
    if(client_name != "anonymous")
       name_fd_map.erase(client_name); 
    fd_name_map.erase(client);
}

int main(int argc, char **argv) {
    if(argc != 2){
        cout << "Usage: server [port]" << endl;
        exit(1);
    }
    Server server(stoi(argv[1]), 20, true);
    server.registerNewConnectionHandler(handleNewConnection);
    server.registerDisconnectionHandler(handleDisconnection);
    server.registerClientHandler(handleClient);
    server.run();
}  
