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
using namespace std;

const int port = 12345;
const int BUFFER_SIZE = 1024;

class Client{
public:
    Client(string addr, int port){
        server_addr.sin_family = AF_INET;  
        inet_pton(AF_INET, addr.c_str(), &server_addr.sin_addr);
        server_addr.sin_port = htons(port);  
        server = socket(AF_INET, SOCK_STREAM, 0);  
    }
    void establish_connection(){
        connect(server, (struct sockaddr *)&server_addr, sizeof(server_addr));  
        // get hello message
        safe_read(server);
        printf("%s", buffer);
        while(true){
            printf("$ ");
            stringstream ss;
            getline(cin, input);
            ss << input;
            if(ss.str() == "exit"){
                close(server);
                exit(0);
            }
            if(input.length() == 0)input = "none";
            safe_write(server, input.c_str());
            safe_read(server);
            printf("%s", buffer);
        }

        close(server);  
    }

protected:
    int safe_read(int sock){
        memset(buffer, 0, sizeof(buffer));
        static int n;
        n = read(sock, buffer, BUFFER_SIZE);
        if(n < 0){
            fprintf(stderr, "Error: Read socket failed.\n");
            exit(1);
        }
        return n;
    }

    int safe_write(int sock, const char* message){
        static int n;
        n = write(sock, message, strlen(message));
        if(n < 0){
            fprintf(stderr, "Error: Write to socket failed.\n");
            exit(1);
        }
        return n;
    }
	int server;  
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE];
    string input;
};

int main(){
    Client client("127.0.0.1", port);
    client.establish_connection();
}  
