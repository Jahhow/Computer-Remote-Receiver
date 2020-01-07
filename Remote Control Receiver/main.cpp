#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>

// Need to link with Ws2_32.lib
#pragma comment (lib, "Ws2_32.lib")
// #pragma comment (lib, "Mswsock.lib")

#define DEFAULT_BUFLEN 512
#define DEFAULT_PORT 1597
#define DEFAULT_PORT_STR "1597"

struct ServerHeader
{
	char Password[4] = { 'U', 'E', 'R', 'J' };
	int Version = htonl(1);
} serverHeader;

u_short boundPort;
bool connected = false;
HANDLE eventConnected;

DWORD WINAPI IpPortPrintingThread(LPVOID lpParam) {
	printf("IpPortPrintingThread\n");

	int iResult;
	addrinfo* result = NULL;
	addrinfo hints;
	HANDLE StdoutHandle = GetStdHandle(STD_OUTPUT_HANDLE);
	COORD coord = { 0,0 };

	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_INET;// IPv4
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = AI_PASSIVE;

	while (1) {
		if (connected) {
			system("cls");
			puts("\n  Connected.");
			iResult = SuspendThread(GetCurrentThread());
			if (iResult == -1) {
				printf("SuspendThread failed with error: %d\n", GetLastError());
				WSACleanup();
				ExitProcess(1);
			}
			connected = false;
		}
		else {
			// Resolve the server address and port
			iResult = getaddrinfo("", DEFAULT_PORT_STR, &hints, &result);
			if (iResult != 0) {
				printf("getaddrinfo failed with error: %d\n", iResult);
				WSACleanup();
				ExitProcess(1);
			}

			system("cls");
			//SetConsoleCursorPosition(StdoutHandle, coord);

			printf(
				"Port: %hu\n"
				"IP:\n", boundPort);
			char ipv4str[16];
			addrinfo* ai = result;
			u_int i = 1;
			while (ai) {
				inet_ntop(ai->ai_family, &((sockaddr_in*)ai->ai_addr)->sin_addr, ipv4str, 16);
				printf("  (%u) %s\n", i, ipv4str);
				ai = ai->ai_next;
				++i;
			}
			freeaddrinfo(result);
			//Sleep(1000);
			WaitForSingleObject(eventConnected, 1000);
		}
	}
}

int __cdecl main(void)
{
	WSADATA wsaData;
	int iResult;

	SOCKET ListenSocket = INVALID_SOCKET;
	SOCKET ClientSocket = INVALID_SOCKET;


	int iSendResult;
	char recvbuf[DEFAULT_BUFLEN];
	int recvbuflen = DEFAULT_BUFLEN;

	// Initialize Winsock
	iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != 0) {
		printf("WSAStartup failed with error: %d\n", iResult);
		return 1;
	}

	// Create a SOCKET for connecting to server
	ListenSocket = socket(AF_INET/*IPv4*/, SOCK_STREAM, IPPROTO_TCP);
	if (ListenSocket == INVALID_SOCKET) {
		printf("socket failed with error: %ld\n", WSAGetLastError());
		WSACleanup();
		return 1;
	}

	sockaddr_in sa;
	sa.sin_family = AF_INET;
	sa.sin_addr = in4addr_any;
	sa.sin_port = htons(DEFAULT_PORT);
	ZeroMemory(&sa.sin_zero, sizeof(sa.sin_zero));

	// Setup the TCP listening socket
	iResult = bind(ListenSocket, (sockaddr*)&sa, sizeof(sa));
	if (iResult == SOCKET_ERROR) {
		sa.sin_port = 0;// Auto pick available port

		iResult = bind(ListenSocket, (sockaddr*)&sa, sizeof(sa));
		if (iResult == SOCKET_ERROR) {
			printf("bind failed with error: %d\n", WSAGetLastError());
			closesocket(ListenSocket);
			WSACleanup();
			return 1;
		}
	}

	if (sa.sin_port == 0) {
		// Need to fetch port bound
		int size = sizeof(sa);
		iResult = getsockname(ListenSocket, (sockaddr*)&sa, &size);
		if (iResult == SOCKET_ERROR) {
			printf("getsockname failed with error: %d\n", WSAGetLastError());
			closesocket(ListenSocket);
			WSACleanup();
			return 1;
		}
	}
	boundPort = ntohs(sa.sin_port);

	iResult = listen(ListenSocket, SOMAXCONN);
	if (iResult == SOCKET_ERROR) {
		printf("listen failed with error: %d\n", WSAGetLastError());
		closesocket(ListenSocket);
		WSACleanup();
		return 1;
	}

	eventConnected = CreateEvent(0, false, 0, NULL);
	if (eventConnected == NULL) {
		closesocket(ListenSocket);
		WSACleanup();
		return 1;
	}

	DWORD MyThreadFunctionID;
	HANDLE handleIpPortPrintingThread = CreateThread(
		NULL,                   // default security attributes
		0,                      // use default stack size  
		IpPortPrintingThread,       // thread function name
		NULL,                   // argument to thread function 
		0,                      // use default creation flags 
		&MyThreadFunctionID);   // returns the thread identifier 

	// Check the return value for success.
	// If CreateThread fails, terminate execution. 
	// This will automatically clean up threads and memory. 
	if (handleIpPortPrintingThread == NULL)
	{
		puts("CreateThread IpPortPrintingThread failed");
		closesocket(ListenSocket);
		WSACleanup();
		return 1;
	}

	while (1) {
		// Accept a client socket
		ClientSocket = accept(ListenSocket, NULL, NULL);
		if (ClientSocket == INVALID_SOCKET) {
			printf("accept failed with error: %d\n", WSAGetLastError());
			break;
		}

		iSendResult = send(ClientSocket, (const char*)&serverHeader, sizeof(serverHeader), 0);
		if (iSendResult == SOCKET_ERROR) {
			printf("send failed with error: %d\n", WSAGetLastError());
			closesocket(ClientSocket);
			continue;
		}

		connected = true;
		iResult = SetEvent(eventConnected);
		if (iResult == 0) {
			printf("SetEvent(eventConnected) failed with error: %d\n", GetLastError());
			closesocket(ClientSocket);
			break;
		}

		// Receive until the peer shuts down the connection
		do {
			iResult = recv(ClientSocket, recvbuf, recvbuflen, 0);
			if (iResult > 0) {
				printf("Bytes received: %d\n", iResult);

				// Echo the buffer back to the sender
				/*iSendResult = send(ClientSocket, recvbuf, iResult, 0);
				if (iSendResult == SOCKET_ERROR) {
					printf("send failed with error: %d\n", WSAGetLastError());
					closesocket(ClientSocket);
					WSACleanup();
					return 1;
				}
				printf("Bytes sent: %d\n", iSendResult);*/
			}
			else if (iResult == 0)
				printf("Connection closing...\n");
			else {
				printf("recv failed with error: %d\n", WSAGetLastError());
				closesocket(ListenSocket);
				closesocket(ClientSocket);
				WSACleanup();
				ExitProcess(1);
			}

		} while (iResult > 0);

		// shutdown the connection since we're done
		iResult = shutdown(ClientSocket, SD_SEND);
		if (iResult == SOCKET_ERROR) {
			printf("shutdown failed with error: %d\n", WSAGetLastError());
			closesocket(ClientSocket);
			break;
		}

		// cleanup
		closesocket(ClientSocket);
		connected = false;
		ResetEvent(eventConnected);
		ResumeThread(handleIpPortPrintingThread);
	}

	// No longer need server socket
	closesocket(ListenSocket);
	WSACleanup();
	ExitProcess(1);
}