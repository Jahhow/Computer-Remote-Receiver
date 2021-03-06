#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <bluetoothapis.h>
#include <bthdef.h>
#include <bthsdpdef.h>
#include <ws2bth.h>
#include <stdlib.h>
#include <stdio.h>
#include <cguid.h>
#include <objbase.h>
#include <Wininet.h>
#include "SCS1.h"

#pragma comment (lib, "Wininet.lib")
#pragma comment (lib, "Ws2_32.lib")
#pragma comment (lib, "Bthprops.lib")

#define DEFAULT_BUFLEN 512
#define DEFAULT_PORT 1597
#define DEFAULT_PORT_STR "1597"

char ClientHeader[] = { 'R', 'C', 'R', 'H' };

#pragma pack(push, 1)// This makes sure variables in the structs are not reordered.
struct ServerHeader
{
	char Password[4] = { 'U', 'E', 'R', 'J' };
	int Version = htonl(2);
};

struct BroadcastData
{
	USHORT port;
	ServerHeader serverHeader;
} broadcastData;
#pragma pack(pop)

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
bool connectedAndVerified = false;
bool tcpipReady = false;
bool bluetoothReady = false;
HANDLE eventConnected;
HANDLE eventDoUdpBroadcast = CreateEvent(NULL, TRUE, FALSE, NULL);
BLUETOOTH_RADIO_INFO radioInfo;

BLUETOOTH_RADIO_INFO previousRadioInfo = {};
addrinfo* previousResult = NULL;
bool prevBluetoothReady = false;
bool internetConnected = false;
BOOL ServerInfoUpdated(addrinfo* result) {
	bool ret;
	addrinfo* pai = previousResult;
	addrinfo* ai = result;
	DWORD internetFlags;
	bool newInternetConnected = InternetGetConnectedState(&internetFlags, 0);
	if (newInternetConnected != internetConnected) {
		ret = true;
		goto Return;
	}
	if (prevBluetoothReady != bluetoothReady) {
		ret = true;
		goto Return;
	}
	if (previousRadioInfo.address.ullLong != radioInfo.address.ullLong) {
		ret = true;
		goto Return;
	}

	while (1) {
		if ((ai == NULL) != (pai == NULL)) {
			ret = true;
			goto Return;
		}
		if (ai == NULL)
			break;
		if (memcmp(ai->ai_addr, pai->ai_addr, ai->ai_addrlen)) {
			ret = true;
			goto Return;
		}
		ai = ai->ai_next;
		pai = pai->ai_next;
	}
	ret = false;

Return:
	freeaddrinfo(previousResult);
	previousResult = result;
	internetConnected = newInternetConnected;
	prevBluetoothReady = bluetoothReady;
	previousRadioInfo = radioInfo;
	return ret;
}

void MyExitProcess();

void cls()
{
	// Get the Win32 handle representing standard output.
	// This generally only has to be done once, so we make it static.
	static const HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);

	CONSOLE_SCREEN_BUFFER_INFO csbi;
	COORD topLeft = { 0, 0 };

	// std::cout uses a buffer to batch writes to the underlying console.
	// We need to flush that to the console because we're circumventing
	// std::cout entirely; after we clear the console, we don't want
	// stale buffered text to randomly be written out.
	fflush(stdout);

	// Figure out the current width and height of the console window
	if (!GetConsoleScreenBufferInfo(hOut, &csbi)) {
		// TODO: Handle failure!
		abort();
	}
	DWORD length = csbi.dwSize.X * csbi.dwSize.Y;

	DWORD written;

	// Flood-fill the console with spaces to clear it
	FillConsoleOutputCharacter(hOut, TEXT(' '), length, topLeft, &written);

	// Reset the attributes of every character to the default.
	// This clears all background colour formatting, if any.
	//FillConsoleOutputAttribute(hOut, csbi.wAttributes, length, topLeft, &written);

	// Move the cursor back to the top left for the next sequence of writes
	SetConsoleCursorPosition(hOut, topLeft);
}

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
		if (connectedAndVerified) {
			cls();
			puts("\n  Connected.");
			ResetEvent(eventDoUdpBroadcast);
			iResult = SuspendThread(GetCurrentThread());
			if (iResult == -1) {
				//printf("SuspendThread failed with error:%d\n", GetLastError());
				MyExitProcess();
			}
			goto UpdateServerInfo;
		}
		else {
			// Resolve the server address and port
			iResult = getaddrinfo("", DEFAULT_PORT_STR, &hints, &result);
			if (iResult != 0) {
				//printf("getaddrinfo failed with error:%d\n", integer);
				MyExitProcess();
			}
			if (ServerInfoUpdated(result)) {
			UpdateServerInfo:
				cls();
				if (bluetoothReady)
					wprintf(
						L"\n"
						"  Bluetooth:\n\n"
						"    %s\n"
						"    %02X:%02X:%02X:%02X:%02X:%02X\n\n",
						radioInfo.szName,
						radioInfo.address.rgBytes[0],
						radioInfo.address.rgBytes[1],
						radioInfo.address.rgBytes[2],
						radioInfo.address.rgBytes[3],
						radioInfo.address.rgBytes[4],
						radioInfo.address.rgBytes[5]);
				else
					puts(
						"\n"
						"  Bluetooth:\n\n"
						"    Bluetooth is disabled\n"
						"    or isn't supported.\n");

				puts(
					"\n"
					"  Internet:");
				if (internetConnected) {
					puts(
						"\n"
						"    [IP]");
					char ipv4str[16];
					addrinfo* ai = result;
					u_int i = 1;
					while (ai) {
						IN_ADDR* inAddr = &((sockaddr_in*)ai->ai_addr)->sin_addr;
						inet_ntop(ai->ai_family, inAddr, ipv4str, 16);
						printf("         (%u)  %s\n", i, ipv4str);
						ai = ai->ai_next;
						++i;
					}
					printf(
						"\n"
						"    [Port] =      %hu\n", boundPort);
					SetEvent(eventDoUdpBroadcast);
				}
				else {
					ResetEvent(eventDoUdpBroadcast);
					puts("\n    You\'re not connected.");
				}
			}
			WaitForSingleObject(eventConnected, 1000);
		}
	}
}


int scanCodeArraySize = 8;
short* scanCodeArray = (short*)malloc(scanCodeArraySize << 1);

// I use 0 to indicates that RepeatKeyStrokeThread is currently not repeating key stroke.
// Take care of this variable. For the semaphore 'eventDoRepeatKeyStroke' relies on this number to work properly.
int numScanCodesFilled = 0;
HANDLE eventDoRepeatKeyStroke = CreateEvent(NULL, TRUE, FALSE, NULL);
HANDLE mutexScanCodeArray = CreateMutex(NULL, FALSE, NULL);

DWORD WINAPI RepeatKeyStrokeThread(LPVOID lpParam) {

	INPUT input;
	input.type = INPUT_KEYBOARD;
	input.ki.dwFlags = KEYEVENTF_SCANCODE;
	input.ki.time = 0;

	while (1) {
		WaitForSingleObject(eventDoRepeatKeyStroke, INFINITE);
		WaitForSingleObject(mutexScanCodeArray, INFINITE);
		for (int i = 0; i < numScanCodesFilled; ++i) {
			input.ki.wScan = scanCodeArray[i];
			SendInput(1, &input, sizeof(input));
		}
		ReleaseMutex(mutexScanCodeArray);
		Sleep(30);
	}
}

SOCKET ListenSocket = INVALID_SOCKET;
SOCKET ClientSocket = INVALID_SOCKET;
SOCKET btSocket = INVALID_SOCKET;
SOCKET UdpSocket = INVALID_SOCKET;

char recvbuf[DEFAULT_BUFLEN];
INPUT mouseInput = {};
HWND window = GetConsoleWindow();
HANDLE handleIpPortPrintingThread;
HANDLE handleRepeatKeyStrokeThread;
HANDLE handleBluetoothThread;
HANDLE handleBroadcastThread;

void MyExitProcess() {
	if (ListenSocket != INVALID_SOCKET) closesocket(ListenSocket);
	if (ClientSocket != INVALID_SOCKET) closesocket(ListenSocket);
	if (btSocket != INVALID_SOCKET) closesocket(btSocket);
	if (UdpSocket != INVALID_SOCKET) closesocket(UdpSocket);
	freeaddrinfo(previousResult);
	WSACleanup();
	ExitProcess(0);
}

HANDLE mutexClientSocket = CreateMutex(NULL, false, NULL);
void AcceptRoutine(SOCKET mSocket, bool ExitProcessOnError = true) {
	while (1) {
		int iResult;
		{
			SOCKET TempClientSocket = accept(mSocket, NULL, NULL);
			if (TempClientSocket == INVALID_SOCKET) {
				//printf("accept failed with error:%d\n", WSAGetLastError());
				goto EXIT;
			}
			WaitForSingleObject(mutexClientSocket, INFINITE);
			ClientSocket = TempClientSocket;
		}

		int iSendResult = send(ClientSocket, (const char*)&broadcastData.serverHeader, sizeof(broadcastData.serverHeader), 0);
		if (iSendResult == SOCKET_ERROR) {
			//printf("send failed with error:%d\n", WSAGetLastError());
			goto CloseClient;
		}

		iResult = recv(ClientSocket, recvbuf, 4, MSG_WAITALL);
		if (iResult != 4 || memcmp(recvbuf, ClientHeader, sizeof(ClientHeader))) {
			goto CloseClient;
		}

		connectedAndVerified = true;
		SetEvent(eventConnected);

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
						goto CloseClient;
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
						SendInput(1, &input, sizeof(input));
						WaitForSingleObject(mutexScanCodeArray, INFINITE);
						scanCodeArray[0] = scanCode;
						numScanCodesFilled = 1;
						ReleaseMutex(mutexScanCodeArray);
						SetEvent(eventDoRepeatKeyStroke);
						break; }
					case ButtonAction::Up: {
						ResetEvent(eventDoRepeatKeyStroke);
						WaitForSingleObject(mutexScanCodeArray, INFINITE);
						numScanCodesFilled = 0;
						ReleaseMutex(mutexScanCodeArray);
						input.ki.dwFlags |= KEYEVENTF_KEYUP;
						SendInput(1, &input, sizeof(input));
						break; }
					}
					break; }
				case Msg::KeyboardScanCodeCombination: {
					iResult = recv(ClientSocket, recvbuf, 2, MSG_WAITALL);
					if (iResult != 2) {
						goto CloseClient;
					}

					char buttonAction = recvbuf[0];
					u_char scanCodeByteLen = recvbuf[1];

					iResult = recv(ClientSocket, recvbuf, scanCodeByteLen, MSG_WAITALL);
					if (iResult != scanCodeByteLen) {
						goto CloseClient;
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
						WaitForSingleObject(mutexScanCodeArray, INFINITE);
						if (numScanCodes > scanCodeArraySize) {
							void* newScanCodeArray = realloc(scanCodeArray, scanCodeByteLen);
							if (newScanCodeArray == NULL) {
								goto CloseClient;
							}
							scanCodeArraySize = numScanCodes;
							scanCodeArray = (short*)newScanCodeArray;
						}
						memcpy(scanCodeArray, scanCodes, scanCodeByteLen);
						numScanCodesFilled = numScanCodes;
						ReleaseMutex(mutexScanCodeArray);
						SetEvent(eventDoRepeatKeyStroke);
						break; }
					case ButtonAction::Up: {
						ResetEvent(eventDoRepeatKeyStroke);
						WaitForSingleObject(mutexScanCodeArray, INFINITE);
						numScanCodesFilled = 0;
						ReleaseMutex(mutexScanCodeArray);
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
						goto CloseClient;
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
						goto CloseClient;
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
						goto CloseClient;
					}

					int textByteLen = ntohl(*(int*)recvbuf);
					int numWchars = textByteLen >> 1;
					char inputTextMode = recvbuf[4];
					bool hold = recvbuf[5];

					HANDLE textMemHandle = GlobalAlloc(GMEM_MOVEABLE, textByteLen + 2);
					if (textMemHandle == NULL) {
						goto CloseClient;
					}
					// Lock the handle and copy the text to the buffer. 
					char16_t* wstr = (char16_t*)GlobalLock(textMemHandle);
					if (wstr == NULL) {
						GlobalFree(textMemHandle);
						goto CloseClient;
					}
					iResult = recv(ClientSocket, (char*)wstr, textByteLen, MSG_WAITALL);
					if (iResult != textByteLen) {
						GlobalFree(textMemHandle); // This can free a locked memory.
						goto CloseClient;
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
							WaitForSingleObject(mutexScanCodeArray, INFINITE);
							scanCodeArray[0] = SCS1::L_CTRL;
							scanCodeArray[1] = SCS1::V;
							numScanCodesFilled = 2;
							ReleaseMutex(mutexScanCodeArray);
							SetEvent(eventDoRepeatKeyStroke);
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
						goto CloseClient;
					}
					break; }
				default: {
					goto CloseClient;
				}
				}
			}
			else if (iResult == SOCKET_ERROR) {
				//printf("recv failed with error:%d\n", WSAGetLastError());
				goto CloseClient;
			}
		} while (iResult > 0);

	CloseClient:
		closesocket(ClientSocket);
		ClientSocket = NULL;
		ReleaseMutex(mutexClientSocket);
		ResetEvent(eventDoRepeatKeyStroke);
		if (connectedAndVerified) {
			connectedAndVerified = false;
			ResetEvent(eventConnected);
			ResumeThread(handleIpPortPrintingThread);
		}
	}

EXIT:
	if (ExitProcessOnError)
		MyExitProcess();
	else {
		ReleaseMutex(mutexClientSocket);
		ResetEvent(eventDoRepeatKeyStroke);
		if (connectedAndVerified) {
			connectedAndVerified = false;
			ResetEvent(eventConnected);
			ResumeThread(handleIpPortPrintingThread);
		}
	}
}

DWORD WINAPI BluetoothThread(LPVOID ignored) {
	int iResult;
	int saBTsize = sizeof(SOCKADDR_BTH);

	// {C937E0B7-8C64-C221-4A25-F40120B3064E}
	unsigned char btServerGuid[16] = { 0xB7,0xE0,0x37,0xC9, 0x64,0x8C, 0x21,0xC2, 0x4A,0x25,0xF4,0x01,0x20,0xB3,0x06,0x4E };
	WSAQUERYSET qsRegInfo;
	BLUETOOTH_FIND_RADIO_PARAMS params = { sizeof(params) };
	HANDLE radio;

	radioInfo.dwSize = sizeof(radioInfo);

	while (1) {
		HBLUETOOTH_RADIO_FIND hRadioFind = BluetoothFindFirstRadio(&params, &radio);
		if (hRadioFind == NULL) {
			goto ContinueLater;
		}
		BluetoothGetRadioInfo(radio, &radioInfo);
		if (NDR_LOCAL_ENDIAN == NDR_LITTLE_ENDIAN) {
			radioInfo.address.ullLong = htonll(radioInfo.address.ullLong) >> 16;
		}
		CloseHandle(radio);
		BluetoothFindRadioClose(hRadioFind);

		if (!BluetoothEnableIncomingConnections(NULL, TRUE)) {
			//puts("BluetoothEnableIncomingConnections failed.");
			goto ContinueLater;
		}
		if (!BluetoothEnableDiscovery(NULL, TRUE)) {
			//puts("BluetoothEnableDiscovery failed.");
			goto ContinueLater;
		}

		btSocket = socket(AF_BTH, SOCK_STREAM, BTHPROTO_RFCOMM);
		if (btSocket == INVALID_SOCKET) {
			goto ContinueLater;
		}

		SOCKADDR_BTH saBT;
		saBT.addressFamily = AF_BTH;
		saBT.btAddr = 0;
		saBT.serviceClassId = GUID_NULL;
		saBT.port = BT_PORT_ANY;

		iResult = bind(btSocket, (sockaddr*)&saBT, sizeof(saBT));
		if (iResult == SOCKET_ERROR) {
			//printf("bind(btSocket) failed with error:%d\n", WSAGetLastError());
			goto ContinueLater;
		}

		iResult = listen(btSocket, 1);
		if (iResult == SOCKET_ERROR) {
			//printf("listen(btSocket) failed with error:%d\n", WSAGetLastError());
			goto ContinueLater;
		}

		iResult = getsockname(btSocket, (sockaddr*)&saBT, &saBTsize);
		if (iResult == SOCKET_ERROR) {
			//printf("getsockname(btSocket) failed with error:%d\n", WSAGetLastError());
			goto ContinueLater;
		}

		CSADDR_INFO sockinfo;
		sockinfo.iProtocol = BTHPROTO_RFCOMM;
		sockinfo.iSocketType = SOCK_STREAM;
		sockinfo.LocalAddr = { (LPSOCKADDR)&saBT,sizeof(saBT) };
		sockinfo.RemoteAddr = { (LPSOCKADDR)&saBT,sizeof(saBT) };

		ZeroMemory(&qsRegInfo, sizeof(WSAQUERYSET));
		qsRegInfo.dwSize = sizeof(WSAQUERYSET);
		qsRegInfo.lpszServiceInstanceName = (LPTSTR)TEXT("J");// length must > 0
		qsRegInfo.lpServiceClassId = (GUID*)btServerGuid;
		qsRegInfo.dwNameSpace = NS_BTH;
		qsRegInfo.dwNumberOfCsAddrs = 1;
		qsRegInfo.lpcsaBuffer = &sockinfo;

		iResult = WSASetService(&qsRegInfo, RNRSERVICE_REGISTER, 0);
		if (iResult == SOCKET_ERROR) {
			//printf("WSASetService failed with error:%d\n", WSAGetLastError());
			goto ContinueLater;
		}

		bluetoothReady = true;
		AcceptRoutine(btSocket, false);
		bluetoothReady = false;

	ContinueLater:
		Sleep(1000);
		continue;
	}
}

sockaddr_in udpRecvAddr;
inline void Broadcast(IN_ADDR* inAddr) {
	udpRecvAddr.sin_addr = *inAddr;
	udpRecvAddr.sin_addr.S_un.S_un_b.s_b4 = 255;
	//udpRecvAddr.sin_addr.S_un.S_un_b = { 192,168,0,255 };   // OK
	//udpRecvAddr.sin_addr.S_un.S_un_b = { 192,168,255,255 }; // Not working
	//udpRecvAddr.sin_addr.S_un.S_addr = INADDR_BROADCAST;    // Not working

	int iResult = sendto(UdpSocket, (LPCSTR)&broadcastData, sizeof(broadcastData), 0, (SOCKADDR*)&udpRecvAddr, sizeof(udpRecvAddr));
	if (iResult == SOCKET_ERROR) {
		//wprintf(L"sendto failed with error: %d\n", WSAGetLastError());
		MyExitProcess();
	}
}

DWORD WINAPI BroadcastThread(LPVOID ignored) {
	while (1) {
		WaitForSingleObject(eventDoUdpBroadcast, INFINITE);
		addrinfo* ai = previousResult;
		while (ai) {
			IN_ADDR* inAddr = &((sockaddr_in*)ai->ai_addr)->sin_addr;
			Broadcast(inAddr);
			ai = ai->ai_next;
		}
		Sleep(1000);
	}
}

int main()
{
	DWORD prev_mode;
	HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
	HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
	CONSOLE_CURSOR_INFO cursorInfo = { 1 };
	SMALL_RECT rect = { 0,0,32,14 };
	COORD screenBufSize = { 33,15 };
	SetConsoleTextAttribute(hOut, BACKGROUND_BLUE | BACKGROUND_GREEN | BACKGROUND_RED | BACKGROUND_INTENSITY);
	SetConsoleWindowInfo(hOut, TRUE, &rect);
	SetConsoleScreenBufferSize(hOut, screenBufSize);
	GetConsoleMode(hIn, &prev_mode);
	SetConsoleMode(hIn, ENABLE_EXTENDED_FLAGS | (prev_mode & ~ENABLE_QUICK_EDIT_MODE));
	SetConsoleCursorInfo(hOut, &cursorInfo);
	SetConsoleTitle(TEXT("Receiver"));

	if (scanCodeArray == NULL || eventDoRepeatKeyStroke == NULL || eventDoUdpBroadcast == NULL
		|| mutexScanCodeArray == NULL)
		return 1;

	WSADATA wsaData;
	int integer;
	mouseInput.type = INPUT_MOUSE;

	// Initialize Winsock
	integer = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (integer != 0)
		return 1;

	// Create a SOCKET for connecting to server
	ListenSocket = socket(AF_INET/*IPv4*/, SOCK_STREAM, IPPROTO_TCP);
	if (ListenSocket == INVALID_SOCKET)
		goto CleanupExit;

	sockaddr_in sa;
	sa.sin_family = AF_INET;
	sa.sin_addr = in4addr_any;
	sa.sin_port = htons(DEFAULT_PORT);
	ZeroMemory(&sa.sin_zero, sizeof(sa.sin_zero));

	// Setup the TCP listening socket
	integer = bind(ListenSocket, (sockaddr*)&sa, sizeof(sa));
	if (integer == SOCKET_ERROR) {
		sa.sin_port = 0;// Auto pick available port

		integer = bind(ListenSocket, (sockaddr*)&sa, sizeof(sa));
		if (integer == SOCKET_ERROR)
			goto CleanupExit;
	}

	if (sa.sin_port == 0) {
		// Need to fetch port bound
		int size = sizeof(sa);
		integer = getsockname(ListenSocket, (sockaddr*)&sa, &size);
		if (integer == SOCKET_ERROR)
			goto CleanupExit;
	}
	broadcastData.port = sa.sin_port;
	boundPort = ntohs(sa.sin_port);

	integer = listen(ListenSocket, 2);
	if (integer == SOCKET_ERROR)
		goto CleanupExit;

	eventConnected = CreateEvent(0, false, 0, NULL);
	if (eventConnected == NULL)
		goto CleanupExit;

	UdpSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (UdpSocket == INVALID_SOCKET) {
		//wprintf(L"socket failed with error: %ld\n", WSAGetLastError());
		goto CleanupExit;
	}

	integer = TRUE;// Enabled
	if (setsockopt(UdpSocket, SOL_SOCKET, SO_BROADCAST, (LPCSTR)&integer, sizeof(integer)) == SOCKET_ERROR) {
		//perror("setsockopt");
		goto CleanupExit;
	}
	udpRecvAddr.sin_family = AF_INET;
	udpRecvAddr.sin_port = htons(DEFAULT_PORT);

	handleIpPortPrintingThread = CreateThread(
		NULL,                   // default security attributes
		0,                      // use default stack size  
		IpPortPrintingThread,   // thread function name
		NULL,                   // argument to thread function 
		0,                      // use default creation flags 
		NULL);                  // returns the thread identifier 
	if (handleIpPortPrintingThread == NULL)
		goto CleanupExit;

	handleRepeatKeyStrokeThread = CreateThread(
		NULL,                   // default security attributes
		0,                      // use default stack size  
		RepeatKeyStrokeThread,  // thread function name
		NULL,                   // argument to thread function 
		0,                      // use default creation flags 
		NULL);                  // returns the thread identifier 
	if (handleRepeatKeyStrokeThread == NULL)
		goto CleanupExit;

	handleBluetoothThread = CreateThread(
		NULL,                   // default security attributes
		0,                      // use default stack size  
		BluetoothThread,        // thread function name
		NULL,                   // argument to thread function 
		0,                      // use default creation flags 
		NULL);                  // returns the thread identifier 
	if (handleBluetoothThread == NULL)
		goto CleanupExit;

	handleBroadcastThread = CreateThread(
		NULL,                   // default security attributes
		0,                      // use default stack size  
		BroadcastThread,        // thread function name
		NULL,                   // argument to thread function 
		0,                      // use default creation flags 
		NULL);                  // returns the thread identifier 
	if (handleBroadcastThread == NULL)
		goto CleanupExit;

	AcceptRoutine(ListenSocket);

CleanupExit:
	MyExitProcess();
}