#include "platform_adpt.h"
#include <string.h>
#include <ctype.h>
#include <stdio.h>

#ifdef WIN32

//

#elif defined(__LINUX__)

#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#endif

BOOL ResolveHost(const char* host, uint32* pIP)
{
	if((*pIP = inet_addr(host)) == INADDR_NONE)
	{
		struct hostent *h = gethostbyname(host);
		if(h)
		{
			*pIP = *(uint32*)(h->h_addr_list[0]);
			return TRUE;
		}
	}
	else
		return TRUE;

	return FALSE;
}

int strncpyz(char *dest, const char *s, int n)
{
	int i;
	for(i=0; i<n-1 && s[i]; i++)
	{
		dest[i] = s[i];
	}
	dest[i] = '\0';
	return i;
}

int init_sai(struct sockaddr_in* sai, const char* shost, unsigned short port)
{
	memset(sai, 0, sizeof(struct sockaddr_in));
	if(!ResolveHost(shost, (uint32*)&sai->sin_addr.s_addr)) return -1;
	sai->sin_family = AF_INET;
	sai->sin_port = htons(port);
	return 0;
}

/*
 * 创建套接字并绑定到指定端口
 */
int NewSocketAndBind(int sock_type, unsigned long bind_ip, unsigned short port)
{
	struct sockaddr_in sa;
	int fd, opt;

	fd = socket(AF_INET, sock_type, 0);
	if(fd == INVALID_SOCKET) return INVALID_SOCKET;
	opt = 1;
	setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(int)); 
	
	memset(&sa, 0, sizeof(sa));
	sa.sin_family = AF_INET;
	sa.sin_addr.s_addr = bind_ip;//htonl(INADDR_ANY);
	sa.sin_port = htons(port);
	if(bind(fd, (struct sockaddr*)&sa, sizeof(sa)) < 0) 
	{
#ifndef WIN32
		perror("bind");
#endif
		PA_SocketClose(fd);
		return INVALID_SOCKET;
	}
	return fd;
}

int CreateServiceSocket(int sock_type, unsigned long bind_ip, unsigned short port)
{
	int fd = NewSocketAndBind(sock_type, bind_ip, port);
	if(fd != INVALID_SOCKET && sock_type == SOCK_STREAM) listen(fd, 5);
	return fd;
}


int timed_wait_fd(int fd, unsigned int ms)
{
	fd_set rfds;
	struct timeval tv;

	FD_ZERO(&rfds);
	FD_SET(fd, &rfds);
	tv.tv_sec = ms/1000;
	tv.tv_usec = (ms%1000)*1000;
	return select(fd+1, &rfds, NULL, NULL, &tv);
}

int timed_recv(int sk, void* ptr, int size, unsigned int ms)
{
	int r = timed_wait_fd(sk, ms);
	if(r < 0) return  -1;
	if(r == 0) return -2;
	r = recv(sk, ptr, size, 0);
	if(r < 0) return -1;
	return r;
}

int timed_recv_from(int sk, void* ptr, int size, struct sockaddr* addr, int *sock_len, unsigned int ms)
{
	int r = timed_wait_fd(sk, ms);
	if(r < 0) return  -1;
	if(r == 0) return -2;

	r = recvfrom(sk, ptr, size, 0, addr, sock_len);
	if(r < 0) { return -1; }
	return r;
}

int timed_wait_fd_w(int fd, unsigned int ms)
{
	fd_set wfds;
	struct timeval tv;

	FD_ZERO(&wfds);
	FD_SET(fd, &wfds);
	tv.tv_sec = ms/1000;
	tv.tv_usec = (ms%1000)*1000;
	return select(fd+1, NULL, &wfds, NULL, &tv);
}

int writen(int sock, void *p, int len, unsigned int tmout/*ms*/)
{
	fd_set wfds, rfds, efds;
	struct timeval tv;

	int wtotal = 0, sel;
	while(wtotal < len)
	{
		FD_ZERO(&rfds);
		FD_SET(sock, &rfds);
		FD_ZERO(&wfds);
		FD_SET(sock, &wfds);
		FD_ZERO(&efds);
		FD_SET(sock, &efds);
		if(tmout >= 0)
		{
			tv.tv_sec = tmout/1000;
			tv.tv_usec = (tmout%1000)*1000;
		}
		sel = select(sock+1, &rfds, &wfds, &efds, (tmout==~0L)?&tv:NULL);
		if(sel < 0)
		{
#ifdef WIN32
			if(WSAGetLastError() == WSAEINTR) continue;
#else
			if(errno == EINTR) continue;
#endif
			return -1;
		}
		if(sel == 0) return -1;
		if(FD_ISSET(sock, &efds)) return -1;
		if(  FD_ISSET(sock, &rfds ) )
		{
			char buf[101];
			if(recv(sock, buf, 100, 0) == 0) return 0;
		}

		if(FD_ISSET(sock, &wfds))
		{
#ifdef WIN32
			int wlen = send(sock, p, len - wtotal, 0);
#else
			int wlen = write(sock, p, len - wtotal);
#endif

			if(wlen < 0)
			{
#ifdef WIN32
				if(WSAGetLastError() == WSAEINTR) continue;
#else
				if(errno == EINTR) continue;
#endif
			   	return -2;
			}
			wtotal += wlen;
			p = (char*)p + wlen;
		}
		else
			continue;
	}
	return wtotal;
}

void key2string(const unsigned char key[16], char str[25])
{
	int i, j;
	//0~12
	int ci, bi, val;
	for(i=0, j=0; i<20; i++, j++)
	{
		if(i==4 || i==8 || i==12 || i==16) str[j++] = '-';
		ci = i*5/8;
		bi = (i*5)%8;
		if(bi < 3)
			val = (key[ci] >> (3-bi)) & 0x1F;
		else
			val = ((key[ci] << (bi-3)) | (key[ci+1] >> (11-bi))) & 0x1F;
		if(val <= 9)
			str[j] = '0' + val;
		else
			str[j] = 'A' + val - 10;
	}
	str[24] = '\0';
}

BOOL string2key(const char *str, unsigned char key[16])
{
	int i, j;
	int ci, bi;
	char strkey[25];

	memset(key, 0, 16);
	for(i=0, j=0; str[j]; j++)
	{
		if(isalnum(str[j])) strkey[i++] = toupper(str[j]);
		else if(str[j] != '-') return FALSE;
	}
	strkey[i++] = '\0';
	if(i != 21) return 0;
	for(i=0; i<20; i++)
	{
		if(isdigit(strkey[i])) strkey[i] -= '0';
		else strkey[i] -= ('A' - 10);
	}
	for(i=0; i<13; i++)
	{
		ci = 8*i/5;
		bi = (8*i)%5;
		if(bi<=2)
			key[i] = (strkey[ci] << (bi+3)) | (strkey[ci+1] >> (2-bi));
		else
			key[i] = (strkey[ci] << (bi+3)) | (strkey[ci+1] << (bi-2)) | (strkey[ci+2] >> (7-bi));
	}
	key[12] &= 0xF0;
	return TRUE;
}

#if defined(__LINUX__)
unsigned long filelength(int fno)
{
	struct stat _stat;
	fstat(fno, &_stat);
	return _stat.st_size;
}
#endif

#ifdef MISC_TEST
#include <pthread.h>
void* misc_thread(void* p)
{
	int i;
	for(i=0; i<4; i++)
	{
		NewSocketAndBind(SOCK_STREAM, 0, 1234);
		printf("create sock: %d\n", i);
	}
	return NULL;
}
int main()
{
	pthread_t thd;
	NewSocketAndBind(SOCK_STREAM, 0, 1234);
	pthread_create(&thd, NULL, misc_thread, NULL);
	sleep(5);
	return 0;
}
#endif
