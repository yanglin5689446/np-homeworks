
#include <string>
#include "receiver.hpp"
using namespace std;

int main(int argc, char **argv) {
    if(argc != 2){
        cout << "Usage: receiver [port]" << endl;
        exit(1);
    }
    Receiver receiver(stoi(argv[1]) ,true);
    receiver.run();
}  
