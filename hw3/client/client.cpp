#include <string.h>
#include <sstream>
#include <iostream>
#include <netdb.h>
#include <regex>
#include <map>
#include <stdio.h>  
#include <stdlib.h>
#include <unistd.h>  
#include <arpa/inet.h>  
#include <sys/types.h>  
#include <sys/socket.h>  
#include <netinet/in.h>  
#include <fstream>
#include <functional>
#include <sstream>
using namespace std;

#define BUFFER_SIZE 16384

string client_name;

class Client{
public:
    Client(std::string addr, int port, const string& name) : name(name){
        server_addr.sin_family = AF_INET;  
        inet_pton(AF_INET, addr.c_str(), &server_addr.sin_addr);
        server_addr.sin_port = htons(port);  
        // command fd
        fd_cmd = socket(AF_INET, SOCK_STREAM, 0);  
        // upload fd
        fd_upload = socket(AF_INET, SOCK_STREAM, 0);  
        FD_ZERO(&available);
    }
    void write_file(string file_name){
        // open file and send 
        char write_buffer[BUFFER_SIZE];
        fstream file(file_name, fstream::in | fstream::binary);
        while(!file.eof()){
            memset(write_buffer, 0, sizeof(write_buffer));
            memcpy(write_buffer, file_name.c_str(), file_name.length()); 
            write_buffer[file_name.length()] = ' ';
            file.read(write_buffer + file_name.length() + 1, BUFFER_SIZE - (file_name.length() + 1) - 1);
            int data_size = file_name.length() + 1 + file.gcount();
            non_block_write(fd_cmd, write_buffer, data_size);
            cout << data_size << endl;
        }
        file.close();
    }
    void establish_connection(){
        connect(fd_cmd, (struct sockaddr *)&server_addr, sizeof(server_addr));  
        connecting = true;
        non_block_write(fd_cmd, client_name.c_str(), client_name.length()); 
        while(connecting){
            // add server and stdin to fd set
            FD_SET(fd_cmd, &available);
            FD_SET(STDIN_FILENO, &available);
            if(select(fd_cmd + 1, &available, NULL, NULL, NULL) < 0){
                perror("select error.\n");
                exit(1);
            }
            // handling server fd
            if(FD_ISSET(fd_cmd, &available)){
                memset(buffer, 0, sizeof(buffer));
                int n = read(fd_cmd, buffer, BUFFER_SIZE);
                if(n < 0){
                    perror("read server message error.\n");
                    exit(1);
                }
                server_response(n);
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
                client_input(input);
            }
        }
        close(fd_cmd);  
    }

    void server_response(int buffer_size){ 
        if(!buffer_size){
            end_connection();
            exit(0);
        }
        stringstream ss(buffer);
        string token;
        ss >> token;
        if(token == "Welcome")cout << buffer << flush;
        else if(token == "/put"){
            ss >> token;
            file_map[token] = fstream(token, fstream::out | fstream::binary);
            int size;
            ss >> size;
            progress[token] = make_pair(0, size);
        }
        else {
            int data_size = buffer_size - token.length() - 1;
            file_map[token].write(buffer + token.length() + 1, data_size);
            progress[token].first += data_size;
            if(progress[token].first == progress[token].second)file_map[token].close();
        }
    }
    void client_input(const string& input){ 
        stringstream ss(input);
        string in;
        while(getline(ss, in)){
            vector<string> tokens;
            stringstream tokenizer(in);
            string token;
            while(tokenizer >> token)tokens.push_back(token);
            if(tokens.empty())return; // empty input error
            if(tokens[0] == "/put" && tokens.size() == 2){
                ifstream file(tokens[1], ifstream::binary | ifstream::ate);
                if(!file)return; // open file error
                int n = file.tellg();
                file.close();
                in = in + ' ' + to_string(n);
                //connect(fd_upload, (struct sockaddr *)&server_addr, sizeof(server_addr));  
                non_block_write(fd_cmd, in.c_str(), in.length());
                write_file(tokens[1]);
            }
            else if(tokens[0] == "/sleep" && tokens.size() == 2){
                int s;
                if(regex_match(tokens[1], regex("[0-9]+")))s = stoi(tokens[1]);
                else return; // parse int error
                non_block_write(fd_cmd, in.c_str(), in.length());
                for(int i = 0 ;i < s ; i ++){
                    sleep(1);
                    cout << "sleep " << i + 1 << endl;
                }
            }
            else if(tokens[0] == "/exit" && tokens.size() == 1){
                non_block_write(fd_cmd, in.c_str(), in.length());
                end_connection();
                exit(0);
            }
        } 
    }
    void end_connection(){
        connecting = false;
    }
    void non_block_write(int fd, const char* msg, int size){
        int n = 0;
        int nwrite;
        while(n < size){
            nwrite = write(fd, msg + n, size - n);
            if(nwrite < 0 && errno == EAGAIN)break;
            n += nwrite;
        }
    }

	int fd_cmd;  
    int fd_upload;
    int connecting;
    sockaddr_in server_addr;
    map<string, pair<int, int>> progress;
    map<string, fstream> file_map;
    char buffer[BUFFER_SIZE];
    std::string message;
    std::string input;
    std::string name;
    fd_set available;
};



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
    client.establish_connection();
}  
