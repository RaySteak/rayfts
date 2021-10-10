#include "remote_shutdown.h"

#if __has_include("../common_utils.h")
#include "../common_utils.h"
#else
#include "common_utils.h"
#endif

#ifndef _WIN32
#include <spawn.h>
#endif

int main()
{
	if (strlen(REMOTE_SHUTDOWN_KEYWORD) > BUFLEN)
	{
		std::cerr << "Keyword size too long\n";
		exit(1);
	}
	char buffer[BUFLEN + 1];
	sockaddr_in serv_addr, cli_addr;
	socklen_t socklen = sizeof(sockaddr_in);
#ifndef _WIN32
	int n, i, ret;
	int listenfd, newsockfd;
	socklen_t socklen;
	listenfd = socket(AF_INET, SOCK_DGRAM, 0);
	DIE(listenfd < 0, "socket");
#else
	WSADATA wsaData;
	int iResult;
	SOCKET ListenSocket = INVALID_SOCKET;
	struct addrinfo hints;

	int iSendResult;
	int buflen = BUFLEN;

	// Initialize Winsock
	iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != 0)
	{
		printf("WSAStartup failed with error: %d\n", iResult);
		return 1;
	}
	// Create a SOCKET for connecting to server
	ListenSocket = socket(AF_INET, SOCK_DGRAM, 0);
	if (ListenSocket == INVALID_SOCKET)
	{
		printf("socket failed with error: %ld\n", WSAGetLastError());
		WSACleanup();
		return 1;
	}
#endif
	memset((char *)&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(REMOTE_SHUTDOWN_PORT);
	serv_addr.sin_addr.s_addr = INADDR_ANY;
#ifndef _WIN32

	int option = 1;
	ret = setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(int));
	DIE(ret < 0, "reuse_addr");

	ret = bind(listenfd, (sockaddr *)&serv_addr, sizeof(sockaddr));
	DIE(ret < 0, "bind");
#else
	// Setup the UDP listening socket
	iResult = bind(ListenSocket, (sockaddr *)&serv_addr, sizeof(serv_addr));
	if (iResult == SOCKET_ERROR)
	{
		printf("bind failed with error: %d\n", WSAGetLastError());
		closesocket(ListenSocket);
		WSACleanup();
		return 1;
	}
#endif

#ifndef _WIN32
	n = recvfrom(newsockfd, buffer, strlen(REMOTE_SHUTDOWN_KEYWORD), 0, (sockaddr *)&cli_addr, &socklen); //TODO: change to recv exactly;
	buffer[n] = 0;
#else
	iResult = recvfrom(ListenSocket, buffer, strlen(REMOTE_SHUTDOWN_KEYWORD), 0, (sockaddr *)&cli_addr, &socklen);
	buffer[iResult] = 0;
#endif
	if (!strcmp(buffer, REMOTE_SHUTDOWN_KEYWORD))
	{
#ifdef _WIN32
		system("shutdown /s /t 0");
#else
		char argv0[] = "/usr/sbin/poweroff";
		char *const argv[] = {argv0, NULL};
		pid_t pid;
		posix_spawn(&pid, argv0, NULL, NULL, argv, NULL);
#endif
	}
	// if something else than the keyword is sent, maybe another application wants to use this port,
	// so we exit the program to allow it. Recommended course of action is to change the port macro in remote_shutdown.h
	return 0;
}