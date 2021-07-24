.PHONY: all clean run_server

CC = g++
CFLAGS = -Wall -Wextra -std=c++17 -c
LDFLAGS = -Wall -Wextra -std=c++17
LDLIBS = -lstdc++fs
HEADERS = common_utils.h WebServer.h HTTPresponse.h Cookie.h SessionCookie.h

all: server

server: server.o WebServer.o HTTPresponse.o Cookie.o SessionCookie.o
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

server.o: server.cpp $(HEADERS)
	$(CC) $(CFLAGS) -o $@ $< $(LDLIBS)

WebServer.o: WebServer.cpp $(HEADERS)
	$(CC) $(CFLAGS) -o $@ $< $(LDLIBS)

HTTPresponse.o: HTTPresponse.cpp $(HEADERS)
	$(CC) $(CFLAGS) -o $@ $< $(LDLIBS)

Cookie.o: Cookie.cpp $(HEADERS)
	$(CC) $(CFLAGS) -o $@ $< $(LDLIBS)

SessionCookie.o: SessionCookie.cpp Cookie.o $(HEADERS)
	$(CC) $(CFLAGS) -o $@ $< $(LDLIBS)

clean:
	rm -rf server *.o

run_server: server
	clear
	./server 42069 a a