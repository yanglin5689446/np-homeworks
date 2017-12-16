#include <string.h>
#include <sstream>
#include <iostream>
#include <netdb.h>
#include "client.hpp"
using namespace std;

string client_name;

void handleInitConnection(Client& client) {
    client.writeToServer(client_name);
}

void handleServerResponse(Client& client, const string& message) {
    if(!message.length()){
        client.endConnection();
        exit(0);
    }
    cout << message << flush;
}
void handleClientInput(Client& client, const string& input){
    client.writeToServer(input);
    if(input == "exit"){
        client.endConnection();
        exit(0);
    }
}

int main(int argc, char** argv){
    if(argc != 4){
        cout << "Usage: client [host(default localhost)] [port] [name]" << endl;
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

    Client client(remote_host, port, name);
    client.registerInitConnectionHandler(handleInitConnection);
    client.registerServerResponseHandler(handleServerResponse);
    client.registerClientInputHandler(handleClientInput);
    client.establishConnection();
}  
