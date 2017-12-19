
#ifndef CLIENT_H
#define CLIENT_H

#include <stdio.h>  
#include <stdlib.h>
#include <string.h>  
#include <unistd.h>  
#include <arpa/inet.h>  
#include <sys/types.h>  
#include <sys/socket.h>  
#include <netinet/in.h>  
#include <iostream>
#include <string>
#include <fstream>
#include <functional>
#include <sstream>
using namespace std;

#define BUFFER_SIZE 16384

class Client{
public:
    Client(std::string addr, int port, const string& name) : name(name){
        server_addr.sin_family = AF_INET;  
        inet_pton(AF_INET, addr.c_str(), &server_addr.sin_addr);
        server_addr.sin_port = htons(port);  
        server = socket(AF_INET, SOCK_STREAM, 0);  
        FD_ZERO(&available);
    }
    void establishConnection(){
        connect(server, (struct sockaddr *)&server_addr, sizeof(server_addr));  
        connecting = true;
        init_connection_handler(*this);
        while(connecting){
            // add server and stdin to fd set
            FD_SET(server, &available);
            FD_SET(STDIN_FILENO, &available);
            if(select(server + 1, &available, NULL, NULL, NULL) < 0){
                perror("select error.\n");
                exit(1);
            }
            // handling server fd
            if(FD_ISSET(server, &available)){
                memset(buffer, 0, sizeof(buffer));
                int n = read(server, buffer, BUFFER_SIZE);
                if(n < 0){
                    perror("read server message error.\n");
                    exit(1);
                }
                message = buffer;
                server_response_handler(*this, message);
            }
            // handling server stdin 
            if(FD_ISSET(STDIN_FILENO, &available)){
                memset(buffer, 0, sizeof(buffer));
                int n = read(STDIN_FILENO, buffer, sizeof(buffer)); 
                if(n <= 0){
                    perror("read from stdin failed.\n");
                    exit(1);
                }
                input = buffer;
                if(input.length() > 1)input.pop_back();
                client_input_handler(*this, input);
            }
        }
        close(server);  
    }

    void registerInitConnectionHandler(function<void(Client&)> f){ 
        init_connection_handler = f;
    }
    void registerServerResponseHandler(function<void(Client&, const std::string&)> f){ 
        server_response_handler = f;
    }
    void registerClientInputHandler(function<void(Client&, const std::string&)> f){ 
        client_input_handler = f;
    }
    void endConnection(){
        connecting = false;
    }
    void writeToServer(string s){
        write(server, s.c_str(), s.length());
    }
    void writeFile(string file_name){
        // open file and send 
        fstream file(file_name, fstream::in | fstream::binary);
        while(file){
            file.getline(buffer, BUFFER_SIZE);
            string packet = file_name + " " + buffer;
            write(server, packet.c_str(), packet.length());
        }
        file.close();
    }

protected:
	int server;  
    int connecting;
    sockaddr_in server_addr;
    char buffer[BUFFER_SIZE];
    std::string message;
    std::string input;
    std::string name;
    fd_set available;
    function<void(Client&)> init_connection_handler; 
    function<void(Client&, const std::string&)> server_response_handler; 
    function<void(Client&, const std::string&)> client_input_handler; 
};
#endif
