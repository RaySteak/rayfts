# rayfts
HTTP file transfer server for linux. Created for personal use, designated to run on low-end hardware like the Raspberry Pi
## Installation guide:
- First, clone the repository in any empty directory of your choice (**note**: this is where files will be stored as well, so make sure it's on the storage drive you want).
```
git clone https://github.com/RaySteak/rayfts .
```
- Next, install the boost filesystem library. Example for Debian based systems:
```
sudo apt install libboost-filesystem-dev -y
```
- Finally, compile the executable for the server using `make server` or simply `make`.

## Using the command:
The server has 3 parameters: port, username and password, in this order.

An example is listed in the Makefile, which you can try with `make run_server`, which runs the server on port 42069, with credentials "a" and "a".
