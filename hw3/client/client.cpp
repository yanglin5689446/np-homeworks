#include <string.h>
#include <sstream>
#include <iostream>
#include <netdb.h>
#include <unistd.h>
#include <regex>
#include <map>
#include "client.hpp"
using namespace std;

string client_name;
map<string, pair<int, int>> progress;
map<string, fstream> file_map;

void handleInitConnection(Client& client) {
    client.writeToServer(client_name); 
}

void handleServerResponse(Client& client, const string& message) {
    if(!message.length()){
        client.endConnection();
        exit(0);
    }
    stringstream ss(message);
    string token;
    ss >> token;
    if(token == "Welcome")cout << message << flush;
    else if(token == "/put"){
        ss >> token;
        file_map[token] = fstream(token, fstream::out | fstream::binary);
        int size;
        ss >> size;
        progress[token] = make_pair(0, size);
    }
    else {
        file_map[token] << message.substr(token.length());
        progress[token].first += message.length() - token.length() - 1;
        if(progress[token].first == progress[token].second)file_map[token].close();
    }
}
void handleClientInput(Client& client, const string& input){
    vector<string> tokens;
    stringstream tokenizer(input);
    string token;
    while(tokenizer >> token)tokens.push_back(token);
    if(tokens.empty())return; // empty input error
    if(tokens[0] == "/put" && tokens.size() == 2){
        ifstream file(tokens[1], ifstream::binary | ifstream::ate);
        if(!file)return; // open file error
        int n = file.tellg();
        file.close();
        client.writeToServer(input + ' ' + to_string(n));
        client.writeFile(tokens[1]);
    }
    else if(tokens[0] == "/sleep" && tokens.size() == 2){
        int s;
        if(regex_match(tokens[1], regex("[0-9]+")))s = stoi(tokens[1]);
        else return; // parse int error
        client.writeToServer(input);
        for(int i = 0 ;i < s ; i ++){
            sleep(1);
            cout << "sleep " << i + 1 << endl;
        }
        client.writeToServer("awake");
    }
    else if(tokens[0] == "/exit" && tokens.size() == 1){
        client.writeToServer(input);
        client.endConnection();
        exit(0);
    }
    
}

int main(int argc, char** argv){
    if(argc != 4){
        cout << "Usage: client [host] [port] [name]" << endl;
        exit(1);
    }
    string remote_host;
    int port;
    string in = argv[1];
    hostent *he = gethostbyname(in.c_str());
    if(he){
        in_addr **addr_list = (in_addr**)he->h_addr_list;
        remote_host = inet_ntoa(*addr_list[0]);
    }
    else remote_host = in;
    port = stoi(argv[2]);
    client_name = argv[3];

    Client client(remote_host, port, client_name);
    client.registerInitConnectionHandler(handleInitConnection);
    client.registerServerResponseHandler(handleServerResponse);
    client.registerClientInputHandler(handleClientInput);
    client.establishConnection();
}  
