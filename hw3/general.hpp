
#ifndef GENERAL_HPP
#define GENERAL_HPP

#include <fstream>
#include <fcntl.h>
#include <string>
using namespace std;

#define BUFFER_SIZE 16384

int get_file_size(string file_name){
    ifstream file(file_name, ifstream::binary | ifstream::ate);
    // open file error
    if(!file.is_open())return -1; 
    // get_file_size
    int size = file.tellg();
    file.close();
    return size;
}

struct FileInfo {
    FileInfo() = default;
    FileInfo(string file_name, int size, bool mode) : file_name(file_name), current(0), size(size) {
        file = fstream(file_name, (mode ? fstream::out : fstream::in ) | fstream::binary);
    };
    bool done() { return current == size; }
    int current;
    int size;
    fstream file;
    string file_name;

    static const bool in = 0;
    static const bool out = 1;
};

bool set_nonblocking(int &sock){
    int opts;
    opts = fcntl(sock, F_GETFL);
    if(opts < 0){
        perror("error on getting sock staus.\n");
        return false;
    }
    opts |= O_NONBLOCK;
    if(fcntl(sock, F_SETFL, opts) < 0){
        perror("error on setting sock non-blocking.\n");
        return false;
    }
    return true;
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
int non_block_read(int client, char* buffer, int max_size = BUFFER_SIZE){
    memset(buffer, 0, max_size);
    int n = 0, nread = 0;
    while((nread = read(client, buffer + n, max_size - n - 1)) > 0 ) n += nread;
    return n;
}


#endif
