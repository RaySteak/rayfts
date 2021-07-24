.PHONY: all clean run_server

CC = g++
CFLAGS = -Wall -Wextra -std=c++17 -lstdc++fs -c
LDFLAGS = -Wall -Wextra
HEADERS = common_utils.h WebServer.h HTTPresponse.h Cookie.h SessionCookie.h

all: server

server: server.o WebServer.o HTTPresponse.o Cookie.o SessionCookie.o
	$(CC) $(LDFLAGS) -o $@ $^

server.o: server.cpp $(HEADERS)
	$(CC) $(CFLAGS) -o $@ $<

WebServer.o: WebServer.cpp $(HEADERS)
	$(CC) $(CFLAGS) -o $@ $<

HTTPresponse.o: HTTPresponse.cpp $(HEADERS)
	$(CC) $(CFLAGS) -o $@ $<

Cookie.o: Cookie.cpp $(HEADERS)
	$(CC) $(CFLAGS) -o $@ $<

SessionCookie.o: SessionCookie.cpp Cookie.o $(HEADERS)
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -rf server *.o

run_server: server
	clear
	./server 42069 a a