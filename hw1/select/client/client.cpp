#include <string.h>
#include <iostream>
#include "client.hpp"
using namespace std;

const int port = 12345;

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

int main(){
    Client client("127.0.0.1", port);
    client.registerServerResponseHandler(handleServerResponse);
    client.registerClientInputHandler(handleClientInput);
    client.establishConnection();
}  
