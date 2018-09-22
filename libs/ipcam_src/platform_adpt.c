#include "platform_adpt.h"
#include <stdlib.h>

#ifdef WIN32

void PA_NetLibInit()
{
	WSADATA wd;
	WSAStartup(0x0202, &wd);
}

int PA_Write(PA_HFILE hFile, const void *pBuff, unsigned int size)
{
	DWORD dwWritten;
	if( ! WriteFile(hFile, pBuff, size, &dwWritten, NULL) )
		return -1;
	return dwWritten;
}
int PA_Read(PA_HFILE hFile, void *pBuff, unsigned int size)
{
	DWORD dwReaden;
	if( ! ReadFile(hFile, pBuff, size, &dwReaden, NULL) )
		return -1;
	return dwReaden;
}

PA_HTHREAD PA_ThreadCreate(PA_ThreadRoutine* routine, void* data)
{
	return CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)routine, data, 0, NULL);
}

void *PA_ThreadWaitUntilTerminate(PA_HTHREAD hThread) 
{ 
	DWORD dwExitCode; 
	WaitForSingleObject(hThread, INFINITE); 
	GetExitCodeThread(hThread, &dwExitCode); 
	return (void*)dwExitCode; 
}

BOOL PA_PipeCreate(PA_PIPE *pHPipeRd, PA_PIPE *pHPipeWrt)
{
	return CreatePipe(pHPipeRd, pHPipeWrt, NULL, 0) == TRUE;
}
BOOL PA_PipeClose(PA_PIPE hPipe)
{
	return CloseHandle(hPipe);
}

#elif defined(__LINUX__)

BOOL PA_EventWaitTimed(PA_EVENT e, DWORD ms)
{
	struct timespec ts;

	//clock_gettime(CLOCK_REALTIME, &ts);
	gettimeofday((struct timeval*)&ts, NULL);
	ts.tv_nsec *= 1000;
	ts.tv_sec += ms/1000;
	ts.tv_nsec += (ms%1000)*1000000;
	return sem_timedwait(e, &ts) == 0;
}

PA_HTHREAD PA_ThreadCreate(PA_ThreadRoutine* routine, void* data)
{
	pthread_t thd;
	if(pthread_create(&thd, NULL, routine, data) == 0)
		return thd;
	else
		return 0;
}

void* PA_ThreadWaitUntilTerminate(PA_HTHREAD hThread) 
{ 
	void *tmp; 
	pthread_join(hThread, &tmp); return tmp; 
}

BOOL PA_PipeCreate(PA_PIPE *pHPipeRd, PA_PIPE *pHPipeWrt)
{
	PA_PIPE fds[2];
	if(pipe(fds) < 0) return FALSE;
	*pHPipeRd = fds[0];
	*pHPipeWrt = fds[1];
	return TRUE;
}
BOOL PA_PipeClose(PA_PIPE hPipe)
{
	return 0 == close(hPipe);
}

void PA_Sleep(UINT ms)
{
	struct timeval tv;
	tv.tv_sec = ms/1000;
	tv.tv_usec = (ms%1000)*1000;
	select(0, NULL, NULL, NULL, &tv);
}

DWORD PA_GetTickCount()
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return 1000*(tv.tv_sec - 100000) + tv.tv_usec/1000;
}

BOOL PA_DeleteFile(const char *fn)
{
	return unlink(fn) == 0; 
}

#endif	//linux

#ifdef WIN32
int PA_SocketSetNBlk(PA_SOCKET s, BOOL b)
{
	return ioctlsocket(s, FIONBIO, (u_long*)&b);
}

#elif defined(__LINUX__)
int PA_SocketSetNBlk(PA_SOCKET s, BOOL b)
{
	int opt = fcntl(s, F_GETFL, &opt, 0);
	if(b) opt |= O_NONBLOCK;
	else opt &= ~O_NONBLOCK;
	return fcntl(s, F_SETFL, opt);
}
#endif

int PA_SocketSetLinger(PA_SOCKET s, int onoff, int linger)
{
	struct linger opt = { onoff, linger };
	return setsockopt(s, SOL_SOCKET, SO_LINGER, 
#ifdef WIN32
		(const char*)&opt, 
#else
		&opt,
#endif
		sizeof(opt));
}

#ifdef _DEBUG
void dbg_bin(const char *title, const void *p, int size)
{
	int i;
	unsigned char *byts = (unsigned char*)p;
	printf(title);
	for(i=0; i<size; i++)
	{
		printf("%02X ", byts[i]);
		if(i>0 && (i&31) == 31) printf("\n");
	}
	printf("\n");
}
#else
#define dbg_bin(x, y, z)
#endif
