.PHONY: all clean run_server debug run_server_debug

CC = g++
CFLAGS = -Wall -Wextra -std=c++17 -O3 -D_FILE_OFFSET_BITS=64 -Wno-psabi -c
LDFLAGS = -s
LDLIBS = -lboost_filesystem -lboost_system -lpthread
HEADERS = lock_writable_unordered_map.h web_utils.h common_utils.h WebServer.h HTTPresponse.h Cookie.h SessionCookie.h wake_on_lan.h ping_device.h arduino/arduino_constants.h

all: server

debug: CFLAGS := $(filter-out -O3,$(CFLAGS))
debug: LDFLAGS := $(filter-out -s, $(LDFLAGS))
debug: CFLAGS += -g -DSERVER_DEBUG
debug: server

server: server.o WebServer.o HTTPresponse.o Cookie.o SessionCookie.o web_utils.o wake_on_lan.o ping_device.o common_utils.o arduino/arduino_constants.o
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

%.o: %.cpp $(HEADERS)
	$(CC) $(CFLAGS) -o $@ $<

arduino/arduino_constants.o: arduino/arduino_constants.cpp
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -rf server *.o

run_server_debug: debug run_server

run_server: server
	clear
	./server 42069 a a