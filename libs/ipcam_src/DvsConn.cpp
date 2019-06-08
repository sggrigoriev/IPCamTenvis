#include "platform_adpt.h"
#include <DvsConn.h>
#include <errdefs.h>
#include <stdlib.h>
#include <ctype.h>
#include "misc.h"
#include "IpcamClt.h"

#define max(x, y) ((x)<(y)?(y):(x))
//===============================================================
void* ConcurentExcutionThread(void *pParam);
#ifdef WIN32
void* WINAPI AlertReceiveThread(LPVOID pParam);
#else
void* AlertReceiveThread(void *pParam);
#endif


LPEVENTCALLBACK g_pEventCallback = NULL;
int g_hPipe = -1;

LPEVENTCALLBACK SetEventCallback(LPEVENTCALLBACK pFunc)
{
	LPEVENTCALLBACK p = g_pEventCallback;
	g_pEventCallback = pFunc;
	return p;
}
void SetEventListener(int hPipe)
{
	g_hPipe = hPipe;
}
//===============================================================
BOOL IsTheHost(const DVSCONN *pConn, const char *name)
{
	const char *colon = strchr(name, ':');
	if(colon)
	{
		return strncmp(name, pConn->cHost, colon-name) == 0 && atoi(colon+1) == pConn->iPort;
	}
	else
	{
		return strcmp(pConn->cHost, name) == 0 && pConn->iPort == DEFAULT_SERVER_PORT;
	}
}
const char* Conn2Str(const DVSCONN *pConn)
{
	static char s[80];
	if(pConn->pAgent) return pConn->cHost;
	if(pConn->iPort == 8001) return pConn->cHost;
	else { sprintf(s, "%s:%d", pConn->cHost, pConn->iPort); return s; }
}
//===============================================================
void SortIdNames(IDNAME *pIdNames, UINT count)
{
	IDNAME tmp;
	UINT i, j;
	if(!pIdNames || count == 0) return;
	for(i = count-1; i > 0; i--)
	{
		for(j=0; j<i; j++)
		{
			if(pIdNames[j].id > pIdNames[j+1].id)
			{
				memcpy(&tmp, &pIdNames[j], sizeof(IDNAME));
				memcpy(&pIdNames[j], &pIdNames[j+1], sizeof(IDNAME));
				memcpy(&pIdNames[j+1], &tmp, sizeof(IDNAME));
			}
		}
	}
}
int InsertIdName(LPIDNAME* ppIdNames, UINT* pcount, const IDNAME *pIN)
{
	return InsertIdName(ppIdNames, pcount, pIN->id, pIN->name);
}
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-compare"
int InsertIdName(LPIDNAME* ppIdNames, UINT* pcount, int id, const char *sName)
{
	int count = *pcount;
	LPIDNAME pIdNames = *ppIdNames;
	for(int i=0; i<count; i++)
	{
		if(pIdNames[i].id == id)
		{
			strcpy(pIdNames[i].name, sName);
			return i;
		}
	}
	pIdNames = (IDNAME*)realloc(pIdNames, sizeof(IDNAME)*(count + 1));
	pIdNames[count].id = id;
	strcpy(pIdNames[count].name, sName);
	(*pcount) ++;
	*ppIdNames = pIdNames;
	return count;
}

BOOL RemoveIdName(LPIDNAME pIdNames, UINT* pcount, int id)
{
	for(UINT i=0; i<*pcount; i++)
	{
		if(pIdNames[i].id == id)
		{
			memcpy(&pIdNames[i], &pIdNames[i+1], sizeof(IDNAME)*(*pcount - i - 1));
			(*pcount)--;
			return TRUE;
		}
	}
	return FALSE;
}

IDNAME* IdNameFindID(LPIDNAME pIdNames, UINT count, int id)
{
	if(count == 0 || !pIdNames) return NULL;
	for(UINT i = 0; i<count; i++)
	{
		if(pIdNames[i].id == id) return &pIdNames[i];
	}
	return NULL;
}
#pragma GCC diagnostic pop

UINT GetUnusedID(const IDNAME *pIdNames, UINT count)
{
	UINT rlt = 1, i;
	while(1)
	{
		for(i=0; i<count; i++)
		{
			if(pIdNames[i].id == rlt) break;
		}
		if(i < count) rlt ++;
		else return rlt;
	}
}
////////////////////////////////////////////////////////////////////////
///////////////////// HEADERHELPER /////////////////////////////////////
HEADERHELPER::HEADERHELPER(DVSCONN *pConn, const char *extra_headers)
{
	headers = (char*)extra_headers;
	if(pConn->pAgent)
	{
		if(extra_headers && extra_headers[0])
		{
			headers = (char*)malloc(strlen(extra_headers) + 100);
			sprintf(headers, "Host: %s\r\n%s", pConn->cHost, extra_headers);
		}
		else
		{
			headers = (char*)malloc(100);
			sprintf(headers, "Host: %s\r\n", pConn->cHost);
		}
		bNeedFree = TRUE;
	}
	else
		bNeedFree = FALSE;

}
HEADERHELPER::~HEADERHELPER()
{
	if(bNeedFree && headers) free(headers);
}
HEADERHELPER::operator LPCSTR ()
{
	return headers;
}
////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////
void LibNotify(int notify, void *param, int param_len)
{
	if( (!g_pEventCallback || g_pEventCallback(notify, param) == 0) && g_hPipe > 0 )
	{
		PA_Write(g_hPipe, &notify, sizeof(uint32));
		PA_Write(g_hPipe, &param_len, sizeof(uint32));
		if(param_len > 0)
			PA_Write(g_hPipe, param, param_len);
		else
			PA_Write(g_hPipe, &param, sizeof(void*));
	}
}
////////////////////////////////////////////////////////////////////////

_DVSCONN::_DVSCONN(_DVSCONN *pProxy, const char *psOemId)
{
	bProxy = FALSE;
	cHost[0] = cUser[0] = cPassword[0] = sid[0] = cDestIP[0] = cOEMId[0] = 0;
	if(psOemId) strncpyz(cOEMId, psOemId, sizeof(cOEMId));
	memset(&devInfo, 0, sizeof(devInfo));
	uiWaitTimeout = 5000;
	pConnMgr = NULL;
	INIT_LIST_HEAD(&cmd_list);

	hAlertSock = hSocket = INVALID_SOCKET;
	iStatus = dwRight = 0;
	iPort = DEFAULT_SERVER_PORT;

	bPassive = FALSE;
	iAsynErrCode = 0;
	dwConnFlags = 0;
	iIdleSec = 0;
	if(pProxy) 
	{
		pAgent = pProxy;
	}
	else
	{
		pAgent = NULL;
	}
}

int _DVSCONN::PassiveMode(PA_SOCKET sk)
{
	if(iStatus != 0) return -1;

	bPassive = TRUE;
	hSocket = sk;
	int namelen = sizeof(devAddr);
	PA_GetPeerName(sk, (struct sockaddr*)&devAddr, &namelen);
	//strcpy(cHost, inet_ntoa(devAddr.sin_addr));
	//iPort = ntohs(devAddr.sin_port);

	dwConnFlags = CF_CREATEALERTCONN;
	iStatus = DEVICESTATUS_CONNECTING;

	int rlt;
	if( (rlt = OnConnect()) )
	{
		if( !(dwConnFlags & CF_RESUME) ) OnDisconnect();
		iStatus = 0;
		LibNotify(rlt==401?DEVICEEVENT_AUTH_FAILED:DEVICEEVENT_CONNECT_FAILED, this, 0);
	}
	else
	{
		iStatus = DEVICESTATUS_CONNECTED;	//connected;
		LibNotify(DEVICEEVENT_CONNECTED, this, 0);
	}
	return rlt;
}

_DVSCONN::~_DVSCONN()
{
	if(iStatus) 
	{
		if(iStatus == DEVICESTATUS_CONNECTED || iStatus == DEVICESTATUS_RESUMEFAILED) Disconnect();
	}
}

void _DVSCONN::Disconnect()
{
	dwConnFlags = 0;	//Clear CF_RESUME, so Ondisconnect will clean resources correctly
	if(hSocket != INVALID_SOCKET)
	{
		iStatus = 0;	//不再执行CTP命令

		ClearCmdList();

		OnDisconnect();

		LibNotify(DEVICEEVENT_DISCONNECTED, this, 0);
	}
	iStatus = 0;
}

//带CF_RESUME标志(Resume)调用失败，iStatus 为0而其它资源还是连接状态并没有释放，
//hSocket!=INVALID_SOCKET可作为资源未释放的标志
#if 1  //Use Asyn. way
int _DVSCONN::Connect(DWORD connflags)
{
	int rlt = 0;

	if(bPassive) return E_PASSIVEMODE;
	if(iStatus == DEVICESTATUS_CONNECTING) { return E_BUSY; }
	if(iStatus == DEVICESTATUS_CONNECTED && !(connflags & CF_RESUME)) { return E_OTHER; }
	if(iStatus == DEVICESTATUS_RESUMEFAILED && !(connflags & CF_RESUME))
	{ 
		if( (rlt = Resume()) == 0)
			LibNotify(DEVICEEVENT_RESUMED, this, 0); 
		return rlt; 
	}
			
	dwConnFlags = connflags & ~CF_ASYNC;
	bPassive = FALSE;
	iIdleSec = 0;

	if( (rlt = BeginAsynConnect()) == 0)
	{
		iStatus = DEVICESTATUS_CONNECTING;

		if(!pAgent)
		{
			LibNotify(DEVICEEVENT_CONNECTING, this, 0);

			CMDNODE *pCmd = pConnMgr->GetNode();
			pCmd->dwEvent = FEVENT_WRITE|FEVENT_ERROR|FEVENT_CONNECT;
			pCmd->timeWait = this->uiWaitTimeout;
			pConnMgr->QueueCmd(pAgent?pAgent:this, pCmd);
			PA_EventWait(pCmd->hEvent);

			if(pCmd->dwEvent & FEVENT_WRITE)
			{
				pConnMgr->MarkCmdFinished(pCmd);
				if(!pAgent)
				{
					PA_SocketSetNBlk(hSocket, FALSE);
				}
			}
			else
			{
				pConnMgr->MarkCmdFinished(pCmd);
				PA_SocketClose(hSocket);
				hSocket = INVALID_SOCKET;
				iStatus = 0;
				LibNotify(DEVICEEVENT_CONNECT_FAILED, this, 0);
				return E_CTP_CONNECT;
			}
		}

		if( (rlt = OnConnect()) )
		{
			OnDisconnect();
			if( !(dwConnFlags & CF_RESUME) ) iStatus = 0;
			else iStatus = DEVICESTATUS_RESUMEFAILED;
			LibNotify(rlt==401?DEVICEEVENT_AUTH_FAILED:DEVICEEVENT_CONNECT_FAILED, this, 0);
		}
		else
		{
			iStatus = DEVICESTATUS_CONNECTED;	//connected;
			LibNotify((dwConnFlags&CF_RESUME)?DEVICEEVENT_RESUMED:DEVICEEVENT_CONNECTED, this, 0);
		}
		return rlt;
	}
	return rlt;
}
#else  //Normal way
int _DVSCONN::Connect(DWORD connflags)
{
	int rlt = 0;

	if(bPassive) return E_PASSIVEMODE;
	if(iStatus == DEVICESTATUS_CONNECTING) { rlt = E_BUSY; goto out; }
	if(iStatus == DEVICESTATUS_CONNECTED && !(connflags & CF_RESUME)) { rlt = E_OTHER; goto out; }
	if(iStatus == DEVICESTATUS_RESUMEFAILED && !(connflags & CF_RESUME))
		{ rlt = Resume(); goto out; }
			
	dwConnFlags = connflags;
	bPassive = FALSE;

	if(pAgent)	//被代理
	{
/*		获得了子设备，说明 pAgent 已经连接了
		if(!(pAgent->GetStatus() == DEVICESTATUS_CONNECTED))
		{
			rlt = pAgent->Connect();
			if(rlt != E_OK) goto out;
		}
*/
		iStatus = DEVICESTATUS_CONNECTING;
		hSocket = pAgent->hSocket;
		strcpy(cDestIP, pAgent->cDestIP);
		memcpy(&devAddr, &pAgent->devAddr, sizeof(devAddr));
		dwRight = pAgent->dwRight;
	}
	else
	{
		if(dwConnFlags & CF_RESUME) PA_SocketClose(hSocket);
		else	//新连接
		{
			memset(&devAddr, 0, sizeof(devAddr));
			devAddr.sin_family = AF_INET;
			devAddr.sin_port = htons(iPort);
			if( !ResolveHost(cHost, (ULONG*)&devAddr.sin_addr.s_addr) )
			{
				cDestIP[0] = 0;
				rlt = E_CTP_HOST;
				goto out;
			}
		}


		hSocket = socket(AF_INET, SOCK_STREAM, 0);
		if(hSocket == INVALID_SOCKET) { rlt = E_CTP_OTHER; goto out; }

		iStatus = DEVICESTATUS_CONNECTING;
		LibNotify(DEVICEEVENT_CONNECTING, this, 0);

		rlt = connect(hSocket, (struct sockaddr*)&devAddr, sizeof(devAddr));
		if(rlt < 0) {
			PA_SocketClose(hSocket);
			hSocket = INVALID_SOCKET;
			iStatus = 0;
			LibNotify(DEVICEEVENT_CONNECT_FAILED, this, 0);
			rlt = E_CTP_CONNECT;
			goto out;
		}
	}

	if( (rlt = OnConnect()) )
	{
		if( !(dwConnFlags & CF_RESUME) ) OnDisconnect();
		iStatus = 0;
		LibNotify(rlt==401?DEVICEEVENT_AUTH_FAILED:DEVICEEVENT_CONNECT_FAILED, this, 0);
		goto out;
	}

	iStatus = DEVICESTATUS_CONNECTED;	//connected;

out:
	iIdleSec = 0;
	if(rlt == 0)
	{
		LibNotify((dwConnFlags&CF_RESUME)?DEVICEEVENT_RESUMED:DEVICEEVENT_CONNECTED, this, 0);
	}

	return rlt;
}
#endif
//static 
__STDCALL void AsynConnectCB(DVSCONN *pConn/*命令队列*/, int err, DWORD events, void *p/*发生事件*/)
{
	DVSCONN *pActConn = (DVSCONN*)p;
	if(err == 0 && (events & FEVENT_WRITE))
	{
		if(!pConn->pAgent)
		{
			PA_SocketSetNBlk(pConn->hSocket, 0);
		}
		if( (pActConn->iAsynErrCode = pActConn->OnConnect()) == 0)
		{
			//if(!pConn->pAgent && (pConn->dwConnFlags & CF_CREATEALERTCONN))
			//	pConn->CreateAlertConnection();
		}
		else
		{
			//pConn->OnDisconnect();
		}
	}
	else
	{
		dbg_msg("AsynConnectCB: E_CTP_SYSTEM\n");
		pActConn->iAsynErrCode = E_CTP_SYSTEM;
		pActConn->OnDisconnect();
	}

	LibNotify(pActConn->iAsynErrCode ? (pActConn->iAsynErrCode == 401 ? DEVICEEVENT_AUTH_FAILED : DEVICEEVENT_CONNECT_FAILED) : 
						( (pActConn->dwConnFlags & CF_RESUME) ? DEVICEEVENT_RESUMED : DEVICEEVENT_CONNECTED ), 
			pActConn, 0);

	if(pActConn->iAsynErrCode == 0) pActConn->iStatus = DEVICESTATUS_CONNECTED;
	else if(pActConn->dwConnFlags & CF_RESUME) pActConn->iStatus = DEVICESTATUS_RESUMEFAILED;
	else pActConn->iStatus = DEVICESTATUS_DISCONNECTED;
}
int _DVSCONN::AsynConnect(DWORD connflags)
{
	if(!pConnMgr) return E_OTHER;
	//if(!TryLock()) return E_BUSY;

	if(bPassive) return E_PASSIVEMODE;

	int rlt = 0;

	if(iStatus == DEVICESTATUS_CONNECTING) goto out;
	if(iStatus == DEVICESTATUS_CONNECTED && !(connflags & CF_RESUME)) { rlt = E_OTHER; goto out; }
	if(iStatus == DEVICESTATUS_RESUMEFAILED && !(connflags & CF_RESUME))
	{
		rlt = AsynResume();
		goto out;
	}

	struct list_head *p;
	list_for_each(p, &cmd_list)
	{
		CMDNODE *pCmd = list_entry(p, CMDNODE, cmd_list);
		if(pCmd->dwEvent & FEVENT_CONNECT)	//已经有一个连接请求在队列中
			goto out;
	}
	bPassive = FALSE;
	dwConnFlags = connflags | CF_ASYNC;
	if(BeginAsynConnect() == 0)
	{
		iStatus = DEVICESTATUS_CONNECTING;
		LibNotify(DEVICEEVENT_CONNECTING, this, 0);
		pConnMgr->QueueCmd(pAgent?pAgent:this, FEVENT_WRITE|FEVENT_ERROR|FEVENT_CONNECT, AsynConnectCB, this, 500000/*Large enough that connection will finish*/, NULL, 0, 0);
	}
	else
	{
		iStatus = DEVICESTATUS_DISCONNECTED;
		LibNotify(DEVICEEVENT_CONNECT_FAILED, this, 0);
	}

out:
	//Unlock();

	return rlt;
}

int _DVSCONN::BeginAsynConnect()
{
	if(pAgent)	//被代理
	{
		hSocket = pAgent->hSocket;
		strcpy(cDestIP, pAgent->cDestIP);
		dwRight = pAgent->dwRight;
		return 0;
	}
	else
	{
		memset(&devAddr, 0, sizeof(devAddr));
		devAddr.sin_family = AF_INET;
		devAddr.sin_port = htons(iPort);
		if( !ResolveHost(cHost, (ULONG*)&devAddr.sin_addr.s_addr) )
		{
			cDestIP[0] = 0;
			return E_CTP_HOST;
		}

		if(hSocket != INVALID_SOCKET)  PA_SocketClose(hSocket);
		hSocket = socket(AF_INET, SOCK_STREAM, 0);
		if(hSocket == INVALID_SOCKET) return E_CTP_OTHER;
		iStatus = DEVICESTATUS_CONNECTING;

		PA_SocketSetNBlk(hSocket, 1);
		//setsockopt(hSocket, IPPROTO_TCP, TCP_NODELAY, (const char*)&opt, sizeof(opt));
		if(connect(hSocket, (struct sockaddr*)&devAddr, sizeof(devAddr)) == PA_SOCKET_ERROR &&
#ifdef WIN32
			WSAGetLastError() == WSAEWOULDBLOCK
#elif defined(__LINUX__)
			errno == EINPROGRESS
#else
#error "Platform specified feature must be implemented !"
#endif
			) 
				return 0;
		return E_CTP_CONNECT;
	}
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wparentheses"
int _DVSCONN::Resume()
{
	return Connect(CF_RESUME | dwConnFlags & ~CF_ASYNC);//(hAlertSock != INVALID_SOCKET ? CF_CREATEALERTCONN : 0));
}
#pragma GCC diagnostic pop

int _DVSCONN::AsynResume()
{
	return AsynConnect(CF_ASYNC | CF_RESUME | dwConnFlags);//(hAlertSock != INVALID_SOCKET ? CF_CREATEALERTCONN : 0));
}

int _DVSCONN::OnConnect()
{
	int rlt = 0;
	if(!pAgent)
	{
		if(cUser[0]) 
		{
			if( (rlt = CtpLogin(hSocket, cUser, cPassword, cOEMId, sid, &dwRight, NULL, uiWaitTimeout)) )
				return rlt;
		}
		else
		{
			KEYVAL kv[] = { 
				{ "right", KEYVALTYPE_INT, &dwRight }
			};
			CString str;
			strcpy(sid, cPassword);
			dwRight = 0x80000000;		//Internal flag, Indicate that should send cookie
			rlt = QueryCmd("attachclt", str, kv, 1);
			//PA_SocketClose(hSocket); hSocket = INVALID_SOCKET;
			if(rlt) return rlt;
		}
	}
	if(dwConnFlags & CF_RESUME)
	{
		if(bProxy)
		{
			for(UINT i=0; i<proxyInfo.pvConn->size(); i++)
			{
				DVSCONN *pConn = proxyInfo.pvConn->at(i);
				pConn->hSocket = hSocket;
				if(pConn->GetStatus() >= DEVICESTATUS_CONNECTED)
					pConn->Resume();
			}
		}
		else
		{
			//无论如何要与设备通信一次以确认的确连接上了。但用不能修改原来的参数，否则单路被换成4路，程序就崩溃了
			DEVICEINFO di;
			rlt = CTPGetDeviceInfo(&di);
		}
	}
	else
	{
		strcpy(cDestIP, inet_ntoa(devAddr.sin_addr));
		if(bProxy)
		{
			KEYVAL kv[] = {
				{ "-name", KEYVALTYPE_STRING, proxyInfo.cName, sizeof(proxyInfo.cName) }
			};
			CString str;
			if(!proxyInfo.pvConn) proxyInfo.pvConn = new DvsConnArray();
			else proxyInfo.pvConn->clear();
			if( (rlt = ::QueryCmd(hSocket, "proxyinfo", str, kv, sizeof(kv)/sizeof(KEYVAL))) == 0)
			{
				rlt = FetchSubConns();
			}
		}
		else
		{
			if( (rlt = CTPGetDeviceInfo(&devInfo)) == 0)
			{
#if 0 && !defined(_DEBUG)
				if( strcmp(cOEMId, "TAS-Test-For-All") && isalpha(devInfo.cModel[0]) && strnicmp(devInfo.cModel, cOEMId, strlen(cOEMId)) ) 
					rlt = E_CTP_OTHER;
#endif
			}
		}
	}

	/* 创建报警连接 */
	if(!pAgent && (dwConnFlags & CF_CREATEALERTCONN))
	{
		CreateAlertConnection();
	}

	return rlt;
}
void _DVSCONN::OnDisconnect()
{
	if(bProxy && proxyInfo.pvConn && !(dwConnFlags & CF_RESUME))
	{
		for(UINT i=0; i<proxyInfo.pvConn->size(); i++)
		{
			DVSCONN *p = proxyInfo.pvConn->at(i);
			if(p->GetStatus() >= DEVICESTATUS_CONNECTED) p->Disconnect();
			delete p;
		}
		if(proxyInfo.pvConn)
		{
			delete proxyInfo.pvConn;
			proxyInfo.pvConn = NULL;
		}
	}
	if(!pAgent)
	{
		if(hAlertSock != INVALID_SOCKET)
			PA_SocketClose(hAlertSock);
		if(hSocket)
			PA_SocketClose(hSocket);
	}
	hAlertSock = INVALID_SOCKET;
	hSocket = INVALID_SOCKET;
}

//static 
__STDCALL BOOL _ListDevCB(char *line, void *arg)
{
	char fmt[60];
	DVSCONN *pAgent = (DVSCONN*)arg, *pConn;
	pConn = pAgent->NewConnObj();

	if(line[0] != 'd' || line[1] != ' ') return TRUE;
	//sprintf(fmt, "%%%ds %%%ds %%%ds", sizeof(pConn->cHost), sizeof(pConn->devInfo.cDevName), sizeof(pConn->devInfo.cDevSn));
	//if(ss_sscanf(',', line+2, fmt, pConn->cHost, pConn->devInfo.cDevName, pConn->devInfo.cDevSn) < 3)
	sprintf(fmt, "%%%d[^,], %%%d[^,], %%%d[^,]", sizeof(pConn->cHost), sizeof(pConn->devInfo.cDevName), sizeof(pConn->devInfo.cDevSn));
	if(sscanf(line+2, fmt, pConn->cHost, pConn->devInfo.cDevName, pConn->devInfo.cDevSn) < 2)
	{
		delete pConn;
	}
	else
	{
		pAgent->proxyInfo.pvConn->push_back(pConn);
	}

	return TRUE;
}

_DVSCONN* _DVSCONN::NewConnObj()
{
	return new _DVSCONN(this);
}
int _DVSCONN::FetchSubConns()
{
	int rlt = 0;
	if(bProxy)
	{
		CString str;
		rlt = CTPCommandWithCallback("listdev", str, _ListDevCB, this, NULL);
		if(rlt == 204)	//204 No Content
			rlt = 0;
	}

	return rlt;
}

UINT _DVSCONN::GetSID(char cltid[20])
{
	if(DEVICESTATUS_CONNECTED == iStatus)
	{
		strcpy(cltid, sid);
		return strlen(sid);
	}
	return 0;
}

int _DVSCONN::RequestNewConn(/*OUT*/PA_SOCKET* hSock)
{
	int rlt;
	CString str;
	for(int i=0; i<16; i++)
		conn_id[i] = 0x21 + (rand()%0x5F);
	conn_id[16] = '\0';
	str.Format("-conn_id %s\r\n\r\n", conn_id);
	if( (rlt = ExecCmd("newconn", str)) )
		return rlt;

	CMDNODE *pCmd = pConnMgr->GetNode();
	pCmd->dwEvent = FEVENT_NEWCONN;
	pCmd->timeWait = 7000;
	if(pConnMgr->QueueCmd(this, pCmd))
	{
		PA_EventWait(pCmd->hEvent);
		
		if(pCmd->dwEvent == 0) rlt = E_TIMEOUT;
		else *hSock = hNewConn;
		pConnMgr->MarkCmdFinished(pCmd);
	}
	return rlt;
}
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wparentheses"
BOOL _DVSCONN::CreateAlertConnection()	//????? Connect to Proxy or Real-Device
{
	if(pAgent) return FALSE;

	BOOL bCreateAlertOK = FALSE;
	if(hAlertSock != INVALID_SOCKET)
	{
		PA_SocketClose(hAlertSock);
		hAlertSock = INVALID_SOCKET;
	}

	if(bPassive && RequestNewConn(&hAlertSock) == 0 ||
					(hAlertSock = socket(AF_INET, SOCK_STREAM, 0)) > 0 && 
					connect(hAlertSock, (struct sockaddr*)&devAddr, sizeof(devAddr)) == 0)
	{
		CString ss;
		ss.Format("-clientid %s\r\n\r\n", sid);
		if(::ExecCmd(hAlertSock, "setalrm", ss, NULL, uiWaitTimeout) == 0)
		{
			bCreateAlertOK = TRUE;
		}
	}
	if(!bCreateAlertOK)
	{
		PA_SocketClose(hAlertSock);
		hAlertSock = INVALID_SOCKET;
	}

	return hAlertSock != INVALID_SOCKET;
}
#pragma GCC diagnostic pop

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wparentheses"
int DVSCONN::__DoRecv(char *cBuffer, UINT uiBuffSize, int iRecvLen, CString& strBody, VSCMDLINECB pCmdCbFunc, void *arg, BOOL bBinary)
{
rewait:
	if(iRecvLen == -2) { strBody = "Timeout"; return E_TIMEOUT; }
	if(iRecvLen < 0) { strBody = "Socket error"; return E_CTP_SYSTEM; }
	if(iRecvLen == 0) { strBody = "closed"; return E_PEERCLOSED; }


	int rlt;
	char *phead;
	REQUESTOPTIONS reqopt;

	/* Response line */
	cBuffer[iRecvLen] = '\0';
	if( (phead = strstr(cBuffer, "\r\n")) == NULL ) return E_ERROR_ACK;
	*phead = '\0'; phead += 2;
	rlt = ParseResponseHeaderLine(cBuffer, strBody);
	if(rlt == 100) 
	{
		iRecvLen = Recv(hSocket, cBuffer, uiBuffSize-1, uiWaitTimeout);
		goto rewait;
	}
	if(strcmp(phead, "\r\n") == 0) return rlt;	//无头部

	/* Headers */
	if(ParseRequestOptions(phead, &reqopt) != 0) return 400;
	if(rlt == 503)
	{
		rlt |= reqopt.retry_after << 16;
		return rlt;
	}
	else
	{
		if(reqopt.cookie) strcpy(sid, reqopt.cookie);
	}

	/* 
	 * 接 收 数 据 
	 */

		//缓冲区不够大或者content_length==0("ls")或者以chuncked方式传输(content_length<=0), 必须存在回调函数
	if((reqopt.content_length>0 && uiBuffSize<reqopt.content_length+1 || reqopt.content_length<=0 || reqopt.chunked_transfer) && !pCmdCbFunc) return E_CTP_OTHER;

	int clen = cBuffer + iRecvLen - reqopt.body; //收到的 content 长度
	int iTotal = clen;
		/* 接收数据直到收到 Content-Length 字节, 如果Content-Length超过缓冲区大小，必须要有回调函数存在 */
	memcpy(cBuffer, reqopt.body, clen+1);
	while(reqopt.content_length>0 && iTotal<=reqopt.content_length)
	{
		if(pCmdCbFunc)	//兼容snapshot
		{
			VSCMDBINCB pBinCb = (VSCMDBINCB)pCmdCbFunc;
			if(clen && pBinCb((BYTE*)cBuffer, clen, arg) == 0) 
				return 0;
			clen = 0;
		}
#ifndef min
#define min(x,y) ((x)>(y)?(y):(x))
#endif
		iRecvLen = min(uiBuffSize-clen-1, reqopt.content_length - iTotal);
		if(iRecvLen == 0) break;
		iRecvLen = Recv(hSocket, cBuffer + clen, iRecvLen, uiWaitTimeout/2);
		if(iRecvLen <= 0) goto rewait;
		clen += iRecvLen;
		iTotal += iRecvLen;
		cBuffer[clen] = '\0';
	}
	if(reqopt.content_length > 0)
	{
		if(!pCmdCbFunc) strBody = cBuffer;
		return 0;
	}

		/* chuncked 或者 按行返回 */
	char *pp, *pcontent = cBuffer;
	while(1)
	{
		while(1)
		{
			pp = strstr(pcontent, "\r\n");
			if(!pp) break;

			if(reqopt.chunked_transfer)
			{
				int chunk_len = strtoul(pcontent, NULL, 16);
				if(chunk_len == 0) return 0;
				if(chunk_len+2 > cBuffer+clen-(pp+2)) break;	//chunk data 之后为 \r\n
				if(bBinary)
				{
					VSCMDBINCB pBinCb = (VSCMDBINCB)pCmdCbFunc;
					pBinCb((BYTE*)pp+2, chunk_len, arg);
				}
				else
				{
					pp[chunk_len+2] = '\0';
					pCmdCbFunc(pp+2, arg);
				}
				pcontent = pp + 2 + chunk_len + 2;
			}
			else
			{
				*pp = '\0';
				if(pcontent == pp || !pCmdCbFunc(pcontent, arg))  return 0;
				pcontent = pp + 2;
			}
		}

		clen -= (pcontent-cBuffer);
		memcpy(cBuffer, pcontent, clen);
		pcontent = cBuffer;
		iRecvLen = Recv(hSocket, cBuffer + clen, uiBuffSize-1-clen, 4000);
		if(iRecvLen > 0) 
		{
			clen += iRecvLen;
			cBuffer[clen] = '\0';
		}
		else
			goto rewait;
	}

	return 0;
}
int _DVSCONN::_CTPCommandWithCallback(const char *scmd, /*INOUT*/ CString &strBody, const char *extra_headers, VSCMDLINECB pCmdCbFunc, void *arg, BOOL bBinary)
{
	HEADERHELPER hdrs(this, extra_headers);
	int rlt = 0;
#define CMDBUF_SIZE	5000		
	char *cBuffer = (char*)malloc(CMDBUF_SIZE), *ptr;
#ifdef WIN32
	if(pConnMgr && pConnMgr->GetConThreadId() != GetCurrentThreadId())
#elif defined(__LINUX__)
	if(pConnMgr && pConnMgr->GetConThreadId() != getpid())
#endif
	{	
		ptr = SetCTPProtoclHead(cBuffer, scmd, strBody.GetLength(), hdrs);
		if(sid[0] && (dwRight & 0x80000000)) ptr += sprintf(ptr-2, "Cookie: cltid=%s\r\n\r\n", sid) - 2;
		if(strBody.GetLength() > 0)	strcpy(ptr, strBody);

		CMDNODE *pCmd = pConnMgr->GetNode();
		pCmd->dwEvent = FEVENT_READ|FEVENT_ERROR;
		pCmd->pCmdToSend = cBuffer;
		pCmd->nCmdLen = strlen(cBuffer);
		pCmd->timeWait = uiWaitTimeout;
		pCmd->flags = CCFLAG_DONTFREEMEM;
		if(pConnMgr->QueueCmd(this, pCmd))
		{
			PA_EventWait(pCmd->hEvent);
			
			if(pCmd->dwEvent == 0) rlt = E_TIMEOUT;
			else if(pCmd->dwEvent & FEVENT_ERROR) 
			{ 
				iIdleSec = 0; rlt = E_CTP_SYSTEM; 
				dbg_msg("select: E_CTP_SYSTEM\n");
			}
			else if(pCmd->dwEvent & FEVENT_READ)
			{
				iIdleSec = 0;
				int iRecvLen = recv(hSocket, cBuffer, CMDBUF_SIZE-1, 0);
				if(iRecvLen == 0)
				{
					LibNotify(DEVICEEVENT_PEERCLOSED, this, 0);
				}
				else
					rlt = __DoRecv(cBuffer, CMDBUF_SIZE, iRecvLen, strBody, pCmdCbFunc, arg, bBinary);
			}
			pConnMgr->MarkCmdFinished(pCmd);
		}
	}
	else
	{
		while(Recv(hSocket, cBuffer, CMDBUF_SIZE, 0) > 0);	//读以前命令超时返回

		ptr = SetCTPProtoclHead(cBuffer, scmd, strBody.GetLength(), hdrs);
		if(sid[0] && (dwRight & 0x80000000)) ptr += sprintf(ptr-2, "Cookie: cltid=%s\r\n\r\n", sid) - 2;
		if(strBody.GetLength() > 0)	strcpy(ptr, strBody);

		send(hSocket, cBuffer, strlen(cBuffer), 0);
		int iRecvLen = Recv(hSocket, cBuffer, CMDBUF_SIZE, uiWaitTimeout);
		rlt = __DoRecv(cBuffer, CMDBUF_SIZE, iRecvLen, strBody, pCmdCbFunc, arg, bBinary);
	}
	free(cBuffer);
#undef CMDBUF_SIZE	
	return rlt;
}
#pragma GCC diagnostic pop
int _DVSCONN::CTPCommand(const char *cmd, /*INOUT*/ CString &str, const char *extra_headers)
{
	return _CTPCommandWithCallback(cmd, str, extra_headers, NULL, NULL, 0);
}
int _DVSCONN::ExecCmd(const char *cmd, CString &str, const char *extra_headers)
{
	return CTPCommand(cmd, str, extra_headers);
}
int _DVSCONN::ExecCmd(CString &str, const char *extra_headers)
{
	extern char *findpara(char *str);
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

	int rlt = ExecCmd(cmd, str, extra_headers);
	free(buf);
	return rlt;
}
int _DVSCONN::QueryCmd(const char *cmd, CString &str/*IN:body, OUT:reason or data*/, KEYVAL *pKv, UINT size, DWORD flags, const char *extra_headers)
{
	int rlt = CTPCommand(cmd, str, extra_headers);
	if(rlt == 0)
	{
		ParseBody(str.GetBuffer(str.GetLength()), pKv, size, flags);
		str.ReleaseBuffer();
	}
	return rlt;
}

int _DVSCONN::CTPCommandWithCallback(const char *scmd, /*INOUT*/CString &strBody, VSCMDLINECB pCmdCbFunc, void *arg, const char *extra_headers)
{
	return _CTPCommandWithCallback(scmd, strBody, extra_headers, pCmdCbFunc, arg, FALSE);
}
int _DVSCONN::CTPCommandWithCallbackBin(const char *scmd, /*INOUT*/CString &strBody, VSCMDBINCB pCmdCbFunc, void *arg, const char *extra_headers)
{
	return _CTPCommandWithCallback(scmd, strBody, extra_headers, (VSCMDLINECB)pCmdCbFunc, arg, TRUE);
}
void _DVSCONN::ClearCmdList()
{
	if(pConnMgr)
	{
		struct list_head *p, *q;
		pConnMgr->m_QueueLock.Lock();
		list_for_each_safe(p, q, &cmd_list)		
		{
			CMDNODE *pCmd = list_entry(p, CMDNODE, cmd_list);
			if(!(pCmd->flags & CCFLAG_DONTFREEMEM)) 
			{
				if(pCmd->pCmdToSend) free(pCmd->pCmdToSend);
				pCmd->pCmdToSend = NULL;
			}
			if(pCmd->lpFunc)	pCmd->lpFunc(this, E_TIMEOUT, 0, pCmd->pParam);
			else
			{
				pCmd->dwEvent = 0;
				pCmd->flags = 0;
				PA_EventSet(pCmd->hEvent);
			}
			list_del_init(&pCmd->cmd_list);
			list_add_tail(&pCmd->cmd_list, &pConnMgr->m_freeCmdList);
			PA_EventSet(pConnMgr->m_hEventNodeAvailable);
		}
		pConnMgr->m_QueueLock.Unlock();
	}
}

//====================================================================
static void ba2str(const BYTE *ba, int n, char *s)
{
	int i;
	s[0] = '\0';
	for(i=0; i<n; i++) s += sprintf(s, "%02X ", ba[i]);
}

BOOL _DVSCONN::PTZCmdUD(const PTZSETTING *ptz, const BYTE *buf, UINT len)
{
	return CTPRelay485(&ptz->comm, buf, len);
}

int _DVSCONN::CTPGetDeviceInfo(DEVICEINFO *pDevInfo)
{
	DWORD rlt;
	KEYVAL kv[] = {
		{ "-chnamnt", KEYVALTYPE_INT, &pDevInfo->iNumOfVChn },
		{ "-achnamnt", KEYVALTYPE_INT, &pDevInfo->iNumOfAChn },
		{ "-svrno", KEYVALTYPE_STRING, pDevInfo->cDevSn, sizeof(pDevInfo->cDevSn) },
		{ "-versoft", KEYVALTYPE_STRING, pDevInfo->cVerSoft, sizeof(pDevInfo->cVerSoft) },
		{ "-verhard", KEYVALTYPE_STRING, pDevInfo->cVerHard, sizeof(pDevInfo->cVerHard) },
		{ "-svrname", KEYVALTYPE_STRING, pDevInfo->cDevName, sizeof(pDevInfo->cDevName) },
		{ "-almin", KEYVALTYPE_INT, &pDevInfo->iAlarmIn },
		{ "-almout", KEYVALTYPE_INT, &pDevInfo->iAlarmOut },
		{ "-typeid", KEYVALTYPE_INT, &pDevInfo->uTypeId },
		{ "-model", KEYVALTYPE_STRING, pDevInfo->cModel, sizeof(pDevInfo->cModel) },
		{ "-pubtime", KEYVALTYPE_STRING, pDevInfo->cPubTime, sizeof(pDevInfo->cPubTime) },
		{ "-hwflags", KEYVALTYPE_INT, &pDevInfo->dwHwFlags }
	};
	CString str("-list\r\n\r\n");

	if( (rlt = QueryCmd("cfgsvr", str, kv, sizeof(kv)/sizeof(KEYVAL))) == 0)
	{
#if !defined(_DEBUG) && 0
		if( strcmp(pDevInfo->cOEMId, "TAS-Test-For-All") && isalpha(cModel[0]) && strnicmp(cModel, pDevInfo->cOEMId, strlen(cOEMId)) ) return E_CONNECTION;
#endif
	}

	return rlt;
}

int _DVSCONN::CTPSetServerName(const char *svrName)
{
	if(bProxy) return E_INVALID_OPERATION;
	if(iStatus == DEVICESTATUS_DISCONNECTED || iStatus == DEVICESTATUS_RESUMEFAILED) return E_NO_CONN;
	if(!(dwRight & USERRIGHT_SETUP_PARAM)) return E_CTP_RIGHT;;
		
	CString strCmd;
	strCmd.Format("cfgsvr -svrname \"%s\"", svrName);
	return ExecCmd(strCmd);
}

//If Secondary stream is not supported, iSubMode is -1
int _DVSCONN::CTPGetVChnInfo(UINT vchn, VCHNINFO *pVChnInfo)
{
	if(iStatus == DEVICESTATUS_DISCONNECTED || iStatus == DEVICESTATUS_RESUMEFAILED) return E_NO_CONN;

	memset(pVChnInfo, 0, sizeof(VCHNINFO));
	pVChnInfo->iSubMode = -1;

	CString str;
	KEYVAL kv[] = {
		{ "-active", KEYVALTYPE_INT, &pVChnInfo->iActive },
		{ "-cname", KEYVALTYPE_STRING, pVChnInfo->cName, sizeof(pVChnInfo->cName) },
		{ "-cif", KEYVALTYPE_INT, &pVChnInfo->iMode },
		{ "-suben", KEYVALTYPE_INT, &pVChnInfo->iSubEn },
		{ "cifsub", KEYVALTYPE_INT, &pVChnInfo->iSubMode },
		{ "-norm", KEYVALTYPE_INT, &pVChnInfo->iNorm },
		{ "-chnai", KEYVALTYPE_INT, &pVChnInfo->iAuChn }
	};
	
 	str.Format("-chn %d\r\n" "-list\r\n\r\n", vchn);
	return QueryCmd("cfgviattr", str, kv, sizeof(kv)/sizeof(KEYVAL), PF_DONTINITVALS);
}
int _DVSCONN::CTPSetVChnInfo(UINT vchn, const VCHNINFO *pVChnInfo)
{
	if(iStatus == DEVICESTATUS_DISCONNECTED || iStatus == DEVICESTATUS_RESUMEFAILED) return E_NO_CONN;
	if(!(dwRight & USERRIGHT_SETUP_PARAM)) return E_CTP_RIGHT;;

	CString str;

	str.Format("-chn %d\r\n-cname \"%s\"\r\n-cif %d\r\n-norm %d\r\n-chnai %d\r\n-suben %d\r\n-cifsub %d\r\n", 
		vchn, pVChnInfo->cName, pVChnInfo->iMode, pVChnInfo->iNorm, pVChnInfo->iAuChn, pVChnInfo->iSubEn, pVChnInfo->iSubMode);
	return ExecCmd("cfgviattr", str);
}

int _DVSCONN::CTPGetVChnInfo2(UINT vchn, VCHNINFO2 *pVChnInfo)
{
	if(iStatus == DEVICESTATUS_DISCONNECTED || iStatus == DEVICESTATUS_RESUMEFAILED) return E_NO_CONN;

	memset(pVChnInfo, 0, sizeof(VCHNINFO2));

	CString str;
	KEYVAL kv[] = {
		//{ "-active", KEYVALTYPE_INT, &pVChnInfo->iActive },
		{ "-cname", KEYVALTYPE_STRING, pVChnInfo->cName, sizeof(pVChnInfo->cName) },
		{ "-norm", KEYVALTYPE_INT, &pVChnInfo->vstd },
		{ "-achn", KEYVALTYPE_INT, &pVChnInfo->iAuChn }
	};
	
	pVChnInfo->iActive = 1;
 	str.Format("-chn %d\r\n" "-list\r\n\r\n", vchn);
	return QueryCmd("viattr", str, kv, sizeof(kv)/sizeof(KEYVAL), PF_DONTINITVALS);
}
int _DVSCONN::CTPSetVChnInfo2(UINT vchn, const VCHNINFO2 *pVChnInfo)
{
	if(iStatus == DEVICESTATUS_DISCONNECTED || iStatus == DEVICESTATUS_RESUMEFAILED) return E_NO_CONN;
	if(!(dwRight & USERRIGHT_SETUP_PARAM)) return E_CTP_RIGHT;;

	CString str;

	str.Format("-chn %d\r\n-cname \"%s\"\r\n-norm %d\r\n-achn %d\r\n", 
		vchn, pVChnInfo->cName, pVChnInfo->vstd, pVChnInfo->iAuChn);
	return ExecCmd("viattr", str);
}

int _DVSCONN::CTPGetAChnInfo(UINT achn, ACHNINFO *pAChnInfo)
{
	CString str;
	KEYVAL kv[] = {
		{ "-smpbit", KEYVALTYPE_INT, &pAChnInfo->iPrec },
		{ "-smprate", KEYVALTYPE_INT, &pAChnInfo->iFreq },
		{ "-encode", KEYVALTYPE_INT, &pAChnInfo->iEncode }
	};

	str.Format("-chn %d\r\n" "-list\r\n\r\n", achn);
	return QueryCmd("cfgaudio", str, kv, sizeof(kv)/sizeof(KEYVAL));
}

int _DVSCONN::CTPSetAChnInfo(UINT achn, const ACHNINFO *pAChnInfo)
{
	if(iStatus == DEVICESTATUS_DISCONNECTED || iStatus == DEVICESTATUS_RESUMEFAILED) return E_NO_CONN;
	if(!(dwRight & USERRIGHT_SETUP_PARAM)) return E_CTP_RIGHT;;

	CString str;
	str.Format("-chn %d\r\n-smpbit %d\r\n-smprate %d\r\n-encode %d\r\n\r\n", 
		achn, pAChnInfo->iPrec, pAChnInfo->iFreq, pAChnInfo->iEncode );
	return ExecCmd("cfgaudio", str);
}

int _DVSCONN::CTPGetPtzSetting(UINT vchn, PTZSETTING *pPtzParam)
{
	CString str;
	KEYVAL kv[] = {
		{ "proto", KEYVALTYPE_STRING, pPtzParam->cProto, 32 },
		{ "step", KEYVALTYPE_INT, &pPtzParam->iDefSpeed },
		{ "addr", KEYVALTYPE_INT, &pPtzParam->iPtzAddr },
		{ "bps", KEYVALTYPE_INT, &pPtzParam->comm.bps },
		{ "data", KEYVALTYPE_INT, &pPtzParam->comm.databits },
		{ "stop", KEYVALTYPE_INT, &pPtzParam->comm.stopbits },
		{ "parity", KEYVALTYPE_INT, &pPtzParam->comm.parity }
	};

	str.Format("-chn %d\r\n" "-list\r\n\r\n", vchn);
	return QueryCmd("ptzcfg", str, kv, sizeof(kv)/sizeof(KEYVAL));
}

int _DVSCONN::CTPSetPtzSetting(UINT vchn, const PTZSETTING *pPtzParam)
{
	if(iStatus == DEVICESTATUS_DISCONNECTED || iStatus == DEVICESTATUS_RESUMEFAILED) return E_NO_CONN;
	if(!(dwRight & USERRIGHT_SETUP_PARAM)) return E_CTP_RIGHT;;

	CString str;

	str.Format("-chn %d\r\n-proto %s\r\n-addr %d\r\n-step %d\r\n-bps %d\r\n-data %d\r\n-stop %d\r\n-parity %d\r\n\r\n", 
		vchn, pPtzParam->cProto, pPtzParam->iPtzAddr, pPtzParam->iDefSpeed, 
		pPtzParam->comm.bps, pPtzParam->comm.databits,pPtzParam->comm.stopbits, pPtzParam->comm.parity);

	return ExecCmd("ptzcfg", str);
}

static int isSeperator(int ch)
{
	return ch>0 && (isspace(ch) || ch == ':' || ch == '=');
}
#define MAX_IDNAME_COUNT	128
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wparentheses"
static int ParseIdNameBody(char *pBody, LPIDNAME *ppIdName)
{
	int cnt;
	LPIDNAME pIdName;
	char *p;

	*ppIdName = NULL;

	cnt = 0;
	pIdName = (LPIDNAME)malloc(sizeof(IDNAME)*MAX_IDNAME_COUNT);
	while(p = strstr(pBody, "\r\n"))
	{
		char key[10];
		char *s1 = pBody;
		pBody = p + 2;
		while(isSeperator(*(p-1))) p--;		//rtrim()
		*p = '\0';							//p = Value End
		while(isspace(*s1)) s1++;			//ltrim(), s1 = Key Start
		if(*s1 == '-') s1++;				//Jump over the first '-'

		key[0] = 0;
		sscanf(s1, "%s %s", key, pIdName[cnt].name);
		if(strncmp(key, "name", 4) == 0)
		{
			pIdName[cnt].id = atoi(key+4);	// -nameX
			cnt++;
			if(cnt >= MAX_IDNAME_COUNT)
			{
				*ppIdName = pIdName;
				return cnt;
			}
		}
	}
	if(cnt == 0) { free(pIdName); pIdName = NULL; }
	else pIdName = (LPIDNAME)realloc(pIdName, sizeof(IDNAME)*cnt);

	*ppIdName = pIdName;
	return cnt;
}
#pragma GCC diagnostic pop

int _DVSCONN::CTPGetAlarmInChnInfo(UINT chn, ALARMINCHNINFO *pAici)
{
	if(chn >= devInfo.iAlarmIn) return E_INVALID_PARAM;
		
	KEYVAL kv[] = {
		{ "tapech", KEYVALTYPE_INT, &pAici->dwRecordChns },
		{ "dealmode", KEYVALTYPE_INT, &pAici->dwActions },
		{ "ptzmode", KEYVALTYPE_INT, &pAici->iPtzMode },
		{ "posline", KEYVALTYPE_INT, &pAici->iPos },
		{ "chnvi", KEYVALTYPE_INT, &pAici->iVChn },
		{ "name", KEYVALTYPE_STRING, pAici->cName, sizeof(pAici->cName) },

		{ "ts0", KEYVALTYPE_STRING, NULL },
		{ "ts1", KEYVALTYPE_STRING, NULL },
		{ "ts2", KEYVALTYPE_STRING, NULL },
		{ "ts3", KEYVALTYPE_STRING, NULL },
		{ "ts4", KEYVALTYPE_STRING, NULL },
		{ "ts5", KEYVALTYPE_STRING, NULL },
		{ "ts6", KEYVALTYPE_STRING, NULL },
		{ "ts7", KEYVALTYPE_STRING, NULL },
		{ "ts8", KEYVALTYPE_STRING, NULL },
		{ "ts9", KEYVALTYPE_STRING, NULL },
		{ "ts10", KEYVALTYPE_STRING, NULL },
		{ "ts11", KEYVALTYPE_STRING, NULL },
		{ "ts12", KEYVALTYPE_STRING, NULL },
		{ "ts13", KEYVALTYPE_STRING, NULL },
		{ "ts14", KEYVALTYPE_STRING, NULL },
		{ "ts15", KEYVALTYPE_STRING, NULL }
	};

	DWORD rlt;
	CString str;

	pAici->nTSeg = 0;
	str.Format("-chn %d\r\n-list\r\n\r\n", chn);
	rlt = QueryCmd("cfgalertin", str, kv, sizeof(kv)/sizeof(KEYVAL));
	if(rlt == 0)
	{
		KEYVAL *pKvTs = &kv[6];
		for(int i=0; i<16; i++, pKvTs++)
		{
			if(pKvTs->sVal && str2timeseg(pKvTs->sVal, &pAici->tseg[i]))
				pAici->nTSeg++;
		}
	}

	return rlt;
}

int _DVSCONN::CTPSetAlarmInChnInfo(UINT chn, const ALARMINCHNINFO *pAici)
{
	if(iStatus == DEVICESTATUS_DISCONNECTED || iStatus == DEVICESTATUS_RESUMEFAILED) return E_NO_CONN;
	if(!(dwRight & USERRIGHT_SETUP_PARAM)) return E_CTP_RIGHT;;

	if(chn >= devInfo.iAlarmIn) return E_INVALID_PARAM;
		
	CString str, strTmp;
	str.Format("-chn %d\r\n-name \"%s\"\r\n-dealmode %d\r\n-ptzmode %d\r\n-tapech %d\r\n-chnvi %d\r\n",
		chn, pAici->cName, pAici->dwActions, pAici->iPtzMode, pAici->dwRecordChns, pAici->iVChn);
	if(pAici->iPtzMode > 0)
	{
		strTmp.Format(" -posline %d\r\n", pAici->iPos);
		str += strTmp;
	}
	for(UINT i=0; i<pAici->nTSeg; i++)
	{
		strTmp.Format("-ts%d %s\r\n", i, (const char*)Timeseg2Str(&pAici->tseg[i]));
		str +=  strTmp;
	}
	str += "\r\n";
	return ExecCmd("cfgalertin", str);
}

int _DVSCONN::CTPGetAlarmOutChnInfo(UINT chn, ALARMOUTCHNINFO *pAoci)
{
	if(chn >= devInfo.iAlarmOut) return E_INVALID_PARAM;
		
	KEYVAL kv[] = {
		{ "trig", KEYVALTYPE_INT, &pAoci->iTrig },
		{ "time", KEYVALTYPE_INT, &pAoci->iDuration },
		{ "name", KEYVALTYPE_STRING, pAoci->cName, sizeof(pAoci->cName) },

		{ "ts0", KEYVALTYPE_STRING, NULL },
		{ "ts1", KEYVALTYPE_STRING, NULL },
		{ "ts2", KEYVALTYPE_STRING, NULL },
		{ "ts3", KEYVALTYPE_STRING, NULL },
		{ "ts4", KEYVALTYPE_STRING, NULL },
		{ "ts5", KEYVALTYPE_STRING, NULL },
		{ "ts6", KEYVALTYPE_STRING, NULL },
		{ "ts7", KEYVALTYPE_STRING, NULL },
		{ "ts8", KEYVALTYPE_STRING, NULL },
		{ "ts9", KEYVALTYPE_STRING, NULL },
		{ "ts10", KEYVALTYPE_STRING, NULL },
		{ "ts11", KEYVALTYPE_STRING, NULL },
		{ "ts12", KEYVALTYPE_STRING, NULL },
		{ "ts13", KEYVALTYPE_STRING, NULL },
		{ "ts14", KEYVALTYPE_STRING, NULL },
		{ "ts15", KEYVALTYPE_STRING, NULL }
	};
	CString str;
	DWORD rlt;
	
	pAoci->nTSeg = 0;
	str.Format("-chn %d\r\n-list\r\n\r\n", chn);
	if( (rlt = QueryCmd( "cfgalertout", str, kv, sizeof(kv)/sizeof(KEYVAL))) == 0 )
	{
		KEYVAL *pKvTs = &kv[3];
		for(int i=0; i<16 && pKvTs->sVal; i++, pKvTs++)
		{
			str2timeseg(pKvTs->sVal, &pAoci->tseg[i]);
			pAoci->nTSeg++;
		}
	}
	return rlt;
}

int _DVSCONN::CTPSetAlarmOutChnInfo(UINT chn, const ALARMOUTCHNINFO *pAoci)
{
	if(iStatus == DEVICESTATUS_DISCONNECTED || iStatus == DEVICESTATUS_RESUMEFAILED) return E_NO_CONN;
	if(!(dwRight & USERRIGHT_SETUP_PARAM)) return E_CTP_RIGHT;;

	if(chn >= devInfo.iAlarmOut) return E_INVALID_PARAM;
		
	CString str;
	str.Format("-chn %d\r\n-name \"%s\"\r\n-trig %d\r\n-time %d\r\n",
		chn, pAoci->cName, pAoci->iTrig, pAoci->iDuration);
	for(UINT i=0; i<pAoci->nTSeg; i++)
	{
		CString strTmp;
		strTmp.Format("-ts%d %s\r\n", i, (const char*)Timeseg2Str(&pAoci->tseg[i]));
		str += strTmp;
	}
	str += "\r\n";
	return ExecCmd("cfgalertout", str);
}

int _DVSCONN::CTPGetVEncParam(UINT vchn, VIDEOENCPARAM *pParam)
{
	if(vchn >= devInfo.iNumOfVChn) return E_INVALID_PARAM;
		
	KEYVAL kv[] = {
		{ "fmode", KEYVALTYPE_INT, &pParam->fMode },
		{ "sub", KEYVALTYPE_INT, &pParam->code },
		{ "bps", KEYVALTYPE_INT, &pParam->bps },
		{ "fps", KEYVALTYPE_INT, &pParam->fps },
		{ "gop", KEYVALTYPE_INT, &pParam->gop },
		{ "quality", KEYVALTYPE_INT, &pParam->quality }
	};
	CString str;
	str.Format("-chn %d\r\n-sub %d\r\n-list\r\n\r\n", vchn, pParam->code);
	return QueryCmd("cfgh264", str, kv, sizeof(kv)/sizeof(KEYVAL));
}
int _DVSCONN::CTPSetVEncParam(UINT vchn, const VIDEOENCPARAM *pParam)
{
	if(iStatus == DEVICESTATUS_DISCONNECTED || iStatus == DEVICESTATUS_RESUMEFAILED) return E_NO_CONN;
	if(!(dwRight & USERRIGHT_SETUP_PARAM)) return E_CTP_RIGHT;;

	if(vchn >= devInfo.iNumOfVChn) return E_INVALID_PARAM;

	CString str;
	str.Format("-chn %d\r\n-fmode %d\r\n-sub %d\r\n-bps %d\r\n-fps %d\r\n-gop %d\r\n-quality %d\r\n\r\n",
		vchn, pParam->fMode, pParam->code, pParam->bps, pParam->fps, pParam->gop, pParam->quality);
	return ExecCmd("cfgh264", str);
}

int _DVSCONN::CTPGetVEncParam2(UINT vchn, VIDEOENCPARAM2 *pParam)
{
	if(vchn >= devInfo.iNumOfVChn) return E_INVALID_PARAM;
		
	KEYVAL kv[] = {
		{ "encoder", KEYVALTYPE_INT, &pParam->encoder },
		{ "res", KEYVALTYPE_INT, &pParam->res },
		{ "fmode", KEYVALTYPE_INT, &pParam->fMode },
		{ "sub", KEYVALTYPE_INT, &pParam->vstrm },
		{ "bps", KEYVALTYPE_INT, &pParam->bps },
		{ "fps", KEYVALTYPE_INT, &pParam->fps },
		{ "gop", KEYVALTYPE_INT, &pParam->gop },
		{ "quality", KEYVALTYPE_INT, &pParam->quality },

		{ "res_mask", KEYVALTYPE_INT, &pParam->dwResMask },
		{ "max_fps", KEYVALTYPE_INT, &pParam->maxFps }
	};
	CString str;
	str.Format("-chn %d\r\n-sub %d\r\n-list\r\n\r\n", vchn, pParam->vstrm);
	return QueryCmd("veattr", str, kv, sizeof(kv)/sizeof(KEYVAL));
}

int _DVSCONN::CTPSetVEncParam2(UINT vchn, const VIDEOENCPARAM2 *pParam)
{
	if(iStatus == DEVICESTATUS_DISCONNECTED || iStatus == DEVICESTATUS_RESUMEFAILED) return E_NO_CONN;
	if(!(dwRight & USERRIGHT_SETUP_PARAM)) return E_CTP_RIGHT;;

	if(vchn >= devInfo.iNumOfVChn) return E_INVALID_PARAM;

	CString str;
	str.Format("-chn %d\r\n-sub %d\r\n-encoder %d\r\n-res %d\r\n-fmode %d\r\n-bps %d\r\n-fps %d\r\n-gop %d\r\n-quality %d\r\n\r\n",
		vchn, pParam->vstrm, pParam->encoder, pParam->res, pParam->fMode, pParam->bps, pParam->fps, pParam->gop, pParam->quality);
	return ExecCmd("veattr", str);
}

int _DVSCONN::CTPGetVideoColor(UINT vchn, VIDEOCOLOR *pVC)
{
	if(vchn >= devInfo.iNumOfVChn) return E_INVALID_PARAM;

	CString str;
	KEYVAL kv[] = {
		{ "lum", KEYVALTYPE_INT, &pVC->lum },
		{ "cst", KEYVALTYPE_INT, &pVC->contrast },
		{ "sat", KEYVALTYPE_INT, &pVC->saturation },
		{ "hue", KEYVALTYPE_INT, &pVC->hue },
		{ "agc", KEYVALTYPE_INT, &pVC->agc },
		{ "aec", KEYVALTYPE_INT, &pVC->aec },
		{ "env", KEYVALTYPE_INT, &pVC->env }
	};

	str.Format("-list\r\n" "-chn %d\r\n\r\n", vchn);
	return QueryCmd("cfglum", str, kv, sizeof(kv)/sizeof(KEYVAL));
}
int _DVSCONN::CTPSetVideoColor(UINT vchn, const VIDEOCOLOR *p)
{
	if(iStatus == DEVICESTATUS_DISCONNECTED || iStatus == DEVICESTATUS_RESUMEFAILED) return E_NO_CONN;
	if(!(dwRight & USERRIGHT_SETUP_PARAM)) return E_CTP_RIGHT;;

	if(vchn >= devInfo.iNumOfVChn) return E_INVALID_PARAM;

	CString str;
	str.Format("-chn %d\r\n-lum %d\r\n-cst %d\r\n-sat %d\r\n-hue %d\r\n-agc %d\r\n-aec %d\r\n-env %d\r\n\r\n",
		vchn, p->lum, p->contrast, p->saturation, p->hue, p->agc, p->aec, p->env);
	return ExecCmd("cfglum", str);
}
int _DVSCONN::CTPGetCmosEnvs(UINT vchn, CMOSENVS *pEnvs)
{
	KEYVAL kv[] = {
		{ "1", KEYVALTYPE_STRING, NULL },
		{ "2", KEYVALTYPE_STRING, NULL },
		{ "3", KEYVALTYPE_STRING, NULL },
		{ "4", KEYVALTYPE_STRING, NULL },
		{ "5", KEYVALTYPE_STRING, NULL },
		{ "6", KEYVALTYPE_STRING, NULL }
	};
	CString str;
	int rlt, n = sizeof(kv)/sizeof(KEYVAL);

	str.Format("-chn %d\r\n-list\r\n\r\n", vchn);
	if( (rlt = QueryCmd("cfgcmos", str, kv, n)) ) return rlt;

	for(int i=0; i<n; i++)
	{
		if(kv[i].sVal)
		{
			ss_sscanf(',', kv[i].sVal, "%40s %d %d", pEnvs->envs[i].name, &pEnvs->envs[i].agc, &pEnvs->envs[i].aec);
		}
		else
		{
			pEnvs->envs[i].name[0] = '\0';
			pEnvs->envs[i].aec = pEnvs->envs[i].agc = 0;
		}
	}
	return 0;
}
int _DVSCONN::CTPSetCmosEnv(UINT vchn, UINT index, const CMOSENV *pEnv)
{
	if(iStatus == DEVICESTATUS_DISCONNECTED || iStatus == DEVICESTATUS_RESUMEFAILED) return E_NO_CONN;
	if(!(dwRight & USERRIGHT_SETUP_PARAM)) return E_CTP_RIGHT;;

	if(index >= sizeof(CMOSENVS)/sizeof(CMOSENV)) return E_CTP_PARAMETER;
	CString str;
	str.Format("-chn %d\r\n-env %d\r\n-name %s\r\n-aec %d\r\n-agc %d\r\n\r\n", vchn, index, index<3?"":pEnv->name, pEnv->aec, pEnv->agc);
	return ExecCmd("cfgcmos", str);
}

int _DVSCONN::CTPGetChnOsd(UINT vchn, OSDPARAM *pOsd)
{
	if(vchn >= devInfo.iNumOfVChn) return E_INVALID_PARAM;

	KEYVAL kv[] = {
		{ "chibl", KEYVALTYPE_INT, &pOsd->bChnInfo },
		{ "chifg", KEYVALTYPE_INT, &pOsd->idxClrChnInfo },
		{ "chilc", KEYVALTYPE_INT, &pOsd->iChnInfoPos },

		{ "stibl", KEYVALTYPE_INT, &pOsd->bStatus },
		{ "stifg", KEYVALTYPE_INT, &pOsd->idxClrStatus },
		{ "stilc", KEYVALTYPE_INT, &pOsd->iStatusPos },

		{ "tmibl", KEYVALTYPE_INT, &pOsd->bTime },
		{ "tmifg", KEYVALTYPE_INT, &pOsd->idxClrTime },
		{ "tmilc", KEYVALTYPE_INT, &pOsd->iTimePos },
		{ "tmifm", KEYVALTYPE_INT, &pOsd->iTimeFmt },

		{ "ctibl", KEYVALTYPE_INT, &pOsd->bCustom },
		{ "ctifg", KEYVALTYPE_INT, &pOsd->idxClrCustom },
		{ "ctilc", KEYVALTYPE_INT, &pOsd->iCustomPos },
		{ "ctitx", KEYVALTYPE_STRING, pOsd->cCustom, sizeof(pOsd->cCustom) }
		//,{ "ctifl", KEYVALTYPE_INT, & }
	};

	CString str;

	str.Format("-chn %d\r\n-list\r\n\r\n", vchn);
	return QueryCmd("osdcfg", str, kv, sizeof(kv)/sizeof(KEYVAL));
}

int _DVSCONN::CTPSetChnOsd(UINT vchn, const OSDPARAM *pOsd)
{
	if(iStatus == DEVICESTATUS_DISCONNECTED || iStatus == DEVICESTATUS_RESUMEFAILED) return E_NO_CONN;
	if(!(dwRight & USERRIGHT_SETUP_PARAM)) return E_CTP_RIGHT;;

	if(vchn >= devInfo.iNumOfVChn) return E_INVALID_PARAM;

	CString strCmd, strTmp;
	
	if(!pOsd->bChnInfo) strCmd = " -chibl 0";
	else strCmd.Format(" -chibl 1 -chifg %d -chilc %d", pOsd->idxClrChnInfo, pOsd->iChnInfoPos);

	if(!pOsd->bStatus) strCmd += " -stibl 0";
	else {
		strTmp.Format(" -stibl 1 -stifg %d -stilc %d", pOsd->idxClrStatus, pOsd->iStatusPos);
		strCmd += strTmp;
	}

	if(!pOsd->bTime) strCmd += " -tmibl 0";
	else {
		strTmp.Format(" -tmibl 1 -tmifg %d -tmilc %d", pOsd->idxClrTime, pOsd->iTimePos);
		strCmd += strTmp;
	}

	if(!pOsd->bCustom) strCmd += " -ctibl 0";
	else {
		strTmp.Format(" -ctibl 1 -ctifg %d -ctilc %d -ctitx \"%s\" -ctifl 0",
			pOsd->idxClrCustom, pOsd->iCustomPos, pOsd->cCustom);
		strCmd += strTmp;
	}
	
	strTmp = strCmd;
	strCmd.Format("osdcfg -chn %d %s", vchn, (const char*)strTmp);
	return ExecCmd(strCmd);
}

#ifndef WIN32
BOOL IsRectEmpty(const RECT *pRc)
{
	return pRc->right <= pRc->left || pRc->bottom <= pRc->top;
}
#endif
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat="
#pragma GCC diagnostic ignored "-Wsign-compare"
int _DVSCONN::CTPGetChnMD(UINT vchn, MDPARAM *pMd)
{
	if(vchn >= devInfo.iNumOfVChn) return E_INVALID_PARAM;

	KEYVAL kv[] = {
		{ "rect0", KEYVALTYPE_STRING, NULL },
		{ "rect1", KEYVALTYPE_STRING, NULL },
		{ "rect2", KEYVALTYPE_STRING, NULL },
		{ "rect3", KEYVALTYPE_STRING, NULL },
		{ "dealmode", KEYVALTYPE_INT, &pMd->dwActions },
		{ "recch" , KEYVALTYPE_INT, &pMd->dwRecChn },

		{ "ts0", KEYVALTYPE_STRING, NULL },
		{ "ts1", KEYVALTYPE_STRING, NULL },
		{ "ts2", KEYVALTYPE_STRING, NULL },
		{ "ts3", KEYVALTYPE_STRING, NULL },
		{ "ts4", KEYVALTYPE_STRING, NULL },
		{ "ts5", KEYVALTYPE_STRING, NULL },
		{ "ts6", KEYVALTYPE_STRING, NULL },
		{ "ts7", KEYVALTYPE_STRING, NULL },
		{ "ts8", KEYVALTYPE_STRING, NULL },
		{ "ts9", KEYVALTYPE_STRING, NULL },
		{ "ts10", KEYVALTYPE_STRING, NULL },
		{ "ts11", KEYVALTYPE_STRING, NULL },
		{ "ts12", KEYVALTYPE_STRING, NULL },
		{ "ts13", KEYVALTYPE_STRING, NULL },
		{ "ts14", KEYVALTYPE_STRING, NULL },
		{ "ts15", KEYVALTYPE_STRING, NULL }
	};

	memset(pMd, 0, sizeof(MDPARAM));
	CString str;
	str.Format("-chn %d\r\n-list\r\n\r\n", vchn);
	DWORD rlt;
	if( (rlt = QueryCmd("cfgalertmd", str, kv, sizeof(kv)/sizeof(KEYVAL))) == 0)
	{
		int i;
		for(i=0, pMd->nRv=0; i<4; i++)
		if(kv[i].pVal)
		{
			sscanf((const char*)kv[i].pVal, "%d,%d,%d,%d,%d", &pMd->rv[pMd->nRv].rect.left, 
					&pMd->rv[pMd->nRv].rect.top, &pMd->rv[pMd->nRv].rect.right, &pMd->rv[pMd->nRv].rect.bottom, &pMd->rv[pMd->nRv].dwVal);
			pMd->rv[pMd->nRv].rect.right += pMd->rv[pMd->nRv].rect.left;
			pMd->rv[pMd->nRv].rect.bottom += pMd->rv[pMd->nRv].rect.top;
			if(!IsRectEmpty(&pMd->rv[pMd->nRv].rect)) pMd->nRv++;
		}
		for(i = 6, pMd->nTSeg = 0; i < sizeof(kv)/sizeof(KEYVAL) && pMd->nTSeg<16; i++)
		{
			if(strncmp(kv[i].sKey, "ts", 2) == 0 && kv[i].pVal)
			{
				if(str2timeseg((const char*)kv[i].pVal, &pMd->tseg[pMd->nTSeg]))
				{
					pMd->nTSeg++;
				}
			}
		}
	}
	return rlt;
}
#pragma GCC diagnostic pop

int _DVSCONN::CTPSetChnMD(UINT vchn, const MDPARAM *pMd)
{
	if(iStatus == DEVICESTATUS_DISCONNECTED || iStatus == DEVICESTATUS_RESUMEFAILED) return E_NO_CONN;
	if(!(dwRight & USERRIGHT_SETUP_PARAM)) return E_CTP_RIGHT;;

	CString str, strTmp;
	UINT i;

	str.Format("-chn %d\r\n", vchn);
	if( pMd->nRv > 0 )
	{
		for(i=0; i<pMd->nRv; i++)
		{
			strTmp.Format(" -rect%d %d,%d,%d,%d,%d\r\n", i, 
				pMd->rv[i].rect.left, pMd->rv[i].rect.top, pMd->rv[i].rect.right-pMd->rv[i].rect.left, pMd->rv[i].rect.bottom-pMd->rv[i].rect.top, pMd->rv[i].dwVal);
			str += strTmp;
		}
	}
	else
		str += " -clear\r\n";

	strTmp.Format(" -dealmode %d\r\n-recch %d\r\n-tapech %d\r\n", pMd->dwActions, pMd->dwRecChn, pMd->dwRecChn);	//客户端与设备端录像与相同值
	str += strTmp;
	
	for(i=0; i<pMd->nTSeg; i++)
	{
		strTmp.Format("-ts%d \"%s\"\r\n", i, (const char*)Timeseg2Str(&pMd->tseg[i]));
		str += strTmp;
	}
	str += "\r\n";

	return ExecCmd("cfgalertmd", str);
}

int _DVSCONN::CTPGetDDNS(DDNSPARAM *pDdns)
{
	KEYVAL kv[] = {
			{ "ddnssp", KEYVALTYPE_INT, &pDdns->iSP },
			{ "ddnsip", KEYVALTYPE_STRING, pDdns->cServer, 40 },
			{ "ddnsusr", KEYVALTYPE_STRING, pDdns->cUser, 32 },
			{ "ddnspsw", KEYVALTYPE_STRING, pDdns->cPswd, 20 },
			{ "ddnsport", KEYVALTYPE_INT, &pDdns->iPort },
			{ "ddn", KEYVALTYPE_STRING, pDdns->cDn, 48 }
	};
	CString strCmd("-list\r\n\r\n");
	return QueryCmd("cfgddns", strCmd, kv, sizeof(kv)/sizeof(KEYVAL));
}

int _DVSCONN::CTPSetDDNS(const DDNSPARAM *pDdns)
{
	if(iStatus == DEVICESTATUS_DISCONNECTED || iStatus == DEVICESTATUS_RESUMEFAILED) return E_NO_CONN;
	if(!(dwRight & USERRIGHT_SETUP_PARAM)) return E_CTP_RIGHT;;

	CString str;
	if(pDdns->iSP > 0)
	{
		str.Format("-ddnssp %d\r\n-ddnsip %s\r\n-ddnsusr %s\r\n-ddnspsw %s\r\n-ddnsport %d\r\n-ddn %s\r\n\r\n",
					pDdns->iSP, pDdns->cServer, pDdns->cUser, pDdns->cPswd, pDdns->iPort, pDdns->cDn);
	}
	else
		str = "-ddnssp 0\r\n\r\n";

	return ExecCmd("cfgddns", str);
}

int _DVSCONN::CTPGetNICInfo(NICPARAM *pNic)
{
	KEYVAL kv[] = {
			{ "dhcpen", KEYVALTYPE_INT, &pNic->bDhcp },
			{ "svrip", KEYVALTYPE_STRING, pNic->cIp, 16 },
			{ "netmask", KEYVALTYPE_STRING, pNic->cNetMask, 16 },
			{ "gateway", KEYVALTYPE_STRING, pNic->cDefGW, 16 },
			{ "mac", KEYVALTYPE_STRING, pNic->cMac, 32 },
			{ "mdns", KEYVALTYPE_STRING, pNic->cDns1, 16 },
			{ "sdns", KEYVALTYPE_STRING, pNic->cDns2, 16 },
			{ "portctp", KEYVALTYPE_INT, &pNic->iPort },
			{ "autodns", KEYVALTYPE_INT, &pNic->bAutoDNS }
	};
	CString strCmd("-list\r\n\r\n");
	int rlt = QueryCmd("cfgipmac", strCmd, kv, sizeof(kv)/sizeof(KEYVAL));
	pNic->bDhcp = pNic->bDhcp?TRUE:FALSE;
	pNic->bAutoDNS = pNic->bAutoDNS?TRUE:FALSE;
	if(!pNic->bDhcp) pNic->bAutoDNS = FALSE;
	return rlt;
}
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wparentheses"
int _DVSCONN::CTPSetNICInfo(const NICPARAM *pNic)
{
	if(iStatus == DEVICESTATUS_DISCONNECTED || iStatus == DEVICESTATUS_RESUMEFAILED) return E_NO_CONN;
	if(!(dwRight & USERRIGHT_SETUP_PARAM)) return E_CTP_RIGHT;;

	if(!pNic->bDhcp)	//执行有效性检查
	{
		const char *pdot = strrchr(pNic->cIp, '.');
		if(!pdot) return E_INVALID_PARAM;
		int val = atoi(pdot+1);
		if(val == 0 || val > 254) return E_INVALID_PARAM;
		else
		{
			val = atoi(pNic->cIp);
			if(val == 127 || val == 0 || val >= 214 && val <= 239) return E_INVALID_PARAM;
		}
	}

	CString strCmd, strTmp;
	int port = pNic->iPort;
	if(port == 0 || port == 7999) port = 8001;

	if(!pNic->bDhcp && pNic->bAutoDNS) return E_INVALID_PARAM;
	//{ pNic->bAutoDNS = 0; strcpy(pNic->cDns1, "0.0.0.0"); strcpy(pNic->cDns2, "0.0.0.0"); }

	strCmd.Format("cfgipmac -dhcpen %d -autodns %d -portctp %d", pNic->bDhcp?1:0, pNic->bAutoDNS?1:0, pNic->iPort);
	if(!pNic->bDhcp) 
	{
		strTmp.Format(" -svrip %s -netmask %s -gateway %s -mac \"%s\"", 
			pNic->cIp, pNic->cNetMask, pNic->cDefGW, pNic->cMac);
		strCmd += strTmp;
	}
	if(!pNic->bAutoDNS)
	{
		strTmp.Format(" -mdns %s -sdns %s", pNic->cDns1, pNic->cDns2);
		strCmd += strTmp;
	}
	return ExecCmd(strCmd);
}
#pragma GCC diagnostic pop

int _DVSCONN::CTPIwList(struct iw_ap **ppAp, UINT *pNAp)
{
	struct iw_ap *pap;
	int n_ap, size_ap;
	int ret;
	CString str;

	n_ap = size_ap = 0;
	pap = NULL;
	if( (ret = CTPCommand("iwlist", str)) == 0 )
	{
		char *s = str.GetBuffer(str.GetLength());
		char *sep;
		for(;*s;)
		{
			if( (sep = strstr(s, "\r\n")) )
				*sep = '\0';

			if(n_ap >= size_ap)
			{
				size_ap += 10;
				pap = (struct iw_ap*)realloc(pap, sizeof(struct iw_ap) * size_ap);
			}
			if(ss_sscanf(',', s, "%s %d %d %d %d %d %d", pap[n_ap].essid, &pap[n_ap].channel, &pap[n_ap].encrypted, 
					&pap[n_ap].wpav, &pap[n_ap].cipher, &pap[n_ap].auth, &pap[n_ap].quality) == 7)
			{
				n_ap ++;
			}

			if(sep)
				s = sep + 2;
		}
		str.ReleaseBuffer();

		*pNAp = n_ap;
		*ppAp = pap;

		return 0;
	}
	return ret;
}

int _DVSCONN::CTPGetWirelessNICInfo(WIRELESSNICINFO *pWNic)
{
	if( (devInfo.dwHwFlags & HWFLAG_WIRELESS_NIC) == 0 ) return E_INVALID_OPERATION;
	int bWepEn;
	KEYVAL kv[] = {
		{ "enable", KEYVALTYPE_INT, &pWNic->bEn },
		{ "ssid", KEYVALTYPE_STRING, pWNic->cSSID, sizeof(pWNic->cSSID) },

		{ "key", KEYVALTYPE_STRING, pWNic->cKey, sizeof(pWNic->cKey) },

		{ "encryptype", KEYVALTYPE_STRING, pWNic->cEncrypType, sizeof(pWNic->cEncrypType) },
		{ "authmode", KEYVALTYPE_STRING, pWNic->cAuthMode, sizeof(pWNic->cAuthMode) },

			{ "dhcpen", KEYVALTYPE_INT, &pWNic->bDhcp },
			{ "svrip", KEYVALTYPE_STRING, pWNic->cIp, 16 },
			{ "netmask", KEYVALTYPE_STRING, pWNic->cNetMask, 16 },
			{ "gateway", KEYVALTYPE_STRING, pWNic->cDefGW, 16 },
			{ "mac", KEYVALTYPE_STRING, pWNic->cMac, 32 }
	};
	CString str("-list\r\n\r\n");
	int rlt = QueryCmd("cfgwnic", str, kv, sizeof(kv)/sizeof(KEYVAL));
	return rlt;
}

int _DVSCONN::CTPSetWirelessNICInfo(const WIRELESSNICINFO *pWNic)
{
	if(iStatus == DEVICESTATUS_DISCONNECTED || iStatus == DEVICESTATUS_RESUMEFAILED) return E_NO_CONN;
	if(!(dwRight & USERRIGHT_SETUP_PARAM)) return E_CTP_RIGHT;;

	if( (devInfo.dwHwFlags & HWFLAG_WIRELESS_NIC) == 0 ) return E_INVALID_OPERATION;	
	CString str;
	if(!pWNic->bEn) str = "-enable 0\r\n\r\n";
	else 
	{
		CString str2;
		str.Format("-authmode %s\r\n-encryptype %s\r\n-key %s\r\n", pWNic->cAuthMode, pWNic->cEncrypType, pWNic->cKey);

		if(pWNic->bDhcp)
		{
			str2.Format("-enable 1\r\n-ssid %s\r\n-dhcpen 0\r\n-mac %s\r\n"
				"-svrip %s\r\n-netmask %s\r\n-gateway %s\r\n\r\n",
				pWNic->cSSID, pWNic->cMac, pWNic->cIp, pWNic->cNetMask, pWNic->cDefGW);
		}
		else
		{
			str2.Format("-enable 1\r\n-ssid %s\r\n-dhcpen 1\r\n-mac %s\r\n\r\n",
				pWNic->cSSID, pWNic->cMac);
		}
		str += str2;
	}
	return ExecCmd("cfgwnic", str);
}

int _DVSCONN::CTPGetMulticastParam(char cIp[16], int* piPort)
{
	KEYVAL kv[] = {
			{ "mcastip", KEYVALTYPE_STRING, cIp, 16 },
			{ "mcastport", KEYVALTYPE_INT, piPort }
	};
	CString strCmd("-list\r\n\r\n");
	return QueryCmd("cfgmcast", strCmd, kv, sizeof(kv)/sizeof(KEYVAL));
}
int _DVSCONN::CTPSetMulticastParam(const char* cIp, int iPort)
{
	if(iStatus == DEVICESTATUS_DISCONNECTED || iStatus == DEVICESTATUS_RESUMEFAILED) return E_NO_CONN;
	if(!(dwRight & USERRIGHT_SETUP_PARAM)) return E_CTP_RIGHT;;

	CString str;
	str.Format("-mcastip %s\r\n-mcastport %d\r\n\r\n", cIp,  iPort);
	return ExecCmd("cfgmcast", str);
}

int _DVSCONN::CTPGetPPPoEParam(PPPOEPARAM *pParam)
{
	KEYVAL kv[] = {
			{ "poeen", KEYVALTYPE_INT, &pParam->bEn },
			{ "poename", KEYVALTYPE_STRING, pParam->cUser, 48 },
			{ "poepsw", KEYVALTYPE_STRING, pParam->cPswd, 24 }
	};
	CString strCmd("-list\r\n\r\n");
	return QueryCmd("cfgpppoe", strCmd, kv, sizeof(kv)/sizeof(KEYVAL));
}

int _DVSCONN::CTPSetPPPoEParam(const PPPOEPARAM *pParam)
{
	if(iStatus == DEVICESTATUS_DISCONNECTED || iStatus == DEVICESTATUS_RESUMEFAILED) return E_NO_CONN;
	if(!(dwRight & USERRIGHT_SETUP_PARAM)) return E_CTP_RIGHT;;

	CString str;
	if(pParam->bEn) str.Format("-poeen 1\r\n-poename %s\r\n-poepsw %s\r\n\r\n",
					pParam->cUser, pParam->cPswd);
	else
		str = "-poeen 0\r\n\r\n";
		
	return ExecCmd("cfgpppoe", str);
}

int _DVSCONN::CTPGetSMTP(SMTPPARAM *pParam)
{
	KEYVAL kv[] = {
			{ "smtpen", KEYVALTYPE_INT, &pParam->bEn },
			{ "smtpip", KEYVALTYPE_STRING, pParam->cSmtpSvr, sizeof(pParam->cSmtpSvr) },
			{ "smtpfr", KEYVALTYPE_STRING, pParam->cSender, sizeof(pParam->cSender) },
			{ "smtpuser", KEYVALTYPE_STRING, pParam->cUser, sizeof(pParam->cUser) },
			{ "smtppsw", KEYVALTYPE_STRING, pParam->cPswd, sizeof(pParam->cPswd) },
			{ "smtpto", KEYVALTYPE_STRING, pParam->cReceiver, sizeof(pParam->cReceiver) },
			{ "smtpport", KEYVALTYPE_INT, &pParam->iSmtpPort }
	};
	CString str("-list\r\n\r\n");
	return QueryCmd("cfgsmtp", str, kv, sizeof(kv)/sizeof(KEYVAL));
}

int _DVSCONN::CTPSetSMTP(const SMTPPARAM *pParam)
{
	if(iStatus == DEVICESTATUS_DISCONNECTED || iStatus == DEVICESTATUS_RESUMEFAILED) return E_NO_CONN;
	if(!(dwRight & USERRIGHT_SETUP_PARAM)) return E_CTP_RIGHT;;

	CString str;
	if(pParam->bEn)
		str.Format("-smtpen 1\r\n-smtpip %s\r\n-smtpport %d\r\n-smtpfr %s\r\n-smtpto %s\r\n-smtpuser %s\r\n-smtppsw %s\r\n\r\n", 
				pParam->cSmtpSvr, pParam->iSmtpPort, pParam->cSender, pParam->cReceiver, pParam->cUser, pParam->cPswd);
	else
		str = "-smtpen 0 \r\n\r\n";
	return ExecCmd("cfgsmtp", str);
}

int _DVSCONN::CTPSendTestMail()
{
	CString str;
	return ExecCmd("cfgsmtptest", str);
}

int _DVSCONN::CTPGetMailStatus(int *piStatus)
{
	KEYVAL kv[] = {
		{ "status", KEYVALTYPE_INT, piStatus }
	};
	CString str;

	*piStatus = -1;
	return QueryCmd("getsmtpstatus", str, kv, sizeof(kv), PF_DONTINITVALS);
}

int _DVSCONN::CTPListUser(USERRIGHT **ppUR, UINT *puiSize)
{
	if(iStatus == DEVICESTATUS_DISCONNECTED || iStatus == DEVICESTATUS_RESUMEFAILED) return E_NO_CONN;
	if((dwRight & USERRIGHT_USER_MANAGEMENT)==0) return E_CTP_RIGHT;

	CString str("-list\r\n\r\n");
	KEYVAL kv[] = {
		{ "guest_en", KEYVALTYPE_INT, NULL },
		{ "list", KEYVALTYPE_STRING, NULL }
	};

	DWORD rlt;

	*puiSize = 0;
	*ppUR = NULL;
	if( (rlt = CTPCommand("cfgusr", str)) == 0)
	{
		if(ParseBody(str.GetBuffer(0), kv, sizeof(kv)/sizeof(KEYVAL)))
		{
			char *p = kv[1].sVal;
			USERRIGHT *pUR;
			UINT cnt = 0, size = 10;
			pUR = (USERRIGHT*)malloc(sizeof(USERRIGHT)*size);
			
			while(p && *p)
			{
				char user[20];
				int right = 0;

				if(ss_sscanf(':', p, "%20s:%d", user, &right) > 0)
				{
					if(cnt >= size)
					{
						size += 10;
						pUR = (USERRIGHT*)realloc(pUR, size);
					}
					strcpy(pUR[cnt].cUser, user);
					pUR[cnt].dwRight = right;
					cnt++;
					
					right = 0;
					p = strchr(p, ';');
					if(!p) break;
					else p ++;
				}
				else
					break;
			}
		*ppUR = pUR;
		*puiSize = cnt;
		}
		str.ReleaseBuffer();
	}
	return rlt;
}

int _DVSCONN::CTPAddUser(const char *sUsr, const char* sPswd, DWORD right)
{
	if(iStatus == DEVICESTATUS_DISCONNECTED || iStatus == DEVICESTATUS_RESUMEFAILED) return E_NO_CONN;
	if((dwRight & USERRIGHT_USER_MANAGEMENT) == 0) return E_CTP_RIGHT;

	CString str;
	str.Format("-add %s:%s:%d\r\n\r\n", sUsr, sPswd, right);
	return ExecCmd("cfgusr", str);
}
int _DVSCONN::CTPDelUser(const char *sUsr)
{
	if(iStatus == DEVICESTATUS_DISCONNECTED || iStatus == DEVICESTATUS_RESUMEFAILED) return E_NO_CONN;
	if((dwRight & USERRIGHT_USER_MANAGEMENT) == 0) return E_CTP_RIGHT;

	CString str;
	str.Format("-del %s\r\n\r\n", sUsr);
	return ExecCmd("cfgusr", str);
}

/*
struct __RRCBWorkParam {
	REMOTERECORDSETTING *pRs;
	int sizeRRS;
	int sizeTSeg;
};
static BOOL ParaLineCB(char *line, void *arg)
{
	struct __RRCBWorkParam* pWkparm = (struct __RRCBWorkParam*)arg;
	REMOTERECORDSETTING *prs = pWkparm->pRs;

	char *key, *val;
	key = line;
	if(*key == '-') key++;
	key = strtok(key, " \t");
	val = strtok(NULL, "\t\r\n");
	if(stricmp(key, "sizelmt") == 0)
		prs->dwSizeLmt = atoi(val);
	else if(stricmp(key, "timelmt") == 0)
		prs->dwTimeLmt = atoi(val);
	else if(stricmp(key, "continuewithnewfile") == 0)
		prs->bContinueWithNewFile = atoi(val);
	else if(stricmp(key, "alrmtrgrec") == 0)
		prs->dwAlarmTriggerRecLmt = atoi(val);
	else 
	//....调度方案........	
	if(strcmp(key, "schedule") == 0)
	{
		if(prs->pRRS == NULL) {
			prs->pRRS = (REMOTERECORDSCHEDULE*)calloc(sizeof(REMOTERECORDSCHEDULE), 8);
			pWkparm->sizeRRS = 8;
		} else {
			prs->pRRS = (REMOTERECORDSCHEDULE*)realloc(prs->pRRS, sizeof(REMOTERECORDSCHEDULT)*(prs->nRRS+1));
		}

		REMOTERECORDSCHEDULE *pRRS = &prs->pRRS[prs->nRRS];
		memset(pRRS, 0, sizeof(*pRRS));
		pRRS->pTimeseg = (TIMESEG*)malloc(20*sizeof(TIMESEG));
		pTheWnd->m_pRRS = pRRS;
		pTheWnd->m_uiSize = 20;
		pTheWnd->m_cmbSchedule.SetItemDataPtr(pTheWnd->m_cmbSchedule.AddString(val?val:""), pRRS);
		strcpy(pRRS->cName, val);
	}
	else if(strcmp(key, "recchn") == 0)
	{
		if(pTheWnd->m_pRRS) pTheWnd->m_pRRS->dwChnMask = strtoul(val, NULL, 0);
	}
	else if(key[0] == 't' && key[1] == 's' && (!key[2] ||isdigit(key[2])))
	{
		pRRS = pTheWnd->m_pRRS;
		if(pRRS->size >= pTheWnd->m_uiSize)
		{
			pTheWnd->m_uiSize += 10;
			pRRS = (REMOTERECORDSCHEDULE*)realloc(pRRS, sizeof(TIMESEG)*pTheWnd->m_uiSize);
			pTheWnd->m_pRRS = pRRS;
		}
		str2timeseg(val, &pRRS->pTimeseg[pRRS->nTSeg++]);
	}

	return TRUE;
}
*/
void FreeRRSResource(REMOTERECORDSETTING *prs)
{
	for(UINT i=0; i<prs->nRRS; i++)
		if(prs->pRRS[i].pTimeseg) free(prs->pRRS[i].pTimeseg);
	free(prs->pRRS);
	prs->pRRS = NULL;
	prs->nRRS = 0;
}
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
int _DVSCONN::CTPGetRecordSetting(REMOTERECORDSETTING *prs)
{
//	struct __RRCBWorkParam param;
//	param.sizeRRS = param.sizeTSeg = 0;
//	param.pRs = prs;
	//if( (devInfo.dwHwFlags & HWFLAG_SDC) == 0 ) return E_INVALID_OPERATION;	

	DWORD rlt;
	CString str("-list\r\n\r\n");
	memset(prs, 0, sizeof(*prs));
	rlt = CTPCommand("cfgrec", str);
	if(rlt == 0)
	{
		char *pn, *line = str.GetBuffer(str.GetLength());
		int sizeRRS, sizeTSeg;
		REMOTERECORDSCHEDULE *pRRS = NULL;	//当前 RRS

		while( (pn = strstr(line, "\r\n")) && pn != line )
		{
			char *key, *val;
			key = line;
			if(*key == '-') key++;
			key = strtok(key, " \t");
			val = strtok(NULL, "\t\r\n");
			if(PA_StrCaseCmp(key, "sizelmt") == 0)
				prs->dwSizeLmt = atoi(val);
			else if(PA_StrCaseCmp(key, "timelmt") == 0)
				prs->dwTimeLmt = atoi(val);
			else if(PA_StrCaseCmp(key, "alrmtrgrec") == 0)
				prs->dwAlarmTriggerRecLmt = atoi(val);
			else 
			//....调度方案........	
			if(strcmp(key, "schedule") == 0)
			{
				if(prs->pRRS == NULL) 
				{
					prs->pRRS = (REMOTERECORDSCHEDULE*)malloc(sizeof(REMOTERECORDSCHEDULE)*8);
					sizeRRS = 8;
				} 
				else if(prs->nRRS >= sizeRRS)
				{
					sizeRRS += 5;
					prs->pRRS = (REMOTERECORDSCHEDULE*)realloc(prs->pRRS, sizeof(REMOTERECORDSCHEDULE)*sizeRRS);
				}

				pRRS = &prs->pRRS[prs->nRRS++];

				memset(pRRS, 0, sizeof(*pRRS));
				sizeTSeg = 20;
				pRRS->pTimeseg = (TIMESEG*)malloc(sizeTSeg*sizeof(TIMESEG));
				strcpy(pRRS->cName, val);
			}
			else if(strcmp(key, "recchn") == 0)
			{
				if(pRRS) pRRS->dwChnMask = strtoul(val, NULL, 0);
			}
			else if(strcmp(key, "media") == 0)
			{
				if(pRRS) pRRS->dwVideoMask = strtoul(val, NULL, 0);
			}
			else if(key[0] == 't' && key[1] == 's' && (!key[2] ||isdigit(key[2])) && pRRS)
			{
				if(pRRS->nTSeg >= sizeTSeg)
				{
					sizeTSeg += 10;
					pRRS->pTimeseg = (TIMESEG*)realloc(pRRS, sizeof(TIMESEG)*sizeTSeg);
				}
				str2timeseg(val, &pRRS->pTimeseg[pRRS->nTSeg++]);
			}

			*pn = '\0';
			line = pn + 2;
		}
	}
	return rlt;

	//return pConn->CTPCommandWithCallback("cfgrec", str, ParaLineCB, &param);
}

int _DVSCONN::CTPSetRecordSetting(const REMOTERECORDSETTING *prs)
{
	if(iStatus == DEVICESTATUS_DISCONNECTED || iStatus == DEVICESTATUS_RESUMEFAILED) return E_NO_CONN;
	if(!(dwRight & USERRIGHT_SETUP_PARAM)) return E_CTP_RIGHT;

	if( (devInfo.dwHwFlags & HWFLAG_SDC) == 0 ) return E_INVALID_OPERATION;	

	CString str;
	str.Format("-sizelmt %d\r\n-timelmt %d\r\n-alrmtrgrec %d\r\n", 
		prs->dwSizeLmt, prs->dwTimeLmt, prs->dwAlarmTriggerRecLmt);

	for(int i=0; i<prs->nRRS; i++)
	{
		CString tmp;
		REMOTERECORDSCHEDULE *pRRS;

		pRRS = &prs->pRRS[i];
		tmp.Format("-schedule %s\r\n", pRRS->cName);
		str += tmp;
		for(int j=0; j<pRRS->nTSeg; j++)
		{
			tmp.Format("-ts%d ", j);
			str += tmp;
			str += Timeseg2Str(pRRS->pTimeseg+j) + "\r\n";
		}
		tmp.Format("-recchn 0x%X\r\n-media 0x%X\r\n", pRRS->dwChnMask, pRRS->dwVideoMask);
		str += tmp;
	}
	str += "\r\n";

	return ExecCmd("cfgrec", str);
}
#pragma GCC diagnostic pop

int _DVSCONN::CTPGetVideoMask(UINT vchn, VIDEOMASK *pMask)
{
	if(vchn >= devInfo.iNumOfVChn) return E_INVALID_PARAM;
	KEYVAL kv[] = {
		{ "maskbl", KEYVALTYPE_INT, &pMask->bEn },
		{ "derx", KEYVALTYPE_INT, &pMask->x },
		{ "dery", KEYVALTYPE_INT, &pMask->y },
		{ "derw", KEYVALTYPE_INT, &pMask->w },
		{ "derh", KEYVALTYPE_INT, &pMask->h },
		{ "mcolor", KEYVALTYPE_INT, &pMask->idxColor }
	};
	CString strCmd;

	strCmd.Format("-chn %d\r\n" "-list\r\n\r\n", vchn);
	int rlt = QueryCmd("cfgoverlay", strCmd, kv, sizeof(kv)/sizeof(KEYVAL));
	if(rlt == 0 && pMask->idxColor > 6) pMask->idxColor = 6;
	return rlt;
}

int _DVSCONN::CTPSetVideoMask(UINT vchn, const VIDEOMASK *pMask)
{
	if(iStatus == DEVICESTATUS_DISCONNECTED || iStatus == DEVICESTATUS_RESUMEFAILED) return E_NO_CONN;
	if(!(dwRight & USERRIGHT_SETUP_PARAM)) return E_CTP_RIGHT;

	if(vchn >= devInfo.iNumOfVChn) return E_INVALID_PARAM;
	CString str;
	//if(pMask->idxColor > 6) pMask->idxColor = 6;
	if(pMask->bEn)
		str.Format("-chn %d\r\n-maskbl 1\r\n-derx %d\r\n-dery %d\r\n-derw %d\r\n-derh %d\r\n-mcolor %u\r\n\r\n",
			vchn, pMask->x, pMask->y, pMask->w, pMask->h, pMask->idxColor);
	else
		str.Format("-chn %d\r\n-maskbl 0\r\n\r\n", vchn);
	return ExecCmd("cfgoverlay", str);
}

static int valOf(char s[2]) {	return (s[0] - '0')*10 + s[1] - '0'; }
int _DVSCONN::CTPGetTime(TIMESETTING *pts)
{
	char sVal[15];
	DWORD rlt;
	KEYVAL kv[] = { 
		{ "ntpen", KEYVALTYPE_INT, &pts->bNtp },
		{ "time", KEYVALTYPE_STRING, sVal, 15 },
		{ "location", KEYVALTYPE_STRING, pts->location, sizeof(pts->location) },
		{ "tz", KEYVALTYPE_STRING, pts->tz, sizeof(pts->tz) },
		{ "ntpserver", KEYVALTYPE_STRING, pts->cNtpServer, sizeof(pts->cNtpServer) }
	};
	CString strCmd("-list\r\n\r\n");
	memset(pts, 0, sizeof(TIMESETTING));
	if( (rlt = QueryCmd("timecfg", strCmd, kv, sizeof(kv)/sizeof(KEYVAL))) == 0 )
	{
		pts->_tm.tm_year = 2000+valOf(sVal)-1900;
		pts->_tm.tm_mon = valOf(sVal+2)-1;
		pts->_tm.tm_mday = valOf(sVal+4);
		pts->_tm.tm_hour = valOf(sVal+6);
		pts->_tm.tm_min = valOf(sVal+8);
		pts->_tm.tm_sec = valOf(sVal+10);
		pts->_tm.tm_isdst = 0;
		pts->_tm.tm_wday = pts->_tm.tm_yday = 0;
	}
	
	return rlt;
}
int _DVSCONN::CTPSetTime(const TIMESETTING *pts)
{
	if(iStatus == DEVICESTATUS_DISCONNECTED || iStatus == DEVICESTATUS_RESUMEFAILED) return E_NO_CONN;
	if(!(dwRight & USERRIGHT_SETUP_PARAM)) return E_CTP_RIGHT;;

	CString str;
	str.Format("-location %s\r\n-ntpen %d\r\n-ntpserver %s\r\n-tz %s\r\n-time %02d%02d%02d%02d%02d%02d\r\n\r\n", 
		pts->location, pts->bNtp, pts->cNtpServer, pts->tz,
		pts->_tm.tm_year+1900-2000, pts->_tm.tm_mon+1, pts->_tm.tm_mday, pts->_tm.tm_hour, pts->_tm.tm_min, pts->_tm.tm_sec);
	return ExecCmd("timecfg", str);
}
//-----------------------------------------------------------------------------------
int _DVSCONN::CTPPTZMove(UINT chn, UINT direction, UINT xspd, UINT yspd)
{
	return CTPPTZCommand(chn, direction, xspd, yspd);
}            
int _DVSCONN::CTPPTZLens(UINT chn, UINT act)
{            
	switch(act)
	{
	case PTZ_IRIS_IN:
	case PTZ_IRIS_OUT:
	case PTZ_FOCUS_ON:
	case PTZ_FOCUS_OUT:
	case PTZ_ZOOM_IN:
	case PTZ_ZOOM_OUT:
		return CTPPTZCommand(chn, act, 0, 0);
	}
	return -1;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
//向量式云台运动控制、预置点/巡航轨迹设置
int _DVSCONN::CTPPTZCommand(UINT chn, UINT code, UINT para1, UINT para2)
{
	const int codecmd[] = {
		PTZ_STOP, 
		PTZ_MOVE_UP, 
		PTZ_MOVE_DOWN,
		PTZ_MOVE_LEFT,
		PTZ_MOVE_RIGHT, 
		PTZ_MOVE_UPLEFT, 
		PTZ_MOVE_DOWNLEFT, 
		PTZ_MOVE_UPRIGHT, 
		PTZ_MOVE_DOWNRIGHT, 
		PTZ_IRIS_IN, 
		PTZ_IRIS_OUT, 
		PTZ_FOCUS_ON, 
		PTZ_FOCUS_OUT, 
		PTZ_ZOOM_IN, 
		PTZ_ZOOM_OUT, 

		PTZ_SET_PSP, 
		PTZ_CALL_PSP, 
		PTZ_DELETE_PSP, 

		PTZ_BEGIN_CRUISE_SET, 
		PTZ_SET_CRUISE, 
		PTZ_END_CRUISE_SET, 
		PTZ_CALL_CRUISE, 
		PTZ_DELETE_CRUISE, 
		PTZ_STOP_CRUISE
	};
	int cmd;
	char *scmd = NULL;

	if(code < 100)
	{
		for(cmd=0; cmd < sizeof(codecmd)/sizeof(int); cmd++)
			if(code == codecmd[cmd]) break;
		if(cmd >= sizeof(codecmd)/sizeof(int)) return E_INVALID_PARAM;
	}
	else if(code < 0x10000)
	{
		cmd = code;
		code -= 100;
	}
	else
		scmd = (char*)code;

	if(chn >= devInfo.iNumOfVChn) return E_INVALID_PARAM;
			
	CString strCmd;
	switch(code)
	{
//------------------设备以前支持的命令-------------------------------------------------
	case PTZ_IRIS_IN: case PTZ_IRIS_OUT: case PTZ_FOCUS_ON:  //镜头控制
	case PTZ_FOCUS_OUT: case PTZ_ZOOM_IN: case PTZ_ZOOM_OUT:
	case PTZ_STOP:				//停止运动
	case PTZ_STOP_CRUISE:			//停止巡航
	case PTZ_AUTO_SCAN: case PTZ_AUTO_SCAN_STOP:
	case PTZ_RAINBRUSH_START:
	case PTZ_RAINBRUSH_STOP:
	case PTZ_LIGHT_ON:
	case PTZ_LIGHT_OFF:
		strCmd.Format("ptzctrl -chn %d -act %d", chn, cmd);
		break;
	case PTZ_MOVE_UPLEFT: case PTZ_MOVE_DOWNLEFT: 	//斜方向
	case PTZ_MOVE_UPRIGHT: case PTZ_MOVE_DOWNRIGHT:	
		strCmd.Format("ptzctrl -chn %d -act %d -para %d,%d", chn, cmd, para1, para2);
		break;
	case PTZ_MOVE_UP: case PTZ_MOVE_DOWN: 		//上/下/左/右
	case PTZ_MOVE_LEFT: case PTZ_MOVE_RIGHT:	//
		if(para1 == 0) strCmd.Format("ptzctrl -chn %d -act %d", chn, cmd);
		else strCmd.Format("ptzctrl -chn %d -act %d -para %d", chn, cmd, para1);
		break;
	case PTZ_CALL_PSP: case PTZ_DELETE_PSP:		//预置点调用/删除
	case PTZ_CALL_CRUISE: case PTZ_DELETE_CRUISE:	//巡航轨迹起始/调用/删除
		strCmd.Format("ptzctrl -chn %d -act %d -para %d", chn, cmd, para1);
		break;
	case PTZ_BEGIN_CRUISE_SET: 
	case PTZ_SET_PSP:							//预置点设置
		strCmd.Format("ptzctrl -chn %d -act %d -para %d -name %s", chn, cmd, para1, (char*)para2);
		break;
	case PTZ_SET_CRUISE:				//巡航轨迹中间命令, -para 预置点,速度,停留秒数
		strCmd.Format("ptzctrl -chn %d -act 19 -para %d,%d,%d", chn, para1, para2>>16, para2&0x0000FFFF);
		break;

//------------------设备新增加支持的命令-------------------------------------------------
	default: 
		if(para1 < 0x10000) 
		{
			if(scmd) strCmd.Format("ptzctrl -chn %d -act %s -para %u, %u", chn, scmd, para1, para2);
			else strCmd.Format("ptzctrl -chn %d -act %d -para %u, %u", chn, cmd, para1, para2);
		}
		else //para1 is a pointer to integer array
		{
			char buf[1000], *p = buf;
			if(scmd) p += sprintf(buf, "ptzctrl -chn %d -act %s -para ", chn, scmd);
			else p += sprintf(buf, "ptzctrl -chn %d -act %d -para ", chn, cmd);
			if(para2)
			{
				BYTE *pi = (BYTE*)para1;
				for(int i=0; i<para2; i++)
					p += sprintf(p, i==0?"%d":", %d", (int)pi[i]);
			}
			else
			{
				strcpy(p, (char*)para1);
			}
			strCmd = buf;
		}
		//return E_INVALID_PARAM;
	}
	return ExecCmd(strCmd);
}


int _DVSCONN::CTPRelay485(const BYTE *cmd, UINT size)
{
	CString str;
	char cv[3];
	str = "-hexstr ";
	for(int i=0; i<size; i++)
	{
		sprintf(cv, "%02X ", cmd[i]);
		str += cv;
	}
	str += "\r\n\r\n";
	return ExecCmd("relay485", str);
}
int _DVSCONN::CTPRelay485(const COMMSETTING *pCfg, const BYTE *cmd, UINT size)
{
	CString str;
	char cv[8];
	
	if(!size) return 0;
	str.Format("-bps %d\r\n-databit %d\r\n-stopbit %d\r\n-parity %d\r\n",
			pCfg->bps, pCfg->databits, pCfg->stopbits, pCfg->parity);
	str += "-hexstr ";
	for(int i=0; i<size; i++)
	{
		sprintf(cv, "%02X ", cmd[i]);
		str += cv;
	}
	str += "\r\n\r\n";

	return ExecCmd("cfg485", str);
}
#pragma GCC diagnostic pop

int _DVSCONN::CTPSetPspName(UINT chn, UINT pos, const char *name)
{
	if(iStatus == DEVICESTATUS_DISCONNECTED || iStatus == DEVICESTATUS_RESUMEFAILED) return E_NO_CONN;
	if(!(dwRight & USERRIGHT_SETUP_PARAM)) return E_CTP_RIGHT;;

	if(chn >= devInfo.iNumOfVChn) return E_INVALID_PARAM;
	CString str;
	str.Format("-chn %d\r\n-name%d %s\r\n\r\n",
		chn, pos, name?name:"");
	return ExecCmd("cfgposname", str);
}

int _DVSCONN::CTPSetCruiseName(UINT chn, UINT id, const char *name)
{
	if(iStatus == DEVICESTATUS_DISCONNECTED || iStatus == DEVICESTATUS_RESUMEFAILED) return E_NO_CONN;
	if(!(dwRight & USERRIGHT_SETUP_PARAM)) return E_CTP_RIGHT;;

	if(chn >= devInfo.iNumOfVChn) return E_INVALID_PARAM;
	CString str;
	str.Format("-chn %d\r\n-name%d %s\r\n\r\n",
		chn, id, name?name:"");
	return ExecCmd("cfgroutename", str);
}

int _DVSCONN::CTPGetPresetPoints(UINT vchn, IDNAME **ppPsp, UINT *puiCount)
{
	if(vchn >= devInfo.iNumOfVChn) return E_INVALID_PARAM;
	if(hSocket == INVALID_SOCKET) return E_NO_CONN;
		
	DWORD rlt;
	CString str;

	str.Format("-chn %d\r\n" "-list\r\n\r\n", vchn);
	if( (rlt = CTPCommand("cfgposname", str)) == 0)
	{
		*puiCount = ParseIdNameBody(str.GetBuffer(str.GetLength()), ppPsp);
		if(*puiCount) SortIdNames(*ppPsp, *puiCount);
		str.ReleaseBuffer();
	}
	return rlt;
}

int _DVSCONN::CTPGetCruiseTracks(UINT vchn, IDNAME **ppTrack, UINT *puiCount)
{
	//if(vchn >= devInfo.iNumOfVChn) return E_INVALID_PARAM;
	if(hSocket == INVALID_SOCKET) return E_NO_CONN;
		
	DWORD rlt;
	CString str;
	
	str.Format(" -chn %d\r\n-list\r\n\r\n", vchn);
	if( (rlt = CTPCommand("cfgroutename", str)) == 0)
	{
		*puiCount = ParseIdNameBody(str.GetBuffer(0), ppTrack);
		str.ReleaseBuffer();
		if(*puiCount) SortIdNames(*ppTrack, *puiCount);
	}
	return rlt;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-compare"
ALARMOUTNAME* _DVSCONN::GetAlarmOutNames()
{
	if(bProxy || !devInfo.iAlarmOut) return NULL;

	KEYVAL kv = { "-name", KEYVALTYPE_STRING, NULL, sizeof(ALARMOUTNAME) };
	ALARMOUTNAME *pAon = (ALARMOUTNAME*)calloc( sizeof(ALARMOUTNAME), devInfo.iAlarmOut );
	for(int i=0; i<devInfo.iAlarmOut; i++) {
		CString str;
		str.Format("-chn %d\r\n -list\r\n\r\n", i);
		kv.pVal = pAon[i].cName;
		pAon[i].cName[0] = '\0';
		QueryCmd("cfgalertout", str, &kv, 1);
	}
	return pAon;
}
#pragma GCC diagnostic pop

int _DVSCONN::CTPReboot()
{
	if(dwRight & USERRIGHT_SETUP_PARAM)
	{
		CString str("restart -obj app");
		return ExecCmd(str);
	}
	return E_CTP_RIGHT;
}

int _DVSCONN::CTPSignalOut(UINT ochn)
{
	CString str;
	str.Format("-chn %d\r\n\r\n", ochn);
	return ExecCmd("signalao", str);
}
//---------------------------------------------------------------------------------------------------
struct SnapshotArg {
	const char *fn;
	PA_HFILE hf;
};
BOOL SnapshotCallback(BYTE *pBytes, int len, void *arg)
{
	struct SnapshotArg *ssarg = (struct SnapshotArg*)arg;
	if(!PA_FileIsValid(ssarg->hf))
	{
#ifdef WIN32
		ssarg->hf = CreateFile(ssarg->fn, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
#elif defined(__LINUX__)
		ssarg->hf = open(ssarg->fn, O_CREAT|O_TRUNC|O_WRONLY, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
#else
#error "Platform specified feature must be implemented!"
#endif
		if(!PA_FileIsValid(ssarg->hf)) return FALSE;
	}
	PA_Write(ssarg->hf, pBytes, len);
	return TRUE;
}
int _DVSCONN::Snapshot(UINT iChn, const char *sFileName)
{
	CString strBody;
	struct SnapshotArg arg = { sFileName, PA_INVALID_HANDLE };
	strBody.Format("-chn %d\r\n\r\n", iChn);
	int rlt = CTPCommandWithCallbackBin("snapshot", strBody, SnapshotCallback, &arg);
	if(PA_FileIsValid(arg.hf)) PA_FileClose(arg.hf);
	return rlt;
}
//---------------------------------------------------------------------------------------------------
typedef struct _tagLFCB {
	LISTFILECALLBACK lfFunc;
	void *arg;
} LFCBPARAM;
__STDCALL static BOOL _ListFileLineCB(char *line, void *arg)
{
	REMOTEFILEINFO rfi;
	LFCBPARAM *pLfcbParam = (LFCBPARAM*)arg;

	//文件名,大小,录像日期时间,时长(s),通道名/号
	ss_sscanf(',', line, "%64s %d %d %d %*s %d", 
			rfi.cFileName, &rfi.dwFileLength, &rfi.tmStart, 
			&rfi.dwDuration, &rfi.dwChannel);
	return pLfcbParam->lfFunc(&rfi, pLfcbParam->arg);
}
int _DVSCONN::CTPListFile(const FILEFILTER* pFilter, LISTFILECALLBACK lfFunc, void *arg)
{
	HEADERHELPER hdrs(this, NULL);
	CString strBody, tmp;
	LFCBPARAM lfcbpar;

	if(pFilter->from && pFilter->to) {
		strBody.Format("-from %d\r\n -to %d\r\n", pFilter->from, pFilter->to);
	}
	if(pFilter->chns) {
		tmp.Format("-chn %d\r\n", pFilter->chns);
		strBody += tmp;
	}
	if(strBody != "") strBody += "\r\n";
	lfcbpar.lfFunc = lfFunc;
	lfcbpar.arg = arg;
	return CTPCommandWithCallback("ls", strBody, _ListFileLineCB, &lfcbpar, hdrs);
}
//---------------------------------------------------------------------------------------------------
int _DVSCONN:: CTPFileSession(const char *fn, char sid[20], int *flen)
{
	CString str;
	KEYVAL kv[] = {
		{ "sessid", KEYVALTYPE_STRING, sid, 20 },
		{ "length", KEYVALTYPE_INT, flen }
	};

	str.Format("-file %s\r\n\r\n", fn);
	return QueryCmd("filesess", str, kv, sizeof(kv)/sizeof(KEYVAL), 0);
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wparentheses"
int _DVSCONN::CTPDownloadFile(const char *remotefile, const char *localfile, DOWNLOADCALLBACK pfDwnldCB, DOWNLOADCBPARAM *arg)
{
	DWORD rlt;
	char sid[20];

	arg->dwBytesTrans = 0;
	if( (rlt = CTPFileSession(remotefile, sid, (int*)&arg->dwBytesTotal)) == 0)
	{
		PA_SOCKET sk;
		CString str;

		rlt = E_CTP_CONNECT;
		if(bPassive && RequestNewConn(&sk)==0 ||
					(sk = socket(AF_INET, SOCK_STREAM, 0)) > 0 && 
					connect(sk, (struct sockaddr*)&devAddr, sizeof(devAddr)) == 0) 
		{
			str.Format("-sessid %s\r\n\r\n", sid);
			if( (rlt = ::ExecCmd(sk, "dwnld", str, NULL, 3000)) == 0)
			{
				PA_HFILE hf;
#ifdef WIN32
			       	hf = CreateFile(localfile, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
#elif defined(__LINUX__)
				hf = open(localfile, O_WRONLY|O_CREAT, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
#else
#error "Platform specified feature needed!"
#endif
				if(PA_FileIsValid(hf))
				{
					int len;
					DWORD dwBytesWritten;
					void *buf = malloc(1500);
					BOOL bCanceled = FALSE;
					while( (arg->dwBytesTrans < arg->dwBytesTotal) && (len = Recv(sk, (char*)buf, 1500, 10000)) > 0)
					{
						PA_Write(hf, buf, len);
						arg->dwBytesTrans += len;
						if(pfDwnldCB && !pfDwnldCB(arg)) 
						{
							bCanceled = TRUE;
							break;
						}
					}
					free(buf);
					PA_FileClose(hf);
					if(bCanceled) PA_DeleteFile(localfile);
				}
			}
		}
		str.Format("-sessid %s\r\n\r\n", sid);
		ExecCmd("stopfilesess", str);
		PA_SocketClose(sk);
	}
	return rlt;
}
#pragma GCC diagnostic pop

//------------------------------------------------------------------------------------------------------
int PTZCommandEx(DVSCONN *pConn, UINT chn, UINT code, UINT para1, UINT para2)
{
	return pConn->CTPPTZCommand(chn, code, para1, para2);
}
PTZCMDHANDLER FPtzCmdHandler = PTZCommandEx;

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

//========================================================================================
//========================================================================================

DvsConnArray::DvsConnArray()
{
}
void DvsConnArray::RemoveConn(DVSCONN *p)
{
	for(DvsConnArray::iterator i = begin(); i != end(); ++i)
	{
		if(p == *i) 
		{
			erase(i);
			break;
		}
	}
}
//========================================================================================
//========================================================================================
//==========================      CDvsConnManager     ====================================
//========================================================================================

#define CMDNODE_SIZE	16
CDvsConnManager::CDvsConnManager()
{
	m_bRun = FALSE;
}

BOOL CDvsConnManager::Initialize(UINT iKeepAliveInterval, unsigned short port)
{
	if(!m_bRun)
	{
		m_iPort = port;
		m_iKeepAliveInterval = iKeepAliveInterval;

		PA_NetLibInit();

		srand(time(NULL));

		INIT_LIST_HEAD(&m_freeCmdList);
		m_pCmdNodes = (CMDNODE*)calloc(sizeof(CMDNODE), CMDNODE_SIZE);
		for(UINT i=0; i<CMDNODE_SIZE; i++)
		{
			INIT_LIST_HEAD(&m_pCmdNodes[i].cmd_list);
			list_add_tail(&m_pCmdNodes[i].cmd_list, &m_freeCmdList);
			PA_EventInit(m_pCmdNodes[i].hEvent);
		}


		m_bRun = TRUE;

		PA_EventInit(m_hEventQueued);
		PA_EventInit(m_hEventNodeAvailable);

		m_hPipeRd = m_hPipeWrt = PA_INVALID_HANDLE;
		PA_PipeCreate(&m_hPipeRd, &m_hPipeWrt);

		m_hConExecThread = PA_ThreadCreate(ConcurentExcutionThread, this);
		m_hAlertThread = PA_ThreadCreate(AlertReceiveThread, this);

		return TRUE;
	}
	return FALSE;
}

CDvsConnManager::~CDvsConnManager()
{
	DvsConnArray::iterator at;
	//m_RWLock.LockW();
	for(at = begin(); at != end(); ++at)
	{
		DVSCONN *pConn = *at;
		pConn->Disconnect();
	}
	//m_RWLock.Unlock();

	StopConExecThread();

	for(at = begin(); at != end(); ++at)
	{
		DVSCONN *pConn = *at;
		delete pConn;
	}
	clear();

	if(m_pCmdNodes)
	{
		for(int i=0; i<CMDNODE_SIZE; i++)
			PA_EventUninit(m_pCmdNodes[i].hEvent);
		free(m_pCmdNodes);
	}
	if(m_hPipeRd != PA_INVALID_HANDLE)
	{
		PA_PipeClose(m_hPipeRd);
		PA_PipeClose(m_hPipeWrt);
	}
	
	PA_NetLibUninit();
}
void CDvsConnManager::StopConExecThread()
{
	if(m_bRun)
	{
		m_bRun = FALSE;
		PA_EventSet(m_hEventQueued);
		PA_ThreadWaitUntilTerminate(m_hConExecThread);
		PA_ThreadWaitUntilTerminate(m_hAlertThread);
		PA_ThreadCloseHandle(m_hConExecThread);
		PA_ThreadCloseHandle(m_hAlertThread);
		PA_EventUninit(m_hEventQueued);
		PA_EventUninit(m_hEventNodeAvailable);
	}
}
DVSCONN *CDvsConnManager::DeviceAdd()
{
	DVSCONN *p = NewItem();
	m_RWLock.LockW();
	push_back(p);
	p->pConnMgr = this;
	m_RWLock.Unlock();
	return p;
}
DVSCONN *CDvsConnManager::NewItem()
{
	return new DVSCONN();
}
void CDvsConnManager::DeviceAdd(DVSCONN *p)
{
	m_RWLock.LockW();
	push_back(p);
	p->pConnMgr = this;
	m_RWLock.Unlock();
}

void CDvsConnManager::DeviceRemove(DVSCONN *pConn)
{
	m_RWLock.LockW();
	DvsConnArray::iterator i;
	for(i = begin(); i != end(); ++i)
	{
		if(pConn == *i) 
		{
			break;
		}
	}
	if(i == end())
	{
		m_RWLock.Unlock();
		return;
	}
	m_RWLock.Unlock();

	pConn->Disconnect();

	m_RWLock.LockW();
	erase(i);
	m_RWLock.Unlock();

	delete pConn;
}

UINT CDvsConnManager::GetCount()
{
	return size();
}

DVSCONN *CDvsConnManager::GetAt(UINT index)
{
	if(index >= size()) return NULL;
	return at(index);
}

DVSCONN * CDvsConnManager::operator[](UINT index)
{
	return GetAt(index);
}

void CDvsConnManager::IncIdleSec()
{
	m_RWLock.LockR();
	for(UINT i=0; i < size(); i++)
	{
		DVSCONN *pConn = at(i);
		if(pConn->GetStatus() == DEVICESTATUS_CONNECTED)
			pConn->iIdleSec++;
	}
	m_RWLock.Unlock();
}

static void KeepAliveCallback(DVSCONN *pConn, int err, DWORD events, void *param)
{
	char buf[100];
	int len;
	if(err)	LibNotify(DEVICEEVENT_NO_RESPONSE, pConn, 0);
	else if( (events&FEVENT_READ) && (len = recv(pConn->hSocket, buf, 100, 0)) <= 0 )
	{
		if(len < 0) LibNotify(DEVICEEVENT_NO_RESPONSE, pConn, 0);
		else LibNotify(DEVICEEVENT_PEERCLOSED, pConn, 0);
	}
	else
	{
		pConn->iIdleSec = 0;
		LibNotify(DEVICEEVENT_IS_ALIVE, pConn, 0);
	}
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-compare"
void CDvsConnManager::KeepAlive()
{
	m_RWLock.LockR();
	for(UINT i=0; i<size(); i++)
	{
		DVSCONN *pConn = at(i);
		//printf("conn status = %d, iIdleSec = %d, cmd_list is empty:%d\n", pConn->GetStatus(), pConn->iIdleSec, list_empty(&pConn->cmd_list));
		if( pConn->GetStatus() == DEVICESTATUS_CONNECTED && list_empty(&pConn->cmd_list) &&
					!pConn->pAgent && pConn->iIdleSec >= m_iKeepAliveInterval)
		{
			//if(pConn->TryLock())
			//{
#if 0
				char buf[120], sid[40];
				pConn->GetSID(sid);
				sprintf(buf, "CMD keepalive CTP/1.0\r\nCookie: %s\r\n\r\n", sid);
				QueueCmd(pConn, FEVENT_READ, KeepAliveCallback, NULL, 3000, buf, strlen(buf), CCFLAG_DONTFREEMEM);
#else
				static char skeepalive[] = "CMD keepalive CTP/1.0\r\n\r\n";
				QueueCmd(pConn, FEVENT_READ, KeepAliveCallback, NULL, 3000, skeepalive, sizeof(skeepalive)-1, CCFLAG_DONTFREEMEM);
#endif
				pConn->iIdleSec = 0;	//清0，这样当前一个命令发生超时，对KeepAlive的调用不会马上又排队一个keepalive到相同连接的命令队列。
										//而是由应用程序对KeepAliveCallback发出的DEVICEEVENT_NO_RESPONSE事件进行处理
			//	pConn->Unlock();
			//}
		}
	}
	m_RWLock.Unlock();
}
#pragma GCC diagnostic pop

BOOL CDvsConnManager::QueueCmd(DVSCONN *pConn, DWORD event, CONCURRENTCMDCALLBACK lpFunc, void *param, int timewait, const char *psczCmd, UINT len, DWORD flags)
{
	CMDNODE *pCmd = GetNode();
	if(!pCmd) return FALSE;
	
	pCmd->dwEvent = event;
	pCmd->lpFunc = lpFunc;
	pCmd->pParam = param;
	pCmd->timeWait = timewait;
	flags &= ~CCFLAG_DONTFREEMEM;	//只保留用户设置位
	pCmd->flags = flags;

	pCmd->pCmdToSend = (char*)psczCmd;
	pCmd->nCmdLen = len;
	return QueueCmd(pConn, pCmd);
}

BOOL CDvsConnManager::QueueCmd(DVSCONN *pConn, CMDNODE *pNode)
{
	if(pConn->hSocket == INVALID_SOCKET || pConn->GetStatus() == DEVICESTATUS_CONNECTED || pConn->GetStatus() == DEVICESTATUS_CONNECTING)
	{
		pNode->flags |= CCFLAG_DONTFREEMEM;
		m_QueueLock.Lock();
		if(pNode->pCmdToSend)
		{
#ifdef WIN32
			dbg_msg("[%s]QueueCmd: %s\n", CTime().GetCurrentTime().Format("%H:%M:%S"), pNode->pCmdToSend);
#endif
			if(list_empty(&pConn->cmd_list))
			{
				char buf[1000];
				while(Recv(pConn->hSocket, buf, 1000, 0) > 0);
				dbg_msg("SendCmd: %s\n", pNode->pCmdToSend);
				send(pConn->hSocket, pNode->pCmdToSend, pNode->nCmdLen, 0);
				pNode->pCmdToSend = NULL;
				pNode->nCmdLen = 0;
			}
			else
			{
				pNode->flags &= ~CCFLAG_DONTFREEMEM;
				char* p = (char*)malloc(pNode->nCmdLen);
				memcpy(p, pNode->pCmdToSend, pNode->nCmdLen);
				pNode->pCmdToSend = p;
			}
		}

#ifdef WIN32
		//OS BUG ???
		//The event-object is not automatic-reseted
		ResetEvent(pNode->hEvent);	
#endif
		list_add_tail(&pNode->cmd_list, &pConn->cmd_list);
		m_QueueLock.Unlock();

		PA_EventSet(m_hEventQueued);
		return TRUE;
	}
	return FALSE;
}
//可能因命令节点不足而等待
CMDNODE *CDvsConnManager::GetNode()
{
	while(1)
	{
		m_QueueLock.Lock();
		if(list_empty(&m_freeCmdList))
		{
			m_QueueLock.Unlock();
			PA_EventWait(m_hEventNodeAvailable);
		}
		else
			break;
	}
	CMDNODE *pCmd = list_entry(m_freeCmdList.next, CMDNODE, cmd_list);
	list_del(m_freeCmdList.next);
	m_QueueLock.Unlock();
		
	INIT_LIST_HEAD(&pCmd->cmd_list);
	pCmd->dwEvent = 0;
	pCmd->pCmdToSend = NULL;
	pCmd->lpFunc = NULL;
	pCmd->pParam = NULL;
	pCmd->timeWait = 100;
	pCmd->flags = 0;

	return pCmd;
}
void CDvsConnManager::PutCmdNode(CMDNODE *pCmd)
{
	list_del_init(&pCmd->cmd_list);
	list_add_tail(&pCmd->cmd_list, &m_freeCmdList);
	PA_EventSet(m_hEventNodeAvailable);
}
void CDvsConnManager::MarkCmdFinished(CMDNODE *pNode)
{
	m_QueueLock.Lock();
	//pNode->flags |= CCFLAG_FINISHED;
	PutCmdNode(pNode);
	m_QueueLock.Unlock();
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
struct NEWCONN {
	PA_SOCKET sock;
	char conn_id[24];

	NEWCONN(PA_SOCKET s)
	{
		sock = s;
		conn_id[0] = '\0';
	}
};
#pragma GCC diagnostic pop

typedef struct _tagConnSock {
	DVSCONN* pConn; 
	PA_SOCKET sock; 
} CONNSOCK;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
void* ConcurentExcutionThread(void *pParam)
{
	UINT i;
	int maxfd;
	DVSCONN *pConn;
	CMDNODE *pCmd;
	char buf[4000];
	struct timeval tv;
	fd_set rfds, wfds, efds;
	CDvsConnManager *pConnMgr = (CDvsConnManager*)pParam;
#ifdef WIN32
	pConnMgr->m_idConThread = GetCurrentThreadId();
#elif defined(__LINUX__)
	pConnMgr->m_idConThread = getpid();
#else
	#error "Platform specified feature must implemented!"
#endif
	while(pConnMgr->m_bRun)
	{
		PA_EventWait(pConnMgr->m_hEventQueued);
		dbg_msg("cmd queued\n");
loop:
		BOOL bIdle = TRUE;
		FD_ZERO(&rfds); FD_ZERO(&wfds); FD_ZERO(&efds);
		pConnMgr->m_RWLock.LockR();
		pConnMgr->m_QueueLock.Lock();

		for(i=0; i<pConnMgr->GetCount(); i++)
		{
			pConn = pConnMgr->GetAt(i);

			if(!list_empty(&pConn->cmd_list))
			{
				pCmd = list_entry(pConn->cmd_list.next, CMDNODE, cmd_list);
				if(pCmd->flags & CCFLAG_FINISHED)
				{
					pConnMgr->PutCmdNode(pCmd);
					if(list_empty(&pConn->cmd_list)) continue;
				}

				if(!(CCFLAG_WAIT & pCmd->flags) && pCmd->dwEvent)
				{
			/*
					//将在断开连接后发出的命令设为超时
					if(pConn->GetStatus() != DEVICESTATUS_CONNECTED && pConn->GetStatus() != DEVICESTATUS_CONNECTING && (pCmd->dwEvent & FEVENT_READ))
					{
						if(!(pCmd->flags & CCFLAG_DONTFREEMEM))	SAFE_FREE(pCmd->pCmdToSend);
						if(pCmd->lpFunc) 
						{
							CDvsConnManager::SERVICEITEM item = { 
								(CDvsConnManager::SAFESERVICECB)pCmd->lpFunc, 
								{ pConn, (void*)E_TIMEOUT, NULL, pCmd->pParam } 
							};
							//pCmd->lpFunc(pConn, E_TIMEOUT, 0, pCmd->pParam);
							pConnMgr->PutCmdNode(pCmd);
						}
						else 
						{
							pCmd->dwEvent = 0;
							SetEvent(pCmd->hEvent);
							pCmd->flags |= CCFLAG_WAIT;
							bIdle = FALSE;
						}
						continue;
					}
			*/
					//将命令标识为正在处理
					pCmd->flags |= CCFLAG_PENDING;

					if(pCmd->pCmdToSend)
					{
						if( pConn->hSocket != INVALID_SOCKET )
						{
							if(pCmd->dwEvent & FEVENT_READ) 
								while( Recv(pConn->hSocket, buf, 4000, 0) > 0 );
							send(pConn->hSocket, pCmd->pCmdToSend, pCmd->nCmdLen, 0);
						}
						if((pCmd->flags & CCFLAG_DONTFREEMEM)) pCmd->pCmdToSend = NULL;
						else SAFE_FREE(pCmd->pCmdToSend);
						pCmd->nCmdLen = 0;
					}
					if( pConn->hSocket != INVALID_SOCKET )
					{
						if(pCmd->dwEvent & FEVENT_READ) FD_SET(pConn->hSocket, &rfds);
						if(pCmd->dwEvent & FEVENT_WRITE) FD_SET(pConn->hSocket, &wfds);
						if(pCmd->dwEvent & FEVENT_ERROR) FD_SET(pConn->hSocket, &efds);
#ifdef __LINUX__
						maxfd = max(maxfd, pConn->hSocket);
#endif
					}

					bIdle = FALSE;
				}
			}//end if
		}//for
		
		pConnMgr->m_QueueLock.Unlock();

		NEWCONN nc(INVALID_SOCKET);
#ifdef WIN32
		DWORD dwBytesRead;
		if(PeekNamedPipe(pConnMgr->m_hPipeRd, &nc, sizeof(nc), &dwBytesRead, NULL, NULL) && 
			dwBytesRead == sizeof(nc))
		{
			ReadFile(pConnMgr->m_hPipeRd, &nc, sizeof(nc), &dwBytesRead, NULL);
			bIdle = FALSE;
		}
#elif defined(__LINUX__)
		if(timed_wait_fd(pConnMgr->m_hPipeRd, 0) > 0)
		{
			PA_Read(pConnMgr->m_hPipeRd, &nc, sizeof(nc));
			bIdle = FALSE;
		}
#else
#error "Platform specified feature ...!"
#endif

		if(!bIdle)
		{
			DWORD timeEplase;
			tv.tv_sec = 0; tv.tv_usec = 100000;
			timeEplase = PA_GetTickCount();

			//If no fds are waiting but a new connection is expected, 
			//select() return -1. 
			//We sleep for a while to let timeEplash be greater than 0,
			//so, if no connection comes, the waiting for new connection
			//will be timeouted eventually.
			if(select(maxfd+1, &rfds, &wfds, &efds, &tv) < 0)
				PA_Sleep(200);

			timeEplase = PA_GetTickCount() - timeEplase;
			for(i=0; i<pConnMgr->GetCount(); i++)
			{
				DWORD events = 0;
				int err = 0;
				void *pParam;
				CONCURRENTCMDCALLBACK lpCB = NULL; 
				
				pConn = pConnMgr->GetAt(i);
				pConnMgr->m_QueueLock.Lock();

				if(!list_empty(&pConn->cmd_list))
				{
					pCmd = list_entry(pConn->cmd_list.next, CMDNODE, cmd_list);
					if((pCmd->flags & (CCFLAG_PENDING|CCFLAG_WAIT)) == CCFLAG_PENDING)
					{
						if(pConn->hSocket == INVALID_SOCKET) events = FEVENT_ERROR;
						else {
							if(FD_ISSET(pConn->hSocket, &rfds)) events |= FEVENT_READ;
							if(FD_ISSET(pConn->hSocket, &wfds)) 
							{
								socklen_t optlen;
								optlen = sizeof(err);
								if(getsockopt(pConn->hSocket, SOL_SOCKET, SO_ERROR, &err, &optlen) == 0 && err == 0)
									events |= FEVENT_WRITE;
								else
								{
									events |= FEVENT_ERROR;
									err = E_CTP_CONNECT;
								}
							}
							if(FD_ISSET(pConn->hSocket, &efds)) 
							{
								events |= FEVENT_ERROR;
								err = E_CTP_SYSTEM;
							}
							if((pCmd->dwEvent & FEVENT_NEWCONN) && nc.sock != INVALID_SOCKET && strcmp(nc.conn_id, pConn->conn_id) == 0)
							{
								events |= FEVENT_NEWCONN;
								pConn->hNewConn = nc.sock;
								nc.sock  = INVALID_SOCKET;
							}
						}

						if(!events) pCmd->timeWait -= timeEplase;
						//printf("timeEplase = %d, pCmd->timeWait = %d\n", timeEplase, pCmd->timeWait);
						if(events || pCmd->timeWait <= 0)
						{
							if(!events && !err &&  pCmd->timeWait<=0) err = E_TIMEOUT;
							if(pCmd->lpFunc)
							{
#ifdef WIN32
								dbg_msg(_T("[%s]Callback. event: 0x%X\n"), 
										CTime().GetCurrentTime().Format(_T("%H:%M:%S")), events);
#endif
								pParam = pCmd->pParam;
								lpCB = pCmd->lpFunc;

								pConnMgr->PutCmdNode(pCmd);	//释放CMDNODE，会请求 m_QueueLock 锁
							}		
							else
							{
#ifdef WIN32
								dbg_msg(_T("[%s]SignalWait: 0x%X\n"), 
										CTime().GetCurrentTime().Format(_T("%H:%M:%S")), events);
#endif
								pCmd->dwEvent = events;
								pCmd->flags |= CCFLAG_WAIT;
								PA_EventSet(pCmd->hEvent);
								bIdle = FALSE;
								//pConn->Unlock();
							}
							pConn->iIdleSec = 0;
						}
					}
				}//if
				
				pConnMgr->m_QueueLock.Unlock();
				if(lpCB) lpCB(pConn, err, events, pParam);
				pConn->iAsynErrCode = err;
			}//for

			if(nc.sock != INVALID_SOCKET)
				PA_SocketClose(nc.sock);
		}

		pConnMgr->m_RWLock.Unlock();

		if(!bIdle) goto loop;
		dbg_msg("**** No command to execute ***\n");
		//else ResetEvent(pConnMgr->m_hEventQueued);
	}

	for(i=0; i<pConnMgr->GetCount(); i++)
	{
		pConn = pConnMgr->GetAt(i);
		pConn->ClearCmdList();
	}

	return 0;
}
#pragma GCC diagnostic pop

//---------------------------- >>> ALERT THREAD <<< ---------------------------------
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-compare"
typedef std::vector<NEWCONN> NEWCONN_VECTOR;
void HandleAlertDvsConn(DVSCONN *pConn)
{
	int len;
	char buf[5000];
	if( (len = recv(pConn->hAlertSock, buf, 300, 0)) <= 0 ) 
	{
		PA_SocketClose(pConn->hAlertSock);
		pConn->hAlertSock = INVALID_SOCKET;
		return;
	}

	int notify;
	char *hdrs;
	if(ParseNotifyMessage(buf, &notify, &hdrs))	//NOTIFY alarm|message CTP/1.0 <CRLF>
	{
		DVSCONN *pRealConn = pConn;
		REQUESTOPTIONS reqopt;
		char *pBody;
		if( /*pBody[0] != '-' || */hdrs && (strstr(hdrs, "Content-Length:")) )	////与以前版本兼容: 以前无头域, 直接以'-'开头的内容
		{
			ParseRequestOptions(hdrs, &reqopt);
			pBody = reqopt.body;
			if(reqopt.body + reqopt.content_length > buf + len)	//Read remained bytes
			{
				int l = Recv(pConn->hAlertSock, buf + len, reqopt.body - buf + reqopt.content_length - len, 400);
				if(l > 0) len += l;
			}
			pBody[reqopt.content_length] = '\0';

			//有Host头，连接来自代理服务器.但当类型为NOTIFICATION_NOTIFY时，不一定存在Host对应的指针
			if(pBody && pConn->bProxy && reqopt.host && notify != NOTIFICATION_NOTIFY)
			{
				for(int i=0; i<pConn->proxyInfo.pvConn->size(); i++)
				{
					if(strcmp(pConn->proxyInfo.pvConn->at(i)->cHost, reqopt.host) == 0)
					{
						pRealConn = pConn->proxyInfo.pvConn->at(i);
						break;
					}
				}
			}
		}
		else
		{
			memset(&reqopt, 0, sizeof(reqopt));
			pBody = hdrs;
		}
		buf[len] = '\0';

		switch(notify)
		{
		case NOTIFICATION_ALARM:
			if(1)
			{
				int event;
				KEYVAL kv[] = {
					{ "type", KEYVALTYPE_STRING, NULL, 0 },
					{ "chn", KEYVALTYPE_INT, NULL, 0 },
					{ "onoff", KEYVALTYPE_INT, NULL, 0 },
					{ "recchn", KEYVALTYPE_INT, NULL, 0 },
					{ "vchn", KEYVALTYPE_INT, NULL, 0 }
				};
				ParseBody(pBody, kv, sizeof(kv)/sizeof(KEYVAL));
				if(strcmp(kv[0].sVal, "io") == 0) event = IO_EVENT;
				else if(strcmp(kv[0].sVal, "md") == 0) event = MD_EVENT;
				else if(strcmp(kv[0].sVal, "ab") == 0) event = AB_EVENT;
				else if(strcmp(kv[0].sVal, "snd") == 0) event = SND_EVENT;
				else break;
				NOTIFYSTRUCT ns;
				ns.notification = NOTIFICATION_ALARM;
				ns.alarm.event = event;
				ns.alarm.chn = kv[1].iVal;
				ns.alarm.onoff = kv[2].iVal;
				ns.alarm.recChn = kv[3].iVal;
				ns.alarm.vchn = kv[4].sVal?kv[4].iVal:-1;
				ns.pConn = pRealConn;
				LibNotify(notify, &ns, sizeof(NOTIFYSTRUCT));
			}
			break;
		case NOTIFICATION_LOGIN:
		case NOTIFICATION_LOGOUT:
			if(1)
			{
				NOTIFYSTRUCT ns;
				KEYVAL kv[] = {
					{ "user", KEYVALTYPE_STRING, ns.loginout.user, sizeof(ns.loginout.user) },
					{ "ip", KEYVALTYPE_STRING, ns.loginout.ip, sizeof(ns.loginout.ip) },
					{ "usercnt", KEYVALTYPE_INT, &ns.loginout.userCnt }
				};
				ParseBody(pBody, kv, sizeof(kv)/sizeof(KEYVAL));
				ns.notification = notify;
				ns.pConn = pRealConn;
				LibNotify(notify, &ns, sizeof(ns));
			}
			break;
		case NOTIFICATION_REBOOT:
		case NOTIFICATION_STOPSERVICE:
			LibNotify(notify, pRealConn, 0);
			break;
		case NOTIFICATION_MSG:
			if(1)
			{
				NOTIFYSTRUCT ns;
				ns.notification = notify;
				ns.pConn = pRealConn;
				strcpy(ns.msg, pBody);
				LibNotify(notify, &ns, sizeof(ns));
			}
			break;
		case NOTIFICATION_NOTIFY:
			if(1)
			{
				NOTIFYSTRUCT ns;
				ns.notification = notify;
				if(reqopt.host) strcpy(ns.notify.host, reqopt.host);
				else ns.notify.host[0] = '\0';
				ns.pConn = pConn;	//NOTIFY直接来自代理服务器
				strcpy(ns.notify.msg, pBody);
				LibNotify(notify, &ns, sizeof(ns));
			}
			break;
		}
	}
}
#pragma GCC diagnostic pop

typedef struct _tagNewPassiveConn {
	DVSCONN *pConn;
	PA_SOCKET	sock;
} NEWCONNPARAM;
void *NewPassiveClientHandler(void* pParam)
{
	NEWCONNPARAM *p = (NEWCONNPARAM*)pParam;
	p->pConn->PassiveMode(p->sock);
	free(p);
	return 0;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-compare"
#ifdef WIN32
void* WINAPI AlertReceiveThread(LPVOID pParam)
#else
void* AlertReceiveThread(void *pParam)
#endif
{
	CDvsConnManager *pConnMgr = (CDvsConnManager*)pParam;
	fd_set rfds;
	timeval tv;
	UINT i;
	DVSCONN *pConn;

	NEWCONN_VECTOR *pvNewConn = NULL;
	PA_SOCKET skSvr=INVALID_SOCKET;
	
#ifdef WIN32
	pConnMgr->m_idAlertThread = GetCurrentThreadId();
#elif defined(__LINUX__)
	pConnMgr->m_idAlertThread = getpid();
#else
	#error "Platform specified feature must implemented!"
#endif
	//
	// 初始化服务套接字
	//
	if(pConnMgr->m_iPort)
	{
		if( (skSvr = CreateServiceSocket(SOCK_STREAM, 0, pConnMgr->m_iPort)) != INVALID_SOCKET ) 
			pvNewConn = new NEWCONN_VECTOR;
	}


	time_t t1, t2;
	t1 = time(NULL);
	while(pConnMgr->m_bRun)
	{
		int maxfd=-1;
		BOOL bEmpty = TRUE;
		FD_ZERO(&rfds);
		tv.tv_sec = 1; tv.tv_usec = 0;

		//
		// 设置监视套接字
		//
		if(skSvr != INVALID_SOCKET)
		{
			bEmpty = FALSE;
			FD_SET(skSvr, &rfds);
			maxfd = skSvr;
			//New connetions
			for(i=0; i<pvNewConn->size(); i++)
			{
				FD_SET(pvNewConn->at(i).sock, &rfds);
				maxfd = max(maxfd, pvNewConn->at(i).sock);
			}
		}
		pConnMgr->m_RWLock.LockR();
		for(i = 0; i < pConnMgr->GetCount(); i++)
		{
			pConn = pConnMgr->GetAt(i);
			if(pConn->hAlertSock == INVALID_SOCKET || pConn->pAgent) continue;

			FD_SET(pConn->hAlertSock, &rfds);
			bEmpty = FALSE;
			maxfd = max(maxfd, pConn->hAlertSock);
		}
		pConnMgr->m_RWLock.Unlock();


		if(bEmpty) PA_Sleep(1000);
		else if(select(maxfd+1, &rfds, NULL, NULL, &tv) > 0 )
		{
			if(skSvr != INVALID_SOCKET)
			{	// New connections
				for(NEWCONN_VECTOR::iterator it=pvNewConn->begin(); it!=pvNewConn->end(); )
				{
					if(FD_ISSET(it->sock, &rfds))
					{
						char buff[150];
						int len = recv(it->sock, buff, 150, 0);
						if(len <= 0) PA_SocketClose(it->sock);
						else
						{
							PASSIVECLIENTID pcid;
							KEYVAL kv[] = {
								{ "sn", KEYVALTYPE_POINTER, &pcid.sn },
								{ "devid", KEYVALTYPE_POINTER, &pcid.dev_id },
								{ "connid", KEYVALTYPE_STRING, it->conn_id, sizeof(it->conn_id) }
							};
							buff[len] = '\0';
							ParseBody(buff, kv, sizeof(kv)/sizeof(KEYVAL), 0);

							if(pcid.sn && pcid.dev_id)
							{
								pcid.sock = it->sock;
								/* 
								 * If the passive client is allowed, g_pEventCallback should 
								 * allocate or find a connection object to represent the ipcam.
								 * and set the cUser and cPassword for the object ???
								 */
								DVSCONN* pConn;
								if(g_pEventCallback && (pConn = (DVSCONN*)g_pEventCallback(DEVICEEVENT_PASSIVECLT, &pcid)) != NULL)
								{
									NEWCONNPARAM *pParam = (NEWCONNPARAM*)malloc(sizeof(NEWCONNPARAM));
									pParam->pConn = pConn;
									pParam->sock = pcid.sock;
									PA_HTHREAD hThrd = PA_ThreadCreate(NewPassiveClientHandler, pParam);
									PA_ThreadCloseHandle(hThrd);
								}
								else
									PA_SocketClose(it->sock);
							}
							else if(it->conn_id[0])//Data connection
							{
								PA_Write(pConnMgr->m_hPipeWrt, &*it, sizeof(NEWCONN));
							}
							else
								PA_SocketClose(it->sock);
						}

						pvNewConn->erase(it);
					}
					else
						it++;
				}
				// Passive accept
				if(FD_ISSET(skSvr, &rfds))
				{
					dbg_msg("Incomming connection accepted.\n");
					struct sockaddr sa;
					int sa_len = sizeof(sa);
					pvNewConn->push_back(PA_Accept(skSvr, &sa, &sa_len));
				}
			}

			pConnMgr->m_RWLock.LockR();
			for(i = 0; i < pConnMgr->GetCount(); i++)
			{
				pConn = pConnMgr->GetAt(i);
				if(pConn->hAlertSock != INVALID_SOCKET && FD_ISSET(pConn->hAlertSock, &rfds))
				{
					HandleAlertDvsConn(pConn);
				}
			}
			pConnMgr->m_RWLock.Unlock();
		}//if(select(...) > 0)


		t2 = time(NULL);
		if(t2 > t1)
		{
			for(i=0; i<t2-t1; i++)	pConnMgr->IncIdleSec();
			pConnMgr->KeepAlive();
			t1 = t2;
		}
	}
	
	if(skSvr != INVALID_SOCKET) PA_SocketClose(skSvr);
	if(pvNewConn) delete pvNewConn;
	return 0;
}
#pragma GCC diagnostic pop