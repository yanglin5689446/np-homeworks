#include <string.h>
#include <sstream>
#include <iostream>
#include <netdb.h>
#include <regex>
#include <map>
#include <set>
#include <stdio.h>  
#include <stdlib.h>
#include <unistd.h>  
#include <arpa/inet.h>  
#include <sys/types.h>  
#include <sys/socket.h>  
#include <netinet/in.h>  
#include <fstream>
#include <functional>
#include <algorithm>
#include <sstream>
#include "general.hpp"
using namespace std;

string client_name;


class Client{
public:
    Client(std::string addr, int port, const string& name) : name(name){
        server_addr.sin_family = AF_INET;  
        inet_pton(AF_INET, addr.c_str(), &server_addr.sin_addr);
        server_addr.sin_port = htons(port);  
        // command fd
        fd_cmd = new_connection(); 
        FD_ZERO(&read_set);
        FD_ZERO(&write_set);
    }

    void establish_connection(){
        connect(fd_cmd, (struct sockaddr *)&server_addr, sizeof(server_addr));  
        connecting = true;
        // send client name to server
        string set_name = "/name " + client_name;
        non_block_write(fd_cmd, set_name.c_str(), set_name.length()); 
        max_fd = fd_cmd;

        while(connecting){
            // add server and stdin to fd set
            FD_SET(fd_cmd, &read_set);
            for(auto& fd: downloading_fds)FD_SET(fd, &read_set);
            for(auto& fd: uploading_fds)FD_SET(fd, &write_set);
            FD_SET(STDIN_FILENO, &read_set);

            if(select(max_fd + 1, &read_set, &write_set, NULL, NULL) < 0){
                perror("select error.\n");
                exit(1);
            }
            // handling client fd
            if(FD_ISSET(fd_cmd, &read_set)){
                memset(buffer, 0, sizeof(buffer));
                int n = non_block_read(fd_cmd, buffer, BUFFER_SIZE);
                if(n < 0){
                    perror("read server message error.\n");
                    exit(1);
                }
                server_response(n);
            }
            // handling client stdin 
            if(FD_ISSET(STDIN_FILENO, &read_set)){
                memset(buffer, 0, sizeof(buffer));
                int n = read(STDIN_FILENO, buffer, sizeof(buffer)); 
                if(n < 0){
                    perror("read from stdin failed.\n");
                    exit(1);
                }
                input = buffer;
                if(input.length() > 1)input.pop_back();
                handle_input(input);
            }
            // handle download fds
            for(auto& fd :downloading_fds)
                if(FD_ISSET(fd, &read_set))download_file(fd);
            // handle upload fd
            for(auto& fd : uploading_fds)
                if(FD_ISSET(fd, &write_set))upload_file(fd);

            FD_ZERO(&read_set);
            FD_ZERO(&write_set);
        }
        close(fd_cmd);  
    }

    void download_file(int fd){
        int data_size = non_block_read(fd, buffer);
        downloading[fd].file.write(buffer, data_size);
        downloading[fd].current += data_size;
        int progress = 100*downloading[fd].current / downloading[fd].size;
        cout << "Downloading file : " << downloading[fd].file_name << endl;
        cout << "Progress : [";
        for(int i = 0 ;i < 20 ; i++)
            cout << ((progress >= i * 5) ? '#' : ' ');
        cout <<"]" << endl;
        if(downloading[fd].done() || !data_size){
            if(!data_size)perror("error while receiving file: the server may end the connection");
            if(downloading[fd].done())cout << "Download " << downloading[fd].file_name << " complete!" << endl;
            downloading[fd].file.close();
            downloading.erase(fd);
            downloading_fds.erase(fd);
            close(fd);
        }
    }
    void upload_file(int fd){
        static char write_buffer[BUFFER_SIZE];
        if(!uploading[fd].file.eof()){
            memset(write_buffer, 0, sizeof(write_buffer));
            uploading[fd].file.read(write_buffer, BUFFER_SIZE - 1);
            int data_size = uploading[fd].file.gcount();
            uploading[fd].current += data_size;
            int progress = 100*uploading[fd].current / uploading[fd].size;
            cout << "Uploading file : " + uploading[fd].file_name << endl;
            cout << "Progress : [";
            for(int i = 0 ;i < 20 ; i++)
                cout << ((progress >= i * 5) ? '#' : ' ');
            cout <<"]" << endl;
            non_block_write(fd, write_buffer, data_size);
        }
        else {
            if(uploading[fd].done())cout << "Upload " << uploading[fd].file_name << " complete!" << endl;
            uploading[fd].file.close();
            uploading.erase(fd);
            uploading_fds.erase(fd);
            write(fd, write_buffer, 0);
            close(fd);
        }
    }

    int new_connection(){
        int fd = socket(AF_INET, SOCK_STREAM, 0);  
        connect(fd, (struct sockaddr *)&server_addr, sizeof(server_addr));  
        if(fd < 0){
            perror("fail to create new connection");
            exit(1);
        }
        if(fd > max_fd)max_fd = fd;
        set_nonblocking(fd);
        return fd;
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
            string file_name;
            int size;
            ss >> file_name >> size;
            // get file command
            // if no file present in current local_file, download files
            if(local_files.find(file_name) == local_files.end()){
                local_files.insert(file_name);
                // new donwload connection
                int fd = new_connection();
                downloading_fds.insert(fd);
                downloading[fd] = FileInfo(file_name, size, FileInfo::out);
                string get_file = string("/get ") + file_name;
                non_block_write(fd, get_file.c_str(), get_file.length());
            }
        }
    }

    void handle_input(const string& input){ 
        stringstream ss(input);
        string in;
        while(getline(ss, in)){
            vector<string> tokens;
            stringstream tokenizer(in);
            string token;
            while(tokenizer >> token)tokens.push_back(token);
            if(tokens.empty())return; // empty input error
            if(tokens[0] == "/put" && tokens.size() == 2){
                // send fomat: /put file_name who file_size
                int size = get_file_size(tokens[1]);
                if(size < 0 )return;
                string put_file = in + ' ' + to_string(size) + ' ' + client_name;
                // new upload connection
                int fd = new_connection();
                uploading_fds.insert(fd);
                uploading[fd] = FileInfo(tokens[1], size, FileInfo::in);
                local_files.insert(tokens[1]);
                non_block_write(fd, put_file.c_str(), put_file.length());
                // workaound
                usleep(100);
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
	int fd_cmd;  
    int max_fd;
    bool connecting;
    sockaddr_in server_addr;
    map<int, FileInfo> uploading;
    map<int, FileInfo> downloading;
    set<int> downloading_fds;
    set<int> uploading_fds;
    set<string> local_files;
    char buffer[BUFFER_SIZE];
    std::string message;
    std::string input;
    std::string name;
    fd_set read_set, write_set;
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
