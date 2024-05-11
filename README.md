# rayfts
HTTP file transfer server for linux, with large file support for 32-bit systems. Created for personal use, designed to run on low-end hardware like the Raspberry Pi and handle parallel downloading/uploading on a single process and a single thread, and parallel directory zipping by running the zip function on seperate threads (if the default zipping function is used, these threads will also each spawn a child process that runs **7zip**).

## Requirements:
The server should compile on any linux system running a **gcc** version that supports the **C++17** standard and has the boost filesystem library installed. So far, compiling and running has been tested on Ubuntu and Raspberry OS 32-bit. Some functionalities, like directory zipping and the control page require additional programs (see the "Installation guide" section).
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
- Additionaly, the control page requires that **fping** is installed under `/bin/fping`.
- Finally, compile the executable for the server using `make server` or simply `make`.

## Using the command:
The server has 3 parameters: port, username and password SHA-256 digest, in this order.

An example is listed in the Makefile, which you can try with `make run_server`, which runs the server on port 42069, with credentials "a" and "a". You can also use `make run_server_debug` which doesn't require a password. Just make sure to `make clean` if you want to use `make run_server` after running `make run_server_debug` or vice versa.

## Future improvements (in order of importance):
- Photo viewer for folder
- More visual alert for user when upload fails
- Interface for public files.
- Directory interface auto-refreshing to always display up-to-date files by using either polling or finding an easy way to implement websockets in C++, which would also mean replacing all other polling instances with websocket (e.g. Device state in the IoT page).
- Move 'delete' function into context menu.
- Displaying of extra information when pasting fails.
- File copy (easy to implement, but probably will not be implemented as it's not needed).
- gzip encoded transfer for file uploading.
