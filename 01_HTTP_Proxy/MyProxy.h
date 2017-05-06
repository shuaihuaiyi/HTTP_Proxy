#pragma once
#include <vector>
#include <stdio.h>
#include <Windows.h>
#include <process.h>
#include <string.h>
#include <iostream>
#include <unordered_map>

#pragma comment(lib,"Ws2_32.lib")

#define MAXSIZE 65536 //发送数据报文的最大长度
#define HTTP_PORT 80 //http 服务器端口

class MyProxy;
using namespace std;
struct ProxyParam {
	SOCKET clientSocket;
	SOCKET serverSocket;
	MyProxy* myproxy;
};
class MyProxy
{
public:
	MyProxy(int port, vector<string> blackList, unordered_map<string, string> redirect);
	~MyProxy();
private:
	//Http 重要头部数据
	struct HttpHeader {
		char method[4]; // POST 或者 GET，注意有些为 CONNECT，本实验暂不考虑
		char url[1024];       // 请求的 url
		char host[1024]; // 目标主机 
		char cookie[1024 * 10]; //cookie
		HttpHeader() {
			ZeroMemory(this, sizeof(HttpHeader));
		}
	};

	//代理相关参数
	SOCKET ProxyServer;
	sockaddr_in ProxyServerAddr;
	int ProxyPort;
	vector<string> blackList;
	unordered_map<string, string> redirect;
	//函数声明
	static unsigned int __stdcall ProxyThread(LPVOID lpParameter);
	static void ParseHttpHead(char* buffer, HttpHeader* httpHeader);
	static BOOL ConnectToServer(SOCKET* serverSocket, char* host);
};

inline MyProxy::MyProxy(int port, vector<string> blackList, unordered_map<string, string> redirect)
{
	ProxyPort = port;
	this->blackList = blackList;
	this->redirect = redirect;
	WSADATA wsaData;
	//版本 2.2
	WORD wVersionRequested = MAKEWORD(2, 2);
	//加载 dll 文件 Scoket 库
	int err = WSAStartup(wVersionRequested, &wsaData);
	if (err != 0) {
		//找不到 winsock.dll
		printf("加载 winsock 失败，错误代码为: %d\n", WSAGetLastError());
		return;
	}
	if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2)
	{
		printf("不能找到正确的 winsock 版本\n");
		WSACleanup();
		return;
	}
	ProxyServer = socket(AF_INET, SOCK_STREAM, 0);
	if (INVALID_SOCKET == ProxyServer) {
		printf("创建套接字失败，错误代码为：%d\n", WSAGetLastError());
		WSACleanup();
		return;
	}
	ProxyServerAddr.sin_family = AF_INET;
	ProxyServerAddr.sin_port = htons(ProxyPort);
	ProxyServerAddr.sin_addr.S_un.S_addr = INADDR_ANY;
	if (bind(ProxyServer, reinterpret_cast<SOCKADDR*>(&ProxyServerAddr), sizeof(SOCKADDR)) == SOCKET_ERROR) {
		printf("绑定套接字失败\n");
		return;
	}
	if (listen(ProxyServer, SOMAXCONN) == SOCKET_ERROR) {
		printf("监听端口%d 失败", ProxyPort);
		return;
	}
	printf("代理服务器正在运行，监听端口 %d\n", ProxyPort);
	while (true) {
		SOCKET acceptSocket = accept(ProxyServer, nullptr, nullptr);
		ProxyParam *lpProxyParam = new ProxyParam;
		if (lpProxyParam == nullptr) {
			continue;
		}
		lpProxyParam->clientSocket = acceptSocket;
		lpProxyParam->myproxy = this;
		HANDLE hThread = reinterpret_cast<HANDLE>(_beginthreadex(nullptr, 0, &ProxyThread, LPVOID(lpProxyParam), 0, nullptr));
		CloseHandle(hThread);
		Sleep(200);
		delete lpProxyParam;
	}
}

inline MyProxy::~MyProxy()
{
	closesocket(ProxyServer);
	WSACleanup();
}

//************************************
// Method:	ProxyThread
// FullName:	ProxyThread
// Access:	public
// Returns:	unsigned int __stdcall
// Qualifier: 线程执行函数
// Parameter: LPVOID lpParameter
//************************************
inline unsigned int __stdcall MyProxy::ProxyThread(LPVOID lpParameter)
{
	char Buffer[MAXSIZE];
	char* buffer,*temp;
	char *CacheBuffer;
	ZeroMemory(Buffer, MAXSIZE);
	int recvSize;
	recvSize = recv(static_cast<ProxyParam*>(lpParameter)->clientSocket, Buffer, MAXSIZE, 0);
	if (recvSize <= 0)
	{
		Sleep(200);
		for (int i = 0;closesocket(static_cast<ProxyParam*>(lpParameter)->clientSocket) == SOCKET_ERROR && i < 10;i++);
		_endthreadex(0);
		return 0;
	}
	HttpHeader* httpHeader = new HttpHeader();
	CacheBuffer = new char[recvSize + 1];
	ZeroMemory(CacheBuffer, recvSize + 1);
	memcpy(CacheBuffer, Buffer, recvSize);
	ParseHttpHead(CacheBuffer, httpHeader);
	delete[] CacheBuffer;
	MyProxy* myproxy = static_cast<ProxyParam*>(lpParameter)->myproxy;
	//处理访问控制
	if(find(myproxy->blackList.begin(), myproxy->blackList.end(), httpHeader->host) != myproxy->blackList.end())
	{
		cout << httpHeader->host << "被管理员禁止访问！！" << endl;
		Sleep(200);
		//for (int i = 0;closesocket(static_cast<ProxyParam*>(lpParameter)->clientSocket) == SOCKET_ERROR && i < 10;i++);
		int res = closesocket(static_cast<ProxyParam*>(lpParameter)->clientSocket);
		delete httpHeader;
		_endthreadex(0);
		return 0;
	}
	char host[1024] = "http://";
	strcat_s(host, httpHeader->host);
	//处理网站引导
	unordered_map<string, string>::iterator it = myproxy->redirect.find(httpHeader->host);
	if (it != myproxy->redirect.end())
	{
		const int len = it->second.length();
		char * c = new char[len + 1];
		strcpy_s(c, len+1,it->second.c_str());
		if (!ConnectToServer(&static_cast<ProxyParam*>(lpParameter)->serverSocket, c))
		{
			cout << "连接服务器失败！" << endl;
			Sleep(200);
			for (int i = 0;closesocket(static_cast<ProxyParam*>(lpParameter)->clientSocket) == SOCKET_ERROR && i < 10;i++);
			//closesocket(static_cast<ProxyParam*>(lpParameter)->serverSocket);
			delete httpHeader;
			_endthreadex(0);
			return 0;
		}
		delete[] c;
	}
	else
	{
		if (!ConnectToServer(&static_cast<ProxyParam*>(lpParameter)->serverSocket, httpHeader->host))
		{
			cout << "连接服务器失败！" << endl;
			Sleep(200);
			for (int i = 0;closesocket(static_cast<ProxyParam*>(lpParameter)->clientSocket) == SOCKET_ERROR && i < 10;i++);
			//closesocket(static_cast<ProxyParam*>(lpParameter)->serverSocket);
			delete httpHeader;
			_endthreadex(0);
			return 0;
		}
	}
	//去除浏览器添加的代理标识
	unsigned int  len = strlen(Buffer);
	temp = new char[len + 1];
	buffer = new char[len + 1];
	ZeroMemory(temp, len + 1);
	ZeroMemory(buffer, len + 1);
	for (unsigned int i = 0; i < len; i++)
	{
		if (strncmp(Buffer + i, "Proxy-Connection: keep-alive", 29) == 0)
		{
			strcat_s(temp + i, len + 1, "Connection: close");
			ZeroMemory(temp + i + 18, len - i - 18);
			strcat_s(temp + i + 18, len + 1, Buffer + i + 29);
			recvSize -= 11;
			break;
		}
		temp[i] = Buffer[i];
	}
	for (unsigned int i = 0; i < len; i++)
	{
		if (strncmp(temp + i, host, strlen(host)) == 0)
		{
			strcat_s(buffer + i, len + 1, temp + i + strlen(host));
			recvSize -= strlen(host);
			break;
		}
		buffer[i] = temp[i];
	}
	delete[] temp;
	//printf("代理连接主机 %s 成功\n", httpHeader->host);
	//将客户端发送的 HTTP 数据报文直接转发给目标服务器
	int ret3 = send(static_cast<ProxyParam *>(lpParameter)->serverSocket, buffer, recvSize, 0);
	//等待目标服务器返回数据
	recvSize = recv(static_cast<ProxyParam*>(lpParameter)->serverSocket, Buffer, MAXSIZE, 0);
	if (recvSize <= 0)
		cout << "服务器没有返回数据！" << endl;
	//将目标服务器返回的数据直接转发给客户端 
	int ret1 = send(static_cast<ProxyParam*>(lpParameter)->clientSocket, Buffer, sizeof(Buffer), 0);
	Sleep(200);
	for (int i = 0;closesocket(static_cast<ProxyParam*>(lpParameter)->clientSocket) == SOCKET_ERROR && i < 10;i++);
	int ret2 = closesocket(static_cast<ProxyParam*>(lpParameter)->serverSocket);
	delete httpHeader;
//	delete[] buffer;
	_endthreadex(0);
	return 0;
}

//************************************
// Method:	ParseHttpHead
// FullName:	ParseHttpHead
// Access:	public
// Returns:	void
// Qualifier: 解析 TCP 报文中的 HTTP 头部
// Parameter: char * buffer
// Parameter: HttpHeader * httpHeader
//************************************
inline void MyProxy::ParseHttpHead(char *buffer, HttpHeader * httpHeader)
{
	char *ptr;
	const char * delim = "\r\n";
	char *p = strtok_s(buffer, delim, &ptr);//提取第一行
	printf("%s\n", p);
	if (p[0] == 'G')
	{//GET 方式
		memcpy(httpHeader->method, "GET", 3);
		memcpy(httpHeader->url, &p[4], strlen(p) - 13);
	}
	else if (p[0] == 'P')
	{//POST 方式
		memcpy(httpHeader->method, "POST", 4);
		memcpy(httpHeader->url, &p[5], strlen(p) - 14);
	}
	p = strtok_s(nullptr, delim, &ptr);
	while (p) {
		switch (p[0]) {
		case 'H'://Host
			memcpy(httpHeader->host, &p[6], strlen(p) - 6);
			break;
		case 'C'://Cookie
			if (strlen(p) > 8)
			{
				char header[8];
				ZeroMemory(header, sizeof(header));
				memcpy(header, p, 6);
				if (!strcmp(header, "Cookie"))
				{
					memcpy(httpHeader->cookie, &p[8], strlen(p) - 8);
				}
			}
			break;
		default:
			break;
		}
		p = strtok_s(nullptr, delim, &ptr);
	}
}

//************************************
// Method:	ConnectToServer
// FullName:	ConnectToServer
// Access:	public
// Returns:	BOOL
// Qualifier: 根据主机创建目标服务器套接字，并连接
// Parameter: SOCKET * serverSocket
// Parameter: char * host
//************************************
inline BOOL MyProxy::ConnectToServer(SOCKET *serverSocket, char *host) {
	sockaddr_in serverAddr;
	serverAddr.sin_family = PF_INET;
	serverAddr.sin_port = htons(HTTP_PORT);
	HOSTENT *hostent = gethostbyname(host);
	if (!hostent) {
		return FALSE;
	}
	in_addr Inaddr = *reinterpret_cast<in_addr*>(*hostent->h_addr_list);
	serverAddr.sin_addr.s_addr = inet_addr(inet_ntoa(Inaddr));
	*serverSocket = socket(PF_INET, SOCK_STREAM, 0);
	if (*serverSocket == INVALID_SOCKET)
	{
		return FALSE;
	}
	if (connect(*serverSocket, reinterpret_cast<SOCKADDR *>(&serverAddr), sizeof(serverAddr)) == SOCKET_ERROR)
	{
		closesocket(*serverSocket);
		return FALSE;
	}
	return TRUE;
}
