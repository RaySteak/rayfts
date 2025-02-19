.PHONY: all clean run_server debug run_server_debug

CC = g++
CFLAGS = -Wall -Wextra -std=c++17 -O3 -D_FILE_OFFSET_BITS=64 -Wno-psabi -c
LDFLAGS = -s
LDLIBS = -lboost_filesystem -lboost_system -lpthread -lz
HEADERS = lock_writable_unordered_map.h web_utils.h common_utils.h WebServer.h HTTPresponse.h Cookie.h SessionCookie.h wake_on_lan.h ping_device.h arduino/arduino_constants.h

# Dummy server params
PORT = 42069
USER = a
SALT = this_is_a_salt
# SHA3-512 digest of the dummy salt concatenated with dummy password "a"
DIGEST_DUMMY_SALT_PASS = e072da9305f63fa35fc0b18d3c4b8e35994368567fcdf9d3675fd0503de9f764ca4a53ef19c428bd62419fe1f6792864deac8a00bdbfcbfce67bc835839856bb

all: server

debug: CFLAGS := $(filter-out -O3,$(CFLAGS))
debug: LDFLAGS := $(filter-out -s, $(LDFLAGS))
debug: CFLAGS += -g -DSERVER_DEBUG
debug: server

server: server.o WebServer.o HTTPresponse.o Cookie.o SessionCookie.o web_utils.o wake_on_lan.o ping_device.o common_utils.o arduino/arduino_constants.o crypto/sha3.o
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

%.o: %.cpp $(HEADERS)
	$(CC) $(CFLAGS) -o $@ $<

arduino/%.o: arduino/%.cpp
	$(CC) $(CFLAGS) -o $@ $<

crypto/%.o: crypto/%.cpp
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -rf server *.o arduino/*.o crypto/*.o

run_server_debug: debug run_server

run_server: server
	clear
	./server $(PORT) $(USER) $(SALT) $(DIGEST_DUMMY_SALT_PASS)

run_valgrind: debug
	clear
	valgrind --tool=memcheck --leak-check=full --show-leak-kinds=all ./server $(PORT) $(USER) $(SALT) $(DIGEST_DUMMY_SALT_PASS)

run_gdb: debug
	clear
	gdb --args ./server $(PORT) $(USER) $(SALT) $(DIGEST_DUMMY_SALT_PASS)