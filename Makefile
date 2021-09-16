.PHONY: all clean run_server

CC = g++
CFLAGS = -Wall -Wextra -std=c++17 -O3 -D_FILE_OFFSET_BITS=64 -c
LDFLAGS = -Wall -Wextra -std=c++17 -O3 -D_FILE_OFFSET_BITS=64
LDLIBS = -lboost_filesystem -lpthread
HEADERS = lock_writable_unordered_map.h common_utils.h WebServer.h HTTPresponse.h Cookie.h SessionCookie.h

all: server

server: server.o WebServer.o HTTPresponse.o Cookie.o SessionCookie.o
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

%.o: %.cpp $(HEADERS)
	$(CC) $(CFLAGS) -o $@ $< $(LDLIBS)

clean:
	rm -rf server *.o

run_server: server
	clear
	./server 42069 a a