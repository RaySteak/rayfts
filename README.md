# rayfts
HTTP file transfer server for linux, with large file support for 32-bit systems. Created for personal use, designed to run on low-end hardware like the Raspberry Pi and handle parallel downloading/uploading on a single process and a single thread, and parallel directory zipping by running the default zip function on seperate threads (if the default zipping function is used, these threads will also each spawn a child process that runs **7zip**).

## Requirements:
The server should compile on any linux system running a **gcc** version that supports the **C++17** standard. So far, compiling and running has been tested on Ubuntu and Raspberry OS 32-bit.
## Installation guide:
- First, clone the repository in any empty directory of your choice (**note**: this is where files will be stored as well, so make sure it's on the storage drive you want):
```
git clone https://github.com/RaySteak/rayfts .
```
- Next, install the boost filesystem library. Example for Debian based systems:
```
sudo apt install libboost-filesystem-dev -y
```
- If you want to use the default zip function, you must also have **7zip** installed and located in `/bin/7z`. The code in *server.cpp* can also be changed to use any zip function, by using the secondary constructor of WebServer. If neither of those options appeal to you, the server will still run but will crash when you use the zip feature.
- Finally, compile the executable for the server using `make server` or simply `make`.

## Using the command:
The server has 3 parameters: port, username and password, in this order.

An example is listed in the Makefile, which you can try with `make run_server`, which runs the server on port 42069, with credentials "a" and "a".
