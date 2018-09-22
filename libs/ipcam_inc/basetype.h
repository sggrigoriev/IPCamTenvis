#ifndef __basetype_h__
#define __basetype_h__

typedef unsigned char           uint8;
typedef char		        sint8;
typedef unsigned char           uchar;
typedef short			sint16;
typedef unsigned short          uint16;
typedef int			sint32;
typedef unsigned int            uint32;
typedef unsigned long long      uint64;
typedef long long		sint64;

#ifdef WIN32

typedef __int64			INT64;
typedef unsigned __int64 	UINT64;

#else

typedef unsigned char BYTE;
typedef unsigned short WORD;
typedef unsigned long DWORD;

typedef unsigned char UCHAR;
typedef unsigned short USHORT;
typedef unsigned int UINT;
typedef unsigned long ULONG;

typedef long long INT64;
typedef unsigned long long UINT64;

typedef long LONG;

typedef const char * LPCSTR;
typedef char * LPSTR;

#define BOOL	int
#define TRUE	1
#define FALSE	0

#endif

#endif
