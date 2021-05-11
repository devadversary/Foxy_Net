#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <WinSock2.h>
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <io.h>
#include "resource.h"
#pragma comment (lib , "ws2_32.lib")

#define AD_PORT_UDP 21700
#define DATA_PORT_TCP 21400
#define CHAT_PORT_UDP 21500
#define HIDDEN_PORT_UDP 21600
#define LOADBAR_SIZE 200
#define WM_CHATPOLLING WM_USER+1024

#define X_BUTTON 1020

int nCmdShow_copy;

struct Host_List		// ȣ��Ʈ�� �ּҿ� ���� �������� üũ�Ѵ�;
{
	WCHAR Host[20];
	unsigned int Life;
	BOOL FileFlag;
	unsigned int ProgressPoint;
	WCHAR FileName[10];
};
struct Sock_List
{
	SOCKET s;
	SOCKADDR_IN peer;
};
struct FileSendInfo
{
	WCHAR IPAddress[20];
	WCHAR FilePath[MAX_PATH];
	WCHAR FileName[MAX_PATH];
	unsigned long long FileSize;
};

HANDLE AD_Thread , RECV_Thread , CHAT_Thread , LIFE_Thread , SRCH_Thread , LBSRM_Thread , FLSRV_Thread;			//�����带 ������ �� �ֵ��� �ڵ��� �޾Ƶε��� ����;
LPCWSTR lpClassName = L"Foxy_Net :: Network USB";
HBITMAP X_BUT , LOGO_BIT , LOADBAR_BIT , STATE_BIT;
HINSTANCE hInstance_copy;
SOCKET ad_sock;  //�ֱ����� ���� �ְ���� �������� ���� �����̴�.;
SOCKET chat_sock; // ä�� ��� ����;
SOCKADDR_IN ad_sp={0,}; // �� ������ �ּұ���ü;
SOCKADDR_IN chat_dest = {0,};
WNDPROC oldProc;

WCHAR AddressResolution[20] = {0,};
WCHAR Localhost[16];										// �� IP �ּҰ� ����� �����̴�;
struct Host_List Detected_Host[253] = {0,};					// ������ ȣ��Ʈ���� �ּҸ� ����;
WCHAR TestAddr[16] = {0,};								// ������ ȣ��Ʈ�� ������ ȣ��Ʈ�� �����ϴ����� ���θ� �Ǵ��ϱ����� �񱳸� ������ �ӽù���;
unsigned char Detected_Host_Index = 0;				// ������ ȣ��Ʈ�� ��;
int error_check;
int StreamLoadbar = 10;
int FileSendFlag=0 , FileRecvFlag =0;
CRITICAL_SECTION cs_list;
CRITICAL_SECTION cs_chat;
OPENFILENAME FILENAME;  // ������ ���� �̸��� ���� - GetOpenFileName �Լ��� �̿��ؾ� �Ѵ�;
HWND hWnd_ChatList_copy;
HWND hWnd_Res_1;

int distance_calc(int _Operand , int _Length)
{
	int __Current = _Length / _Operand ;
	int __Distance = _Length ;

	//__Distance -= __Current ;

	return __Current ;
}
int MovementPoint(int Src , int Dest , int Frame , BOOL* Alarm)
{
	int temp;
	int movepoint;
	if(Src == Dest){ *Alarm=TRUE; return Src;}
	temp = Dest - Src;
	movepoint = temp / Frame;

	if(movepoint == 0)
	{
		if(Src > Dest) return Src-1;
		else return Src+1;
	}
	else return Src + movepoint;
}
int CurrentProgress(__int64 TotalProcess , __int64 CurrentProcess , int LoadbarPixel)
{
	float  _Percentage;
	_Percentage = (float)CurrentProcess / (float)TotalProcess;
	return (int)(_Percentage * LoadbarPixel);
}

/*BOOL SafeDrawText(HDC hDC , HFONT hFont , int x , int y , LPCWSTR buf , size_t Buf_Len)
{
	SelectObject()
}*/
LRESULT CALLBACK WndProc(HWND , UINT , WPARAM , LPARAM);
LRESULT CALLBACK TITLE_Proc(HWND , UINT , WPARAM , LPARAM);
LRESULT CALLBACK X_BUTTON_Proc(HWND , UINT  , WPARAM  , LPARAM );
LRESULT CALLBACK IP_LIST_Proc(HWND , UINT , WPARAM , LPARAM);
LRESULT CALLBACK CHAT_LIST_Proc(HWND , UINT , WPARAM , LPARAM);
LRESULT CALLBACK hWnd_Input_Sub(HWND , UINT , WPARAM , LPARAM);
LRESULT CALLBACK LOGO_Proc(HWND , UINT , WPARAM , LPARAM);
LRESULT CALLBACK FILE_BUTTON_Proc(HWND , UINT , WPARAM , LPARAM);
LRESULT CALLBACK TRANSFER_STATE_Proc(HWND , UINT , WPARAM , LPARAM);

DWORD WINAPI Thread_StreamBar(LPVOID temp)
{
	while(1)
	{
		if(StreamLoadbar == 0) StreamLoadbar = 10;
		StreamLoadbar -=1;
		Sleep(40);
	}

	return 0;
}  

DWORD WINAPI Thread_LifeCheck(LPVOID temp) // 1�ʸ��� �������� ������ ������ �����ð� ���� ����Ʈ�� ȣ��Ʈ�� �����Ѵ�;
{
	int TestIndex, SubIndex=0;
	while(1)
	{
		EnterCriticalSection(&cs_list);
		for(TestIndex = 0; TestIndex < Detected_Host_Index ; TestIndex++ )
		{
			
			if(Detected_Host[TestIndex].Life > 0)
				Detected_Host[TestIndex].Life --;
			
			if(Detected_Host[TestIndex].Life <= 0)
			{
				for(SubIndex = TestIndex ; SubIndex < Detected_Host_Index ; SubIndex++)
					memcpy_s((Host_List*)&Detected_Host[SubIndex] , sizeof(Host_List), (Host_List*)&Detected_Host[SubIndex+1] , sizeof(Host_List));
				memset((Host_List*)&Detected_Host[Detected_Host_Index] , 0 , sizeof(Detected_Host[0]));
				Detected_Host_Index--;
				TestIndex--;
			}
		}
		LeaveCriticalSection(&cs_list);
		InvalidateRect((HWND)temp , 0 , FALSE);		//ȭ�� ������ ���� �޼����� ������.;
		Sleep(990);
	}

	return 0;
} 

DWORD WINAPI Thread_BroadcastAD(LPVOID temp)  //  �ֱ����� ���� ���� ������ , ���Ʈĳ��Ʈ�� �̿��ϸ� 21300�� ��Ʈ�� ����Ѵ�.;
{
	char opt = 1;
	char buf[10] = "hello"; // Ȥ�� �� �ߺ��� ���ϱ����� Ű���� ���� (���žƴ�);
	
	ad_sock = socket(AF_INET, SOCK_DGRAM , NULL);
	if (ad_sock == INVALID_SOCKET)
	{
		MessageBox(NULL , L"��Ʈ��ũ ���� (���� ����)" , L"����ŷ",  MB_OK);
		PostQuitMessage(0);
	}
	setsockopt(ad_sock , SOL_SOCKET , SO_BROADCAST , &opt , sizeof(opt));
	ad_sp.sin_addr.s_addr = htonl(INADDR_BROADCAST);
	ad_sp.sin_family = AF_INET;
	ad_sp.sin_port = htons(AD_PORT_UDP);

	while(1)
	{
		sendto(ad_sock , buf , strlen(buf) , NULL , (SOCKADDR*)&ad_sp , 16);			// hello ��� ���ڿ��� �Ǹ� �����͸� �ֱ������� ������ ���ӵ� ȣ��Ʈ���� �ڽ��� �˸���.;
		Sleep(2000);																				// ��� ������ Ʈ���� ���ϰ� ¿���ִϱ� 2�� �������θ� �����൵ ����ϴ�;
	}
	return 0;
}

DWORD WINAPI Thread_Chatting(LPVOID hWnd)
{
	SOCKADDR_IN sp;
	WCHAR buf[256] , source[20] , text[240] ,	 data[256];
	DWORD len;
	int error;
	int sockaddr_size = 16;
	
	while(1)
	{
		memset(buf , 0 , sizeof(buf));
		memset(text , 0 , sizeof(text));
		memset(source , 0 , sizeof(source));
		memset(&sp , 0 , sizeof(sp));

		if ( recvfrom(chat_sock , (char*)&text , sizeof(text) , NULL , (SOCKADDR*)&sp , &sockaddr_size) == SOCKET_ERROR)
		{
			error = WSAGetLastError();
			//wsprintf(buf , L" ä�ý����� %d ����" , error);
			//MessageBox(NULL , buf , L"����" , MB_OK);
			SendMessage((HWND)hWnd , WM_DESTROY , 0 , 0);
			ExitThread(-1);
		}
		sp.sin_port = NULL;
		len =  sizeof(source);
		WSAAddressToString((SOCKADDR*)&sp , sizeof(SOCKADDR) , NULL , source , &len ); 
		
		error = WSAGetLastError();

		wsprintf(buf, L"%s : : %s" , source , text);
		wcscpy_s(data , sizeof(data) , buf);
		SendMessage((HWND)hWnd , WM_USER , (WPARAM)&data , NULL );
	}
	return 0;
}

DWORD WINAPI Thread_GetHost(LPVOID temp) // ȣ��Ʈ�� ������ ������ �� �������̴�. ��ǻ� ���� �߿��ҵ�;
{
	char detect = 0;																		//��ȣź;
	DWORD len ;									//��򰡿� ���� ����. ������ ���̸� �������ش�.;
	int TestIndex ;													//���ڿ� ���۷� �峭ĥ�� �� �����̴�;
	char buf[30];																			//���۴� �׳� ����.. ������ �׳� �Ѿ˹��̷θ� �����̱� ������;
	char opt = 1;																		//��ε�ĳ��Ʈ �ɼ��� Ȱ��ȭ ��Ű�� ���� �÷��׷� �� �����̴�. 1����Ʈ�� �޸𸮸� �� �԰���;
	int error;
	char name[100];
	size_t Localhost_Len = sizeof(Localhost);
	char *mb_localhost;
	SOCKET sock = socket(AF_INET , SOCK_DGRAM , NULL);
	SOCKADDR_IN sv = {0,};															//�� �������� ������ ����;
	SOCKADDR_IN host = {0,};															//�׷��ٸ� ���� �����Ͱ� ��� �Դ��� ����ϰھ�! ;
	SOCKADDR_IN localhost = {0,};													//�׷��ٸ� ���� �� IP �ּҸ� �������� ! ;
	HOSTENT *localname;
	int hostlen = sizeof(host);															//���ϱ���ü�� ���̸� �������ش�.;

	sv.sin_addr.s_addr = htonl(INADDR_ANY);
	sv.sin_port = htons(AD_PORT_UDP);
	sv.sin_family = AF_INET;
	
	localhost.sin_addr.s_addr = htonl(INADDR_BROADCAST);
	localhost.sin_family = AF_INET;
	localhost.sin_port = htons(AD_PORT_UDP);

	setsockopt(sock , SOL_SOCKET , SO_BROADCAST , &opt , sizeof(opt));

	if (bind(sock, (SOCKADDR*)&sv , 16)==SOCKET_ERROR)
	{
		MessageBox(NULL , L"bind ���� !" ,L"����", MB_OK );
		PostQuitMessage(NULL);
	}

	
	gethostname(name,sizeof(name));
	localname = gethostbyname(name);
	mb_localhost = inet_ntoa(*(IN_ADDR*)*localname->h_addr_list);
	mbstowcs_s(&Localhost_Len ,Localhost , mb_localhost , strlen(mb_localhost));
	SendMessage(GetParent((HWND)temp) , WM_USER , NULL , NULL);		//ȭ�� ������ ���� �޼����� ������.;

	while(1)
	{
		memset(buf , 0 , sizeof(buf));
		memset(&host , 0 , sizeof(host));
		if( recvfrom(sock , (char *)buf , sizeof(buf) , NULL , (SOCKADDR*)&host , &hostlen ) == SOCKET_ERROR)
		{
			error = WSAGetLastError();
			MessageBox(NULL , L"���� �߻�" , L"����" , MB_OK );
			SendMessage(GetParent((HWND)temp) , WM_DESTROY , NULL ,NULL);
		}
		error = WSAGetLastError();

		host.sin_port = NULL;
		 len = sizeof(Detected_Host[0].Host);
		WSAAddressToString((SOCKADDR*)&host ,16 , 0 , TestAddr , &len );			// TestAddr�� �������� �ٿ����� �ϴ� �����س��´�;
		error = WSAGetLastError();

		EnterCriticalSection(&cs_list);
		if ( strcmp(buf , "hello")==0 )		//hello ��Ŷ�� �����ϸ�;
		{
			if(wcscmp(Localhost , TestAddr) == 0) {LeaveCriticalSection(&cs_list);continue;}	// �׷��� �װ� ���� ���´��Ÿ� ��ȿ;
			//if( lstrlen(TestAddr) == 0)  {LeaveCriticalSection(&cs_list);continue;}					// ���� ������ ���涧�� �ִµ� �̶��� ��ȿ.;

			if(Detected_Host_Index == 0) // Ž���� ȣ��Ʈ�� ������ 0 �̶�� �ߺ��˻�� ������ ���ϱ� �ϴ� �׳� ù �迭�� �־������;
			{
				wcscpy_s(Detected_Host[Detected_Host_Index].Host , TestAddr);		// ù �迭�� ����;
				Detected_Host_Index++;												// ȣ��Ʈ ���� 1 ����.;
			}

			for(TestIndex=0 ; TestIndex<Detected_Host_Index ; TestIndex++)		// ���� Ž���� ȣ��Ʈ�� ������ ����Ʈ�� �̹� �����ߴ����� �˾ƺ��� �뵵;
			{				
				detect = 1;												// ���ο� ���� �Դٴ� ������ ���� �Ѵ�;

				if( wcscmp(TestAddr , Detected_Host[TestIndex].Host) == 0)				// �� �ּҰ� ������;
				{
					detect = 0;																// ���ο� ���� �ȿ����� ��ȣ�� �� ����;
					Detected_Host[TestIndex].Life = 20;									// �׷��� �̻��� ��� ��ȣ�� ������ ������ ������ 30 ����;
					break;
				}
			}

			if(detect == 1)		// ��ȣź�� �����Ǹ�;
			{
					wcscpy_s(Detected_Host[Detected_Host_Index].Host , TestAddr);	// ȣ��Ʈ������ �ش��ϴ� �� ��ȣ�� �ּҸ� �������ش�.;
					Detected_Host[Detected_Host_Index].Life = 20;					// ������ 20 ����;
					Detected_Host[Detected_Host_Index].FileFlag = 0;	

					Detected_Host_Index++;											// ȣ��Ʈ ���� 1 ����.;
					detect = 0;															// ��ȣź ����;
			}

			InvalidateRect((HWND)temp , 0 , FALSE);		//ȭ�� ������ ���� �޼����� ������.;
		}


		if ( strcmp(buf , "byebye")==0)
		{
			error_check = WSAGetLastError();
			if(Detected_Host_Index > 0) Detected_Host_Index --; // ����ó���� ������. �ȱ׷��� Ŭ��;
			for(TestIndex=0 ; TestIndex<Detected_Host_Index+1 ; TestIndex++)
			{
				if(wcscmp(TestAddr , Detected_Host[TestIndex].Host) == 0)				//���̹��� ��Ŷ�� ���ΰ��� �ִ� �迭�� ã�Ƴ���;
				{
					//memset(&Detected_Host[TestIndex] , 0 , sizeof(Detected_Host[TestIndex]) );
					for( ;TestIndex<Detected_Host_Index ; TestIndex++)
						wcscpy_s(Detected_Host[TestIndex].Host , 20  ,Detected_Host[TestIndex+1].Host );

					memset(Detected_Host[Detected_Host_Index].Host , 0 , sizeof(Detected_Host[0].Host));
					break;
				}

			}
			InvalidateRect((HWND)temp , 0 , FALSE);		//ȭ�� ������ ���� �޼����� ������.;
		}
		LeaveCriticalSection(&cs_list);
	}

	return 0;
}

DWORD WINAPI Thread_FileRecv(LPVOID temp) // ���� ������ �ޱ����� �Ϸ��� ������ ��ĥ ������;
{
	int error;
	BYTE data[8094];
	unsigned long long readbyte;
	DWORD writebyte;
	WCHAR King_Path[MAX_PATH];
	unsigned long long summaryreadbyte = 0;
	struct FileSendInfo fsi = {0,};
	Sock_List Socket_Info = {0,};
	SOCKET sock;
	SOCKADDR_IN peer;
	HANDLE hFile;
	FileRecvFlag ++;
	memcpy( &Socket_Info ,temp , sizeof(Sock_List));
	CreateDirectory(L"C:\\Foxy_Net_Download" , NULL);
	sock = Socket_Info.s;
	recv(sock , (char*)&fsi , sizeof(fsi) , NULL);
	error = WSAGetLastError();
	wsprintf(King_Path , L"C:\\Foxy_Net_Download\\%s" , fsi.FileName);
	hFile = CreateFile(King_Path , GENERIC_WRITE , NULL , NULL , CREATE_ALWAYS , NULL , NULL );
	error = GetLastError();
	if(hFile == INVALID_HANDLE_VALUE)
	{
		FileRecvFlag --;
		MessageBox(NULL , L"���� ���� �ڵ� ����" , L"����" , MB_OK);
		ExitThread(-2);
	}

	while(1)
	{
		memset(data , NULL , sizeof(data));
		readbyte = recv(sock , (char*)data , sizeof(data) , NULL);
		WriteFile(hFile , data , readbyte ,  &writebyte , NULL );
		summaryreadbyte +=writebyte ;
		if(summaryreadbyte >= fsi.FileSize) 
		{
			break;
		}
	}
	
	FileRecvFlag --;
	CloseHandle(hFile);
	closesocket(sock);

	return 0;
}

DWORD WINAPI Thread_FileSend(LPVOID temp) // ������ ������ ���� �Ϸ��� ������ ��ĥ ������;
{
	BYTE data[8094];
	LARGE_INTEGER li;
	unsigned long long summarysentbyte = 0;
	unsigned long long sentbyte;
	int TestIndex;
	int sig=0; //send �� �� ���� ���ڿ� �˻縦 �ϸ� ��ǳ �����⶧���� ���� �ֱ�� �ڵ带 �����Ű������ ������;
	int error;
	DWORD readbyte;
	int sockaddrsize = sizeof(SOCKADDR);
	HANDLE hFile;
	struct FileSendInfo fsi;
	SOCKET sock;
	SOCKADDR_IN peer;
	memset(&peer , NULL , sizeof(peer));
	memcpy(&fsi , temp , sizeof(fsi));
	FileSendFlag ++;
	sock = socket(AF_INET , SOCK_STREAM , NULL);

	WSAStringToAddress(fsi.IPAddress , AF_INET , NULL , (SOCKADDR*)&peer , &sockaddrsize);
	peer.sin_family = AF_INET ;
	peer.sin_port = htons(DATA_PORT_TCP);
	hFile = CreateFile(fsi.FilePath , GENERIC_READ , FILE_SHARE_READ , NULL , OPEN_EXISTING , NULL , NULL);

	if(hFile == INVALID_HANDLE_VALUE)
	{
		MessageBox(NULL , L"������ ���µ��� ������ �߻� �߽��ϴ�. \r\n ������ �ڲ� ������ �ʴ´ٸ� �����ڿ��� ���� �ֽø� �����ϰڽ��ϴ�.\r\n Ʈ���� : @Fox_King_" , L"����" , MB_OK);
		ExitThread(-2);
	}

	GetFileSizeEx(hFile, &li);
	fsi.FileSize = li.QuadPart;

	if ( connect(sock , (SOCKADDR*)&peer , sockaddrsize) == SOCKET_ERROR) 
	{
		error = WSAGetLastError();
		FileSendFlag --;
		MessageBox(NULL , L"ȣ��Ʈ ���ῡ �����߽��ϴ�." , L"����" ,  MB_OK);
		ExitThread(-3);
	}

	send(sock , (char*)&fsi , sizeof(fsi) , NULL); // ���� �̸��� �ϴ� ����;

	while(1)
	{
		memset(data , 0 , sizeof(data));
		sentbyte = 0;
		ReadFile(hFile , (char*)&data , 8094 , &readbyte , NULL);
		if(readbyte == 0)	break;
		while (1)
		{
			if (readbyte != sentbyte)
			{
				sentbyte += send(sock, (char*)(data+sentbyte), readbyte-sentbyte, NULL);
				continue;
			}
			break;
		}
		summarysentbyte += sentbyte;
		error = WSAGetLastError();
		if(sig == 255)
		{
			EnterCriticalSection(&cs_list);
			for(TestIndex = 0 ; TestIndex<Detected_Host_Index ; TestIndex++)
			{
				if(wcscmp(fsi.IPAddress , Detected_Host[TestIndex].Host) == 0)
				{
					Detected_Host[TestIndex].FileFlag = TRUE;
					Detected_Host[TestIndex].ProgressPoint = CurrentProgress(fsi.FileSize , summarysentbyte , LOADBAR_SIZE);
				}
			}
			LeaveCriticalSection(&cs_list);
			sig = 0;
		}
		if(error != 0)
		{
			return 0;
		}
		sig++;
	}

	EnterCriticalSection(&cs_list);
	for(TestIndex = 0 ; TestIndex<Detected_Host_Index ; TestIndex++)
	{
		if(wcscmp(fsi.IPAddress , Detected_Host[TestIndex].Host) == 0)
		{
			Detected_Host[TestIndex].FileFlag = FALSE;
		}
	}
	LeaveCriticalSection(&cs_list);
	FileSendFlag --;
	CloseHandle(hFile);
	closesocket(sock);
	return 0;
}

DWORD WINAPI Thread_TrafficManagement(LPVOID temp) // �����͸� �ְ���� ��Ʈ�ѷ� ������;
{
	HANDLE hFile;
	SOCKET sock ;
	SOCKADDR_IN sp = {0,};
	struct Sock_List Socket_Info = {0,};
	int len = sizeof(SOCKADDR);
	int cnt = 0;
	int error;

	sp.sin_addr.s_addr = htonl(INADDR_ANY);
	sp.sin_family = AF_INET;
	sp.sin_port = htons(DATA_PORT_TCP);

	sock = socket(AF_INET , SOCK_STREAM , NULL);

	if( bind(sock , (SOCKADDR*)&sp , sizeof(SOCKADDR)) == SOCKET_ERROR)	
	{
		MessageBox(NULL , L"���� �ۼ��� ��⸦ �� �� ���׿�" , L"����" , MB_OK);
		ExitThread(-1);
	}
	if ( listen(sock , 255) == SOCKET_ERROR) ExitThread(-2);
	error = WSAGetLastError();

	while(1)
	{
		Socket_Info.s = accept(sock , (SOCKADDR*)&Socket_Info.peer , &len);
		if(Socket_Info.s != SOCKET_ERROR)
			CreateThread(NULL , NULL , Thread_FileRecv , &Socket_Info , NULL , NULL);
	}
	return 0;
}

int __stdcall WinMain(HINSTANCE hInstance , HINSTANCE hPrevInstance, LPSTR lpCmdLine , int nCmdShow)
{
	WNDCLASS winset;
	WNDCLASSEX winsetex;
	MSG Message;
	HWND hWnd;
	int error;
	hInstance_copy = hInstance;
	nCmdShow_copy = nCmdShow;

	winsetex.cbClsExtra = NULL;
	winsetex.cbWndExtra = NULL;
	winsetex.hbrBackground = (HBRUSH)CreateSolidBrush(RGB(0x27, 0x27 , 0x27));
	winsetex.hCursor = LoadCursor(NULL, IDC_ARROW);
	winsetex.hIcon = (HICON)LoadIcon(hInstance , MAKEINTRESOURCE(IDI_ICON));
	winsetex.hIconSm = (HICON)LoadIcon(hInstance , MAKEINTRESOURCE(IDI_ICON));
	error = GetLastError();
	winsetex.hInstance = hInstance;
	winsetex.lpfnWndProc = (WNDPROC)WndProc;
	winsetex.lpszClassName = lpClassName;
	winsetex.lpszMenuName = NULL;
	winsetex.cbSize = sizeof(WNDCLASSEX);
	winsetex.style = CS_HREDRAW | CS_VREDRAW;
	RegisterClassEx(&winsetex);

	winset.cbClsExtra = NULL;
	winset.hCursor = LoadCursor(NULL, IDC_ARROW);
	winset.hIcon = (HICON)LoadIcon(NULL , IDI_APPLICATION);
	winset.hInstance = hInstance;
	winset.lpszMenuName = NULL;
	winset.style = CS_HREDRAW | CS_VREDRAW;
	winset.hbrBackground = (HBRUSH)CreateSolidBrush(RGB(0x37 , 0x37 , 0x37));
	winset.cbWndExtra = sizeof(LONG_PTR);
	winset.lpfnWndProc = (WNDPROC)TITLE_Proc;
	winset.lpszClassName = L"title";
	RegisterClass(&winset);

	winset.lpfnWndProc = (WNDPROC)X_BUTTON_Proc;
	winset.lpszClassName = L"X";
	RegisterClass(&winset);

	winset.hbrBackground = (HBRUSH)CreateSolidBrush(RGB(0x20 , 0x20 , 0x20));
	winset.lpfnWndProc = (WNDPROC)IP_LIST_Proc;
	winset.lpszClassName = L"iplist";
	RegisterClass(&winset);

	winset.hbrBackground = (HBRUSH)CreateSolidBrush(RGB(0x20 , 0x20 , 0x20));
	winset.lpfnWndProc = (WNDPROC)CHAT_LIST_Proc;
	winset.lpszClassName = L"chatlist";
	RegisterClass(&winset);

	winset.hbrBackground = (HBRUSH)CreateSolidBrush(RGB(0x20 , 0x20 , 0x20));
	winset.lpfnWndProc = (WNDPROC)FILE_BUTTON_Proc;
	winset.lpszClassName = L"filesend";
	RegisterClass(&winset);

	winset.hbrBackground = (HBRUSH)CreateSolidBrush(RGB(0x20 , 0x20 , 0x20));
	winset.lpfnWndProc = (WNDPROC)LOGO_Proc;
	winset.lpszClassName = L"logo";
	RegisterClass(&winset);

	winset.lpfnWndProc = (WNDPROC)TRANSFER_STATE_Proc;
	winset.lpszClassName = L"transferstate";
	RegisterClass(&winset);

	hWnd = CreateWindow(lpClassName , lpClassName , WS_POPUP , GetSystemMetrics(SM_CXSCREEN)/2 - 210  , GetSystemMetrics(SM_CYSCREEN)/2 - 305 , 420 , 610 , NULL , (HMENU)NULL , hInstance , NULL);
	CreateWindow(L"logo" , NULL , WS_POPUP | WS_VISIBLE ,  GetSystemMetrics(SM_CXSCREEN)/2 - 300  , GetSystemMetrics(SM_CYSCREEN)/2 - 225 , 600 , 450 , hWnd , NULL , hInstance_copy , NULL);

	while(GetMessage(&Message , 0 , 0 , 0))
	{
		TranslateMessage(&Message);
		DispatchMessage(&Message);
	}

	return Message.wParam;
}

LRESULT CALLBACK WndProc(HWND hWnd , UINT Message , WPARAM wParam , LPARAM lParam)
{
	HDC hDC , MemDC;
	static HPEN hPen = CreatePen(NULL , 1 , RGB(50,230,230));
	static char framecolor , buf[] = "byebye";
	static HPEN KhPen = CreatePen(NULL , 1 , RGB(95,95,95));
	static HFONT hFont_L = CreateFont(15,0,0,0,1000,0,0,0,HANGEUL_CHARSET,0,0,0,FF_ROMAN,L"���� ���");
	static HBRUSH hBrush  = CreateSolidBrush(RGB(0x20, 0x20 , 0x20));
	WSADATA wsa;
	static HWND hWnd_List , hWnd_Chat , hWnd_Input , hWnd_FileBut , hWnd_State; 
	static HANDLE Chat_Thread;
	PAINTSTRUCT ps;
	
	switch(Message)
	{
	case WM_ACTIVATEAPP:
		framecolor = 10; // 10 �̶�� ȸ�� �׵θ�;
		InvalidateRect(hWnd , NULL , TRUE);
		return 0;


	case WM_ACTIVATE:
		framecolor = 100; // 100�̸� �ķ���;
		InvalidateRect(hWnd , NULL , TRUE);
		return 0;

	case WM_CREATE:
		WSAStartup(MAKEWORD(2,2),&wsa);
		InitializeCriticalSection(&cs_list);
		InitializeCriticalSection(&cs_chat);
		hWnd_Res_1 = hWnd;

		CreateWindow(L"title" , NULL , WS_CHILD | WS_VISIBLE , 1 , 1 , 418 , 20 , hWnd , NULL , NULL , NULL);
		hWnd_FileBut	= CreateWindow(L"filesend" , NULL , WS_CHILD | WS_VISIBLE , 280 , 555 , 125 , 40 , hWnd , NULL , hInstance_copy , NULL);
		hWnd_Res_1 = hWnd_FileBut;
		hWnd_List		= CreateWindow(L"iplist" , NULL , WS_CHILD | WS_VISIBLE , 15 , 43 , 390 , 315 , hWnd , (HMENU)0 , hInstance_copy , &hWnd_FileBut);
		X_BUT				= LoadBitmap(GetModuleHandle(NULL) , MAKEINTRESOURCE(BIT_X_BUTTON));
		LOGO_BIT		= LoadBitmap(GetModuleHandle(NULL) , MAKEINTRESOURCE(BIT_LOGO));
		LOADBAR_BIT	= LoadBitmap(GetModuleHandle(NULL) , MAKEINTRESOURCE(BIT_LOADBAR));
		STATE_BIT		= LoadBitmap(GetModuleHandle(NULL) , MAKEINTRESOURCE(BIT_STATE));
		framecolor		= 100;

		AD_Thread		= CreateThread(NULL , NULL , Thread_BroadcastAD, NULL , NULL , NULL);

		hWnd_Chat		= CreateWindow(L"chatlist" , NULL , WS_CHILD | WS_VISIBLE , 15, 368 , 390 , 150 , hWnd , (HMENU)0 , hInstance_copy , NULL);
		hWnd_Input		= CreateWindow(L"edit" , NULL , WS_CHILD | WS_VISIBLE , 70 , 528 , 335 , 15 , hWnd , (HMENU)0 , hInstance_copy , NULL);
		hWnd_State		= CreateWindow(L"transferstate" , NULL , WS_CHILD | WS_VISIBLE , 30 , 550 , 220 , 50 , hWnd , NULL , hInstance_copy , NULL);
		*(HWND*)&hWnd_ChatList_copy = hWnd_Chat;

		SendMessage(hWnd_Input, WM_SETFONT, (WPARAM)hFont_L, MAKELPARAM(FALSE,0));  // ����Ʈ ��Ʈ�� ��Ʈ �ٲ�;

		chat_dest.sin_addr.s_addr = htonl(INADDR_BROADCAST);
		chat_dest.sin_family = AF_INET;
		chat_dest.sin_port = htons(CHAT_PORT_UDP);
		oldProc = (WNDPROC)SetWindowLongPtr(hWnd_Input , GWLP_WNDPROC , (LONG_PTR)hWnd_Input_Sub); //����Ŭ����;
		SRCH_Thread		= CreateThread(NULL , NULL , Thread_GetHost, (LPVOID)hWnd_List , NULL , NULL);
		SendMessage(hWnd_List , WM_USER+10 , (WPARAM)hWnd_FileBut , NULL);
		return 0;

	case  WM_CTLCOLOREDIT : //����Ʈ�ڽ� �����̳� �ѹ� �ٲ㺼��?;
		if((HWND)lParam == (HWND)hWnd_Input)
		{
			SetTextColor( (HDC)wParam , RGB(0xe7 , 0xe7 , 0xe7) );
			SetBkColor( (HDC)wParam , RGB(0x20, 0x20 , 0x20) );
			return (LRESULT)hBrush;
		}
		break;

	case WM_USER:
		InvalidateRect(hWnd , NULL , FALSE);
		return 0;
		
	case WM_USER+10:
		Chat_Thread = (HANDLE)wParam;
		return 0;

	case WM_PAINT :
		hDC = BeginPaint(hWnd , &ps);
		SelectObject(hDC , (HBRUSH)GetStockObject(NULL_BRUSH) );
		SelectObject(hDC , (HFONT)hFont_L);
		SetTextColor(hDC , RGB(0x8e , 0x8e , 0x8e) );
		SetBkMode(hDC , TRANSPARENT);
		if(framecolor == 10) //��Ŀ���� �Ҿ�����;
			SelectObject(hDC , (HPEN)KhPen );

		if(framecolor == 100) // ��Ŀ���� �������;
			SelectObject(hDC , (HPEN)hPen);

		Rectangle(hDC , 0 , 0 , 420 , 610);
		TextOut(hDC , 17 ,25 , L"������ ����Ʈ" , 7);
		TextOut(hDC , 240 , 25 , L"�� IP �ּ� : ", 11 );
		TextOut(hDC , 15 , 528 , L"�޽���  : " , 7 );
		SetTextColor(hDC , RGB(50,230,230));
		TextOut(hDC , 308 , 25 , Localhost , lstrlen(Localhost));
		EndPaint(hWnd , &ps);
		return 0;

	case WM_DESTROY:
		sendto(ad_sock , buf , 6 , 0 , (SOCKADDR*)&ad_sp , 16 );
		SuspendThread(SRCH_Thread);	// ȣ��Ʈ Ž�� �����带 ��� ���������� �� �����ϱ� ���ؼ� �ۼ�����. - �̰� �� ������ �ȿ��ִ� recvfrom���� �ڲ� �˼����� ������ ���� ���ذǵ� ������ ���� �𸣰ڴ� ;
		//SuspendThread(LIFE_Thread);
		SuspendThread(Chat_Thread);
		SuspendThread(FLSRV_Thread);
		SuspendThread(LBSRM_Thread);
		DeleteObject((HBITMAP) X_BUT);
		DeleteObject((HBITMAP) LOGO_BIT);
		DeleteObject((HBRUSH)hBrush);
		//_close(chat_sock);
		//DeleteCriticalSection(&cs_list);
		//DeleteCriticalSection(&cs_chat);

		WSACleanup();
		PostQuitMessage(0);
		return 0;

	}

	return DefWindowProc(hWnd , Message , wParam , lParam);
}

LRESULT CALLBACK IP_LIST_Proc(HWND hWnd, UINT Message, WPARAM wParam, LPARAM lParam)
{
	HBITMAP hBit;
	static HFONT hFont_List  , OldFont;
	static unsigned char Host_Index;
	static RECT crt;
	static POINT pt;
	static HDC hDC , MemDC , BitmapDC;
	static HBITMAP OldBitmap ,OldLoadbarBitmap;
	static HBRUSH hBrush_Hot , OldBrush_Hot;
	static HBRUSH hBrush_bk  , OldBrush_bk;
	static HBRUSH hBrush_ld  , OldBrush_ld;
	static HBRUSH hBrush_hl  , OldBrush_hl;
	static HPEN hPen , OldPen , hPen_NULL , OldNullpen;
	static WCHAR ExitCount[5] = {0,};
	static int init_state = 0  , init_select=-1;
	static int hl=0 , TimerControler ;
	static HWND hWnd_FileBut;
	PAINTSTRUCT ps;

	switch(Message)
	{
	case WM_CREATE:
		GetWindowRect(hWnd , &crt);
		LIFE_Thread			= CreateThread(NULL , NULL , Thread_LifeCheck , hWnd , NULL ,  NULL);
		LBSRM_Thread = CreateThread(NULL , NULL , Thread_StreamBar , hWnd , NULL ,  NULL);
		return 0;

	case WM_LBUTTONDOWN:
		SetFocus(GetParent(hWnd));
		GetCursorPos(&pt);
		GetWindowRect(hWnd , &crt);

		pt.y =pt.y-crt.top;
		init_select = pt.y / 15;						// ���õ� ��ǥ�� ����ؼ� ȸ�� ���·� ���� �������� �ʰ� ���ش� ;
		hl =252;
		if(init_select < Detected_Host_Index)	
		{
			wcscpy_s(AddressResolution , 20, Detected_Host[init_select].Host);
			SendMessage(hWnd_FileBut , WM_USER , NULL , NULL);
		}
		
		SetTimer(hWnd , 100 , 10 , NULL);
		return 0;
	
	case WM_MOUSEMOVE:
		SetCapture(hWnd);
		GetCursorPos(&pt);
		if(PtInRect(&crt , pt) == FALSE)
		{
			ReleaseCapture();
		}
		InvalidateRect(hWnd , NULL , FALSE);
		return 0;

	case WM_USER:
		InvalidateRect(hWnd , NULL , FALSE);
		return 0;

	case WM_USER+10:
		hWnd_FileBut = (HWND)wParam;
		return 0;

	case WM_PAINT:
		GetCursorPos(&pt);
		GetWindowRect(hWnd , &crt);
		
		hDC = BeginPaint(hWnd , &ps);
		hBit = CreateCompatibleBitmap(hDC , 390 , 315);
		MemDC = CreateCompatibleDC(hDC);
		BitmapDC = CreateCompatibleDC(hDC);

		hPen = CreatePen(NULL , 1 , RGB(70,70,70) );
		hFont_List = CreateFont(14 , 0 , 0 , 0 , 400 , 0 , 0 , 0 , HANGEUL_CHARSET , 0 , 0 , 0 , FF_ROMAN , L"���� ���");
		hBrush_bk = CreateSolidBrush(RGB(0x20,0x20,0x20));

		OldBitmap = (HBITMAP)SelectObject(MemDC , hBit);
		OldBrush_bk = (HBRUSH)SelectObject(MemDC , (HBRUSH)hBrush_bk);
		OldFont = (HFONT)SelectObject(MemDC , (HFONT)hFont_List);
		
		SetBkMode(MemDC , TRANSPARENT);
		SetTextColor(MemDC , RGB(0xe7 , 0xe7 ,0xe7));
		pt.y =pt.y-crt.top;
		pt.y /= 15;
		Rectangle(MemDC , -1 , -1 , 391 , 316);

		hPen_NULL = (HPEN)GetStockObject(NULL_PEN);
		OldNullpen = (HPEN)SelectObject(MemDC , hPen_NULL);
		hBrush_Hot = CreateSolidBrush(RGB(0x37,0x37,0x37) );
		OldBrush_Hot = (HBRUSH)SelectObject(MemDC , (HBRUSH)hBrush_Hot);

		EnterCriticalSection(&cs_list);
		if(pt.y < Detected_Host_Index)
			Rectangle(MemDC , 0 , pt.y*15 , crt.right-crt.left , (pt.y*15)+15);
		
		hBrush_hl = (HBRUSH)CreateSolidBrush(RGB(hl,hl,hl));
		OldBrush_hl = (HBRUSH)SelectObject(MemDC , hBrush_hl);
		if(init_select < Detected_Host_Index)
			Rectangle(MemDC , 0 , init_select*15 , crt.right-crt.left , (init_select*15)+15);

		TimerControler = 0;
		for(Host_Index = 0 ; Host_Index < Detected_Host_Index ; Host_Index++)
		{
			if(Detected_Host[Host_Index].Life < 16)
			{
				SetTextColor(MemDC , RGB(0xff , 0xcc ,0x66));
				if(Detected_Host[Host_Index].Life < 9)	SetTextColor(MemDC , RGB(0xff , 0x88 ,0xaa)); 			
			}

			TextOut(MemDC , 2 , (Host_Index*15)+1 , (LPCWSTR)Detected_Host[Host_Index].Host , lstrlen((LPCWSTR)Detected_Host[Host_Index].Host));
			wsprintf(ExitCount , L"<%d>" , Detected_Host[Host_Index].Life);
			TextOut(MemDC , 90 , (Host_Index*15)+1 , ExitCount ,lstrlen(ExitCount));
			/////////////////////////////////////////////////;���⿡ �ε��� ǥ�� ��ƾ ���� ����;////////////////////////////////////////////////////
			if(Detected_Host[Host_Index].FileFlag == TRUE)
			{
				TimerControler = 1;
				SetTimer(hWnd , 200 , 0 , NULL);
				hBrush_ld = CreateSolidBrush(RGB(0x17 , 0x17 , 0x17));

				OldBrush_ld = (HBRUSH)SelectObject(MemDC , hBrush_ld);
				Rectangle(MemDC , 150 , (Host_Index*15)+4 , 150+LOADBAR_SIZE , (Host_Index*15)+10 ); // �ε��� ���;
				SelectObject(MemDC , OldBrush_ld);
				DeleteObject(hBrush_ld);

				OldLoadbarBitmap = (HBITMAP)SelectObject(BitmapDC , LOADBAR_BIT);
				BitBlt(MemDC , 150 , (Host_Index*15)+4 , Detected_Host[Host_Index].ProgressPoint , 5 , BitmapDC , StreamLoadbar , 0 , SRCCOPY);
				SelectObject(BitmapDC , OldLoadbarBitmap);
			}

			////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
			SetTextColor(MemDC , RGB(0xe7 , 0xe7 ,0xe7));
		}

		if(TimerControler == 0) KillTimer(hWnd , 200 );

		LeaveCriticalSection(&cs_list);
		BitBlt(hDC , 0 , 0 , 390 , 315 , MemDC , 0 , 0 , SRCCOPY);

		SelectObject(MemDC , OldBitmap);
		SelectObject(MemDC , OldBrush_bk);
		SelectObject(MemDC , OldBrush_hl);
		SelectObject(MemDC , OldBrush_Hot);
		SelectObject(MemDC , OldNullpen);
		SelectObject(MemDC , OldPen);
		SelectObject(MemDC , OldFont);

		DeleteObject(hBit);
		DeleteObject(hBrush_bk);
		DeleteObject(hBrush_hl);
		DeleteObject(hBrush_Hot);
		DeleteObject(hPen_NULL);
		DeleteObject(hPen);
		DeleteObject(hFont_List);

		DeleteDC(MemDC);
		DeleteDC(BitmapDC);
		EndPaint(hWnd , &ps);
		if(hl == 0x20){
			KillTimer(hWnd , 100); init_select = -1;}
		return 0;

	case WM_TIMER:
		switch(wParam)
		{
		case 100:
			hl-=5;
			InvalidateRect(hWnd , NULL , FALSE);
			return 0;

		case 200:
			InvalidateRect(hWnd , NULL , FALSE);
			return 0;
		}
		return 0;
	}

	return DefWindowProc(hWnd , Message , wParam, lParam);
}

LRESULT CALLBACK TITLE_Proc(HWND hWnd , UINT Message , WPARAM wParam , LPARAM lParam)
{
	static int InitSet = 0 ;
	PAINTSTRUCT ps;
	static HFONT hFont;
	HDC hDC ;
	static RECT crt;
	static POINT pt;
	static int x;
	static int y;
	HWND P_hWnd = GetParent(hWnd);

	switch(Message)
	{
	case WM_CREATE:
		hFont = CreateFont( 13 , 0 , 0 , 0 , 700 , 0 , 0 , 0 , HANGEUL_CHARSET , 0 , 0 , 0 , FF_ROMAN , L"���� ���");
		CreateWindow(L"X" , NULL , WS_CHILD | WS_VISIBLE , 378 , 0 , 40 , 20 , hWnd , NULL ,NULL , NULL);
		return 0;

	case WM_PAINT :
		hDC = BeginPaint(hWnd , &ps);
		SelectObject(hDC , hFont);
		SetTextColor(hDC,RGB(225,225,225));									//��Ʈ�� ��� �迭�� �������ְ�;
		SetBkMode(hDC , TRANSPARENT);									//��� ������ �����ϰ� �ٲ��� ��;
		TextOut(hDC , 140 , 4,lpClassName,lstrlen(lpClassName));			//�ؽ�Ʈ ���;
		DeleteDC(hDC);
		EndPaint(hWnd , &ps);													//�׸��� ����;
		return 0;

	case WM_LBUTTONDOWN:				// ���콺 ��ư�� ������;
		GetCursorPos(&pt);					//�ϴ� ���콺 Ŀ���� ��ǥ�� ���ϰ�;
		GetWindowRect(P_hWnd , &crt);	// ������ ��ü ũ�⸦ ���Ѵ���.;
		x =  pt.x - crt.left;						//�����찡 �׷����� �����ϴ� ��ǥ��;
		y =  pt.y - crt.top;						// ���ϰ� ����;
		SetCapture(hWnd);					// ���콺 ĸ�ĸ� �����Ѵ�.;
		InitSet = 1;								// �׸��� ������ ������ 1�� ������ ���콺 ���� �޼��� ��ƾ�� ó�� �� �� �ֵ��� ���ش�.;
		return 0;

	case WM_MOUSEMOVE:
		if(InitSet == 1)																		// ������ ������ 1�̶��;
		{
			GetCursorPos(&pt);															// ���콺�� �����϶����� ��ǥ�� ���ϰ�;	
			MoveWindow(P_hWnd , pt.x - x , pt.y -y , 420 , 610 , TRUE );		// ���콺 ��ǥ�� ���� �����츦 �̵������ش�;
		}
		return 0;

	case WM_LBUTTONUP :
		ReleaseCapture();			// ���콺 ��ư�� �������� ĸ�ĸ� �׸� �ΰ� ������ �̵��� �׸� �д�;
		InitSet = 0;					// ������ ������ 0���� ������ ���콺 ���� �޼����� ���ϰ� �ɸ��°��� �����Ѵ�;
		return 0;
	}

	return DefWindowProc(hWnd,Message,wParam,lParam);
}

LRESULT CALLBACK X_BUTTON_Proc(HWND hWnd , UINT Message , WPARAM wParam , LPARAM lParam)
{
	HDC hDC , MemDC ;
	POINT CurPT;
	RECT crt;
	PAINTSTRUCT ps;
	static int lux=0;
	static int LockInit = 0;
	static char buf[] = "byebye";
	switch(Message)
	{	
	case WM_MOUSEMOVE:
		if(LockInit == 0)
		{
			LockInit = 1;
			SetTimer(hWnd,X_BUTTON,20,NULL);
		}
		return 0;

	case WM_PAINT :
		hDC = BeginPaint(hWnd , &ps);
		MemDC = CreateCompatibleDC(hDC);
		SelectObject(MemDC , X_BUT);
		BitBlt(hDC , 0 , 0 , 40 , 20 , MemDC , lux*40 , 0 , SRCCOPY);
		DeleteDC(MemDC);
		DeleteDC(hDC);
		EndPaint(hWnd , &ps);
		return 0;

	case WM_TIMER:
		GetWindowRect(hWnd ,&crt);
		GetCursorPos(&CurPT);

		hDC = GetDC(hWnd);
		MemDC = CreateCompatibleDC(hDC);
		SelectObject(MemDC , X_BUT);

		switch(wParam)
		{
		case X_BUTTON:

			if(LockInit == 0)
				LockInit = 1;

			if(lux<13)
			{
				lux++;
				BitBlt(hDC , 0 , 0 , 40 , 20 , MemDC , lux*40 , 0, SRCCOPY);
			}

			if(PtInRect(&crt , CurPT) == FALSE)
			{	
				SetTimer(hWnd , X_BUTTON+100 , 20 , NULL);
				KillTimer(hWnd , X_BUTTON);
				LockInit = 0;
			}
			DeleteDC(MemDC);
			ReleaseDC(hWnd , hDC);
			return 0;

		case X_BUTTON+100:
			if(lux<14)
			{
				lux--;
				BitBlt(hDC , 0 , 0 , 40 , 20 , MemDC , lux*40 , 0, SRCCOPY);
			}

			if(PtInRect(&crt , CurPT) == TRUE)
			{
				LockInit = 0;
				SetTimer(hWnd , X_BUTTON , 20 ,NULL);
				KillTimer(hWnd , X_BUTTON+100);
			}

			if(lux == 0)
			{
				KillTimer(hWnd, X_BUTTON+100);
			}
			DeleteDC(MemDC);
			ReleaseDC(hWnd , hDC);
			return 0;
		}
		DeleteObject(X_BUT);
		return 0;

	case WM_LBUTTONUP:
		DeleteObject(X_BUT);
		SendMessage(GetParent( GetParent(hWnd) ) , WM_DESTROY , NULL , NULL);
		return 0;
	}

	return DefWindowProc(hWnd , Message , wParam , lParam);
}

LRESULT CALLBACK CHAT_LIST_Proc(HWND hWnd , UINT Message , WPARAM wParam , LPARAM lParam)
{
	HDC hDC,MemDC;
	static HFONT hFont ,OldFont;
	static HFONT hFont_S , OldFont_S;
	static HPEN hPen , OldPen;
	static HBITMAP OldBitmap;
	static HBRUSH hBrush , OldBrush;
	HBITMAP hBit;
	HANDLE CHAT_Thread;
	SOCKADDR_IN sp = {0,};
	static SOCKADDR_IN peer;
	PAINTSTRUCT ps;
	static RECT crt;

	static WCHAR buf[12][256];
	static int line , recvnum=0;

	char opt = 1;
	int error;
	switch(Message)
	{
	case  WM_CREATE :
		chat_sock = socket(AF_INET , SOCK_DGRAM , IPPROTO_UDP);
		if(chat_sock == INVALID_SOCKET)
		{
			MessageBox(hWnd , L"ä�ü��� ���� ����" , L"����" , MB_OK);
			SendMessage(GetParent(hWnd) , WM_DESTROY , NULL ,NULL);
		}
		setsockopt(chat_sock , SOL_SOCKET , SO_BROADCAST , &opt , sizeof(opt));

		sp.sin_port = htons((u_short)CHAT_PORT_UDP);
		sp.sin_addr.s_addr = htonl(INADDR_ANY);
		sp.sin_family = AF_INET;
		error = WSAGetLastError();
		if(bind(chat_sock , (SOCKADDR*)&sp , 16) != 0)
		{
			MessageBox(hWnd , L"ä���� �� �Ͻð� �Ǿ��׿�" , L"����" , MB_OK);
			SendMessage(GetParent(hWnd) , WM_DESTROY , NULL ,NULL);
		}
		CHAT_Thread	= CreateThread(NULL , NULL , Thread_Chatting , (LPVOID)hWnd , NULL , NULL);
		SendMessage(GetParent(hWnd) , WM_USER+10 , (WPARAM)CHAT_Thread , NULL);
		GetWindowRect(hWnd , &crt);
		return 0;

	case WM_USER:
		wcscpy_s(buf[recvnum] , sizeof(buf[0]), (WCHAR*)wParam);
		recvnum++;
		if(recvnum > 11)
		{
			for(line = 0 ; line < 11 ; line ++)
				wcscpy_s(buf[line] , sizeof(buf[0]), buf[line+1]);

			recvnum=11;
	
		}
		InvalidateRect(hWnd , NULL ,FALSE);
		return 0;

	case WM_PAINT:
		hDC = BeginPaint(hWnd , &ps);
		hBit = CreateCompatibleBitmap(hDC , crt.right , crt.bottom);
		MemDC = CreateCompatibleDC(hDC);
		//hFont =  CreateFont(15,0,0,0,1000,0,0,0,HANGEUL_CHARSET,0,0,0,FF_ROMAN,L"���� ���");
		hFont_S =  CreateFont(12,0,0,0,1000,0,0,0,HANGEUL_CHARSET,0,0,0,FF_ROMAN,L"���� ���");
		hPen = (HPEN)GetStockObject(NULL_PEN);
		OldBitmap = (HBITMAP)SelectObject(MemDC , hBit);
		OldPen = (HPEN)SelectObject(MemDC , hPen);
		hBrush = (HBRUSH)CreateSolidBrush(RGB(0x20,0x20,0x20));
		OldBrush = (HBRUSH)SelectObject(MemDC , hBrush);
		SetBkMode(MemDC , TRANSPARENT);
		SetTextColor(MemDC , RGB(0xe7 ,0xe7 ,0xe7));
		OldFont_S = (HFONT)SelectObject(MemDC , (HFONT) hFont_S);

		Rectangle(MemDC , 0 , 0 , crt.right , crt.bottom);
		for(line=0 ; line < 11 ; line++)
		{
			if(wcscmp(buf[line], L"������ ���� �� �� �����ϴ�.") == 0)
				//SelectObject();
			if( lstrlen(buf[line]) == 0) return 0;		// ��������� �Ѿ��;
			TextOut(MemDC , 10 , 10 + line*12 , buf[line] , lstrlen(buf[line]));
		}
		BitBlt(hDC , 0 , 0 , crt.right , crt.bottom , MemDC , 0 , 0 , SRCCOPY);

		SelectObject(MemDC , OldBitmap);
		SelectObject(MemDC , OldBrush);
		SelectObject(MemDC , OldFont_S);
		SelectObject(MemDC , OldPen);

		DeleteObject(hBit);
		DeleteObject(hBrush);
		DeleteObject(hFont_S);
		DeleteObject(hPen);

		DeleteDC(MemDC);
		EndPaint(hWnd , &ps);
		return 0;
	}

	return DefWindowProc(hWnd , Message ,wParam , lParam);
}

LRESULT CALLBACK hWnd_Input_Sub(HWND hWnd , UINT Message, WPARAM wParam, LPARAM lParam)
{
	static WCHAR buf[256];
	static int error ;
	switch(Message)
	{
	case WM_KEYDOWN:
		switch(wParam)
		{
		case VK_RETURN:
			memset(buf , 0 , sizeof(buf));
			GetWindowText(hWnd , buf , sizeof(buf));

			if( lstrlen(buf) == 0)
			{
				wcscpy_s(buf , sizeof(buf) , L"������ ���� �� �� �����ϴ�.");
				SendMessage(hWnd_ChatList_copy , WM_USER , (WPARAM)&buf , NULL );
				return 0;
			}

			if( sendto(chat_sock , (char*)&buf , lstrlen(buf)<<1 , NULL , (SOCKADDR*)&chat_dest , sizeof(SOCKADDR)) == SOCKET_ERROR )
			{
				wsprintf(buf , L" sendto %d ����" , WSAGetLastError());
				MessageBox(hWnd , (LPCWSTR)buf , L"����" , MB_OK);
			}

			SetWindowText(hWnd , NULL);
			return 0;
		}
		return 0;
	}
	return CallWindowProc((WNDPROC)oldProc , hWnd , Message , wParam , lParam);
}

LRESULT CALLBACK LOGO_Proc(HWND hWnd , UINT Message , WPARAM wParam , LPARAM lParam)
{
	HDC hDC , MemDC;
	PAINTSTRUCT ps;
	static int i=0;

	switch(Message)
	{
	case WM_CREATE:
		SetWindowLong(hWnd , GWL_EXSTYLE , GetWindowLong(hWnd , GWL_EXSTYLE) | WS_EX_LAYERED );
		SetWindowPos(hWnd , HWND_TOPMOST , 0,0,0,0, SWP_NOMOVE | SWP_NOSIZE);
		SetTimer(hWnd , 1000 , 0 , NULL);
		InvalidateRect(hWnd , NULL , FALSE);
		return 0;

	case WM_PAINT:
		hDC = BeginPaint(hWnd , &ps);
		MemDC = CreateCompatibleDC(hDC);
		SelectObject(MemDC , (HBITMAP)LOGO_BIT);
		BitBlt(hDC , 0 , 0 , 600 , 450 , MemDC , 0 , 0, SRCCOPY);
		SelectObject(hDC , (HPEN)CreateSolidBrush(RGB(0,0,0)));
		SelectObject(hDC , (HBRUSH)GetStockObject(NULL_BRUSH));

		Rectangle(hDC , 0 , 0 , 600 , 450);
		EndPaint(hWnd , &ps);
		DeleteDC(MemDC);
		return 0;

	case WM_LBUTTONDOWN:
		KillTimer(hWnd , 1000);
		SetTimer(hWnd , 2000 , 0 , NULL);
		return 0;

	case WM_TIMER:
		switch(wParam)
		{
		case 1000:
			SetLayeredWindowAttributes(hWnd , 0 , i , LWA_ALPHA);
			i+=5;
			if(i > 255) 
			{
				KillTimer(hWnd , 1000);
				i-=5;
			}
			return 0;

		case 2000:
			SetLayeredWindowAttributes(hWnd , 0 , i , LWA_ALPHA);
			i-=5;
			if(i<0)
			{
				KillTimer(hWnd , 2000);
				ShowWindow(hWnd , SW_HIDE);
				ShowWindow(GetParent(hWnd) , SW_SHOW);
			}
		}
	}

	return DefWindowProc(hWnd , Message , wParam , lParam);
}

LRESULT CALLBACK FILE_BUTTON_Proc(HWND hWnd , UINT Message , WPARAM wParam , LPARAM lParam)
{
	HDC hDC , MemDC;
	HANDLE FMThread;
	PAINTSTRUCT ps;
	HBITMAP hBit , OldhBit;
	OPENFILENAME ofn = {0,};
	static struct FileSendInfo fsi;
	static POINT pt;
	static RECT crt;
	static BOOL IPsel = FALSE , arm = FALSE;
	static WCHAR Address_Buffer[20] = {0,} , res[20]={0,};
	static HFONT hFont;
	static HFONT hFont_Big , OldhFont;
	static int wx,wy,pnt=0,i=0 , clr=0 , movement = 0 , g_pnt=0 , flag=0 , src;
	static HBRUSH hBrush , OldhBrush , hBrush_Green , OldhBrush_Green;
	static HPEN hPen , OldhPen;
	static SIZE sz;
	static int Index;
	switch(Message)
	{ 
	case WM_CREATE:
		GetWindowRect(hWnd , &crt);
		wx = crt.right - crt.left;
		wy = crt.bottom - crt.top;
		wsprintf(Address_Buffer , L"<IP �ּ� ����>");
		src = 0;
		FLSRV_Thread = CreateThread(NULL , NULL , Thread_TrafficManagement , NULL , NULL , NULL);
		return 0;

	case WM_MOUSEMOVE:
		if(IPsel == TRUE)
		{
			GetWindowRect(hWnd , &crt);
			GetCursorPos(&pt);

			if(flag == 0)
			{
				arm = FALSE;
				flag =1;
				SetCapture(hWnd);
				KillTimer(hWnd , 300);
				SetTimer(hWnd , 200 , 15 , NULL);
			}

			if(PtInRect(&crt , pt) == FALSE)
			{
				arm = FALSE;
				flag =0;
				KillTimer(hWnd , 200);
				SetTimer(hWnd , 300 , 15 , NULL);
				ReleaseCapture();
			}
		}
		return 0;

	case WM_LBUTTONDOWN:
		if(IPsel == TRUE)
		{
			wcscpy_s(fsi.IPAddress , Address_Buffer);
			ofn.lStructSize = sizeof(ofn);
			ofn.hwndOwner = hWnd;
			ofn.lpstrFilter = L"��� ����(*.*)\0*.*\0";
			ofn.lpstrFile = fsi.FilePath;
			ofn.lpstrFileTitle = fsi.FileName;
			ofn.nMaxFile = MAX_PATH;
			ofn.nMaxFileTitle = MAX_PATH;
			for(Index = 0 ; Index<Detected_Host_Index ; Index++)
			{
				if(wcscmp(Address_Buffer , Detected_Host[Index].Host) == 0)
				{
					if(Detected_Host[Index].FileFlag == TRUE)
					{
						MessageBox(hWnd , L"�̹� �� ȣ��Ʈ���� ������ ������ �Դϴ�." , L"����" , MB_OK);
						flag = 0;
						arm = FALSE;
						KillTimer(hWnd , 200);
						SetTimer(hWnd , 300 , 15 , NULL);
						return 0;
					}
				}
			}
			if( GetOpenFileName(&ofn) != 0)
				CreateThread(NULL , NULL , Thread_FileSend , &fsi , NULL , NULL);
			flag = 0;
			arm = FALSE;
			KillTimer(hWnd , 200);
			SetTimer(hWnd , 300 , 15 , NULL);
		}
		return 0;

	case WM_USER: // IP�ּ� ����Ʈ���� IP �ּҸ� Ŭ���ϸ� �� �޼����� ������ �ȴ�;
		IPsel = TRUE;
		wcscpy_s(res , 20 , Address_Buffer);
		wcscpy_s(Address_Buffer , 20 , AddressResolution);
		SetTimer(hWnd , 100 , 15 , NULL);
		i=20;
		pnt = 210;
		InvalidateRect(hWnd , NULL , FALSE);
		return 0;

	case WM_TIMER:
		switch(wParam)
		{
		case 100:
			pnt = pnt-i;
			--i;
			InvalidateRect(hWnd , NULL , FALSE);
			return 0;

		case 200:
			KillTimer(hWnd , 300);
			if(arm == TRUE)
			{
				arm = FALSE;
				KillTimer(hWnd , 200);
			}
			src = g_pnt;
			g_pnt = MovementPoint(src , 125 , 10 , &arm);
			InvalidateRect(hWnd , NULL , FALSE);
			return 0;

		case 300:
			KillTimer(hWnd , 200);
			if(arm == TRUE) 
			{
				arm = FALSE;
				KillTimer(hWnd , 300);
			}
			src = g_pnt;
			g_pnt = MovementPoint(src , 0 , 10 , &arm);
			InvalidateRect(hWnd , NULL , FALSE);
			return 0;
		}
		return 0;

	case WM_PAINT:
		hDC = BeginPaint(hWnd , &ps);
		MemDC = CreateCompatibleDC(hDC);
		hBit = CreateCompatibleBitmap(hDC, wx<<1 , wy );
		OldhBit = (HBITMAP)SelectObject(MemDC , hBit);
		SetBkMode(MemDC , TRANSPARENT);
		SetTextColor(MemDC , RGB(255,255,255));
		hPen = (HPEN)CreatePen(NULL , 1 , RGB(0x4d,0x4d,0x4d));
		hFont = CreateFont(15 , 0 , 0 , 0 , 600 , 0 , 0 , 0 , HANGEUL_CHARSET , 0 , 0 , 0 , FF_ROMAN , L"���� ���");
		hBrush = (HBRUSH)CreateSolidBrush(RGB(65,65,65));
		OldhFont = (HFONT)SelectObject(MemDC , (HFONT)hFont);
		OldhBrush = (HBRUSH)SelectObject(MemDC , (HBRUSH)hBrush);
		OldhPen = (HPEN)SelectObject(MemDC , hPen);
		Rectangle(MemDC , 0 , 0 , wx , wy);
		TextOut(MemDC , 35 , 21 , L"�����ֱ�" , 4);
		hBrush_Green = CreateSolidBrush(RGB(115,215,135));
		if(IPsel != FALSE) 
		{
			GetTextExtentPoint(MemDC , res , lstrlen(res) ,&sz );
			TextOut(MemDC ,  pnt-(wx/2)-(sz.cx/2)-125 , 5 , res , lstrlen(res));
			GetTextExtentPoint(MemDC , Address_Buffer , lstrlen(Address_Buffer) ,&sz );
			TextOut(MemDC , pnt+(wx/2)-(sz.cx/2) , 5 , Address_Buffer , lstrlen(Address_Buffer));
		}
		else
		{			
			GetTextExtentPoint(MemDC , Address_Buffer , lstrlen(Address_Buffer) ,&sz );
			TextOut(MemDC , (wx/2)-(sz.cx/2), 5 , Address_Buffer , lstrlen(Address_Buffer));
		}
		OldhBrush_Green = (HBRUSH)SelectObject(MemDC , hBrush_Green);
		hFont_Big = CreateFont(30 , 0 , 0 , 0 , 1200 , 0 , 0 , 0 , HANGEUL_CHARSET , 0 , 0 , 0 , FF_ROMAN , L"���� ���");
		OldhFont = (HFONT)SelectObject(MemDC , (HFONT)hFont_Big);
		Rectangle(MemDC , wx , 0 , wx<<1 , wy);
		SetTextColor(MemDC , RGB(255,255,255));
		TextOut(MemDC , wx + 16 , 4 , L"���� ����" , 5);
		BitBlt(hDC , 0 - g_pnt , 0 , wx<<1 , wy , MemDC , 0 , 0 , SRCCOPY);

		SelectObject(MemDC , (HPEN)OldhPen);
		SelectObject(MemDC , (HBRUSH)OldhBrush);
		SelectObject(MemDC , (HPEN)OldhPen);
		SelectObject(MemDC , (HFONT)OldhFont);
		SelectObject(MemDC , (HBITMAP)OldhBit);
		SelectObject(MemDC , (HBRUSH)OldhBrush_Green);
		DeleteObject(hPen);
		DeleteObject(hBit);
		DeleteObject(hBrush);
		DeleteObject(hFont);
		DeleteObject(hFont_Big);
		DeleteObject(hBrush_Green);
		DeleteDC(MemDC);
		EndPaint(hWnd , &ps);
		if(pnt == 0)KillTimer(hWnd,100);
		return 0;
	}
	return DefWindowProc(hWnd , Message , wParam , lParam);	
}

LRESULT CALLBACK TRANSFER_STATE_Proc(HWND hWnd , UINT Message , WPARAM wParam , LPARAM lParam)
{
	PAINTSTRUCT ps;
	HDC hDC , MemDC , BitmapDC , StreamSBitmapDC , StreamRBitmapDC;
	HPEN hPen , OldSPen , OldRpen;
	HBRUSH hBrush , hBrushLT , OldBrush , OldBrushLT;
	HBITMAP hBit , hBitSStream , hBitRStream , OldBitmap , OldSStremBit , OldRStreamBit, OldbackBit;
	static int point = 10 , OvalAmount;

	switch(Message)
	{
	case WM_CREATE:
		SetTimer(hWnd , 100 , 20 , NULL);
		return 0;

	case WM_TIMER:
		point ++;
		if(point == 20) point = 10;
		InvalidateRect(hWnd , NULL , FALSE);
		return 0;

	case WM_PAINT:
		hDC = BeginPaint(hWnd , &ps);
		MemDC = CreateCompatibleDC(hDC);
		BitmapDC = CreateCompatibleDC(MemDC);
		StreamSBitmapDC = CreateCompatibleDC(MemDC);
		StreamRBitmapDC = CreateCompatibleDC(MemDC);
		hBit = CreateCompatibleBitmap(hDC , 220 , 50);
		hBitSStream = CreateCompatibleBitmap(hDC , 180 , 7);
		hBitRStream = CreateCompatibleBitmap(hDC , 180 , 7);

		OldBitmap = (HBITMAP)SelectObject(MemDC , hBit);


		OldbackBit = (HBITMAP)SelectObject(BitmapDC , (HBITMAP)STATE_BIT); // ��� �׷��ֱ�;
		BitBlt(MemDC , 0 , 0 , 220 , 50 , BitmapDC , 0 , 0 , SRCCOPY);
		SelectObject(BitmapDC , OldbackBit);
		DeleteDC(BitmapDC);


		OldSStremBit = (HBITMAP)SelectObject(StreamSBitmapDC , hBitSStream);
		OldRStreamBit = (HBITMAP)SelectObject(StreamRBitmapDC , hBitRStream);

		hPen = (HPEN)GetStockObject(NULL_PEN);
		hBrush = (HBRUSH)CreateSolidBrush(RGB(0x27,0x27,0x27));
		OldSPen = (HPEN)SelectObject(StreamSBitmapDC , (HPEN)hPen);
		OldBrush = (HBRUSH)SelectObject(StreamSBitmapDC , (HBRUSH)hBrush);
		Rectangle(StreamSBitmapDC , 0 , 0 , 181 , 8);
		SelectObject(StreamSBitmapDC , OldBrush);
		

		OldRpen = (HPEN)SelectObject(StreamRBitmapDC , (HPEN)hPen);
		OldBrush = (HBRUSH)SelectObject(StreamRBitmapDC , (HBRUSH)hBrush);
		Rectangle(StreamRBitmapDC , 0 , 0 , 181 , 8);
		SelectObject(StreamRBitmapDC , OldBrush);
		DeleteObject(hBrush);

		for(OvalAmount = 0 ; OvalAmount < 20 ; OvalAmount++)
		{
			if(FileRecvFlag > 0) hBrushLT = (HBRUSH)CreateSolidBrush(RGB(150,225,150));
			else hBrushLT = (HBRUSH)CreateSolidBrush(RGB(0x4d,0x4d,0x4d));
			OldBrushLT = (HBRUSH)SelectObject(StreamSBitmapDC , hBrushLT);
			Ellipse(StreamSBitmapDC , OvalAmount*10 , 0 , (OvalAmount*10)+5 , 5); // ��Ʈ���ٸ� ��Ʈ������ �ϴ� �� ���´�;
			SelectObject(StreamSBitmapDC , OldBrushLT);
			DeleteObject(hBrushLT);

			if(FileSendFlag > 0) hBrushLT = (HBRUSH)CreateSolidBrush(RGB(150,225,150));
			else hBrushLT = (HBRUSH)CreateSolidBrush(RGB(0x4d,0x4d,0x4d));
			OldBrushLT = (HBRUSH)SelectObject(StreamRBitmapDC , hBrushLT);
			Ellipse(StreamRBitmapDC , OvalAmount*10 , 0 , (OvalAmount*10)+5 , 5); // ��Ʈ���ٸ� ��Ʈ������ �ϴ� �� ���´�;
			SelectObject(StreamRBitmapDC , OldBrushLT);
			DeleteObject(hBrushLT);
		}
		SelectObject(StreamSBitmapDC , OldSPen);  //////////////// �ʵ� !! ��������� ���� ��Ʈ����Ʈ�� DC�� ������Ʈ�� �����Ǿ����� ���� !! ���� ��Ʈ�� �� �Ű� �׸��� �ʾƼ� ����� �ȴ� !!;
		SelectObject(StreamRBitmapDC , OldSPen);  //////////////// �ʵ� !! ��������� ���� ��Ʈ����Ʈ�� DC�� ������Ʈ�� �����Ǿ����� ���� !! ���� ��Ʈ�� �� �Ű� �׸��� �ʾƼ� ����� �ȴ� !!;

		DeleteObject(hBrush);
		DeleteObject(hPen);
		DeleteObject(hBrushLT);



		BitBlt(MemDC , 54 , 7 , 120 , 7 , StreamSBitmapDC , point , 0 , SRCCOPY );
		BitBlt(MemDC , 54 , 38 , 120 , 7 , StreamRBitmapDC , 30-point , 0 , SRCCOPY );

		SelectObject(StreamSBitmapDC , OldSStremBit);
		DeleteObject(hBitSStream);
		DeleteObject(hBitRStream);
		DeleteDC(StreamSBitmapDC);
		DeleteDC(StreamRBitmapDC);

		BitBlt(hDC , 0 , 0 , 220 , 50 , MemDC , 0 , 0 , SRCCOPY);
		SelectObject(MemDC , OldbackBit);
		DeleteObject(hBit);


		DeleteObject(hPen);
		DeleteObject(hBrush);
		DeleteObject(hBit);

		DeleteDC(MemDC);
		EndPaint(hWnd , &ps);;
		return 0;
	}
	return DefWindowProc(hWnd , Message , wParam , lParam);
}