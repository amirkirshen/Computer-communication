#include "ServerUtils.h"
#include <sstream>
#include <unordered_map>
#include <string>
using namespace std;

#define OK_MSG 200
#define CREATED_MSG 201
#define NO_CONTENT_MSG 204
#define NOT_FOUND_MSG 404
#define NOT_IMPLEMENTED_MSG 501
#define PRECONDITION_FAILED_MSG 412

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
		sockets[index].buffer[len + bytesRecv - 1] = '\0'; //add the null-terminating to make it a string
		cout << "HTTP Server: Recieved: " << bytesRecv << " bytes of \"" << &sockets[index].buffer[len] << "\message.\n\n";
		sockets[index].socketDataLen += bytesRecv;

		if (sockets[index].socketDataLen > 0)
		{
			sockets[index].send = SEND;

			if (strncmp(sockets[index].buffer, "GET", 3) == 0)
			{
				sockets[index].httpReq = GET;
				strcpy(sockets[index].buffer, &sockets[index].buffer[5]);
				sockets[index].socketDataLen = strlen(sockets[index].buffer);
				sockets[index].buffer[sockets[index].socketDataLen] = NULL;
				return;
			}
			else if (strncmp(sockets[index].buffer, "HEAD", 4) == 0)
			{
				sockets[index].httpReq = HEAD;
				strcpy(sockets[index].buffer, &sockets[index].buffer[6]);
				sockets[index].socketDataLen = strlen(sockets[index].buffer);
				sockets[index].buffer[sockets[index].socketDataLen] = NULL;
				return;
			}
			else if (strncmp(sockets[index].buffer, "PUT", 3) == 0)
			{
				sockets[index].httpReq = PUT;
				return;
			}
			else if (strncmp(sockets[index].buffer, "DELETE", 6) == 0)
			{
				sockets[index].httpReq = R_DELETE;
				return;
			}
			else if (strncmp(sockets[index].buffer, "TRACE", 5) == 0)
			{
				sockets[index].httpReq = TRACE;
				strcpy(sockets[index].buffer, &sockets[index].buffer[5]);
				sockets[index].socketDataLen = strlen(sockets[index].buffer);
				sockets[index].buffer[sockets[index].socketDataLen] = NULL;
				return;
			}
			else if (strncmp(sockets[index].buffer, "OPTIONS", 7) == 0)
			{
				sockets[index].httpReq = OPTIONS;
				strcpy(sockets[index].buffer, &sockets[index].buffer[9]);
				sockets[index].socketDataLen = strlen(sockets[index].buffer);
				sockets[index].buffer[sockets[index].socketDataLen] = NULL;
				return;
			}
			else if (strncmp(sockets[index].buffer, "POST", 4) == 0)
			{
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
	char* subBuff;
	char tempBuff[BUFF_SIZE], readBuff[BUFF_SIZE];
	string fullMessage, fileSizeString, fileAddress;
	ifstream inFile;
	time_t currentTime;
	time(&currentTime); // Get current time
	SOCKET msgSocket = sockets[index].id;
	sockets[index].prevActivity = time(0); // Reset activity

	switch (sockets[index].httpReq)
	{
	case HEAD:
	{
		subBuff = strtok(sockets[index].buffer, " ");
		fileAddress = "C:\\Temp\\en\\index.html"; // Default english file
		inFile.open(fileAddress);
		if (!inFile)
		{
			fullMessage = "HTTP/1.1 " + to_string(NOT_FOUND_MSG) + " Not Found ";
			fileSize = 0;
		}
		else
		{
			fullMessage = "HTTP/1.1 " + to_string(OK_MSG) + " OK ";
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
		string FileContent = "";
		subBuff = strtok(sockets[index].buffer, " ");
		fileAddress = "C:\\Temp\\"; // we redirect to default english file
		string langValue = get_query_param(subBuff, "lang" ); // search if there are query params
		if (langValue.empty()) // default - english page
		{
			fileAddress += "en";
		}
		else
		{			
			fileAddress += langValue;
		}

		fileAddress += '\\';
		fileAddress.append("index.html");
		inFile.open(fileAddress);
		if (!inFile)
		{
			fullMessage = "HTTP/1.1 " + to_string(NOT_FOUND_MSG) + " Not Found ";
			inFile.open("C:\\Temp\\error.html"); // In case an unsupported language requested - we open the error page
		}
		else
		{
			fullMessage = "HTTP/1.1 " + to_string(OK_MSG) + " OK ";
		}

		if (inFile)
		{
			// Read from file to temp buffer and get its length
			while (inFile.getline(readBuff, BUFF_SIZE))
			{
				FileContent += readBuff;
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
		fullMessage += FileContent; // Get content
		buffLen = fullMessage.size();
		strcpy(sendBuff, fullMessage.c_str());
		inFile.close();
		break;
	}

	case PUT:
	{
		char fileName[BUFF_SIZE];
		int res = put_request(index, fileName, sockets);
		switch (res)
		{
		case PRECONDITION_FAILED_MSG:
		{
			cout << "PUT " << fileName << "Failed";
			fullMessage = "HTTP/1.1 " + to_string(PRECONDITION_FAILED_MSG) + " Precondition failed \r\nDate: ";
			break;
		}

		case OK_MSG:
		{
			fullMessage = "HTTP/1.1 " + to_string(OK_MSG) + " OK \r\nDate: ";
			break;
		}

		case CREATED_MSG:
		{
			fullMessage = "HTTP/1.1 " + to_string(CREATED_MSG) + " Created \r\nDate: ";
			break;
		}

		case NO_CONTENT_MSG:
		{
			fullMessage = "HTTP/1.1 " + to_string(NO_CONTENT_MSG) + " No Content \r\nDate: ";
			break;
		}

		default:
		{
			fullMessage = "HTTP/1.1 " + to_string(NOT_IMPLEMENTED_MSG) + " Not Implemented \r\nDate: ";
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
		string fileName = get_query_param(sockets[index].buffer, "fileName");
		
		fileName = string{ "C:\\temp\\" } + fileName;
		fileName += string{ ".html" };
		if (remove(fileName.c_str()) != 0)
		{
			fullMessage = "HTTP/1.1 " + to_string(NO_CONTENT_MSG) + " No Content \r\nDate: "; // File deleted failed
		}
		else
		{
			fullMessage = "HTTP/1.1 " + to_string(OK_MSG) + " OK \r\nDate: "; // File deleted succesfully
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
		fullMessage = "HTTP/1.1 " + to_string(OK_MSG) + " OK \r\nContent-type: message/http\r\nDate: ";
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
		fullMessage = "HTTP/1.1 " + to_string(NO_CONTENT_MSG) + " No Content\r\nOptions: OPTIONS, GET, HEAD, POST, PUT, TRACE, DELETE\r\n";
		fullMessage += "Content-length: 0\r\n\r\n";
		buffLen = fullMessage.size();
		strcpy(sendBuff, fullMessage.c_str());
		break;
	}

	case POST:
	{
		fullMessage = "HTTP/1.1 " + to_string(OK_MSG) + "OK \r\nDate:";
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

	cout << "HTTP Server: Sent: " << bytesSent << "\\" << buffLen << " bytes of \n \"" << sendBuff << "\message.\n";
	sockets[index].send = IDLE;

	return true;
}

int put_request(int index, char* filename, SocketState* sockets)
{
	int buffLen = 0;
	int retCode = OK_MSG, content_len = 0;
	string str_buffer = { sockets[index].buffer };
	string value, file_name;

	// Get body length
	value = get_field_value(str_buffer, "Content-Length");
	content_len = stoi(value);
	// Get file name
	file_name = get_query_param(str_buffer, "fileName");
	// Get content
	value = get_field_value(str_buffer, "body");

	strcpy(filename, file_name.c_str());
	file_name = string{ filename };
	file_name = string{ "C:\\temp\\" } + file_name + string{".html"};
	fstream outPutFile;
	outPutFile.open(file_name);
	// File does not exict
	if (!outPutFile.good())
	{
		outPutFile.open(file_name.c_str(), ios::out);
		retCode = 201; // New file created
	}

	if (!outPutFile.good())
	{
		cout << "HTTP Server: Error writing file to local storage: " << WSAGetLastError() << endl;
		return PRECONDITION_FAILED_MSG; // Error opening file
	}

	if (value.empty())
	{
		retCode = NO_CONTENT_MSG; // No content
	}
	else
	{
		outPutFile << value;
	}

	outPutFile.close();
	return retCode;
}

string get_field_value(const string& request, const string& field) {
	// Use unordered_map to store key-value pairs
	 unordered_map<string, string> fields;

	// Split the request into lines
	string line, prev_line;
	stringstream request_stream(request);
	while (getline(request_stream, line)) {
		// Split the line into key-value pairs
		size_t sep = line.find(':');
		if (sep != string::npos) {
			string key = line.substr(0, sep);
			string value = line.substr(sep + 1);

			// Trim leading and trailing whitespace from key and value
			key = key.substr(key.find_first_not_of(" \t"));
			key = key.substr(0, key.find_last_not_of(" \t") + 1);
			value = value.substr(value.find_first_not_of(" \t"));
			value = value.substr(0, value.find_last_not_of(" \t") + 1);

			// Add key-value pair to the map
			value.pop_back();
			fields[key] = value;
		}
		prev_line = line;
	}

	// Look up the specified field in the map
	unordered_map<string, string>::iterator it = fields.find(field);
	if (it != fields.end()) {
		// Field found, return its value
		return it->second;
	}
	else {
		// Handle body request
		if (field.find("body") != string::npos)
			return line;
		// Field not found, return an empty string
		return "";
	}
}

string get_query_param(const string& request, const string& param)
{
	string line, paramValue = {""};
	size_t paramValueIndex, endOfparamIndex, paramIndex;
	stringstream request_stream(request);

	// Split the request into lines
	while (getline(request_stream, line)) {
		// Extract file name
		paramIndex = line.find(param);
		if (paramIndex != string::npos) {
			paramValueIndex = line.find("=", paramIndex) + 1;
			endOfparamIndex = line.find(" ", paramValueIndex);
			if (endOfparamIndex == string::npos) {
				endOfparamIndex = line.length();
			}

			paramValue = line.substr(paramValueIndex, endOfparamIndex - paramValueIndex);
			break;
		}
	}

	return paramValue;
}