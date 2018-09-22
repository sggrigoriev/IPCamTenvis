#ifndef __misc_h__
#define __misc_h__

#ifdef __cplusplus
extern "C" {
#endif

#include <platform_adpt.h>

#define SAFE_FREE(p) if(p) { free(p); p=NULL; }
#define SAFE_DELETE(p) if(p) { delete p; p=NULL; }

	
extern BOOL ResolveHost(const char* host, ULONG* pIP);
extern int init_sai(struct sockaddr_in* sai, const char* shost, unsigned short port);
extern char *strncpyz(char *dst, const char *src, int n);

//bind_ip: ip to bind to. network byte order
//port: port to bind. host byte order
int NewSocketAndBind(int sock_type, unsigned long bind_ip, unsigned short port);
//call listen() if sock_type is SOCK_STREAM
int CreateServiceSocket(int sock_type, unsigned long bind_ip, unsigned short port);

//wait to be readable
int timed_wait_fd(int fd, unsigned int ms);
//wait to be writable
int timed_wait_fd_w(int fd, unsigned int ms);
int timed_recv(int sk, void* ptr, int size, unsigned int ms);
int timed_recv_from(int sk, void* ptr, int size, struct sockaddr* addr, int *sock_len, unsigned int ms);

#ifdef WIN32
#include <io.h>
#define filelength(fd) _filelength(fd)
#elif defined(__LINUX__)
unsigned long filelength(int fd);
#endif

//bin <-> string of IPCAM's key
extern void key2string(const unsigned char key[16], char str[25]);
extern BOOL string2key(const char *str, unsigned char key[16]);

#ifdef __cplusplus
}
#endif

#endif
