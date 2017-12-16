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
#include "server.hpp"
using namespace std;
using namespace epoll_server;

#define SERVER_PORT 12345 


map<int, string> fd_name_map;
map<int, int> fd_status;

int processInput(const string& input, vector<string>& tokens){
}

void brodcast(Server& server, string message, int exclude = 0){
    server.forEachClient([&, message](Server& server, int client){
        server.send(client, message);
    }, exclude);
}

string helloMessage(int client){
    // get client address
    return string("Welcome to the dropbox-like server! : ") + fd_name_map[client] + '\n';
}


void handleNewConnection(Server& server, int client){
}
void handleClient(Server& server, int client){
    // split input to tokens
    string s = server.getClientInput(client);
    string input;
    stringstream ss(s);
    while(getline(ss, input)){
        cout << input << endl;
        vector<string> tokens;
        int status = processInput(input, tokens);
        string response;

        if(status[client] == NEW_CONNECTION){
        }
        else if(status[client] == IDLE){
            // tokenize
            string token;
            stringstream tokenizer(input);
            while(tokenizer >> token)tokens.push_back(token);

            if(tokens[0] == "exit"){
                cout << "exit" << endl;
                server.disconnect(client);
                break;
            }
            else if(tokens[0] == "put" && tokens.size() == 1){
            }
            else if(tokens[0] == "sleep" && tokens.size() == 1){
            }
        }
    }
}

void handleDisconnection(Server& server, int client){
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
