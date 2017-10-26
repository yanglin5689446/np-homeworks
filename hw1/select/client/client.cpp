#include <stdio.h>  
#include <stdlib.h>
#include <string.h>  
#include <unistd.h>  
#include <sys/types.h>  
#include <sys/socket.h>  
#include <netinet/in.h>  
#include <arpa/inet.h>  
#include <iostream>
#include <sstream>
#include <fcntl.h>
#include <termios.h>
using namespace std;

const int port = 12345;

#define BUFFER_SIZE 1024

class Client{
public:
    Client(string addr, int port){
        server_addr.sin_family = AF_INET;  
        inet_pton(AF_INET, addr.c_str(), &server_addr.sin_addr);
        server_addr.sin_port = htons(port);  
        server = socket(AF_INET, SOCK_STREAM, 0);  
        FD_ZERO(&available);
    }
    void establish_connection(){
        connect(server, (struct sockaddr *)&server_addr, sizeof(server_addr));  
        // get hello message
        memset(buffer, 0, sizeof(buffer));
        int n = read(server, buffer, BUFFER_SIZE);
        printf("%s", buffer);
        printf("$ ");
        fflush(stdout);
        // set server to async
        string input;
        char c;
        while(true){
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
                    perror("read error.\n");
                    exit(1);
                }
                printf("\n%s", buffer);
                printf("$ ");
                fflush(stdout);
            }
            // handling server stdin 
            if(FD_ISSET(STDIN_FILENO, &available)){
                memset(buffer, 0, sizeof(buffer));
                int n = read(STDIN_FILENO, buffer, sizeof(buffer)); 
                if(n > 1){
                    input = buffer;
                    input.pop_back();
                    if(input == "exit")break;
                    write(server, input.c_str(), input.length());
                    memset(buffer, 0, sizeof(buffer));
                    n = read(server, buffer, BUFFER_SIZE);
                    if(n >= 0)printf("%s", buffer);
                }
                printf("$ ");
                fflush(stdout);
            }
        }
        close(server);  
    }

protected:
	int server;  
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE];
    string input;
    fd_set available;
};

int main(){
    Client client("127.0.0.1", port);
    client.establish_connection();
}  
