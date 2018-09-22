#ifndef __platform_adpt_h__
#define __platform_adpt_h__

#include "basetype.h"
#include <string.h>
#include <stdlib.h>


#define IN
#define INOUT



#include <stdio.h>
#include <string.h>

#ifdef WIN32	//Windows
#include <winsock2.h>
#include <ws2tcpip.h>

#if defined(__cplusplus) && !defined(_CONSOLE)
#include <afx.h>
#else
#include <windows.h>
#endif


#define __STDCALL  __stdcall
#define INLINE	__inline

#ifdef __cplusplus
extern "C" {
#endif

/*
 *  SOCKET Functions
 */
void PA_NetLibInit();
#define PA_NetLibUninit()	WSACleanup()

#define PA_SocketClose closesocket
#define CloseSocket	closesocket
#define PA_Send(s, p, len, f) send(s, (const char*)p, len, f)
#define PA_SendTo(s, p, len, f, paddr, alen) sendto(s, (const char*)p, len, f, paddr, alen)
#define PA_Recv(s, p, len, f) recv(s, (char*)p, len, f)
#define PA_RecvFrom(s, p, len, f, paddr, palen) recvfrom(s, (char*)p, len, f, paddr, palen)
#define PA_GetSockName(s, paddr, paddr_len) getsockname(s, paddr, (int*)paddr_len)
#define PA_GetPeerName(s, paddr, paddr_len) getpeername(s, paddr, (int*)paddr_len)
#define PA_Accept(s, paddr, paddr_len) accept(s, paddr, paddr_len)
#define PA_GetSockOpt(s, level, optname, optval, optlen) setsockopt(s, level, optname, (char*)optval, (int*)optlen)
#define PA_SetSockOpt(s, level, optname, optval, optlen) setsockopt(s, level, optname, (const char*)optval, optlen)

#define PA_SOCKET	SOCKET
#define PA_SocketIsValid(s) (s!=INVALID_SOCKET)
#define PA_SocketGetError() WSAGetLastError()
#define PA_SOCKET_ERROR	SOCKET_ERROR	//return value of socket operations
/*
 *
 */
#define PA_INVALID_HANDLE	INVALID_HANDLE_VALUE
#define PA_IsValidHandle(handle) (handle != INVALID_HANDLE_VALUE)

/*
 *  Synchronous Objects
 */
#define PA_MUTEX	HANDLE
#define PA_EVENT	HANDLE
#define PA_COND		HANDLE
#define PA_SEM		HANDLE	//semaphore
#define PA_PIPE	HANDLE
#define PA_SPIN	CRITICAL_SECTION

#define PA_DEFINEMUTEX(x) PA_MUTEX x = CreateMutex(NULL, FALSE, NULL)
#define PA_MutexInit(x) x = CreateMutex(NULL, FALSE, NULL)
#define PA_MutexUninit(x) CloseHandle(x)
#define PA_MutexLock(x) WaitForSingleObject(x, INFINITE)
#define PA_MutexUnlock(x) ReleaseMutex(x)
#define PA_MutexTryLock(x) (WaitForSingleObject(x, 0) == WAIT_OBJECT_0)

#define PA_SpinInit(x) InitializeCriticalSection(&x)
#define PA_SpinUninit(x) DeleteCriticalSection(&x)
#define PA_SpinLock(x) EnterCriticalSection(&x)
#define PA_SpinTryLock(x) TryEnterCriticalSection(&x)
#define PA_SpinUnlock(x) LeaveCriticalSection(&x)

#define PA_EventInit(x) x = CreateEvent(NULL, FALSE, FALSE, NULL)
#define PA_EventUninit(x) CloseHandle(x)
#define PA_EventSet(x) SetEvent(x)
//#define PA_ResetEvent(x) ResetEvent(x)
#define PA_EventWait(x) WaitForSingleObject(x, INFINITE)
#define PA_EventWaitTimed(e, ms) (WaitForSingleObject(e, ms)==WAIT_OBJECT_0)

#define PA_SemInit(x, max_value) x = CreateSemaphore(NULL, 0, max_value, NULL)
#define PA_SemUninit(x)	CloseHandle(x)
#define PA_SemWait(x) WaitForSingleObject(x, INFINITE)
#define PA_SemPost(x) ReleaseSemaphore(x, 1, NULL)

/*
 *  Threads
 */
#define PA_HTHREAD	HANDLE
#define PA_THREAD_RETTYPE	DWORD
typedef PA_THREAD_RETTYPE (__STDCALL PA_ThreadRoutine)(void*);
#define PA_ThreadCloseHandle(hThread) CloseHandle(hThread)
void *PA_ThreadWaitUntilTerminate(PA_HTHREAD hThread);

#define PA_Sleep(ms) Sleep(ms)

/*
 *  String functions
 */
#define PA_StrCaseCmp stricmp
#define PA_StrNCaseCmp strnicmp
#define PA_StrNCmp	strncmp

/*
 *  File Operations
 */
#define PA_HFILE	HANDLE
int PA_Write(PA_HFILE hFile, const void *pBuff, unsigned int size);
int PA_Read(PA_HFILE hFile, void *pBuff, unsigned int size);
#define PA_FileClose(hf) CloseHandle(hf)
#define PA_FileIsValid(h) (h!=INVALID_HANDLE_VALUE)
#define PA_DeleteFile(f)	DeleteFile(f)

/*
 *  Time
 */
#define PA_GetTickCount() GetTickCount()

#ifdef __cplusplus
}
#endif

#elif defined(__LINUX__)
//
// OS:  Linux
//

#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <netdb.h>
#include <errno.h>
#include <sys/select.h>
#include <sys/time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <semaphore.h>

#define __STDCALL
#define INLINE inline

#ifdef __cplusplus
extern "C" {
#endif

/*
 *  SOCKET Functions
 */
#define PA_NetLibInit()
#define PA_NetLibUninit()

#define PA_SocketClose close
#define CloseSocket	close
#define PA_Send send
#define PA_SendTo sendto
#define PA_Recv recv
#define PA_RecvFrom(s, buf, size, flags, paddr, paddr_len) recvfrom(s, buf, size, flags, paddr, (socklen_t*)paddr_len)
#define PA_GetSockName(s, paddr, paddr_len) getsockname(s, paddr, (socklen_t*)paddr_len)
#define PA_GetPeerName(s, paddr, paddr_len) getpeername(s, paddr, (socklen_t*)paddr_len)
#define PA_Accept(s, paddr, paddr_len) accept(s, paddr, (socklen_t*)paddr_len)
#define PA_GetSockOpt getsockopt
#define PA_SetSockOpt setsockopt

#define PA_SOCKET	int
#define INVALID_SOCKET	-1	
#define PA_SocketIsValid(s) (s>=0)
#define PA_SocketGetError() errno
#define PA_SOCKET_ERROR	-1	//return value of socket operations

/*
 *
 */
#define PA_IsValidHandle(fd) (fd>=0)
#define PA_INVALID_HANDLE -1

/*
 *  Synchronous Objects
 */
#define PA_MUTEX	pthread_mutex_t
#define PA_PIPE	int

#define PA_EVENT sem_t*
#define PA_SPIN	pthread_spinlock_t
#define PA_SEM	sem_t

#define PA_DEFINEMUTEX(x) pthread_mutex_t x = PTHREAD_MUTEX_INITIALIZER
#define PA_MutexInit(x) pthread_mutex_init(&x, NULL)
#define PA_MutexUninit(x) pthread_mutex_destroy(&x)
#define PA_MutexLock(x) pthread_mutex_lock(&x)
#define PA_MutexUnlock(x) pthread_mutex_unlock(&x)
#define PA_MutexTryLock(x) (pthread_mutex_trylock(&x) == 0)

#define PA_SpinInit(x) pthread_spin_init(&x, PTHREAD_PROCESS_PRIVATE)
#define PA_SpinUninit(x) pthread_spin_destroy(&x)
#define PA_SpinLock(x) pthread_spin_lock(&x)
#define PA_SpinTryLock(x) pthread_spin_trylock(&x)
#define PA_SpinUnlock(x) pthread_spin_unlock(&x)

#define PA_EventInit(x)		do { x=(sem_t*)malloc(sizeof(sem_t)); sem_init(x, 0, 0); } while(0)
#define PA_EventUninit(x)	do { sem_destroy(x); free(x); } while(0)
#define PA_EventSet(x)		sem_post(x) 
#define PA_EventWait(x)		sem_wait(x)
BOOL	PA_EventWaitTimed(PA_EVENT e, DWORD ms);

#define PA_SemInit(x, max_value) do { x = (sem_t*)malloc(sizeof(sem_t)); sem_init(x, 0, max_value); } while(0)
#define PA_SemUninit(x)	CloseHandle(x)	{ sem_destroy(x); free(x); }
#define PA_SemWait(x) sem_wait(x)
#define PA_SemPost(x) sem_post(x)

/*
 *  Threads
 */
#define PA_HTHREAD pthread_t
#define PA_THREAD_RETTYPE	void*
typedef PA_THREAD_RETTYPE (__STDCALL PA_ThreadRoutine)(void*);
#define PA_ThreadCloseHandle(hThread) pthread_detach(hThread)
void* PA_ThreadWaitUntilTerminate(PA_HTHREAD hThread);

void PA_Sleep(UINT ms);	//Milliseconds

/*
 *  String functions
 */
#define PA_StrCaseCmp strcasecmp
#define PA_StrNCaseCmp strncasecmp
#define PA_StrNCmp	strncmp

/*
 *  File Operations
 */
#define PA_HFILE	int
#define PA_Write	write
#define PA_Read		read
#define PA_FileIsValid(h) (h>=0)
#define PA_FileClose(f) close(f)
BOOL PA_DeleteFile(const char *fn);

/*
 *  Time
 */
DWORD PA_GetTickCount();

#ifdef __cplusplus
}
#endif

#else //else Linux

#error "Platform must be specified !"

#endif	//#else Linux


#ifdef __cplusplus
extern "C" {
#endif
/*
 *  Common Wrapper
 */
PA_HTHREAD PA_ThreadCreate(PA_ThreadRoutine* routine, void* data);
int PA_SocketSetNBlk(PA_SOCKET s, BOOL b);
int PA_SocketSetLinger(PA_SOCKET s, int onoff, int linger);

/*
 *  Pipe functions
 */
BOOL PA_PipeCreate(PA_PIPE *pHPipeRd, PA_PIPE *pHPipeWrt);
BOOL PA_PipeClose(PA_PIPE hPipe);

/*
 *  Debug
 */
#ifdef _DEBUG
	#if defined(WIN32) && defined(__cplusplus) && !defined(_CONSOLE)
		#define dbg_msg TRACE
		#define PRINTF TRACE
	#else
		#define dbg_msg printf
		#define PRINTF printf
	#endif
void dbg_bin(const char *title, const void *p, int size);
#else
	#ifdef WIN32
		#define dbg_msg(fmt, __VA_ARGS__)
	#else
		#define dbg_msg(fmt, args...)
	#endif
#define dbg_bin(x,y,z)
#endif

#ifdef __cplusplus
}
#endif

#endif	//#ifndef __dcs_platform_h__
