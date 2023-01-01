#pragma once
#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <winsock2.h>
#include <string.h>
#include <time.h>
#include <string>
#include <fstream>
#include <iostream>
using namespace std;


// Server related literals and enums
constexpr int PORT = 27015;
constexpr int MAX_SOCKETS = 60;
constexpr int BUFF_SIZE = 1024;

struct SocketState
{
	SOCKET					id;
	enum eSocketStatus		recv;
	enum eSocketStatus		send;
	enum eRequestType		httpReq;
	char					buffer[BUFF_SIZE];
	time_t					prevActivity;
	int						socketDataLen;
};

enum eSocketStatus {
	EMPTY,
	LISTEN,
	RECEIVE,
	IDLE,
	SEND
};

enum eRequestType
{
	GET = 1,
	HEAD,
	PUT,
	POST,
	R_DELETE,
	TRACE,
	OPTIONS
};


// Server related methods
bool addSocket(SOCKET id, enum eSocketStatus what, SocketState* sockets, int& socketsCount);
void removeSocket(int index, SocketState* sockets, int& socketsCount);
void acceptConnection(int index, SocketState* sockets, int& socketsCount);
void receiveMessage(int index, SocketState* sockets, int& socketsCount);
bool sendMessage(int index, SocketState* sockets);
int PutRequest(int index, char* filename, SocketState* sockets);