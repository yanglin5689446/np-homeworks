
#include <iostream>
#include <fstream>
#include <string>
#include <algorithm>
#include <sstream>
#include <vector>
using namespace std;

string& trim(string& s){
    if(s.empty())return s;
    s.erase(0, s.find_first_not_of(" "));
    s.erase( s.find_last_not_of(" ")+1);
    return s;
}

void split(const string& in, const string& delim, vector<string>& result){
    int last = 0;
    int current = in.find_first_of(delim[0], last);
    while(current != string::npos){
        if(current + delim.length() > in.length())break;
        if(in.substr(current, delim.length()) == delim){
            result.push_back(in.substr(last, current - last));
            last = current + delim.length();
            current = in.find_first_of(delim[0], last);
        }
        else {
            current += 1;
            current = in.find_first_of(delim[0], current);
        }
    }
    if(in.length() - last > 0)result.push_back(in.substr(last, in.length() - last));
}

void process(string& in, const string& delim){
    if(in.substr(0, 7) == "reverse"){
        in.erase(0, 7);
        trim(in);
        reverse(in.begin(), in.end());
        cout << in << endl;
    }
    else if(in.substr(0, 5) == "split"){
        in.erase(0, 5);
        trim(in);
        vector<string> res;
        split(in, delim, res);
        for(auto& token : res)cout << token << ' ';
        cout << endl;
    }
}

int main (int argc, char **argv) {
    if(argc != 3)exit(1);
    fstream f(argv[1], fstream::in);
    const string delim(argv[2]);
    string in;
    while(getline(f, in)){
        cout << in << endl;
        if(trim(in) == "exit")break;
        process(in, delim);
    }
    while(getline(cin, in)){
        if(trim(in) == "exit")break;
        process(in, delim);
    }
}
