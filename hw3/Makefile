
all:
	g++ server/server.cpp -o server --std=c++14
	g++ client/client.cpp -o client --std=c++14

.PHONY: clean

clean: server client
	rm -rf $^
