#include <string.h>
#include <iostream>
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
        cout << "Usage: client [port] [host(default localhost)]" << endl;
        exit(1);
    }
    string remote_host;
    int port = stoi(argv[1]);
    if(argc == 3)remote_host = argv[2];

    Client client(remote_host, port);
    client.registerServerResponseHandler(handleServerResponse);
    client.registerClientInputHandler(handleClientInput);
    client.establishConnection();
}  
