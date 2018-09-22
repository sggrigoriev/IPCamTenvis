#ifndef __DvsConn_h__
#define __DvsConn_h__

#include <Ctp.h>
#include <linux_list.h>
#include <Timeseg.h>
#include <vector>
#include "CString.h"
#include "platform_adpt.h"
#include "RWLock.h"
#include "iwlist.h"

#ifndef WIN32
typedef struct _RECT { 
  LONG left; 
  LONG top; 
  LONG right; 
  LONG bottom; 
} RECT, *PRECT;
#endif

//--------------------------------------------
typedef struct _tagIDName {
	UINT id;
	char name[32];
} IDNAME, *LPIDNAME;

extern int InsertIdName(LPIDNAME* ppIdNames, UINT* pcount, int id, const char *sName);
extern int InsertIdName(LPIDNAME* ppIdNames, UINT* pcount, const IDNAME *pIN);
extern BOOL RemoveIdName(LPIDNAME ppIDNames, UINT* pcount, int id);
extern IDNAME* IdNameFindID(LPIDNAME pIdNames, UINT count, int id);
extern UINT GetUnusedID(const IDNAME *pIdNames, UINT count);
extern void SortIdNames(IDNAME *pIdNames, UINT count);
//--------------------------------------------
/* 图像制式 */
#define NORM_PAL	0
#define NORM_NTSC	1
#define NORM_AUTO	2

typedef enum {
	VRES_QCIF	= 0,
	VRES_CIF	= 1,
	VRES_HALFD1	= 2,
	VRES_D1		= 3,
	VRES_SVGA	= 4,
	VRES_720P	= 5,	//1280*720
	VRES_QuadVGA = 6,	//1280*960
	VRES_SXGA,			//1280*1024
	VRES_UXGA			//1600*1200
} EVIDEORES;

typedef struct _tagVCHNINFO
{
	char	cName[40];
	int	iActive;	//Readonly. 0-Close; 1-Open
	int	iMode;		//0-qcif, 1-cif, 2-halfcif, 3-D1
	int	iSubEn;		//0-Disable secondary stream. 1-Enable secondary stream
	int	iSubMode;	//Mode for secondary stream: 0-qcif, 1-cif, 2-halfcif, 3-D1. 
					//If Secondary stream is not supported, iSubMode is -1

	int	iNorm;	//0-P, 1-NTSC, 2-Auto
	int	iAuChn;	//伴音通道
} VCHNINFO;

typedef struct _tagVCHNINFO2
{
	char	cName[40];
	int		iActive;
	int		vstd;
	int		iAuChn;
} VCHNINFO2;

typedef struct _tagACHNINFO
{
	int iPrec;	//8/16/32
	int iFreq;	//
	int iEncode;	//
} ACHNINFO;

typedef struct _tagDeviceInfo {
	char	cDevName[40];			//Device name
	char	cDevSn[42];
	char	cVerSoft[24];			//software version
	char	cPubTime[24];			//软件发布时间
	char	cVerHard[24];			//hardware version
	char	cModel[64];			//设备型号,可能带说明
	char	cOEMId[20];			//OEM客户标识
	UINT	uTypeId;			//设备型号标识
	DWORD	dwHwFlags;			//硬件标志
	UINT	iAlarmIn;			//报警输入数
	UINT	iAlarmOut;			//报警输出数
	UINT	iNumOfAChn;			//音频通道数
	UINT	iNumOfVChn;			//视频通道数
} DEVICEINFO;

typedef struct _tagCommSetting {
	int		bps;
	int		databits;
	int		stopbits;
	int		parity;
} COMMSETTING;

typedef struct _tagPTZSETTING {
	char	cProto[32];		//协议名
	UINT	iDefSpeed;
	UINT	iPtzAddr;
	COMMSETTING comm;
} PTZSETTING;

#define ALARMACTION_MONITOR			0x80000000
#define ALARMACTION_BEEP			0x40000000
#define ALARMACTION_NOTIFY_CLIENT	0x20000000
#define ALARMACTION_SEND_MAIL		0x10000000
typedef struct _tagALARMINCHNINFO {
	char	cName[40];			//通道名
	DWORD	dwActions;			//High word: combination of ALARMACTION_xxx; Low word: Signal output-port N if bit N is set
	int		iPtzMode;			//0-不联动; 1-预置点; 2-巡航
	int		iPos;				//iPtzMode为1或2时, 为预置点或巡航轨迹编号
	int		iVChn;				//关联的视频通道。发生报警时，传给客户端
	DWORD	dwRecordChns;		//触发录像通道。位n为1表示对视频通道n录像
	UINT	nTSeg;
	TIMESEG	tseg[16];
} ALARMINCHNINFO;

typedef struct _tagAlarmOutChnInfo {
	char	cName[40];
	int 	iTrig;
	int		iDuration;
	UINT	nTSeg;
	TIMESEG	tseg[16];
} ALARMOUTCHNINFO;
typedef struct _tagAlarmOutName {
	char cName[40];
} ALARMOUTNAME;

typedef struct _tagVIDEOEncParam {
	int		code;		//0-Main stream, 1-Secondary stream
	int		fMode;		//0 - Quality first; 1 － Speed first
	int		bps;
	int		fps;
	int		gop;
	int		quality;
} VIDEOENCPARAM;

typedef struct _tagVIDEOEncParam2 {
	int		vstrm;		//0-Main, 1-Sub, 2-Rec ...
	int		encoder;		//0-h.264
	int		res;
	int		fMode;		//0 - Quality first; 1 － Speed first
	int		bps;
	int		fps;
	int		gop;
	int		quality;

	DWORD	dwResMask;
	int		maxFps;
} VIDEOENCPARAM2;

typedef struct _tagVideoColor {
	int lum;
	int contrast;
	int saturation, hue;//仅对 devInfo.dwHwFlags 不含 HWFLAG_OV7725 时有效
	int aec, agc;		//仅对 HWFLAG_OV7725 并且env非0时有效
	int env;			//0-自动 1-室内逆光强 2-室内逆光普通 3-室内逆光弱 4-自定义1 5-自定义2 6-自定义3
} VIDEOCOLOR;

typedef struct _tagCmosEnv {
	char name[40];
	int agc, aec;
} CMOSENV;
typedef struct _tagCmosENVS {
	CMOSENV envs[6];		//分别对应场景 室内逆光强/室内逆光普通/室内逆光弱/自定义1/自定义2/自定义3，前3个名字不可改
} CMOSENVS;

typedef struct _tagOsdParam {
	BOOL	bChnInfo;
	int	idxClrChnInfo;	//
	int	iChnInfoPos;
	
	BOOL	bStatus;
	int	idxClrStatus;
	int	iStatusPos;
	
	BOOL	bTime;
	int	idxClrTime;
	int	iTimePos;
	int	iTimeFmt;
	
	BOOL	bCustom;
	int	idxClrCustom;
	int	iCustomPos;
	char	cCustom[40];
} OSDPARAM;

typedef struct _tagRectVal {
	DWORD	dwVal;
	RECT 	rect;
} RECTVAL;
typedef struct _tagMDPARAM {
	DWORD	dwActions;
	DWORD dwRecChn;		//Record n when bit n is 1.
	UINT	nRv;
	RECTVAL rv[4];
	UINT	nTSeg;
	TIMESEG	tseg[16];
} MDPARAM;

typedef struct _tagDdnsParam {
	int		iSP;		//0-None; 1-Oray; 2-3322
	char	cServer[40];
	UINT	iPort;
	char	cUser[32];
	char	cPswd[20];
	char	cDn[48];
} DDNSPARAM;

typedef struct _tagNICParam {
	BOOL	bDhcp;
	char	cIp[16];
	char	cNetMask[16];
	char	cDefGW[16];
	char	cMac[32];
	BOOL	bAutoDNS;
	char	cDns1[16];
	char	cDns2[16];
	UINT	iPort;
} NICPARAM;


// "AuthMode : NONE, WEP, WPAPSK, WPA2PSK"
// "             NONE       Open system"
// "             WEP        Use WEP"
// "             WPAPSK     For WPA pre-shared key"
// "             WPA2PSK    For WPA2 pre-shared key"

// ""
// "EncrypType: OPEN, SHARE, TKIP, AES, TKIPAES"
// "             OPEN          For AuthMode=NONE or WEP"
// "             SHARED        Shared key system. For AuthMode=WEP"
// "             TKIP          For AuthMode=WPAPSK or WPA2PSK"
// "             AES           For AuthMode=WPAPSK or WPA2PSK"

// "Key: Depend on cAuthMode and cEncrypType " 
// "         If cEncrypType=NONE, cKey is ignored.
// "         Else, cKey is KEY or WPAPSK password."
// ""
typedef struct _tagWirelessNicInfo {
	BOOL	bEn;
	char	cSSID[48];
	char	cAuthMode[24];
	char	cEncrypType[24];
	char	cKey[48];

	/* Address information are ignored(share the setting of NICPARAM */
	BOOL	bDhcp;
	char	cIp[16];
	char	cNetMask[16];
	char	cDefGW[16];
	char	cMac[32];
} WIRELESSNICINFO;

typedef struct _tagPPPoEParam {
	BOOL	bEn;
	char	cUser[48];
	char	cPswd[24];
} PPPOEPARAM;

typedef struct _tagSMTPParam {
	BOOL	bEn;
	char	cSmtpSvr[40];
	UINT	iSmtpPort;		//default = 25;
	char	cUser[32];
	char	cPswd[24];
	char	cSender[48];
	char	cReceiver[48];
} SMTPPARAM;

typedef struct _tagUserRight {
	char cUser[20];
	DWORD dwRight;
} USERRIGHT;

typedef struct _tagVideoMask {
	BOOL	bEn;
	UINT	x, y, w, h;
	UINT	idxColor;		//0~6, 分别对应 红/黄/绿/蓝/白/青/紫
} VIDEOMASK;

typedef struct _tagTimeSetting {
	char location[32];
	char tz[64];
	struct tm _tm;
	BOOL	bNtp;
	char	cNtpServer[50];
} TIMESETTING;
//////////////////////////////////////////////////////////////////////
//
//  内置云台控制接口
//
enum { 
	PTZ_STOP = 0, 
	PTZ_MOVE_UP, 
	PTZ_MOVE_UP_STOP,
	PTZ_MOVE_DOWN,
	PTZ_MOVE_DOWN_STOP,
	PTZ_MOVE_LEFT,
	PTZ_MOVE_LEFT_STOP,
	PTZ_MOVE_RIGHT, 
	PTZ_MOVE_RIGHT_STOP,
	PTZ_MOVE_UPLEFT, 
	PTZ_MOVE_UPLEFT_STOP,
	PTZ_MOVE_DOWNLEFT, 
	PTZ_MOVE_DOWNLEFT_STOP,
	PTZ_MOVE_UPRIGHT, 
	PTZ_MOVE_UPRIGHT_STOP,
	PTZ_MOVE_DOWNRIGHT, 
	PTZ_MOVE_DOWNRIGHT_STOP,
	PTZ_IRIS_IN, 
	PTZ_IRIS_IN_STOP, 
	PTZ_IRIS_OUT, 
	PTZ_IRIS_OUT_STOP, 
	PTZ_FOCUS_ON, 
	PTZ_FOCUS_ON_STOP, 
	PTZ_FOCUS_OUT, 
	PTZ_FOCUS_OUT_STOP, 
	PTZ_ZOOM_IN, 
	PTZ_ZOOM_IN_STOP, 
	PTZ_ZOOM_OUT, 
	PTZ_ZOOM_OUT_STOP, 

	PTZ_SET_PSP, 
	PTZ_CALL_PSP, 
	PTZ_DELETE_PSP, 

	PTZ_BEGIN_CRUISE_SET, 
	PTZ_SET_CRUISE, 
	PTZ_END_CRUISE_SET, 
	PTZ_CALL_CRUISE, 
	PTZ_DELETE_CRUISE, 
	PTZ_STOP_CRUISE, 

	/* DVS 内置协议没有的命令 */

	PTZ_AUTO_SCAN, 
	PTZ_AUTO_SCAN_STOP,

	PTZ_RAINBRUSH_START, 
	PTZ_RAINBRUSH_STOP,
	PTZ_LIGHT_ON, 
	PTZ_LIGHT_OFF,

	PTZ_MAX 
};

//==============================
//
//   远  程  文  件  操  作
//
//=============================
typedef struct _tagFileFilter {
	UINT	chns;		//通道掩码
	time_t	from, to;	//时间范围。取0值无效
} FILEFILTER;

typedef struct _RemoteFileInfo {
	char	cFileName[64];
	DWORD	dwChannel;
	time_t	tmStart;
	time_t	tmEnd;
	DWORD	dwFileLength;
	DWORD	dwDuration;			//以秒计的文件播放时长
} REMOTEFILEINFO;

typedef BOOL (*LISTFILECALLBACK)(REMOTEFILEINFO *prfi, void *arg);

typedef struct _tagDownLoadCBParam {
	DWORD dwBytesTotal;
	DWORD dwBytesTrans;
	void *userData;
} DOWNLOADCBPARAM;
typedef BOOL (*DOWNLOADCALLBACK)(DOWNLOADCBPARAM *arg);

/////////////////////////////////////////////////////////////////////////////

//
// 设备端录像调度
//
typedef struct _tagRemoteRecordSchedule {
	char	cName[60];

	UINT	nTSeg;
	TIMESEG	*pTimeseg;

	DWORD	dwChnMask;
	DWORD	dwVideoMask;	//0 for Main stream, 1 for Secendary stream
} REMOTERECORDSCHEDULE;

typedef struct _tagRecordSetting {
	DWORD	dwSizeLmt;	//M
	DWORD	dwTimeLmt;	//sec
	DWORD	dwAlarmTriggerRecLmt;

	UINT	nRRS;
	REMOTERECORDSCHEDULE *pRRS;
} REMOTERECORDSETTING;

void FreeRRSResource(REMOTERECORDSETTING *prs);
///////////////////////////////////////////////////////////////
#define UE_BOF		1
#define UE_EOF		2
#define UE_WAIT		3
#define UE_PROGRESS	4
#define UE_EXEC		5
typedef void (*UPDATECALLBACK)(UINT event, const char *, void* data);
int VerifyFile(const char *sfile, CString& product, CString& ver, CString& pub);

/* 用户权限 */
#define USERRIGHT_USER_MANAGEMENT		1
#define USERRIGHT_SETUP_PARAM			2
#define USERRIGHT_CONTROL_PTZ			4
#define USERRIGHT_FILE_OPERATION		8

/* 硬件标志 */
#define HWFLAG_WIRELESS_NIC				1	//带无线网卡
#define HWFLAG_SDC						2	//带SD卡
#define HWFLAG_OV7725					4	//CMOS(ov7725)
#define HWFLAG_STDRTP_TS				8	//Resolution of timestamp is 1/90000
#define HWFLAG_3G						16	//

/* 设备事件 */
#define DEVICEEVENT_MIN				100
#define DEVICEEVENT_CONNECTING			(DEVICEEVENT_MIN+10)
#define DEVICEEVENT_CONNECTED			(DEVICEEVENT_MIN+11)
#define DEVICEEVENT_CONNECT_FAILED		(DEVICEEVENT_MIN+12)
#define DEVICEEVENT_AUTH_FAILED			(DEVICEEVENT_MIN+13)
#define DEVICEEVENT_DISCONNECTED		(DEVICEEVENT_MIN+14)
#define DEVICEEVENT_NO_RESPONSE			(DEVICEEVENT_MIN+15)
#define DEVICEEVENT_IS_ALIVE			(DEVICEEVENT_MIN+16)
#define DEVICEEVENT_RESUMED				(DEVICEEVENT_MIN+17)
#define DEVICEEVENT_PEERCLOSED			(DEVICEEVENT_MIN+18)
//Parameter for DEVICEEVENT_PASSIVECLT is a pointer to struct passive_clt_id, 
//This event only be notified via SDK's callback function.
//If the client is verified, the callback function should alloate a new, or 
//locate a existing connection object, set the cUser and cPassword, and return it.
//Then, PassiveMode is called by connection manager.
//Application uses IsPassive() to distinguish passive connections and normal
//connections.
#define DEVICEEVENT_PASSIVECLT			(DEVICEEVENT_MIN+19)	//Return: NULL or Pointer to connection object

#define DEVICE_NOTIFY_REBOOT		NOTIFICATION_REBOOT
#define DEVICE_NOTIFY_STOPSERVICE	NOTIFICATION_STOPSERVICE
#define DEVICE_NOTIFY_LOGIN			NOTIFICATION_LOGIN
#define DEVICE_NOTIFY_LOGOUT		NOTIFICATION_LOGOUT
#define DEVICE_NOTIFY_ALARM			NOTIFICATION_ALARM
#define DEVICE_NOTIFY_MSG			NOTIFICATION_MSG

//-------------------------------------------
#define DEFAULT_SERVER_PORT	8001

struct _DVSCONN;
typedef struct _DVSCONN DVSCONN, *LPDVSCONN;
class DvsConnArray;
class CDvsConnManager;
typedef struct _tagProxyInfo {
	char	cName[40];
	DvsConnArray	*pvConn;
} PROXYINFO;
//Device status
#define DEVICESTATUS_DISCONNECTED	0
#define DEVICESTATUS_CONNECTING		1
#define DEVICESTATUS_CONNECTED		2
#define DEVICESTATUS_RESUMEFAILED	3	//Resume/AsynResume失败, 仍保留会话状态信息。或者继续调用[Asyn]Resume，或者调用Disconnect

//Connection flags
#define CF_ASYNC				1
#define CF_CREATEALERTCONN		2
#define CF_RESUME				4

struct EXTERN _DVSCONN
{
friend void AsynConnectCB(DVSCONN *pConn, int err, DWORD events, void *p);
friend PA_ThreadRoutine ConcurentExcutionThread;

	BOOL	bProxy;					//为FALSE时表是这是一个普通的设备, 否则是一个Proxy
	/* 连接参数 */
	char	cOEMId[20];			//OEM客户标识
	char	cUser[32];			//User name
	char	cPassword[20];		//User password
	char	cHost[120];			//DDNS Host Name or IP. If pAgent!=NULL, cHost is just a identifier
	int		iPort;				//Device port
	
	struct _DVSCONN	*pAgent;
	UINT	uiWaitTimeout;		//等待命令返回 MS
	
  /* 以下信息在连接后有效 */
	DWORD	dwRight;
	char	cDestIP[20];		//Device IP
	PA_SOCKET	hSocket;			//Socket handle
	PA_SOCKET	hAlertSock;
	struct sockaddr_in devAddr;

	int		iIdleSec;			//不活动秒数。每秒钟由使用者加1，执行任何命令置0
	/* 设备信息 */
	union {
		DEVICEINFO	devInfo;
		PROXYINFO proxyInfo;
	};

	CDvsConnManager	*pConnMgr;
	struct list_head	cmd_list;

private:
	char conn_id[20];	//New connection identifier
	PA_SOCKET hNewConn;

protected:
	char	sid[20];			//客户标识串
	DWORD	dwConnFlags;		//连接标志：异步?报警连接?
	int		iAsynErrCode;		//异步操作错误码

	int		iStatus;			//Device Status

	BOOL	bPassive;			//Passive模式下，所有连接由设备先发起

public:
	_DVSCONN(_DVSCONN *pProxy = NULL, const char *psOemId = NULL);
	virtual ~_DVSCONN();

	int PassiveMode(PA_SOCKET sk);
	int Connect(DWORD connflags = CF_CREATEALERTCONN);
	int AsynConnect(DWORD connflags = CF_CREATEALERTCONN);
	int Resume();		//Not Document
	int AsynResume();
	inline int GetAsynErrorCode() const { return iAsynErrCode; }		//异步操作错误码
	int GetStatus() const { return iStatus; }
	inline BOOL IsPassive() { return bPassive; }

	void Disconnect();

	int CTPCommand(const char *cmd, /*INOUT*/ CString &str, const char *extra_headers=NULL);
	int ExecCmd(const char *cmd, CString &str, const char *extra_headers = NULL);
	int ExecCmd(CString &str, const char *extra_headers = NULL);
	int QueryCmd(const char *cmd, CString &str/*IN:body, OUT:reason or data*/, KEYVAL *pKv, UINT size, DWORD flags=0, const char *extra_headers = NULL);
	int CTPCommandWithCallback(const char *scmd, /*INOUT*/CString &strBody, VSCMDLINECB pCmdCbFunc, void *arg, const char *extra_headers=NULL);
	int CTPCommandWithCallbackBin(const char *scmd, /*INOUT*/CString &strBody, VSCMDBINCB pCmdCbFunc, void *arg, const char *extra_headers=NULL);

	UINT  GetSID(char cltid[20]);
public:
	virtual int FetchSubConns();
public:
	//
	// DVS内置云台支持
	//
	int CTPPTZCommand(UINT chn, UINT code, UINT para1, UINT para2);
	int CTPPTZMove(UINT chn, UINT direction, UINT xspd, UINT yspd);
	int CTPPTZLens(UINT chn, UINT act);

	//用户定义云台命令
	int PTZCmdUD(const PTZSETTING *ptz, const BYTE *buf, UINT len);		//User defined cmd

	//透明485传输
	int CTPRelay485(const BYTE *cmd, UINT size);
	int CTPRelay485(const COMMSETTING* pCfg, const BYTE *cmd, UINT size);

	//
	int CTPSetPspName(UINT chn, UINT addr, const char *name);	//设置预置点称
	int CTPSetCruiseName(UINT chn, UINT id, const char *name);//设置巡航轨迹称
	int CTPGetPresetPoints(UINT vchn, IDNAME **ppPsp, UINT *puiCount);
	int CTPGetCruiseTracks(UINT vchn, IDNAME **ppTrack, UINT *puiCount);
	
	//
	int CTPGetDeviceInfo(DEVICEINFO *pDevInfo);
	int CTPSetServerName(const char *svrName);
	int CTPGetVChnInfo(UINT vchn, VCHNINFO *pVChnInfo);		//If Secondary stream is not supported, iSubMode is -1
	int CTPSetVChnInfo(UINT vchn, const VCHNINFO *pVChnInfo);
	int CTPGetVChnInfo2(UINT vchn, VCHNINFO2 *pVChnInfo);		//If Secondary stream is not supported, iSubMode is -1
	int CTPSetVChnInfo2(UINT vchn, const VCHNINFO2 *pVChnInfo);
	int CTPGetAChnInfo(UINT achn, ACHNINFO *pAChnInfo);
	int CTPSetAChnInfo(UINT achn, const ACHNINFO *pAChnInfo);
	int CTPGetPtzSetting(UINT vchn, PTZSETTING *pPtzParam);
	int CTPSetPtzSetting(UINT vchn, const PTZSETTING *pPtzParam);
	int CTPGetAlarmInChnInfo(UINT chn, ALARMINCHNINFO *pAici);
	int CTPSetAlarmInChnInfo(UINT chn, const ALARMINCHNINFO *pAici);
	int CTPGetAlarmOutChnInfo(UINT chn, ALARMOUTCHNINFO *pAoci);
	int CTPSetAlarmOutChnInfo(UINT chn, const ALARMOUTCHNINFO *pAoci);
	int CTPGetVEncParam(UINT vchn, VIDEOENCPARAM *pParam);
	int CTPSetVEncParam(UINT vchn, const VIDEOENCPARAM *pParam);
	int CTPGetVEncParam2(UINT vchn, VIDEOENCPARAM2 *pParam);
	int CTPSetVEncParam2(UINT vchn, const VIDEOENCPARAM2 *pParam);
	int CTPGetVideoColor(UINT vchn, VIDEOCOLOR *pVC);
	int CTPSetVideoColor(UINT vchn, const VIDEOCOLOR *p);
	int CTPGetCmosEnvs(UINT vchn, CMOSENVS *pEnvs);
	int CTPSetCmosEnv(UINT vchn, UINT index, const CMOSENV *pEnv);
	int CTPGetChnOsd(UINT vchn, OSDPARAM *pOsd);
	int CTPSetChnOsd(UINT vchn, const OSDPARAM *pOsd);
	int CTPGetChnMD(UINT vchn, MDPARAM *pMd);
	int CTPSetChnMD(UINT vchn, const MDPARAM *pMd);
	int CTPGetDDNS(DDNSPARAM *pDdns);
	int CTPSetDDNS(const DDNSPARAM *pDdns);
	int CTPGetNICInfo(NICPARAM *pNic);
	int CTPSetNICInfo(const NICPARAM *pNic);
	int CTPGetWirelessNICInfo(WIRELESSNICINFO *pWNic);
	int CTPSetWirelessNICInfo(const WIRELESSNICINFO *pWNic);
	int CTPIwList(struct iw_ap **ppAp, UINT *pNAp);
	int CTPGetMulticastParam(char cIp[16], int* piPort);
	int CTPSetMulticastParam(const char* cIp, int iPort);
	int CTPGetPPPoEParam(PPPOEPARAM *pParam);
	int CTPSetPPPoEParam(const PPPOEPARAM *pParam);
	int CTPGetSMTP(SMTPPARAM *pParam);
	int CTPSetSMTP(const SMTPPARAM *pParam);
	int CTPSendTestMail();
	int CTPGetMailStatus(int *piStatus/*MAIL_STATUS_XXX*/);
	int CTPListUser(USERRIGHT **ppUR, UINT *puiSize);
	int CTPAddUser(const char *sUsr, const char* sPswd, DWORD right);
	int CTPDelUser(const char *sUsr);
	int CTPGetRecordSetting(REMOTERECORDSETTING *prs);
	int CTPSetRecordSetting(const REMOTERECORDSETTING *prs);
	int CTPGetVideoMask(UINT vchn, VIDEOMASK *pMask);
	int CTPSetVideoMask(UINT vchn, const VIDEOMASK *pMask);
	int CTPGetTime(TIMESETTING *pts);
	int CTPSetTime(const TIMESETTING *pts);
	int CTPReboot();
	int CTPListFile(const FILEFILTER* filter, LISTFILECALLBACK lfFunc, void *arg);
	int CTPSignalOut(UINT chn);
	int Snapshot(UINT iChn, const char *sFileName);

	int CTPFileSession(const char *fn, char sid[20], int *flen);
	int CTPDownloadFile(const char *remotefile, const char *localfile, DOWNLOADCALLBACK pfDwnldCB, DOWNLOADCBPARAM *arg);
		
	ALARMOUTNAME* GetAlarmOutNames();
	void ClearCmdList();

	int RequestNewConn(/*OUT*/PA_SOCKET* hSock);

protected:
	friend BOOL _ListDevCB(char *line, void *arg);
	BOOL CreateAlertConnection();
	virtual _DVSCONN* NewConnObj();
	virtual int OnConnect();
	virtual void OnDisconnect();
	int BeginAsynConnect();
	int _CTPCommandWithCallback(const char *cmd, /*INOUT*/ CString &str, const char *extra_headers, VSCMDLINECB pCmdCbFunc, void *arg, BOOL bBinary);
	int __DoRecv(char *cBuffer, UINT uiBuffSize, int iRecvLen, CString& strBody, VSCMDLINECB pCmdCbFunc, void *arg, BOOL bBinary);
};
typedef struct _DVSCONN DVSCONN, *LPDVSCONN;

#define NEVERCONNECTED(p) (p->GetStatus() < DEVICESTATUS_CONNECTED)
#define EVERCONNECTED(p) (p->GetStatus() >= DEVICESTATUS_CONNECTED)

typedef BOOL (*PTZCMDHANDLER)(DVSCONN *pConn, UINT chn, UINT code, UINT para1, UINT para2);
extern PTZCMDHANDLER FPtzCmdHandler;	//SDK用到的云台操作支持

class HEADERHELPER {
public:
	HEADERHELPER(DVSCONN *pConn, const char *extra_headers);
	~HEADERHELPER();
	operator LPCSTR ();
private:
	BOOL bNeedFree;
	char *headers;
};

//DVSCONN, *LPDVSCONN;

//wParam: A pointer to DVSCONN where notification received from
//lParam: NOTIFICATION_XXX, where XXX = STOPSERVICE/REBOOT...
//			or
//		  A pointer to NOTIFYSTRUCT allocated for this notification. the window receives this message must free() the pointer.
typedef struct {
	UINT	notification;	//

	DVSCONN	*pConn;			//notification为NOTIFICATION_NOTIFY是为代理的指针，其余为被代理对象的指针
	union {
		//notification == NOTIFICATION_LOGIN/..._LOGOUT
		struct {
			char	user[20];
			char	ip[16];
			int		userCnt;
		} loginout;

		//notification = NOTIFICATION_ALARM
		struct {
			UINT	event;
			UINT	chn;
			BOOL	onoff;
			DWORD	recChn;	//触发录像通道
			int		vchn;	//I/O报警时关联视频
		} alarm;

		//notification = NOTIFICATION_NOTIFY
		struct {
			char	host[100];		//Host头内容
			char	msg[1];
		} notify;

		//notification = NOTIFICATION_MSG
		char msg[100];
	};
} NOTIFYSTRUCT;

typedef
struct passive_clt_id {
	char *sn;		// S/N
	char *dev_id;	// Identifier from IPCAM, used by client software to verify the device
	int sock;		// The connection
} PASSIVECLIENTID;

// 报警事件和其他事件的报告机制：回调函数或消息
//
// 回调函数先被调用，如果该函数返回0，库会再发送一条异步消息。
typedef int (*LPEVENTCALLBACK)(int, void *);	//
EXTERN LPEVENTCALLBACK SetEventCallback(LPEVENTCALLBACK pFunc);
EXTERN void SetEventListener(int hPipe);
void LibNotify(int notify, void *param, int param_len);

const char* Conn2Str(const DVSCONN *pConn);
BOOL IsTheHost(const DVSCONN *pConn, const char *host/*host is like xxx.yyy.zzz:port*/);

EXTERN DWORD CTPUpdate(DVSCONN *pConn, const char *file, UPDATECALLBACK pFunc, void* data);
//========================================================================================
class EXTERN DvsConnArray : public std::vector<DVSCONN*>
{
public:
	DvsConnArray();
	void RemoveConn(DVSCONN *p);
};

//
//  异步命令执行结构定义
//
#define FEVENT_READ			1
#define FEVENT_WRITE		2
#define FEVENT_ERROR		4
#define FEVENT_CONNECT		8
#define FEVENT_NEWCONN		16
#define CCFLAG_PENDING		1	//internaly used
#define CCFLAG_WAIT			2	//internal
#define CCFLAG_FINISHED		4	//internal
#define CCFLAG_DONTFREEMEM	8	//Set by Caller

typedef void (*CONCURRENTCMDCALLBACK)(DVSCONN *pConn, int err, DWORD events, void *param);
typedef struct _CmdNode {
	struct list_head	cmd_list;

	DWORD	dwEvent;		//INOUT: READ/WRITE/ERROR/CONNECT
	char	*pCmdToSend;
	UINT	nCmdLen;
	//DVSCONN *pConn;
	void	*pParam;
	CONCURRENTCMDCALLBACK	lpFunc;	//可以为NULL
	PA_EVENT	hEvent;				//事件满足则设置信号
	
	int	timeWait;	//ms

	DWORD	flags;	//zero or combination of FEVENT_...
} CMDNODE;

//
//  设备管理
//
class EXTERN CDvsConnManager : public DvsConnArray
{
friend PA_ThreadRoutine ConcurentExcutionThread;
friend PA_ThreadRoutine AlertReceiveThread;
friend struct _DVSCONN;
public:
	CDvsConnManager();
	virtual ~CDvsConnManager();

	BOOL Initialize(UINT iKeepAliveInterval=30, unsigned short port=0);

	DVSCONN *DeviceAdd();
	void DeviceAdd(DVSCONN *p);
	void DeviceRemove(DVSCONN *p);

	UINT GetCount();
	DVSCONN *GetAt(UINT index);
	DVSCONN * operator[](UINT index);
	
	BOOL QueueCmd(DVSCONN *pConn, DWORD event, CONCURRENTCMDCALLBACK lpFunc, void *param, int timewait, const char *psczCmd, UINT len, DWORD flags=0);
	
	CMDNODE *GetNode();
	//不会为要发送的命令分配空间，调用此方法后应马上转入等待. pNode由GetNode返回
	BOOL QueueCmd(DVSCONN *pConn, CMDNODE *pNode);
	void MarkCmdFinished(CMDNODE *pNode);

	inline DWORD GetConThreadId() { return m_idConThread; }
	void StopConExecThread();


	CReadWriteLock m_RWLock;

protected:
	virtual DVSCONN *NewItem();
	void PutCmdNode(CMDNODE *pCmd);
	void KeepAlive();
	void IncIdleSec();
	
protected:
	UINT	m_iKeepAliveInterval;
	unsigned short m_iPort;

public:
	struct list_head m_freeCmdList;
	CSpinLock	m_QueueLock;
private:
	CMDNODE	*m_pCmdNodes;
	BOOL	m_bRun;
	PA_EVENT	m_hEventQueued;
	PA_EVENT	m_hEventNodeAvailable;
	PA_HTHREAD	m_hConExecThread, m_hAlertThread;
	DWORD	m_idConThread, m_idAlertThread;
	PA_PIPE	m_hPipeRd, m_hPipeWrt;
};


#endif
