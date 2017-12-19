
#ifndef CLIENT_H
#define CLIENT_H

#include <stdio.h>  
#include <stdlib.h>
#include <string.h>  
#include <unistd.h>  
#include <arpa/inet.h>  
#include <sys/types.h>  
#include <sys/socket.h>  
#include <sys/time.h>  
#include <signal.h>
#include <netinet/in.h>  
#include <iostream>
#include <fstream>
#include <string>
#include <functional>
#include "../udp_file.hpp"
using namespace std;

const int header_size = sizeof(Header);
const int frame_size = header_size + MAX_DATA_SIZE;

class Sender{
public:
    Sender() = default;
    Sender(std::string addr, int port, string filename, int ms = 50) : filename(filename), retransmit_ms(ms){
        // socket initialization
        sender_addr.sin_family = AF_INET;  
        inet_pton(AF_INET, addr.c_str(), &sender_addr.sin_addr);
        sender_addr.sin_port = htons(port);  
        sender_fd = socket(AF_INET, SOCK_DGRAM, 0);  
        tv.tv_sec = 0;
        tv.tv_usec = retransmit_ms * 1000;
        connect(sender_fd, (struct sockaddr *)&sender_addr, sizeof(sender_addr));  
        connecting = true;
    }
    ~Sender(){}
    void send_file(){

        // initialization
        ifstream file(filename, ios_base::in | ios_base::binary);
        // send file name first
        /*
            Header format:
                bool fin
                int offset
                int data_size
        */
        int syn = 0
        Packet fname_packet(filename.c_str(), (Header){0, syn++, -1, (int)filename.length()});
        while(!send_packet(fname_packet));

        while(connecting){
            int offset = file.tellg(); 
            file.read(buffer, MAX_DATA_SIZE);
            int nbytes = file.gcount();
            Packet packet(buffer, (Header){file.eof(), syn++, offset, nbytes });
            
            // if finished, end connection
            connecting = !packet.header.fin;

            // continuously try to resend while not succeeded
            while(!send_packet(packet));
        }
        close(sender_fd);
        file.close();
    }

protected:
    virtual bool send_packet(Packet& packet){}
    int sender_fd;  
    int retransmit_ms;
    int connecting;
    sockaddr_in sender_addr;
    char buffer[MAX_BUFFER_SIZE];
    string filename;
    string input;
    // select timeout
    timeval tv;
};

class SelectSender : public Sender {
public: 
    SelectSender() = default;
    SelectSender(std::string addr, int port, string filename, int ms = 50) : Sender(addr, port, filename, ms){
        FD_ZERO(&available);
    }
    ~SelectSender() {};
private:
    bool send_packet(Packet& packet){
        // temp timeval variable in case that select() override original one
        static timeval ttv;

        cout << "sending datagram: offset = "  << packet.header.offset << 
                ", size = " << packet.header.data_size << "..." << flush;
        write(sender_fd, packet.buffer, packet.size());
        
        ttv = tv;
        FD_SET(sender_fd, &available);
        int nready = select(sender_fd + 1, &available, NULL, NULL, &ttv);
        if(nready == 0){
            cout << "timeout, try to resend" << endl;
            return false;
        }
        else if(nready < 0){
            cout << "error on select(), exit" << endl;
            exit(1);
        }
        else {
            int recv = read(sender_fd, buffer, MAX_BUFFER_SIZE);
            if(recv < 0){
                cout << "The peer has close connection or unknown system error occured." << endl;
                connecting = false;
                return true;
            }
            cout << "succeed!" << endl;
            return true;
        }
    }
    fd_set available;
};

class SetsockoptSender : public Sender {
public: 
    SetsockoptSender() = default;
    SetsockoptSender(std::string addr, int port, string filename, int ms = 50) : Sender(addr, port, filename, ms){
        setsockopt(sender_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    }
    ~SetsockoptSender() {};
private:
    bool send_packet(Packet& packet){
        // temp timeval variable in case that select() override original one
        static timeval ttv;

        cout << "sending datagram: offset = "  << packet.header.offset << 
                ", size = " << packet.header.data_size << "..." << flush;
        write(sender_fd, packet.buffer, packet.size());

        int recv = read(sender_fd, buffer, MAX_BUFFER_SIZE);
        if(recv < 0){
            if(errno == EWOULDBLOCK){
                cout << "timeout, try to resend" << endl;
                return false;
            }
            else{
                cout << "The peer has close connection or unknown system error occured." << endl;
                connecting = false;
                return true;
            }
        }
        else {
            if(recv == 0){
                cout << "The peer has close connection or unknown system error occured." << endl;
                connecting = false;
                return true;
            }
            cout << "succeed!" << endl;
            return true;
        }
        
    }
};


class AlarmSender : public Sender {
public: 
    AlarmSender() = default;
    AlarmSender(std::string addr, int port, string filename, int ms = 50) : Sender(addr, port, filename, ms){
        // Set Alarm
        signal(SIGALRM, [](int signo){ alarm(1); return; });
        siginterrupt(SIGALRM, 1);
        alarm(1);
    }
    ~AlarmSender() {};
private:
    bool send_packet(Packet& packet){
        // temp timeval variable in case that select() override original one
        static timeval ttv;

        cout << "sending datagram: offset = "  << packet.header.offset << 
                ", size = " << packet.header.data_size << "..." << flush;
        write(sender_fd, packet.buffer, packet.size());

        int recv = read(sender_fd, buffer, MAX_BUFFER_SIZE);
        if(recv < 0){
            if(errno == EINTR){
                cout << "timeout, try to resend" << endl;
                return false;
            }
            else{
                cout << "The peer has close connection or unknown system error occured." << endl;
                connecting = false;
                return true;
            }
        }
        else {
            if(recv == 0){
                cout << "The peer has close connection or unknown system error occured." << endl;
                connecting = false;
                return true;
            }
            cout << "succeed!" << endl;
            return true;
        }
        
    }
};

#endif
