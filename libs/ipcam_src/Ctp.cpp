#include "Ctp.h"
#include "errdefs.h"
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include "misc.h"
//fmt: {%[len]s|S 或者 %d|f}*
//
int ss_sscanf(char seperator, const char *buf, const char *fmt, ...)
{
    int i, n, pos1, pos2, pos3;
    va_list ap;
    va_start(ap, fmt);

    n = 0;
    pos1 = pos2 = 0;
    i = 0;
    while(1)
    {
		int llmt = -1, slen, ignore;
        while(buf[pos2] && buf[pos2] != seperator) pos2++;
        while(buf[pos1] > 0 && isspace(buf[pos1]) && pos1 < pos2-1) pos1 ++;

        while(fmt[i] && fmt[i] != '%') i++;
        if(fmt[i] == 0) { va_end(ap); return n; }

        i++;
		if(fmt[i] == '*') { ignore = 1; i++; }
		else ignore = 0;
fmtchr:
        switch(fmt[i])
        {
            case 'd':
                if(!ignore) *va_arg(ap, int*) = atoi(buf+pos1);
                break;
            case 'f':
                if(!ignore) *va_arg(ap, double*) = atof(buf+pos1);
                break;
            case 's':
			case 'S':
				if(!ignore)
				{
					pos3 = pos2 - 1;
					while(pos3 > 0 && isspace((unsigned char)buf[pos3])) pos3--;
					pos3++;
					slen = pos3 - pos1;
					if(llmt > 0 && slen > llmt) slen = llmt;
					if(fmt[i] == 'S') *va_arg(ap, CString*) = CString(buf+pos1, slen);
					else {
						char *s = va_arg(ap, char*);
						strncpy(s, buf+pos1, slen);
						s[slen] = '\0';
					}
				}
                break;
            default:
				if(isdigit(fmt[i])) {
					llmt = 0;
					while(isdigit(fmt[i])) {
						llmt = 10 * llmt + fmt[i] - '0';
						i++;
					}
					goto fmtchr;
				}
				else
					return n;
        }//switch
        n++;

        if(buf[pos2] == 0) { va_end(ap); return n; }
        pos1 = ++pos2;
    }//while
}

//--------------------------------------------------------------------------------------------------------  

/************************************************ 
* 功能: 生成CTP命令头域
*   @buf,  OUT
*	@cmd
*	@iBodyLength	Content length
*	@header			Extra headers
* Return: 指向头域后的第一个字节(Body 的起始)
************************************************/
char* SetCTPProtoclHead(char *buf, const char *cmd, UINT iBodyLength, const char *extra_headers)
{
	if(iBodyLength > 0)
	{
		if(extra_headers && extra_headers[0])
			return buf + sprintf(buf, "CMD %s CTP/1.0\r\nContent-Length: %d\r\n%s\r\n", cmd, iBodyLength, extra_headers);
		else
			return buf + sprintf(buf, "CMD %s CTP/1.0\r\nContent-Length: %d\r\n\r\n", cmd, iBodyLength);
	}
	else
	{
		if(extra_headers && extra_headers[0])
			return buf + sprintf(buf, "CMD %s CTP/1.0\r\n%s\r\n", cmd, extra_headers);
		else
			return buf + sprintf(buf, "CMD %s CTP/1.0\r\n\r\n", cmd);
	}
}

//解析应答行. 返回 >0 时为状态码, sReason 为错误描述
//				   =0 状态码为 200, sReason 为 "OK"
int ParseResponseHeaderLine(char *buf, CString& sReason)
{
	const char sproto[] = "CTP/1.0";
	int rlt;
	if(strncmp(buf, sproto, strlen(sproto))) return E_ERROR_ACK;
/*
	char *p;
	p = strstr(buf, "\r\n");
	if(!p) return -E_ERROR_ACK;
	*p = '\0';
*/
	buf += strlen(sproto) + 1;
	rlt = strtol(buf, &buf, 10);
	if(rlt < 100 || rlt >= 600) return E_ERROR_ACK;
	sReason = buf;

	if(rlt == 200) rlt = 0;
	return rlt;
}

int Recv(PA_SOCKET sock, char *body, UINT size, UINT timeout/*ms*/)
{
	fd_set rfds;
	struct timeval tv;

	FD_ZERO(&rfds);
	FD_SET(sock, &rfds);
	tv.tv_sec = timeout/1000; tv.tv_usec = 1000*(timeout%1000);
	if(select(sock+1, &rfds, NULL, NULL, &tv) <= 0) return -2;
	return recv(sock, body, size, 0);
}
/************************************************ 
* 功能: 执行一条命令
* 参数: scmd --> 命令名
*		strBody: 命令参数
*		bBinary: 返回内容是无格式的二进制数据(TRUE)/以空行结束的多行文本(FALSE)
* 返回: > 0 PA_SOCKET 层错
*		>= 100 服务器返回的错误码
*		= 0 命令成功执行
* 说明: 如果返回 > 0, strBody 包含错误描述. 
*			返回 0 时, strBody 为命令执行的返回数据
*		当数据为无格式时，如果返回的头部包含 "Content-Length" 域，数据长度由该域指定，
*	否则长度为接收数据至到连接被服务器端关闭。数据为有格式文本时，由空行结束。对接收的
*	的每一行（最后的空行除外）调用回调函数
************************************************/
int RecvResponse(int sock, char* cBuffer, int size, CString &strBody, VSCMDLINECB pCmdCbFunc, void *arg, BOOL bBinary, UINT timeout)
{
	int iRecvLen, rlt;

rewait:
	iRecvLen = Recv(sock, cBuffer, size-1, timeout);
	
	if(iRecvLen == -2) { rlt = E_TIMEOUT; strBody = "Timeout"; }
	else if(iRecvLen < 0) { rlt = E_CTP_SYSTEM; strBody = "Socket error"; }
	else
	do {
		char *phead, *pbody = NULL;
		REQUESTOPTIONS reqopt;

		cBuffer[iRecvLen] = '\0';

		rlt = E_ERROR_ACK;
		if( (phead = strstr(cBuffer, "\r\n")) == NULL ) break;

		rlt = 0;
		*phead = '\0';
		phead += 2;
		rlt = ParseResponseHeaderLine(cBuffer, strBody);
		if(rlt == 100) goto rewait;

		if((rlt == 0 || rlt == 503) && strncmp(phead, "\r\n", 2))	//有头部
		{
			int rlt2;
			if( (rlt2 = ParseRequestOptions(phead, &reqopt)) )
			{
				rlt = rlt2;
			}
			else if(rlt == 503)
			{
				rlt |= reqopt.retry_after << 16;
			}
			else
			{
				pbody = reqopt.body;
			}
		}
		if(rlt) break;

		if(!pCmdCbFunc) strBody = pbody;
		else
		if(bBinary)
		{
			VSCMDBINCB pBinCb = (VSCMDBINCB)pCmdCbFunc;
			int iTotalLen = iRecvLen - (pbody - cBuffer);
			pBinCb((BYTE*)pbody, iTotalLen, arg);
			while(	(reqopt.content_length <= 0 || iTotalLen < reqopt.content_length) && (iRecvLen = Recv(sock, cBuffer, size, 2000)) >= 0 )
			{
				if( !(pBinCb((BYTE*)cBuffer, iRecvLen, arg)) ) break;
				iTotalLen += iRecvLen;
			}
		}
		else
		{
			char *pp;
cbloop:
			while( (pp = strstr(pbody, "\r\n")) )
			{
				*pp = '\0';
				if(pbody == pp || !pCmdCbFunc(pbody, arg)) { return 0; }
				pbody = pp + 2;
			}
			if(strcmp(pbody, "\r\n")) 
			{
				int len = strlen(pbody);
				memcpy(cBuffer, pbody, len);
				pbody = cBuffer;
				iRecvLen = Recv(sock, cBuffer + len, size-1-len, 1000);
				if(iRecvLen > 0) 
				{
					cBuffer[len + iRecvLen] = '\0';
					goto cbloop;
				}
			}
		}
	} while(0);
	return rlt;
}

int _VSCCommandWithCallback(PA_SOCKET sock, const char *scmd, const char *extra_headers, /*INOUT*/CString &strBody, VSCMDLINECB pCmdCbFunc, void *arg, BOOL bBinary, UINT timeout)
{
	char *cBuffer, *ptr;
	int iRecvLen = 0, rlt;
	
	cBuffer = (char*)malloc(4000);
	Recv(sock, cBuffer, 4000, 0);

	ptr = SetCTPProtoclHead(cBuffer, scmd, strBody.GetLength(), extra_headers);
	if(strBody.GetLength() > 0)	strcpy(ptr, strBody);

	send(sock, cBuffer, strlen(cBuffer), 0);
	rlt = RecvResponse(sock, cBuffer, 4000, strBody, pCmdCbFunc, arg, bBinary, timeout);
	free(cBuffer);
	return rlt;
}
int CTPCommandWithCallback(PA_SOCKET sock, const char *scmd, /*INOUT*/CString &strBody, VSCMDLINECB pCmdCbFunc, void *arg, const char *extra_headers, UINT timeout)
{
	return _VSCCommandWithCallback(sock, scmd, extra_headers, strBody, pCmdCbFunc, arg, FALSE, timeout);
}
int CTPCommandWithCallbackBin(PA_SOCKET sock, const char *scmd, /*INOUT*/CString &strBody, VSCMDBINCB pCmdCbFunc, void *arg, const char *extra_headers, UINT timeout)
{
	return _VSCCommandWithCallback(sock, scmd, extra_headers, strBody, (VSCMDLINECB)pCmdCbFunc, arg, TRUE, timeout);
}

/************************************************ 
* 功能: 执行一条命令
* 参数: scmd --> 命令名
*		strBody: 命令参数
* 返回: < 0 PA_SOCKET 层错
*		> 0 服务器返回的错误码
*		= 0 命令成功执行
* 说明: 如果返回 > 0, strBody 包含错误描述. 
*			返回 0 时, strBody 为命令执行的返回数据
************************************************/
int CTPCommand(PA_SOCKET sock, const char *scmd, /*INOUT*/CString &strBody, const char *extra_headers, UINT timeout)
{
	return _VSCCommandWithCallback(sock, scmd, extra_headers, strBody, NULL, NULL, 0, timeout);
}

int ExecCmd(PA_SOCKET hSock, const char* cmd, CString &str, const char *extra_headers, UINT timeout)
{
	return CTPCommand(hSock, cmd, str, extra_headers, timeout);
}

static char* findtok(char *str)
{
	static char *p;
	if(str) p = str;

	while(*p && *p == ' ') p++;

	char *tok = p;
	if(*p)
	{
		if(*p == '\"')
		{
			p++;
			while(*p && *p != '\"') p++;
			if(*p) 
			{
				p++; 
				if(*p) *p++ = '\0';
			}
		}
		else
		{
			while(*p && *p != ' ') p++;
			if(*p) *p++ = '\0';
		}
	}
	return *tok?tok:NULL;
}
char *findpara(char *str)
{
	static char *p;
	if(str) p = str;

	while(*p && isspace(*p)) p++;
	char *para = p;
	if(*p)
	{
		if(*p != '-') return NULL;
		while(*p && *p != ' ') p++;
		while(*p && *p == ' ') p++;
		if(*p)
		{
			if(*p == '-')
			{
				*(p-1) = '\0';
			}
			else
			if(*p == '\"')
			{
				p++;
				while(*p && *p != '\"') p++;
				if(*p) 
				{
					p++; 
					if(*p) *p++ = '\0';
				}
			}
			else
			{
				while(*p && *p != ' ') p++;
				if(*p) *p++ = '\0';
			}
		}
	}
	return *para?para:NULL;
}
int ExecCmd(PA_SOCKET hSock, CString& str, const char *extra_headers, UINT timeout)
{
	char *cmd, *para;
	char *buf = (char*)malloc(str.GetLength()+1);
	strcpy(buf, str);

	cmd = buf;
	while(*cmd && isspace(*cmd)) cmd++;
	para = cmd;
	while(*para && !isspace(*para)) para++;
	*para++ = '\0';

	para = findpara(para);
	str = "";
	while(para)
	{
		str += para;
		str += "\r\n";
		para = findpara(NULL);
	}
	if(str != "") str += "\r\n";
	int rlt = ExecCmd(hSock, cmd, str, extra_headers, timeout);
	free(buf);
	return rlt;
}

//返回: 0 - 结果被解析; >0 - str中包含错误描述; <0 - 其它错误
//说明: 如果 pKv 中有 STRING 类型且其pVal指针为空, 函数返回后在使用完分析后的数据前不能修改 str 的值.
//		否则这样的 pVal/sVal 可能会无效
int QueryCmd(PA_SOCKET hSocket, const char *cmd, CString& str/**/, KEYVAL *pKv, UINT size, DWORD flags, const char *extra_headers, UINT timeout)
{
	int rlt = 0;

	rlt = CTPCommand(hSocket, cmd, str, extra_headers, timeout);
	if(rlt == 0)
	{
		ParseBody(str.GetBuffer(str.GetLength()), pKv, size, flags);
		str.ReleaseBuffer();
	}
	if(rlt < 0) str = "";
	return rlt;
}

//===================================================================================
/*函数: ParseBody
  功能: pKv 提供一个感兴趣的参数的数组，大小为cnt。参数的值初始为缺省或无效值.
		pBody为服务器返回的命令体. 
		函数返回时, KEYVAL结构的sVal指向相应参数的值位于pBody缓冲区中的位置,
		如果没有此参数, sVal为NULL
  返回: 获取所请求的参数个数. 返回 -1 表示 pBody 无效
*/
static int isSeperator(int ch)
{
	return ch>0 && (isspace(ch) || ch == ':' || ch == '=');
}

//可能会修改pBody的内容
int ParseBody(char *pBody, KEYVAL *pKv, int cnt, DWORD flags)
{
	int i, rval = 0;
	char *p;

	if((flags & PF_DONTINITVALS) == 0)
	for(i=0; i<cnt; i++) 
	{ 
		pKv[i].sVal = NULL; 
		if(pKv[i].type == KEYVALTYPE_STRING && pKv[i].pVal) *((char*)pKv[i].pVal) = '\0'; 
		if(pKv[i].type == KEYVALTYPE_INT && pKv[i].pVal) *((int*)pKv[i].pVal) = 0;
		if(pKv[i].type == KEYVALTYPE_POINTER && pKv[i].pVal) *(void**)pKv[i].pVal = NULL;
	}

	while( 1 )
	{
		char *s1 = pBody, *s2;

		if(flags & PF_ZEROTERMINATED)
		{
			p = pBody + strlen(pBody);
			if(p == pBody) break;
			pBody = p + 1;
		}
		else 
		{
			if( !(p = strstr(pBody, "\r\n")) || (p == pBody) ) break;
			pBody = p + 2;
		}

		while(isspace((unsigned char)*(p-1)) || *(p-1) == '\"') p--;
		*p = '\0';							//p = Value End
		while(isspace((unsigned char)*s1)) s1++;			//s1 = Key Start
		if(*s1 == '-') s1++;				//Jump over the first '-'
		s2 = s1;							//
		while(*s2 && !isSeperator(*s2)) s2++;	//s2 = Key End
		if(*s2)
		{
			*s2++ = '\0';
			for(i=0; i<cnt; i++)
			{
				const char *pKey = pKv[i].sKey;
				if(*pKey == '-') pKey++;
				if((flags & PF_CASESENSITIVE) && strcmp(pKey, s1) == 0 || !(flags & PF_CASESENSITIVE) && PA_StrCaseCmp(pKey, s1) == 0)
				{
					while(*s2 && (isSeperator(*s2) || *s2 == '\"') ) s2++;	//s2 = Value Start
					pKv[i].sVal = s2;
					switch(pKv[i].type)
					{
					case KEYVALTYPE_STRING: 
					case KEYVALTYPE_CSTRING:
					case KEYVALTYPE_POINTER:
						if(pKv[i].pVal)
						{
							if(pKv[i].type == KEYVALTYPE_POINTER)
								*((char**)pKv[i].pVal) = s2;
							else
							if(pKv[i].type == KEYVALTYPE_STRING)
							{
								memset(pKv[i].pVal, 0, pKv[i].size);
								strncpy((char*)pKv[i].pVal, s2, pKv[i].size-1); 
							}
							else
								*((CString*)pKv[i].pVal) = s2;
						}
						else
							pKv[i].pVal = s2;
						break;
					case KEYVALTYPE_INT:	
						if(pKv[i].pVal) *(int*)(pKv[i].pVal) = strtol(s2, NULL, 0); 
						else pKv[i].iVal = strtol(s2, NULL, 0);
						break;
					}
					rval ++;
					break;
				}
			}
		}
	}
	return rval;
}

KEYVAL *KvOf(const char *name, KEYVAL *pKv, int size)
{
	for(int i=0; i<size; i++)
	{
		if(strcmp(pKv->sKey, name) == 0) return pKv;
		pKv ++;
	}
	return NULL;
}
//---------------------------------- Server ----------------------------------------
int ParseRequestLine(char *buf, REQUESTLINE *pReqLine)
{
	char *p;
        
    p = strstr(buf, "\r\n");
    if(!p) return 400;
    *p = '\0';
    p += 2;
	if(strncmp(p, "\r\n", 2) == 0) *p = '\0';
	pReqLine->header = p;
    
    pReqLine->method = strtok(buf, " \t");
    pReqLine->uri = strtok(NULL, " \t");
    pReqLine->proto_ver = strtok(NULL, " ");
    if(!pReqLine->proto_ver) return 400;

    return 0;
}

int ParseRequestOptions(char *header, REQUESTOPTIONS *pReqOpt)
{
	char *connection = NULL, *transfer_encoding = NULL;
	KEYVAL kvhead[] = {
		{ "Host", KEYVALTYPE_POINTER, &pReqOpt->host },
		{ "Content-Length", KEYVALTYPE_INT, &pReqOpt->content_length },
		//{ "Accept", KEYVALTYPE_POINTER, &pReqOpt->accept },
		{ "Accept-Language", KEYVALTYPE_POINTER, &pReqOpt->accept_language },
		{ "Accept-Charset", KEYVALTYPE_POINTER, &pReqOpt->accept_charset },
		{ "Content-Type", KEYVALTYPE_POINTER, &pReqOpt->content_type },
		{ "Connection", KEYVALTYPE_POINTER, &connection },
		{ "Cookie", KEYVALTYPE_POINTER, &pReqOpt->cookie },
		{ "CSeq", KEYVALTYPE_INT, &pReqOpt->cseq },
		{ "Session", KEYVALTYPE_POINTER, &pReqOpt->session },
		{ "Authorization", KEYVALTYPE_POINTER, &pReqOpt->authorization },
		{ "Transport", KEYVALTYPE_POINTER, &pReqOpt->transport },
		{ "Transfer-Encoding", KEYVALTYPE_POINTER, &transfer_encoding },
		{ "Retry-After", KEYVALTYPE_INT, &pReqOpt->retry_after }
	};
	memset(pReqOpt, 0, sizeof(REQUESTOPTIONS));

	if(!*header)
	{
		pReqOpt->body = header;
		return 0;
	}

	pReqOpt->body = strstr(header, "\r\n\r\n");
	if(!pReqOpt->body) return 400;
	pReqOpt->body += 4;
	ParseBody(header, kvhead, sizeof(kvhead)/sizeof(KEYVAL), 0);
	if(connection && PA_StrCaseCmp(connection, "Keep-Alive") == 0)
		pReqOpt->keep_alive = 1;
	if(transfer_encoding && PA_StrCaseCmp(transfer_encoding, "chunked")==0)
		pReqOpt->chunked_transfer = 1;
	return 0;
}

typedef struct _tagIdSTR {
	int id;
	const char *str;
} IDSTR;
static const IDSTR ctperrors[] = {
	{ E_CTP_OK,					"Operation successfully completed" },
	{ E_TIMEOUT,				"Timeout" }, //1
	{ E_ERROR_PROTO,			"Bad protocol, should be CTP/1.0" }, //2
	{ E_ERROR_ACK,				"Bad acknowlegement from proxy/device" }, 		//3
	{ E_CTP_HOST,				"Host can't be resolved" }, //4
	{ E_CTP_CONNECT,			"The host is down or unreachable" }, //5
	{ E_CTP_AUTHENTICATION,		"Bad user name or password" }, //6
	{ E_CTP_RIGHT,				"Have no respective rights" }, //7
	{ E_CTP_COMMAND,			"Bad command" }, //8
	{ E_CTP_PARAMETER,			"Bad parameters in command body" }, //9
	{ E_CTP_SYSTEM,				"Socket error." }, //10
	{ E_CTP_OTHER,				"Maybe WSAStartup not called" }, //11
	{ E_CONN,					"Can't connect to device's update-service" }, //12
	{ E_NO_CONN,				"Not connected yet" }, //13
	{ E_BUSY,					"Another command is running" }, //14
	{ E_OTHER,					"Other error" }, //15

	{ E_INVALID_OPERATION,		"Operate on an unsupported hardware configure. " }, //1001
	{ E_INVALID_PARAM,			"Invalid parameter" }, //1002

	{ E_BADFMT,					"Bad file format" }, //1101
	{ E_BADVER,					"Bad file version" }, //1102
	{ E_TAGNOTEXISTED,			"Tag not existed" }, //1103
	{ E_BUFFERTOOSMALL,			"Size of buffer is less than size of frame when read" }, //1104
	{ E_EOF,					"End of file" }, //1105
	{ E_CANNOTOPENFILE,			"Can't open video file" }, //1106
	{ E_WAITDATA,				"RemoteReader is waiting for next frame" }, //1107

	{ 100, "Continue" },
	{ 101, "Switching Protocols" },
	{ 200, "OK" },
	{ 201, "Created" },
	{ 202, "Accepted" },
	{ 203, "Non-Authoritative Information" },
	{ 204, "No Content" },
	{ 205, "Reset Content" },
	{ 206, "Partial Content" },
	{ 300, "Multiple Choices" },
	{ 301, "Moved Permanently" },
	{ 302, "Found" },
	{ 303, "See Other" },
	{ 304, "Not Modified" },
	{ 305, "Use Proxy" },
	{ 307, "Temporary Redirect" },
	{ 400, "Bad Request" },
	{ 401, "Unauthorized" },
	{ 402, "Payment Required" },
	{ 403, "Forbidden" },
	{ 404, "Not Found" },
	{ 405, "Method Not Allowed" },
	{ 406, "Not Acceptable" },
	{ 407, "Proxy Authentication Required" },
	{ 408, "Request Time-out" },
	{ 409, "Confilict" },
	{ 410, "Gone" },
	{ 411, "Length Required" },
	{ 412, "Precondition Failed" },
	{ 413, "Request Entity Too Large" },
	{ 414, "Request-URI Too Large" },
	{ 415, "Unsupported Media Type" },
	{ 416, "Requested range not satisfiable" },
	{ 417, "Expectation Failed" },
	{ 451, "Error Parameters" },
	{ 452, "Conference Not Found" },
	{ 454, "Session Not Found" },
		
	{ 500, "Internal Server Error" },
	{ 501, "Not Implement" },
	{ 502, "Bad Gateway" },
	{ 503, "Service Unavailable" },
	{ 504, "Gateway Time-out" },
	{ 505, "Version not supported" },

	{ 512, "Busy" },

	{ -1, NULL }
};

UINT GetCTPErrorString(int err, char *buf, UINT size)
{
	int e = err & 0xFFFF;
	if(e == 503 && (err & 0xFFFF0000))
	{
		char txt[100];
		sprintf(txt, "Retry after %d seconds", err >> 16);
		if(buf == NULL) return strlen(txt)+1;
		strncpyz(buf, txt, size);
		return strlen(buf) + 1;
	}
	else
	{
		const IDSTR *p = &ctperrors[0];
		while(p->str)
		{
			if(p->id == e) 
			{
				if(buf == NULL) return strlen(p->str) + 1;
				strncpyz(buf, p->str, size);
				return strlen(buf) + 1;
			}
			p++;
		}
		if(!buf) return 1;
		*buf = '\0';
		return 1;
	}
}
	
CString GetCTPErrorString(int err)
{
	UINT size = GetCTPErrorString(err, NULL, 0);
	char *p = (char*)malloc(size);
	GetCTPErrorString(err, p, size);
	CString str(p);
	free(p);
	return str;
}
//--------------------------------------------------------------------------
/*
NOTIFY reboot CTP/1.0
Host: xxx.xxx.xxx          ***

or:
NOTIFY alarm CTP/1.0
Content-Length: xxx        ***
Host: xxx.xxx.xxx          ***

-type x
-chn y
-onoff 0/1
-recchn z

*/
//////////////////////////////////////////////////////////////////////////////
BOOL ParseNotifyMessage(char *buf, int *notify, char **pHeader)
{
	char *tok, *p;

	p = strstr(buf, "\r\n");
	if(!p) return FALSE;
	*p = '\0';
	p += 2;

	tok = strtok(buf, " ");
	if(!tok) return FALSE;
	if(strcmp("NOTIFY", tok) != 0) return FALSE;
	tok = strtok(NULL, " ");
	if(strcmp(tok, "alarm") == 0) *notify = NOTIFICATION_ALARM;
	else if(strcmp(tok, "message") == 0) *notify = NOTIFICATION_MSG;
	else if(strcmp(tok, "stopservice") == 0) *notify = NOTIFICATION_STOPSERVICE;
	else if(strcmp(tok, "reboot") == 0) *notify = NOTIFICATION_REBOOT;
	else if(strcmp(tok, "login") == 0) *notify = NOTIFICATION_LOGIN;
	else if(strcmp(tok, "logout") == 0) *notify = NOTIFICATION_LOGOUT;
	else if(strcmp(tok, "notify") == 0) *notify = NOTIFICATION_NOTIFY;
	else return FALSE;

	if(strncmp(p, "\r\n", 2) == 0)
	{
		*p = '\0';
		*pHeader = NULL;
	}
	else
		*pHeader = p;

	return TRUE;
}

void DefaultNotificationHandler(int notify, void *p)
{
	switch(notify)
	{
	case NOTIFICATION_ALARM:
	case NOTIFICATION_LOGIN:
	case NOTIFICATION_LOGOUT:
	case NOTIFICATION_MSG:
	case NOTIFICATION_NOTIFY:
		free(p);
		break;
	case NOTIFICATION_STOPSERVICE:
	case NOTIFICATION_REBOOT:
		break;
	}
}
//=======================================================================================
//    以下函数据封装了一些常用命令
//
//=======================================================================================
DWORD CtpLogin(PA_SOCKET hSocket, const char *psczUser, const char *psczPswd, const char *pszOemId, /*OUT*/char *pszClientId, /*OUT*/ DWORD *pdwRight, 
			   const char *pszcExtraHeader, UINT timeout)
{
	CString str;
	char sid[20];
	int right, rlt;
	KEYVAL kv[] = { 
		{ "clientid", KEYVALTYPE_STRING, sid, 20 },
		{ "right", KEYVALTYPE_INT, &right }
	};
	str.Format("-login %s:%s\r\n-oemid %s\r\n\r\n", psczUser, psczPswd, pszOemId);
	if( (rlt = QueryCmd(hSocket, "cfgusr", str, kv, sizeof(kv)/sizeof(KEYVAL), 0, pszcExtraHeader, timeout)) ) 
	{
		//if(rlt == E_ERROR_USRPSW) rlt = 401;//E_CTP_AUTHENTICATION;
		return rlt;
	}
	if(pszClientId) strcpy(pszClientId, sid);
	if(pdwRight) *pdwRight = right;
	return 0;
}

BOOL EnumDevices(LPENUMDEVICE *ppEnumDev, UINT *pNDev, const char *pszOemId, UINT try_cnt)
{
	PA_SOCKET sk;
	int salen, iopt;
	struct sockaddr_in sa;
	CString strCmd;
	ENUMDEVICE *pEnumDev;
	int size, cnt;
	char *buf;
	
	sk = socket(AF_INET, SOCK_DGRAM, 0);
	if( !PA_SocketIsValid(sk) ) return FALSE;
	sa.sin_family = AF_INET;
	sa.sin_port = htons(7999);
	sa.sin_addr.s_addr = htonl(INADDR_ANY);
	memset(sa.sin_zero, 0, sizeof(sa.sin_zero));
	bind(sk, (struct sockaddr*)&sa, sizeof(sa));
	salen = sizeof(sa);
	PA_GetSockName(sk, (struct sockaddr*)&sa, &salen);

	iopt = 1; setsockopt(sk, SOL_SOCKET, SO_BROADCAST, (const char*)&iopt, sizeof(iopt));

	/*
	 * Send "SEARCH" and wait for REPLY 
	 */
	if(pszOemId) strCmd.Format("SEARCH * CTP/1.0\r\nFrom:%s\r\nOemId: %s\r\n\r\n", inet_ntoa(sa.sin_addr), pszOemId);
	else strCmd.Format("SEARCH * CTP/1.0\r\nFrom:%s\r\n\r\n", inet_ntoa(sa.sin_addr));
	
	buf = (char *)malloc(512);
	size = 10;
	cnt = 0;
	pEnumDev = (LPENUMDEVICE)malloc(sizeof(ENUMDEVICE) * size);

	for (int i = 0; i < try_cnt; i++) 
	{
		sockaddr_in safrom;
		int sasize, len;
		fd_set rfds;
		struct timeval tv;

		sa.sin_addr.s_addr = inet_addr("255.255.255.255");
		sendto(sk, strCmd, strCmd.GetLength(), 0, (const sockaddr*)&sa, sizeof(sa));
		while(1)
		{
			FD_ZERO(&rfds);
			FD_SET(sk, &rfds);
			tv.tv_sec = 0; tv.tv_usec = 300000;
			if(select(sk+1, &rfds, NULL, NULL, &tv) <= 0) break;
			else
			{
				sasize = sizeof(safrom);
				len = PA_RecvFrom(sk, buf, 512, 0, (sockaddr*)&safrom, &sasize);
				if(len > 0 && strncmp("REPLY", buf, 5) == 0)
				{
					char *p;
					buf[len] = '\0';
					p = strstr(buf, "\r\n");
					if(p)
					{
						CString sOemId;
						//int port_ctp;
						KEYVAL kv[] = { 
							{ "name", KEYVALTYPE_STRING, pEnumDev[cnt].cDevName, sizeof(pEnumDev[cnt].cDevName) },
							{ "sn", KEYVALTYPE_STRING, pEnumDev[cnt].cSN, sizeof(pEnumDev[cnt].cSN) },
							{ "ip", KEYVALTYPE_STRING, pEnumDev[cnt].cIp, sizeof(pEnumDev[cnt].cIp) },
							{ "netmask", KEYVALTYPE_STRING, pEnumDev[cnt].cNetMask, sizeof(pEnumDev[cnt].cNetMask) },
							{ "oemid", KEYVALTYPE_CSTRING, &sOemId },
							{ "portctp", KEYVALTYPE_INT, &pEnumDev[cnt].iPortCtp },
							{ "mdns", KEYVALTYPE_STRING, pEnumDev[cnt].cMDns, sizeof(pEnumDev[cnt].cMDns) },
							{ "sdns", KEYVALTYPE_STRING, pEnumDev[cnt].cSDns, sizeof(pEnumDev[cnt].cSDns) },
							{ "ddnsusr", KEYVALTYPE_STRING, pEnumDev[cnt].cDdnsUser, sizeof(pEnumDev[cnt].cDdnsUser) },
							{ "ddnsp", KEYVALTYPE_STRING, pEnumDev[cnt].cDdnsSvr, sizeof(pEnumDev[cnt].cDdnsSvr) },
							{ "ddn", KEYVALTYPE_STRING, pEnumDev[cnt].cDdn, sizeof(pEnumDev[cnt].cDdn) },
							{ "gw", KEYVALTYPE_STRING, pEnumDev[cnt].cGateway, sizeof(pEnumDev[cnt].cGateway) },
							{ "mac", KEYVALTYPE_STRING, pEnumDev[cnt].cMac, sizeof(pEnumDev[cnt].cMac) },
							{ "version", KEYVALTYPE_STRING, pEnumDev[cnt].cVersion, sizeof(pEnumDev[cnt].cVersion) },
							{ "dhcpen", KEYVALTYPE_INT, &pEnumDev[cnt].bDhcp }
						};
						p += 2;
						ParseBody(p, kv, sizeof(kv)/sizeof(KEYVAL));

						if(pszOemId && sOemId != "" && sOemId != pszOemId) continue;

						int ii;
						for(ii=0; ii<cnt; ii++)
						{
							if(strcmp(pEnumDev[ii].cIp, pEnumDev[cnt].cIp) == 0) break;
						}
						if(ii >= cnt) cnt++;

						if(cnt >= size) {
							size += 5;
							pEnumDev = (ENUMDEVICE*)realloc(pEnumDev, sizeof(ENUMDEVICE)*size);
						}
					}
				}
			}
		}
	}

	free(buf);
	PA_SocketClose(sk);
	if(cnt) *ppEnumDev = pEnumDev;
	else {
		free(pEnumDev);
		*ppEnumDev = NULL;
	}
	*pNDev = cnt;

	return TRUE;
}
