#include "ServerUtils.h"

bool addSocket(SOCKET id, enum eSocketStatus what, SocketState* sockets, int& socketsCount)
{
	for (int i = 0; i < MAX_SOCKETS; i++)
	{
		if (sockets[i].recv == EMPTY)
		{
			sockets[i].id = id;
			sockets[i].recv = what;
			sockets[i].send = IDLE;
			sockets[i].prevActivity = time(0);
			sockets[i].socketDataLen = 0;
			socketsCount++;
			return true;
		}
	}

	return false;
}

void removeSocket(int index, SocketState* sockets, int& socketsCount)
{
	sockets[index].recv = EMPTY;
	sockets[index].send = EMPTY;
	sockets[index].prevActivity = 0;
	socketsCount--;
	cout << "The socket number " << index << " has been removed" << endl;
}

void acceptConnection(int index, SocketState* sockets, int& socketsCount)
{
	SOCKET id = sockets[index].id;
	sockets[index].prevActivity = time(0);
	struct sockaddr_in from;		// Address of sending partner
	int fromLen = sizeof(from);

	SOCKET msgSocket = accept(id, (struct sockaddr*)&from, &fromLen);
	if (INVALID_SOCKET == msgSocket)
	{
		cout << "HTTP Server: Error at accept(): " << WSAGetLastError() << endl;
		return;
	}

	cout << "HTTP Server: Client " << inet_ntoa(from.sin_addr) << ":" << ntohs(from.sin_port) << " is connected." << endl;

	// Set the socket to be in non-blocking mode.
	unsigned long flag = 1;
	if (ioctlsocket(msgSocket, FIONBIO, &flag) != 0)
	{
		cout << "HTTP Server: Error at ioctlsocket(): " << WSAGetLastError() << endl;
	}

	if (addSocket(msgSocket, RECEIVE, sockets, socketsCount) == false)
	{
		cout << "\t\tToo many connections, dropped!\n";
		closesocket(id);
	}

	return;
}

void receiveMessage(int index, SocketState* sockets, int& socketsCount)
{
	SOCKET msgSocket = sockets[index].id;

	int len = sockets[index].socketDataLen;
	int bytesRecv = recv(msgSocket, &sockets[index].buffer[len], sizeof(sockets[index].buffer) - len, 0);

	if (SOCKET_ERROR == bytesRecv)
	{
		cout << "HTTP Server: Error at recv(): " << WSAGetLastError() << endl;
		closesocket(msgSocket);
		removeSocket(index, sockets, socketsCount);
		return;
	}
	if (bytesRecv == 0)
	{
		closesocket(msgSocket);
		removeSocket(index, sockets, socketsCount);
		return;
	}
	else
	{
		sockets[index].buffer[len + bytesRecv] = '\0'; //add the null-terminating to make it a string
		cout << "HTTP Server: Recieved: " << bytesRecv << " bytes of \"" << &sockets[index].buffer[len] << "\" message.\n";
		sockets[index].socketDataLen += bytesRecv;

		if (sockets[index].socketDataLen > 0)
		{
			if (strncmp(sockets[index].buffer, "GET", 3) == 0)
			{
				sockets[index].send = SEND;
				sockets[index].httpReq = GET;
				strcpy(sockets[index].buffer, &sockets[index].buffer[5]);
				sockets[index].socketDataLen = strlen(sockets[index].buffer);
				sockets[index].buffer[sockets[index].socketDataLen] = NULL;
				return;
			}
			else if (strncmp(sockets[index].buffer, "HEAD", 4) == 0)
			{
				sockets[index].send = SEND;
				sockets[index].httpReq = HEAD;
				strcpy(sockets[index].buffer, &sockets[index].buffer[6]);
				sockets[index].socketDataLen = strlen(sockets[index].buffer);
				sockets[index].buffer[sockets[index].socketDataLen] = NULL;
				return;
			}
			else if (strncmp(sockets[index].buffer, "PUT", 3) == 0)
			{
				sockets[index].send = SEND;
				sockets[index].httpReq = PUT;
				return;
			}
			else if (strncmp(sockets[index].buffer, "DELETE", 6) == 0)
			{
				sockets[index].send = SEND;
				sockets[index].httpReq = R_DELETE;
				return;
			}
			else if (strncmp(sockets[index].buffer, "TRACE", 5) == 0)
			{
				sockets[index].send = SEND;
				sockets[index].httpReq = TRACE;
				strcpy(sockets[index].buffer, &sockets[index].buffer[5]);
				sockets[index].socketDataLen = strlen(sockets[index].buffer);
				sockets[index].buffer[sockets[index].socketDataLen] = NULL;
				return;
			}
			else if (strncmp(sockets[index].buffer, "OPTIONS", 7) == 0)
			{
				sockets[index].send = SEND;
				sockets[index].httpReq = OPTIONS;
				strcpy(sockets[index].buffer, &sockets[index].buffer[9]);
				sockets[index].socketDataLen = strlen(sockets[index].buffer);
				sockets[index].buffer[sockets[index].socketDataLen] = NULL;
				return;
			}
			else if (strncmp(sockets[index].buffer, "POST", 4) == 0)
			{
				sockets[index].send = SEND;
				sockets[index].httpReq = POST;
				strcpy(sockets[index].buffer, &sockets[index].buffer[6]);
				sockets[index].socketDataLen = strlen(sockets[index].buffer);
				sockets[index].buffer[sockets[index].socketDataLen] = NULL;
				return;
			}
		}
	}
}


bool sendMessage(int index, SocketState* sockets)
{
	int bytesSent = 0, buffLen = 0, fileSize = 0;
	char sendBuff[BUFF_SIZE];
	char* tempFromTok;
	char tempBuff[BUFF_SIZE], readBuff[BUFF_SIZE];
	string fullMessage, fileSizeString, innerAddress;
	ifstream inFile;
	time_t currentTime;
	time(&currentTime); // Get current time
	SOCKET msgSocket = sockets[index].id;
	sockets[index].prevActivity = time(0); // Reset activity

	switch (sockets[index].httpReq)
	{
	case HEAD:
	{
		tempFromTok = strtok(sockets[index].buffer, " ");
		innerAddress = "C:\\Temp\\en\\index.html"; // we redirect to default english file
		inFile.open(innerAddress);
		if (!inFile)
		{
			fullMessage = "HTTP/1.1 404 Not Found ";
			fileSize = 0;
		}
		else
		{
			fullMessage = "HTTP/1.1 200 OK ";
			inFile.seekg(0, ios::end);
			fileSize = inFile.tellg(); // get length of content in file
		}

		fullMessage += "\r\nContent-type: text/html";
		fullMessage += "\r\nDate:";
		fullMessage += ctime(&currentTime);
		fullMessage += "Content-length: ";
		fileSizeString = to_string(fileSize);
		fullMessage += fileSizeString;
		fullMessage += "\r\n\r\n";
		buffLen = fullMessage.size();
		strcpy(sendBuff, fullMessage.c_str());
		inFile.close();
		break;
	}

	case GET:
	{
		string tempStringFromFile = "";
		tempFromTok = strtok(sockets[index].buffer, " ");
		innerAddress = "C:\\Temp\\"; // we redirect to default english file
		char* langPtr = strchr(tempFromTok, '?'); // search if there are query params
		if (langPtr == NULL) // default - english page
		{
			innerAddress += "en";
		}
		else
		{
			langPtr += 6; // skip to language param ('?lang=' is 6 chars)
			for (int i = 0; i < 2; ++i, langPtr++) // extract language from parameters
				innerAddress += *langPtr;
		}

		innerAddress += '\\';
		tempFromTok = strtok(tempFromTok, "?");
		innerAddress.append(tempFromTok);
		inFile.open(innerAddress);
		if (!inFile)
		{
			fullMessage = "HTTP/1.1 404 Not Found ";
			inFile.open("C:\\Temp\\error.html"); // In case an unsupported language requested - we open the error page
		}
		else
		{
			fullMessage = "HTTP/1.1 200 OK ";
		}

		if (inFile)
		{
			// Read from file to temp buffer and get its length
			while (inFile.getline(readBuff, BUFF_SIZE))
			{
				tempStringFromFile += readBuff;
				fileSize += strlen(readBuff);
			}
		}

		fullMessage += "\r\nContent-type: text/html";
		fullMessage += "\r\nDate:";
		fullMessage += ctime(&currentTime);
		fullMessage += "Content-length: ";
		fileSizeString = to_string(fileSize);
		fullMessage += fileSizeString;
		fullMessage += "\r\n\r\n";
		fullMessage += tempStringFromFile; // Get content
		buffLen = fullMessage.size();
		strcpy(sendBuff, fullMessage.c_str());
		inFile.close();
		break;
	}

	case PUT:
	{
		char fileName[BUFF_SIZE];
		int returnCode = PutRequest(index, fileName, sockets);
		switch (returnCode)
		{
		case 0:
		{
			cout << "PUT " << fileName << "Failed";
			fullMessage = "HTTP/1.1 412 Precondition failed \r\nDate: ";
			break;
		}

		case 200:
		{
			fullMessage = "HTTP/1.1 200 OK \r\nDate: ";
			break;
		}

		case 201:
		{
			fullMessage = "HTTP/1.1 201 Created \r\nDate: ";
			break;
		}

		case 204:
		{
			fullMessage = "HTTP/1.1 204 No Content \r\nDate: ";
			break;
		}

		default:
		{
			fullMessage = "HTTP/1.1 501 Not Implemented \r\nDate: ";
			break;
		}
		}

		fullMessage += ctime(&currentTime);
		fullMessage += "Content-length: ";
		fileSizeString = to_string(fileSize);
		fullMessage += fileSizeString;
		fullMessage += "\r\n\r\n";
		buffLen = fullMessage.size();
		strcpy(sendBuff, fullMessage.c_str());
		break;
	}

	case R_DELETE:
	{
		strtok(&sockets[index].buffer[8], " ");
		strcpy(tempBuff, &sockets[index].buffer[8]);
		if (remove(tempBuff) != 0)
		{
			fullMessage = "HTTP/1.1 204 No Content \r\nDate: "; // We treat 204 code as a case where delete wasn't successful
		}
		else
		{
			fullMessage = "HTTP/1.1 200 OK \r\nDate: "; // File deleted succesfully
		}

		fullMessage += ctime(&currentTime);
		fullMessage += "Content-length: ";
		fileSizeString = to_string(fileSize);
		fullMessage += fileSizeString;
		fullMessage += "\r\n\r\n";
		buffLen = fullMessage.size();
		strcpy(sendBuff, fullMessage.c_str());
		break;
	}

	case TRACE:
	{
		fileSize = strlen("TRACE");
		fileSize += strlen(sockets[index].buffer);
		fullMessage = "HTTP/1.1 200 OK \r\nContent-type: message/http\r\nDate: ";
		fullMessage += ctime(&currentTime);
		fullMessage += "Content-length: ";
		fileSizeString = to_string(fileSize);
		fullMessage += fileSizeString;
		fullMessage += "\r\n\r\n";
		fullMessage += "TRACE";
		fullMessage += sockets[index].buffer;
		buffLen = fullMessage.size();
		strcpy(sendBuff, fullMessage.c_str());
		break;
	}

	case OPTIONS:
	{
		fullMessage = "HTTP/1.1 204 No Content\r\nAllow: OPTIONS, GET, HEAD, POST, PUT, TRACE, DELETE\r\n";
		fullMessage += "Content-length: 0\r\n\r\n";
		buffLen = fullMessage.size();
		strcpy(sendBuff, fullMessage.c_str());
		break;
	}

	case POST:
	{
		fullMessage = "HTTP/1.1 200 OK \r\nDate:";
		fullMessage += ctime(&currentTime);
		fullMessage += "Content-length: 0\r\n\r\n";
		char* messagePtr = strstr(sockets[index].buffer, "\r\n\r\n"); // Skip to body content
		cout << "==================\nMessage received\n\n==================\n"
			<< messagePtr + 4 << "\n==================\n\n";
		buffLen = fullMessage.size();
		strcpy(sendBuff, fullMessage.c_str());
		break;
	}
	}

	bytesSent = send(msgSocket, sendBuff, buffLen, 0);
	memset(sockets[index].buffer, 0, BUFF_SIZE);
	sockets[index].socketDataLen = 0;
	if (SOCKET_ERROR == bytesSent)
	{
		cout << "HTTP Server: Error at send(): " << WSAGetLastError() << endl;
		return false;
	}

	cout << "HTTP Server: Sent: " << bytesSent << "\\" << buffLen << " bytes of \n \"" << sendBuff << "\"\message.\n";
	sockets[index].send = IDLE;
	return true;
}

int PutRequest(int index, char* filename, SocketState* sockets)
{
	char* tempPtr = 0;
	int buffLen = 0;
	int retCode = 200; // 'OK' code

	tempPtr = strtok(&sockets[index].buffer[5], " ");
	strcpy(filename, &sockets[index].buffer[5]);
	tempPtr = strtok(nullptr, ":");
	tempPtr = strtok(nullptr, ":");
	tempPtr = strtok(nullptr, " ");
	sscanf(tempPtr, "%d", &buffLen);

	fstream outPutFile;
	outPutFile.open(filename);

	if (!outPutFile.good())
	{
		outPutFile.open(filename, ios::out);
		retCode = 201; // New file created
	}

	if (!outPutFile.good())
	{
		cout << "HTTP Server: Error writing file to local storage: " << WSAGetLastError() << endl;
		return 0; // Error opening file
	}

	tempPtr = strtok(NULL, "\r\n\r\n");

	if (tempPtr == 0)
	{
		retCode = 204; // No content
	}
	else
	{
		while (*tempPtr != '\0') // Write to file
		{
			outPutFile << tempPtr;
			tempPtr += (strlen(tempPtr) + 1);
		}
	}

	outPutFile.close();
	return retCode;
}