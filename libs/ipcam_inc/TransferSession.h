#ifndef __ChannelSession_h__
#define __ChannelSession_h__

#include <vector>
#include "ReadWriter.h"
#include "ReceiverBuffer.h"
#include <Ctp.h>
#include <hi_rtp.h>
#include <hi_rtp_rcvr.h>
#include "DvsConn.h"
#include "RWLock.h"
#include "IpcamClt.h"

///////////////////////////////////////////////////////////////////////////////////////

typedef enum {
	STREAM_AUDIO_MIN,
	STREAM_AUDIO = 0, 
	STREAM_AUDIO_0 = 0,
	STREAM_AUDIO_1,
	STREAM_AUDIO_2,
	//....
	STREAM_AUDIO_MAX = STREAM_AUDIO_2,
	
	STREAM_VIDEO_MIN,
	STREAM_VIDEO = STREAM_VIDEO_MIN,
	STREAM_VIDEO_0 = STREAM_VIDEO,
	STREAM_VIDEO_1,
	STREAM_VIDEO_2,
	//....
	STREAM_VIDEO_MAX = STREAM_VIDEO_2,
	STREAM_MAX 
} MEDIASTREAM;

#define	TRANSPORT_UDP		RTP_TRANSPORT_UDP
#define TRANSPORT_TCP		RTP_TRANSPORT_TCP
#define TRANSPORT_MULTICAST RTP_TRANSPORT_MULTICAST
#define TRANSPORTTYPE		RTP_TRANSPORT_TYPE_E

#define MPEG4_GENERIC 0x05
#define HISITYPE 0x06

typedef void (* PLAYER_EVENT_CB)(int event, void *param);

typedef struct hiPLAY_AUDIO_INFO_S
{
    ULONG		nSamplesPerSec;  /* sampling rate of audio */
    WORD		wBitsPerSample;  /* bits per sample of audio */
    WORD		nChannels;       /* channels of audio */
    WORD		wCodeType;       /* the type of coding */
    WORD      nPacketType;
} PLAY_AUDIO_INFO_S;/* information of audio */

typedef struct _tagSessParam {

	TRANSPORTTYPE		transport;
	int waitMulticastDataTimeout;		//second. 0:unicast -1:infinite

	/* 调用 InitSess() 后返回 */
	char sessId[20];

	/* 调用 StartReceiverThread 前设置 */
    void	*pCBParam;
    RTP_ON_RECV_CB	OnRecvVideo;
    RTP_ON_RECV_CB	OnRecvAudio;

	//void	*pEventCBParam;			//
	//PLAYER_EVENT_CB	OnPlayerEvent;

    RTP_RECV_S *pRtpRecvStreamVideo;
    RTP_RECV_S *pRtpRecvStreamAudio;
    PLAY_AUDIO_INFO_S *pAudioInfo;
	//RtpMemPool *pMemPool;
} SESSPARAM;

CString StreamName(MEDIASTREAM stream);

///////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////

enum { BUFFERSTRATEGY_YES, BUFFERSTRATEGY_NO, BUFFERSTRATEGY_AUTO };

#define HISTORY_DEPTH	5
typedef struct _tagVideoStatWorkVar {
	DWORD dwStartTickCount;				//统计秒的起始 TickCount
	DWORD dwNumRecvedPacks, dwNumLostPacks, dwNumRecvedBytes;	//在本统计秒内收到包和字节数

	DWORD dwaNumRecvedBytesHistory[HISTORY_DEPTH];				//最近X秒内每秒接收记录
	DWORD dwaNumLostPacksHistory[HISTORY_DEPTH];
	int index;
} VIDEOSTATWORKVAR;

///////////////////////////////////////////////////////////////////////////////////////
#define SESSEVENT_STARTED			(NOTIFICATION_MAX+1)
#define SESSEVENT_STOPPED			(NOTIFICATION_MAX+2)
#define SESSEVENT_RECORD_BREAKED	(NOTIFICATION_MAX+3)
#define SESSEVENT_NO_DATA			(NOTIFICATION_MAX+4)
#define SESSEVENT_MAX				(NOTIFICATION_MAX+100)

///////////////////////////////////////////////////////////////////////////////////////

#define PLAYSTATE_VIDEO		1
#define PLAYSTATE_AUDIO		2
#define PLAYSTATE_TALKBACK	4
#define PLAYSTATE_RECORD	8

typedef enum { SESSSTATE_STOP, SESSSTATE_CONNECTING, SESSSTATE_PLAYING, SESSSTATE_PAUSED } SESSSIONSTATE;
typedef enum { AUDIO_NONE, AUDIO_MUTE, AUDIO_PLAY } AUDIOACTION;

struct _tagAudioPacket;
struct _DVSCONN;
class EXTERN CTransferSession
{
friend int __STDCALL _OnRecvVideo(unsigned char * pBuff,  unsigned int len, void *pParam);
friend int __STDCALL _OnRecvAudio(unsigned char * pBuff,  unsigned int len, void *pParam);
friend void *__STDCALL HI_RTP_RecvHandle(void* args);

public:
	CTransferSession(struct _DVSCONN *pConn, UINT chn);
	virtual ~CTransferSession(void);

public:
	//StartSession()
	//   视频同时只支持播放一个码流. 要切换码流, 须要重启会话:
	//       StopSession() --> SetStreamType(...) --> StartSession();
	virtual int StartSession();
	virtual void StopSession(void);

	virtual BOOL StartRecord(const char* sFileName, BOOL bPrerecord = FALSE, UINT flag = RECORD_STREAM_VIDEO|RECORD_STREAM_AUDIO);
	virtual void StopRecord();
	bool WriteTag(const char *TagName, const void *Tag, UINT len);
	bool WriteEventTag(DWORD timeStamp, UINT event, DWORD data);

	WRITER *GetWriter();
	void SetWriter(WRITER *pWriter);
	CString GetRecordFileName();
	DWORD GetRecordTimeLength();
	DWORD GetRecordFileLength();


	virtual int SetRotation(ROTATIONTYPE rt);	//
	ROTATIONTYPE GetRotation();

	struct _DVSCONN* GetConn() { return m_pConn; }

	bool Snapshot(const char *sFileName);

	void StartAudioReceiving();
	void StopAudioReceiving();
	
	void RequestIFrame();

	UINT GetSessionState();
	virtual UINT GetSessionPlayState();
	void GetVideoStat(/*OUT*/DWORD *pdwRate, /*OUT*/DWORD *pdwLost);	//返回每秒钟收到的字节数/丢包数

public:
	TRANSPORTTYPE	GetTransportType();
	void	SetTransportType(TRANSPORTTYPE transport);	//用于在SetTarget()后切换使用数据连接
	void	SetStreamType(MEDIASTREAM video, MEDIASTREAM audio);

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wreturn-type"
protected:
	virtual BOOL OnFrameReceived(UINT strm, BYTE *pData, UINT len, DWORD ts, DWORD flag);
	virtual BOOL ReceiverThreadBeginning(UINT strm) {}
	virtual void ReceiverThreadEnding(UINT strm) {}
#pragma GCC diagnostic pop
public:
	volatile static DWORD		s_dwMaxFileMB;		//单位: K Bytes
	volatile static DWORD		s_dwMaxFileDuration;//单位: s

	RTP_ON_RECV_CB	OnRecvVideo, OnRecvAudio;
	void		*m_pCBData;
	DWORD		m_dwIdleSec;			//多长时间(s)收不到数据

protected:	
	ROTATIONTYPE m_rt;

	/* Record */
	CString		m_strFile;				//the file name being recording
	UINT		m_fRecordStreams;		//录像流. video/audio or both
	DWORD		m_dwStartTimestamp, m_dwLastTimestamp, m_dwTotalWriteBytes;
	WRITER		*m_pWriter, *m_pWriterTmp;
	void		*m_pWriterData;
	CMutexLock	m_Lock;					//写操作锁
	UINT		m_nEventId;				//录像过程中的事件编号, 从0开始
	
	/* Status */
	UINT		m_SessState;

	PLAY_AUDIO_INFO_S	m_AudioInfo;
	CReceiverBuffer	m_RecvBuffer;

	/* Communication */
	MEDIASTREAM			m_VideoType, m_AudioType;
	struct _DVSCONN			*m_pConn;
	UINT				m_Chn;
	SESSPARAM			m_SessPm;
	TRANSPORTTYPE		m_TransType;
	UINT				m_UdpPort;	//m_TransType == TRANSPORT_UDP 时要求

private:
	void InitMembers();
	
	int CTPInitSess(UINT chn, UINT transport, char sessId[20]);
	int CTPStopSess(const char *sessid);

	int StartReceiverThread(SESSPARAM *pSessPm, MEDIASTREAM stream, int UdpPort/*for TRANSPORT_UDP only*/);
	void StartVideoReceiving();
	void StopVideoReceiving();
private:
	BOOL				m_bPrerecord;
	/* Video Receiving */
	VIDEOSTATWORKVAR	m_VideoStatVar;
	//Vars used by _OnRecvVideo Callback function
	BYTE				m_BufferHead[4];
	WORD				m_u16SN;		//包序号
	BOOL				m_bWaitIDR;		//是否等待重同步
	BOOL	      m_bSearchedSPS;	//录像时是否找到SPS包
	//<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
};
//========================================================================================
#endif
