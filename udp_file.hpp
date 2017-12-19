

#ifndef UDP_FILE_H
#define UDP_FILE_H

#define MAX_BUFFER_SIZE 2048
#define MAX_DATA_SIZE   1400
#define GBN             1024

#include <memory>
#include <string>


struct Header{
    bool fin;
    int offset;
    int data_size;
};

struct Packet{
    Packet() = default;
    Packet(const char* data, Header header){
        this->header = header;
        // copy header
        memcpy(buffer, &header, sizeof(header));
        // copy packet data content
        memcpy(buffer + sizeof(Header), data, header.data_size);
    }
    int size(){
        return sizeof(Header) + header.data_size;
    }

    Header header;
    char buffer[MAX_BUFFER_SIZE];
};

#endif
