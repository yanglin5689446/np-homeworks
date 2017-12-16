#include <string.h>
#include <iostream>
#include <netdb.h>
#include "sender.hpp"
using namespace std;

int main(int argc, char** argv){
    if(argc < 4){
        cout << "Usage: sender [receiver IP] [receiver port] [file name]" << endl;
        exit(1);
    }
    string remote_host;
    string ip(argv[1]);
    int port = stoi(argv[2]);
    string filename(argv[3]);

    SetsockoptSender sender(ip, port, filename);
    sender.send_file();
}  
