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
#include "server.hpp"
using namespace std;
using namespace epoll_server;

#define SERVER_PORT 12345 

enum: int{
    INITIAL,
    IDLE,
    SLEEPING,
    UPLOADING,
};

// file upload format: <file_name> <progress>

map<int, string> name_map;
map<int, int> status_map;
map<string, fstream> file_map;
map<string, set<int>> name_clients_map;
map<string, pair<int, int>> progress;


void handleNewConnection(Server& server, int client){}
void handleClient(Server& server, int client){
    // split input to tokens
    string s = server.getClientInput(client);
    string input;
    stringstream ss(s);
    while(getline(ss, input)){
        
        if(status_map[client] == INITIAL){
            name_map[client] = input;
            // add client to same name clients set
            if(!name_clients_map.count(input))name_clients_map[input] = set<int>();
            name_clients_map[input].insert(client);
            
            status_map[client] = IDLE;
            server.send(client, string("Welcome to the dropbox-like server! : ") + name_map[client] + '\n');
        }
        else if(status_map[client] == IDLE){
            // tokenize
            vector<string> tokens;
            string response;
            string token;
            stringstream tokenizer(input);
            while(tokenizer >> token)tokens.push_back(token);

            if(tokens[0] == "/exit"){
                cout << name_map[client] << " exit." << endl;
                server.disconnect(client);
            }
            else if(tokens[0] == "/put" && tokens.size() == 3){
                status_map[client] = UPLOADING;
                file_map[tokens[1]] = fstream(tokens[1], fstream::out | fstream::binary);
                progress[tokens[1]] = make_pair(0, stoi(tokens[2]));
                for(auto& peer: name_clients_map[name_map[client]])
                    if(status_map[peer] != SLEEPING && peer != client)server.send(client, input);
            }
            else if(tokens[0] == "/sleep" && tokens.size() == 2)
                status_map[client] = SLEEPING;
        }
        else if(status_map[client] == UPLOADING){
            cout << input << endl;
            for(auto& peer: name_clients_map[name_map[client]])
                if(status_map[peer] != SLEEPING && peer != client)server.send(client, input);

            stringstream tokenizer(input);
            string file_name;
            tokenizer >> file_name;
            
            file_map[file_name] << input.substr(file_name.length());
            progress[file_name].first += input.length() - file_name.length();
            if(progress[file_name].first == progress[file_name].second)file_map[file_name].close();
        }
        else if(status_map[client] == SLEEPING && input == "awake")status_map[client] = IDLE;
    }
}

void handleDisconnection(Server& server, int client){
    name_clients_map[name_map[client]].erase(client);
    name_map.erase(client);
    status_map[client] = INITIAL;
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
