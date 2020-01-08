#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>
#include "SCS1.h"

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

class Msg {
public:
	static const char
		KeyboardScanCode = 0,
		KeyboardScanCodeCombination = 1,
		MoveMouse = 2,
		MouseLeftClick = 3,
		MouseLeftDown = 4,
		MouseLeftUp = 5,
		MouseRightClick = 6,
		MouseRightDown = 7,
		MouseRightUp = 8,
		MouseWheel = 9,
		InputText = 10;
};

class ButtonAction {
public:
	static const char
		Click = 0,
		Down = 1,
		Up = 2;
};

class InputTextMode {
public:
	static const char
		SendInput = 0,
		Paste = 1;
};

u_short boundPort;
bool connected = false;
HANDLE eventConnected;

DWORD WINAPI IpPortPrintingThread(LPVOID lpParam) {
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
			puts(
				"\n"
				"  [IP]");
			char ipv4str[16];
			addrinfo* ai = result;
			u_int i = 1;
			while (ai) {
				inet_ntop(ai->ai_family, &((sockaddr_in*)ai->ai_addr)->sin_addr, ipv4str, 16);
				printf("       (%u)  %s\n", i, ipv4str);
				ai = ai->ai_next;
				++i;
			}
			printf(
				"\n"
				"  [Port] =      %hu\n", boundPort);
			freeaddrinfo(result);
			//Sleep(1000);
			WaitForSingleObject(eventConnected, 1000);
		}
	}
}


int scanCodeArraySize = 8;
short* scanCodeArray = (short*)malloc(scanCodeArraySize << 1);

// I use 0 to indicates that RepeatKeyStrokeThread is currently not repeating key stroke.
// Take care of this variable. For the mutex 'scanCodeArrayMutex' relies on this number to work properly.
int numScanCodesFilled = 0;
HANDLE scanCodeArrayMutex = CreateMutex(NULL, true, NULL);

DWORD WINAPI RepeatKeyStrokeThread(LPVOID lpParam) {

	INPUT input;
	input.type = INPUT_KEYBOARD;
	input.ki.dwFlags = KEYEVENTF_SCANCODE;
	input.ki.time = 0;

	while (1) {
		WaitForSingleObject(scanCodeArrayMutex, INFINITE);
		for (int i = 0; i < numScanCodesFilled; ++i) {
			input.ki.wScan = scanCodeArray[i];
			SendInput(1, &input, sizeof(input));
		}
		ReleaseMutex(scanCodeArrayMutex);
		Sleep(30);
	}
}

void ShowLastErrorMessage(DWORD errCode, LPCTSTR errTitle)
{
	LPTSTR errorText = NULL;

	FormatMessage(
		FORMAT_MESSAGE_FROM_SYSTEM |
		FORMAT_MESSAGE_ALLOCATE_BUFFER |
		FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		errCode,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&errorText,
		0,
		NULL);

	if (NULL != errorText)
	{
		wprintf(TEXT("%s:\nError Code: %u\n%s\n"), errTitle, errCode, errorText);

		LocalFree(errorText);
		errorText = NULL;
	}
}

BOOL SetWinSta0Desktop(LPCTSTR szDesktopName)
{
	BOOL bSuccess = FALSE;

	HWINSTA hWinSta0 = OpenWindowStation(TEXT("WinSta0"), FALSE, MAXIMUM_ALLOWED);
	if (NULL == hWinSta0) { ShowLastErrorMessage(GetLastError(), TEXT("OpenWindowStation")); }

	bSuccess = SetProcessWindowStation(hWinSta0);
	if (!bSuccess) { ShowLastErrorMessage(GetLastError(), TEXT("SetProcessWindowStation")); }

	HDESK hDesk = OpenDesktop(szDesktopName, 0, FALSE, MAXIMUM_ALLOWED);
	if (NULL == hDesk) { ShowLastErrorMessage(GetLastError(), TEXT("OpenDesktop")); }

	bSuccess = SetThreadDesktop(hDesk);
	if (!bSuccess) { ShowLastErrorMessage(GetLastError(), TEXT("SetThreadDesktop")); }

	if (hDesk != NULL) { CloseDesktop(hDesk); }
	if (hWinSta0 != NULL) { CloseWindowStation(hWinSta0); }
	return bSuccess;
}

//int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
//	PSTR lpCmdLine, INT nCmdShow)
int main()
{
	/*if (!SetWinSta0Desktop(TEXT("winlogon"))) {
		system("pause");
		return 1;
	}*/
	DWORD prev_mode;
	HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
	HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
	CONSOLE_CURSOR_INFO cursorInfo = { 1 };
	SMALL_RECT rect = { 0,0,32,10 };
	COORD screenBufSize = { 33,11 };
	SetConsoleTextAttribute(hOut, BACKGROUND_BLUE | BACKGROUND_GREEN | BACKGROUND_RED | BACKGROUND_INTENSITY);
	SetConsoleWindowInfo(hOut, TRUE, &rect);
	SetConsoleScreenBufferSize(hOut, screenBufSize);
	GetConsoleMode(hIn, &prev_mode);
	SetConsoleMode(hIn, ENABLE_EXTENDED_FLAGS | (prev_mode & ~ENABLE_QUICK_EDIT_MODE));
	SetConsoleCursorInfo(hOut, &cursorInfo);


	if (scanCodeArray == NULL || scanCodeArrayMutex == NULL) {
		return 1;
	}

	WSADATA wsaData;
	int iResult;

	SOCKET ListenSocket = INVALID_SOCKET;
	SOCKET ClientSocket = INVALID_SOCKET;

	int iSendResult;
	char recvbuf[DEFAULT_BUFLEN];

	INPUT mouseInput;
	mouseInput.type = INPUT_MOUSE;
	ZeroMemory(&mouseInput.mi, sizeof(mouseInput.mi));

	HWND window = GetConsoleWindow();

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

	iResult = listen(ListenSocket, 2);
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

	HANDLE handleIpPortPrintingThread = CreateThread(
		NULL,                   // default security attributes
		0,                      // use default stack size  
		IpPortPrintingThread,   // thread function name
		NULL,                   // argument to thread function 
		0,                      // use default creation flags 
		NULL);                  // returns the thread identifier 
	if (handleIpPortPrintingThread == NULL)
	{
		puts("CreateThread IpPortPrintingThread failed");
		closesocket(ListenSocket);
		WSACleanup();
		return 1;
	}

	HANDLE handleRepeatKeyStrokeThread = CreateThread(
		NULL,                   // default security attributes
		0,                      // use default stack size  
		RepeatKeyStrokeThread,  // thread function name
		NULL,                   // argument to thread function 
		0,                      // use default creation flags 
		NULL);                  // returns the thread identifier 
	if (handleRepeatKeyStrokeThread == NULL)
	{
		puts("CreateThread handleRepeatKeyStrokeThread failed");
		closesocket(ListenSocket);
		WSACleanup();
		ExitProcess(1);
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

		iResult = recv(ClientSocket, recvbuf, 4, MSG_WAITALL);
		if (iResult != 4) {
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
			// Receive a Msg constant
			iResult = recv(ClientSocket, recvbuf, 1, MSG_WAITALL);
			if (iResult > 0) {
				switch (recvbuf[0])
				{
				case Msg::KeyboardScanCode: {
					iResult = recv(ClientSocket, recvbuf, 3, MSG_WAITALL);
					if (iResult != 3) {
						iResult = 0;
						break;
					}

					u_short scanCode = ntohs(*(u_short*)recvbuf);
					char buttonAction = recvbuf[2];

					INPUT input;
					input.type = INPUT_KEYBOARD;
					input.ki.dwFlags = KEYEVENTF_SCANCODE;
					input.ki.wScan = scanCode;
					input.ki.time = 0;

					switch (buttonAction)
					{
					case ButtonAction::Click: {
						SendInput(1, &input, sizeof(input));
						input.ki.dwFlags |= KEYEVENTF_KEYUP;
						SendInput(1, &input, sizeof(input));
						break; }
					case ButtonAction::Down: {
						if (numScanCodesFilled != 0)
							WaitForSingleObject(scanCodeArrayMutex, INFINITE);
						SendInput(1, &input, sizeof(input));
						scanCodeArray[0] = scanCode;
						numScanCodesFilled = 1;
						ReleaseMutex(scanCodeArrayMutex);
						break; }
					case ButtonAction::Up: {
						if (numScanCodesFilled != 0) {
							WaitForSingleObject(scanCodeArrayMutex, INFINITE);
							numScanCodesFilled = 0;
						}
						input.ki.dwFlags |= KEYEVENTF_KEYUP;
						SendInput(1, &input, sizeof(input));
						break; }
					default:
						break;
					}
					break; }
				case Msg::KeyboardScanCodeCombination: {
					iResult = recv(ClientSocket, recvbuf, 2, MSG_WAITALL);
					if (iResult != 2) {
						iResult = 0;
						break;
					}

					char buttonAction = recvbuf[0];
					u_char scanCodeByteLen = recvbuf[1];

					iResult = recv(ClientSocket, recvbuf, scanCodeByteLen, MSG_WAITALL);
					if (iResult != scanCodeByteLen) {
						iResult = 0;
						break;
					}

					INPUT input;
					input.type = INPUT_KEYBOARD;
					input.ki.dwFlags = KEYEVENTF_SCANCODE;
					input.ki.time = 0;

					int numScanCodes = scanCodeByteLen >> 1;
					u_short* scanCodes = (u_short*)recvbuf;
					for (int i = 0; i < numScanCodes; ++i)
						scanCodes[i] = ntohs(scanCodes[i]);
					switch (buttonAction)
					{
					case ButtonAction::Click: {
						for (int i = 0; i < numScanCodes; ++i) {
							input.ki.wScan = scanCodes[i];
							SendInput(1, &input, sizeof(input));
						}

						input.ki.dwFlags |= KEYEVENTF_KEYUP;

						for (int i = 0; i < numScanCodes; ++i) {
							input.ki.wScan = scanCodes[i];
							SendInput(1, &input, sizeof(input));
						}
						break; }
					case ButtonAction::Down: {
						if (numScanCodesFilled != 0)
							WaitForSingleObject(scanCodeArrayMutex, INFINITE);

						if (numScanCodes > scanCodeArraySize) {
							void* newScanCodeArray = realloc(scanCodeArray, scanCodeByteLen);
							if (newScanCodeArray == NULL) {
								iResult = 0;
								break;
							}
							scanCodeArraySize = numScanCodes;
							scanCodeArray = (short*)newScanCodeArray;
						}
						/*for (int i = 0; i < numScanCodes; ++i) {
							input.ki.wScan = scanCodes[i];
							SendInput(1, &input, sizeof(input));
						}*/
						memcpy(scanCodeArray, scanCodes, scanCodeByteLen);
						numScanCodesFilled = numScanCodes;
						ReleaseMutex(scanCodeArrayMutex);
						break; }
					case ButtonAction::Up: {
						if (numScanCodesFilled != 0) {
							WaitForSingleObject(scanCodeArrayMutex, INFINITE);
							numScanCodesFilled = 0;
						}
						input.ki.dwFlags |= KEYEVENTF_KEYUP;
						for (int i = 0; i < numScanCodes; ++i) {
							input.ki.wScan = scanCodes[i];
							SendInput(1, &input, sizeof(input));
						}
						break; }
					default:
						break;
					}
					break; }
				case Msg::MoveMouse: {
					iResult = recv(ClientSocket, recvbuf, 4, MSG_WAITALL);
					if (iResult != 4) {
						iResult = 0;
						break;
					}

					short* delta = (short*)recvbuf;

					mouseInput.mi.dx = (short)ntohs(delta[0]);
					mouseInput.mi.dy = (short)ntohs(delta[1]);
					mouseInput.mi.dwFlags = MOUSEEVENTF_MOVE;
					SendInput(1, &mouseInput, sizeof(mouseInput));
					// No reset for dx,dy seems to be ok for other mouse events.
					break; }
				case Msg::MouseLeftClick: {
					mouseInput.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
					SendInput(1, &mouseInput, sizeof(mouseInput));
					mouseInput.mi.dwFlags = MOUSEEVENTF_LEFTUP;
					SendInput(1, &mouseInput, sizeof(mouseInput));
					break; }
				case Msg::MouseLeftDown: {
					mouseInput.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
					SendInput(1, &mouseInput, sizeof(mouseInput));
					break; }
				case Msg::MouseLeftUp: {
					mouseInput.mi.dwFlags = MOUSEEVENTF_LEFTUP;
					SendInput(1, &mouseInput, sizeof(mouseInput));
					break; }
				case Msg::MouseRightClick: {
					mouseInput.mi.dwFlags = MOUSEEVENTF_RIGHTDOWN;
					SendInput(1, &mouseInput, sizeof(mouseInput));
					mouseInput.mi.dwFlags = MOUSEEVENTF_RIGHTUP;
					SendInput(1, &mouseInput, sizeof(mouseInput));
					break; }
				case Msg::MouseRightDown: {
					mouseInput.mi.dwFlags = MOUSEEVENTF_RIGHTDOWN;
					SendInput(1, &mouseInput, sizeof(mouseInput));
					break; }
				case Msg::MouseRightUp: {
					mouseInput.mi.dwFlags = MOUSEEVENTF_RIGHTUP;
					SendInput(1, &mouseInput, sizeof(mouseInput));
					break; }
				case Msg::MouseWheel: {
					iResult = recv(ClientSocket, recvbuf, 4, MSG_WAITALL);
					if (iResult != 4) {
						iResult = 0;
						break;
					}

					mouseInput.mi.dwFlags = MOUSEEVENTF_WHEEL;
					mouseInput.mi.mouseData = ntohl(*(int*)recvbuf);
					SendInput(1, &mouseInput, sizeof(mouseInput));

					// The document says it should be 0 for non-wheel or X-button mouse events.
					//   https://docs.microsoft.com/en-us/windows/win32/api/winuser/ns-winuser-mouseinput#members
					// But for now its actually working fine without a reset.
					// mouseInput.mi.mouseData = 0;
					break; }
				case Msg::InputText: {
					iResult = recv(ClientSocket, recvbuf, 6, MSG_WAITALL);
					if (iResult != 6) {
						iResult = 0;
						break;
					}

					int textByteLen = ntohl(*(int*)recvbuf);
					int numWchars = textByteLen >> 1;
					char inputTextMode = recvbuf[4];
					bool hold = recvbuf[5];

					HANDLE textMemHandle = GlobalAlloc(GMEM_MOVEABLE, textByteLen + 2);
					if (textMemHandle == NULL)
					{
						iResult = 0;// Close connection
						break;
					}
					// Lock the handle and copy the text to the buffer. 
					char16_t* wstr = (char16_t*)GlobalLock(textMemHandle);
					if (wstr == NULL) {
						iResult = 0;
						GlobalFree(textMemHandle);
						break;
					}
					iResult = recv(ClientSocket, (char*)wstr, textByteLen, MSG_WAITALL);
					if (iResult != textByteLen) {
						iResult = 0;
						GlobalFree(textMemHandle); // This can free a locked memory.
						break;
					}
					switch (inputTextMode)
					{
					case InputTextMode::SendInput: {
						INPUT input;
						input.type = INPUT_KEYBOARD;
						input.ki.wVk = 0;
						input.ki.dwFlags = KEYEVENTF_UNICODE;
						input.ki.time = 0;

						for (int i = 0; i < numWchars; ++i) {
							input.ki.wScan = wstr[i];
							SendInput(1, &input, sizeof(input));
						}
						GlobalFree(textMemHandle);
						break; }
					case InputTextMode::Paste: {
						wstr[numWchars] = 0; // null character 
						GlobalUnlock(textMemHandle);

						int result;
						result = OpenClipboard(window);
						if (result == 0) {
							GlobalFree(textMemHandle);
							break;
						}
						result = EmptyClipboard();
						if (result == 0) {
							CloseClipboard();
							GlobalFree(textMemHandle);
							break;
						}
						HANDLE hResult;
						hResult = SetClipboardData(CF_UNICODETEXT, textMemHandle);
						if (hResult == NULL) {
							CloseClipboard();
							GlobalFree(textMemHandle);
							break;
						}
						CloseClipboard();

						if (hold) {
							if (numScanCodesFilled != 0)
								WaitForSingleObject(scanCodeArrayMutex, INFINITE);
							scanCodeArray[0] = SCS1::L_CTRL;
							scanCodeArray[1] = SCS1::V;
							numScanCodesFilled = 2;
							ReleaseMutex(scanCodeArrayMutex);
						}
						else {
							INPUT input;
							input.type = INPUT_KEYBOARD;
							input.ki.dwFlags = KEYEVENTF_SCANCODE;
							input.ki.time = 0;

							input.ki.wScan = SCS1::L_CTRL;
							SendInput(1, &input, sizeof(input));
							input.ki.wScan = SCS1::V;
							SendInput(1, &input, sizeof(input));

							input.ki.dwFlags |= KEYEVENTF_KEYUP;
							input.ki.wScan = SCS1::L_CTRL;
							SendInput(1, &input, sizeof(input));
							input.ki.wScan = SCS1::V;
							SendInput(1, &input, sizeof(input));
						}
						break; }
					default:
						iResult = 0;
						break;
					}
					break; }
				default: {
					iResult = 0;
					break; }
				}
			}
			else if (iResult < 0) {
				printf("recv failed with error: %d\n", WSAGetLastError());
				closesocket(ListenSocket);
				closesocket(ClientSocket);
				WSACleanup();
				ExitProcess(1);
			}

		} while (iResult > 0);

		// shutdown the 'send' connection since we're done
		/*iResult = shutdown(ClientSocket, SD_SEND);
		if (iResult == SOCKET_ERROR) {
			printf("shutdown failed with error: %d\n", WSAGetLastError());
			closesocket(ClientSocket);
			break;
		}*/

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