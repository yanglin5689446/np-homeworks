
all:
	g++ sender/sender1.cpp -o sender1
	g++ sender/sender2.cpp -o sender2
	g++ sender/sender3.cpp -o sender3
	g++ receiver/receiver1.cpp -o receiver1
	g++ receiver/receiver2.cpp -o receiver2
	g++ receiver/receiver3.cpp -o receiver3

.PHONY: clean

clean: sender1 sender2 sender3 receiver1 receiver2 receiver3
	rm -rf $^
