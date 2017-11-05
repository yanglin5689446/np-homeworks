#include <string.h>
#include <iostream>
#include <netdb.h>
#include "client.hpp"
using namespace std;

void handleServerResponse(Client& client, const string& message) {
    if(!message.length()){
        client.endConnection();
        exit(0);
    }
    cout << endl << message;
    cout << "$ " << flush;
}
void handleClientInput(Client& client, const string& input){
    if(input == "exit"){
        client.endConnection();
        exit(0);
    }
    client.writeToServer(input);
    cout << "\033[1A" << flush;
}

int main(int argc, char** argv){
    if(argc == 1 || argc > 3){
        cout << "Usage: client [host(default localhost)] [port]" << endl;
        exit(1);
    }
    string remote_host;
    int port;
    if(argc == 2)port = stoi(argv[1]);
    if(argc == 3){
        string in = argv[1];
        hostent *he = gethostbyname(in.c_str());
        if(he){
            in_addr **addr_list = (in_addr**)he->h_addr_list;
            remote_host = inet_ntoa(*addr_list[0]);
        }
        else remote_host = in;
        port = stoi(argv[2]);
    }

    Client client(remote_host, port);
    client.registerServerResponseHandler(handleServerResponse);
    client.registerClientInputHandler(handleClientInput);
    client.establishConnection();
}  
