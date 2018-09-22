#ifndef __ctp_h__
#define __ctp_h__
#include "platform_adpt.h"
#include <time.h>
#include "CString.h"

/////////////////////////////////////////////////////////////////////////////////////////
#define KEYVALTYPE_INT		1	//int 
#define KEYVALTYPE_STRING	2	//char[]
#define KEYVALTYPE_CSTRING	3	//CString
#define KEYVALTYPE_POINTER	4	//char*
typedef
struct _tagKeyVal {
	const char *sKey;
	int type;			//KEYVALTYPE_XXX
	void *pVal;			//pVal为指向type所定类型的指针
	union {
		int size;		//type=STRING时为pVal指向的缓冲区的大小
		int iVal;		//type=INT时, 如果pVal为空, 值保存到这里
	};

	char *sVal;
} KEYVAL;

#define PF_CASESENSITIVE	1
#define PF_DONTINITVALS		2
#define PF_ZEROTERMINATED	4
int ParseBody(char *pBody, KEYVAL *pKv, int cnt, DWORD dwFlags=0);

char* SetCTPProtoclHead(char *buf, const char *cmd, UINT iBodyLength, const char *extra_headers);

typedef BOOL (*VSCMDLINECB)(char *content, void *arg);
typedef BOOL (*VSCMDBINCB)(BYTE *content, int len, void *arg);
//
//功能：当DVS返回多行数据，并且没有对应的参数名时或者总长度不便预先确定(如列出文件),对这样的每行数据调用回调函数。
//		并且，在回调函数返回FALSE时中止处理。
//
int CTPCommandWithCallback(PA_SOCKET sock, const char *scmd, /*INOUT*/CString &strBody, VSCMDLINECB pCmdCbFunc, void *arg, const char *pszcExtraHeader = NULL, UINT timeout=5000);
int CTPCommandWithCallbackBin(PA_SOCKET sock, const char *scmd, /*INOUT*/CString &strBody, VSCMDBINCB pCmdCbFunc, void *arg, const char *pszcExtraHeader = NULL, UINT timeout=5000);

int CTPCommand(PA_SOCKET sock, const char *cmd, /*INOUT*/CString &str, const char *pszcExtraHeader = NULL, UINT timeout=5000);
int ExecCmd(PA_SOCKET hSock, const char *cmd, CString &str, const char *pszcExtraHeader = NULL, UINT timeout=5000);
int ExecCmd(PA_SOCKET hSock, CString& str, const char *pszcExtraHeader = NULL, UINT timeout=5000);
int QueryCmd(PA_SOCKET hSocket, const char *cmd, CString &str/*IN:body, OUT:reason or data*/, KEYVAL *pKv, UINT size, DWORD flags=0, const char *pszcExtraHeader = NULL, UINT timeout=5000);

KEYVAL *KvOf(const char *name, KEYVAL *pKv, int size);
#define KVOF(name, kv) KvOf(name, kv, sizeof(kv)/sizeof(KEYVAL))

#define NOTIFICATION_ALARM			1
	#define IO_EVENT		1	//报警输入
	#define MD_EVENT		2	//运动侦测
	#define AB_EVENT		3	//Abnormal
	#define SND_EVENT               4	//声音报警
#define NOTIFICATION_MSG			2	//设备端消息
#define NOTIFICATION_LOGIN			3
#define NOTIFICATION_LOGOUT			4
#define NOTIFICATION_STOPSERVICE	5
#define NOTIFICATION_REBOOT			6
#define NOTIFICATION_NOTIFY			7	//代理服务器通知消息

#define NOTIFICATION_MAX			100
BOOL ParseNotifyMessage(char *buf, int *notify, char **pHeader);
/*************************************
 *
 *      For   Proxy
 *
 *************************************/

#define CONTENTTYPE_TEXTPLAIN	0
#define CONTENTTYPE_APPOCT	1
typedef struct _tagCtpReqOpt {
	int	content_length;
	int	keep_alive;
	int chunked_transfer;
	int retry_after;
	UINT cseq;

	char *host;
	char *content_type;
	char *accept_charset;
	char *accept_encoding;
	char *accept_language;
	char *cookie;
	char *authorization;
	char *session;
	char *transport;

	char	*body;
} REQUESTOPTIONS;

typedef struct _tagRequestLine {
	char *method;
        char *uri;
        char *proto_ver;
        char *header;
 } REQUESTLINE;

int ParseResponseHeaderLine(char *buf, CString& sReason);
int ParseRequestLine(char *buf, REQUESTLINE *pReqLine);
int ParseRequestOptions(char *header, REQUESTOPTIONS *pReqOpt);

/*************************************
 *
 *    常  用  命  令  封  装
 *
 *************************************/
DWORD CtpLogin(PA_SOCKET hSocket, const char *psczUser, const char *psczPswd, const char *pszOemId, /*OUT*/char *pszClientId, /*OUT*/ DWORD *pdwRight, 
			   const char *pszcExtraHeader = NULL, UINT timeout=5000);

typedef struct _tagENUMDEVICE {
	char cDevName[50];
	char cSN[40];
	char cIp[16];
	char cNetMask[16];
	int iPortCtp;

	BOOL bDhcp;
	char cMDns[16];
	char cSDns[16];
	char cDdnsUser[24];
	char cDdnsSvr[64];
	char cDdn[64];
	char cMac[20];
	char cGateway[16];
	char cVersion[48];
} ENUMDEVICE, *LPENUMDEVICE;

EXTERN UINT GetCTPErrorString(int err, char *buf, UINT size);	//返回拷贝字节数+1
EXTERN CString GetCTPErrorString(int err);

extern "C" {
EXTERN int Recv(PA_SOCKET sock, char *body, UINT size, UINT timeout/*ms*/=5000);
EXTERN int ss_sscanf(char seperator, const char *buf, const char *fmt, ...);
//取错误描述
EXTERN BOOL EnumDevices(LPENUMDEVICE *ppEnumDev, UINT *pNDev, const char *pszOemId/*为NULL时返回全部*/, UINT try_cnt);
EXTERN void DefaultNotificationHandler(int notify, void *p);
}

#endif
