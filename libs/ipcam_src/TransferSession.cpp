#include "TransferSession.h"
#include "ReadWriter.h"
#include "FileStruct.h"
#include "Ctp.h"
#include "misc.h"

///////////////////////////////////////////////////////////////////////////////////////
static const char *MediaName[] = { "audio", "video", "composite" };
static const char *TransName[] = { "udp", "tcp", "multicast" };

CString StreamName(MEDIASTREAM stream)
{
	CString strm;
	switch(stream)
	{
	case STREAM_AUDIO/*: case STREAM_AUDIO_0*/: strm = "audio0"; break;
	case STREAM_AUDIO_1: strm = "audio1"; break;

	case STREAM_VIDEO/*: case STREAM_VIDEO_0*/: strm = "video0"; break;
	case STREAM_VIDEO_1: strm = "video1"; break;
	case STREAM_VIDEO_2: strm = "video2"; break;
	}
	return strm;
}
///////////////////////////////////////////////////////////////////////////////////////

/*获取nal类型*/
#define H264_Get_NalType(c)  ( (c) & 0x1F )  

/* Nal Type Definition */
#define NAL_TYPE_SLICE      1
#define NAL_TYPE_IDR        5
#define NAL_TYPE_SEI        6
#define NAL_TYPE_SPS        7
#define NAL_TYPE_PPS        8
#define NAL_TYPE_SEQ_END    9
#define NAL_TYPE_STREAM_END 10

enum {
	SLICE_P		= 0,
	SLICE_B,
	SLICE_I		=2,
	SLICE_SP,
	SLICE_SI	=4,
	SLICE_P2,
	SLICE_B2	=6,
	SLICE_I2	=7,
	SLICE_SP2,
	SLICE_SI2	= 9
};


//WM_USER+1: 用于 CChannelSessionManager 启动声音接收.
//==================================  CTransferSession  =======================================

#define FRAME_BUFFER_SIZE	(2*PLAY_VIDEO_MAX_FRAME_SIZE)

int __STDCALL _OnRecvVideo(unsigned char * pBuff,  unsigned int len, void *pParam);
int __STDCALL _OnRecvAudio(unsigned char * pBuff,  unsigned int len, void *pParam);

volatile DWORD CTransferSession::s_dwMaxFileMB = 100;		//100 M
volatile DWORD CTransferSession::s_dwMaxFileDuration = 60*60;	//1 Hour

CTransferSession::CTransferSession(DVSCONN *pConn, UINT chn)
{
	m_Chn = chn;
	m_UdpPort = pConn->iPort;
	m_pConn = pConn;

	InitMembers();
}

CTransferSession::~CTransferSession()
{
	if(m_SessState != SESSSTATE_STOP)
		StopSession();
}

void CTransferSession::InitMembers()
{
	m_SessState = SESSSTATE_STOP;

	m_pWriter = m_pWriterTmp = NULL;
	OnRecvVideo = OnRecvAudio = NULL;
	m_pCBData = NULL;
	m_dwIdleSec = 0;


	m_BufferHead[0] = m_BufferHead[1] = m_BufferHead[2] = 0;
	m_BufferHead[3] = 1;
	
	m_u16SN = 0;
	m_fRecordStreams = 0;
	m_bSearchedSPS = FALSE;

	m_bWaitIDR = FALSE;

	m_VideoType = STREAM_VIDEO;;	//默认请求主码流
	m_AudioType = STREAM_AUDIO;

	m_TransType = TRANSPORT_TCP;
	m_rt = ROTATION_NONE;

	m_AudioInfo.nSamplesPerSec = 8000;
	m_AudioInfo.wBitsPerSample = 16;
	m_AudioInfo.nChannels = 1;
	m_AudioInfo.nPacketType = HISITYPE;
	m_AudioInfo.wCodeType = 1;		//G711_A/G711_U/ADPCM_A/G726

	/* SESSPARAM */
	memset(&m_SessPm, 0, sizeof(m_SessPm));
	m_SessPm.sessId[0] = '\0';
    m_SessPm.pCBParam = this;
    m_SessPm.OnRecvVideo = _OnRecvVideo;/*call back function*/
    m_SessPm.OnRecvAudio = _OnRecvAudio;
    m_SessPm.pAudioInfo = &m_AudioInfo;
	m_SessPm.waitMulticastDataTimeout = 0;
	m_SessPm.transport = TRANSPORT_TCP;

	memset(&m_VideoStatVar, 0, sizeof(m_VideoStatVar));
}

void CTransferSession::SetWriter(WRITER *pWriter)
{
	m_pWriterTmp = pWriter;
	if(this->m_fRecordStreams == 0) m_pWriter = pWriter;
}

void CTransferSession::StartVideoReceiving()
{
	if(!m_SessPm.pRtpRecvStreamVideo)
	{
		m_RecvBuffer.AllocateBuffer(150*1024, 25);
		//m_pAUBuffer = new BYTE[1024*1024];
		StartReceiverThread(&m_SessPm, m_VideoType, m_UdpPort);
		m_dwIdleSec = 0;
	}
}
void CTransferSession::StopVideoReceiving()
{
	if(m_SessPm.pRtpRecvStreamVideo) 
	{
		HI_RTP_Recv_Destroy(m_SessPm.pRtpRecvStreamVideo);
		SAFE_FREE(m_SessPm.pRtpRecvStreamVideo);
		//SAFE_DELETE(m_pAUBuffer);
		m_RecvBuffer.FreeBuffer();
	}
}
int CTransferSession::StartSession()
{
	int rlt;
	CLockHelper lock(&m_Lock, TRUE);

	if(m_SessState != SESSSTATE_STOP) return 0;

	m_SessState = SESSSTATE_CONNECTING;
	m_SessPm.transport = m_TransType;

	if( (rlt = CTPInitSess(m_Chn, m_SessPm.transport, m_SessPm.sessId)) == 0)
	{
		GetRotation();
		m_SessState = SESSSTATE_PLAYING;
		m_RecvBuffer.ClearBuffer();
		StartVideoReceiving();
		LibNotify(SESSEVENT_STARTED, this, 0);
	}
	else
		m_SessState = SESSSTATE_STOP;
	return rlt;
}
void CTransferSession::StopSession()
{
	if(m_SessState == SESSSTATE_STOP) return;

	if(m_SessPm.sessId[0])
	{
		CTPStopSess(m_SessPm.sessId);	//extern
	}
	m_SessPm.sessId[0] = '\0';

	StopRecord();
	StopVideoReceiving();
	StopAudioReceiving();

	m_u16SN = 0;
	m_fRecordStreams = 0;
	m_bSearchedSPS = FALSE;
	m_bWaitIDR = FALSE;

	m_SessState = SESSSTATE_STOP;
	LibNotify(SESSEVENT_STOPPED, this, 0);
}

BOOL CTransferSession::StartRecord(const char* sFileName, BOOL bPrerecord, UINT strm_flags)
{
	CLockHelper lock(&m_Lock, TRUE);

	if(m_fRecordStreams && sFileName != m_strFile) StopRecord();

	if(m_fRecordStreams) return TRUE;

	m_strFile = "";
	m_pWriter = m_pWriterTmp;
	//m_pWriter = GetWriter();
	if((strm_flags & 3) == 0 || !m_pWriter) return FALSE;
	if(m_SessState != SESSSTATE_PLAYING) return FALSE;
	if(m_pWriter->BeginWriting(sFileName, strm_flags, &m_pWriterData))
	{
		//m_bSearchedSPS = FALSE;
/*  写预录部分的操作放到接收线程，以便于同步，避免最新接收帧漏录或重录
	同时，采用服务器端时标避免本机调度或操作上的延时造成录像时间抖动

		FRAMENODE *fCach;
		fCach = m_RecvBuffer.GetPrerecordStartFrame();
		while(fCach && fCach->nalType != NAL_TYPE_SPS) fCach = m_RecvBuffer.NextFrame(fCach);
		if(fCach)	//缓冲区中已有预录数据,写入预录帧, 在以后写入每个收到的帧
		{
			m_pWriter->WriteTag(TAG_STARTTIME, (const void*)(fCach->time), sizeof(DWORD), m_pWriterData);
			m_dwStartTimestamp = fCach->timeStamp;
			while(fCach)
			{
				m_pWriter->Write(RECORD_STREAM_VIDEO, fCach->timeStamp, fCach->pData, fCach->len, 
						fCach->nalType == NAL_TYPE_SPS ? STREAMFLAG_KEYFRAME : 0, pCS->m_pWriterData);
				m_dwTotalWriteBytes += fCach->len;
				fCach = m_RecvBuffer.NextFrame(fCach);
			}
			m_bSearchedSPS = TRUE;
		}
*/
		m_fRecordStreams = strm_flags;

		m_strFile = sFileName;
		m_nEventId = 0;
		m_dwTotalWriteBytes = 0;
		m_dwStartTimestamp = 0;
		
		m_bPrerecord = bPrerecord;
		if(!bPrerecord) RequestIFrame();

		return TRUE;
	}

	return FALSE;
}

void CTransferSession::StopRecord()
{
	m_Lock.Lock();
	if(m_fRecordStreams)
	{
		m_pWriter->EndWriting(m_pWriterData);
		m_fRecordStreams = 0;
		m_bSearchedSPS = FALSE;
	}
	m_Lock.Unlock();
}

void CTransferSession::RequestIFrame()
{
	CString strCmd;
	UINT uiSave = m_pConn->uiWaitTimeout;
	m_pConn->uiWaitTimeout = 500;
	strCmd.Format("reqiframe -sessid %s -media %s", m_SessPm.sessId, (const char*)StreamName(m_VideoType));
	m_pConn->ExecCmd(strCmd);
	m_pConn->uiWaitTimeout = uiSave;
}

bool CTransferSession::WriteTag(const char *TagName, const void *Tag, UINT len)
{
	CLockHelper lock(&m_Lock, TRUE);
	if(m_fRecordStreams)
	{
		bool rlt;
		rlt = m_pWriter->WriteTag(TagName, Tag, len, m_pWriterData) == TRUE;
		//m_Lock.Unlock();
		return rlt;
	}
	return false;
}
bool CTransferSession::WriteEventTag(DWORD timeStamp, UINT event, DWORD data)
{
	m_Lock.Lock();
	if(m_fRecordStreams)
	{
		char eventName[10];
		EVENTTAG eTag;
		bool bRlt;

		eTag.timeStamp = timeStamp;
		eTag.event = event;
		eTag.inputChannel = data;
		sprintf(eventName, "EVENT%d", m_nEventId++);
		bRlt = m_pWriter->WriteTag(eventName, &eTag, sizeof(eTag), m_pWriterData);
		m_Lock.Unlock();
		return bRlt;
	}
	m_Lock.Unlock();
	return false;
}

struct SnapshotArg {
	const char *fn;
	PA_HFILE hf;
};
extern BOOL SnapshotCallback(BYTE *pBytes, int len, void *arg);
bool CTransferSession::Snapshot(const char *sFileName)
{
	CString strBody;
	struct SnapshotArg arg = { sFileName, PA_INVALID_HANDLE };
	strBody.Format("-chn %d\r\n\r\n", m_Chn);
	int rlt = m_pConn->CTPCommandWithCallbackBin("snapshot", strBody, SnapshotCallback, &arg);
	if(PA_FileIsValid(arg.hf)) PA_FileClose(arg.hf);
	return rlt==0;
}

CString CTransferSession::GetRecordFileName()
{
	return m_strFile;
}
DWORD CTransferSession::GetRecordTimeLength()
{
	return m_dwLastTimestamp - m_dwStartTimestamp;
}
DWORD CTransferSession::GetRecordFileLength()
{
	return m_dwTotalWriteBytes;
}

TRANSPORTTYPE CTransferSession::GetTransportType()
{
	return m_SessState == SESSSTATE_STOP ? m_TransType : m_SessPm.transport;
}
void CTransferSession::SetTransportType(TRANSPORTTYPE trans_type)
{
	if(m_pConn->IsPassive()) m_TransType = TRANSPORT_TCP;
	else m_TransType = trans_type;
}

void CTransferSession::SetStreamType(MEDIASTREAM video, MEDIASTREAM audio)
{
	if(video < STREAM_VIDEO_MIN) video = STREAM_VIDEO_MIN;
	if(video > STREAM_VIDEO_MAX) video = STREAM_VIDEO_MAX;

	if(audio < STREAM_AUDIO_MIN) audio = STREAM_AUDIO_MIN;
	if(audio > STREAM_AUDIO_MAX) audio = STREAM_AUDIO_MAX;

	m_VideoType = video;
	m_AudioType = audio;
}

//-------------------------------------------------------------------------------------------------
static int uev(unsigned char **bytes, int *bitoff)
{
	unsigned char *p = *bytes;
	int boff = *bitoff;
	int zerobits = 0;
	int i, val = 0;
	while( ((0x80 >> boff) & *p) == 0)
	{
		zerobits++;
		boff++;
		if(boff == 8)
		{
			boff = 0;
			p++;
		}
	}
	
	boff++;
	if(boff == 8)
	{
		boff = 0;
		p++;
	}
	
	val = 0;
	for(i=0; i<zerobits; i++)
	{
		val <<= 1;
		if((0x80 >>  boff) & *p)
			val |= 1;
		boff++;
		if(boff == 8)
		{
			boff = 0;
			p++;
		}
	}
	val += (1<<zerobits) - 1;
	*bytes = p;
	*bitoff = boff;
	return val;
}

int __STDCALL _OnRecvVideo(unsigned char * pBuff,  unsigned int len, void *pParam)
{
	BYTE *pData, nNalType;
	unsigned short	nSQ;
	DWORD ts;
	int hdrLen, dataLen;
	BOOL bFirstISlice, bFirstMB = 0, bFirstFrag;
	FRAMENODE *f;
	int packet_mode = 0;
	CTransferSession *pCS = (CTransferSession *)pParam;
	VIDEOSTATWORKVAR *pStatVar = &pCS->m_VideoStatVar;

	/* 包头信息 */
	if(pCS->m_SessPm.transport == TRANSPORT_TCP)
	{
		MEDIA_HDR_S *pHdr;
		pHdr = (MEDIA_HDR_S*)pBuff;
		pData = pBuff + sizeof(MEDIA_HDR_S);
		nSQ = ntohs(pHdr->seqno);
		/*
		nSQ = pHdr->seqno;
		nSQ = (nSQ << 8) | (nSQ >> 8);
		*/
		hdrLen = sizeof(MEDIA_HDR_S);
		ts = ntohl(pHdr->pts);
	}
	else
	{
		RTP_HDR_S *pHdr = (RTP_HDR_S*)pBuff;
		pData = pBuff + (sizeof(RTP_HDR_S));// + 3;//PACK_TYPE_RTP_STAP
		nSQ = ntohs(pHdr->seqno);
		hdrLen = sizeof(RTP_HDR_S);// + 3;
		ts = ntohl(pHdr->ts);
	}
	if(pCS->m_pConn->devInfo.dwHwFlags & 8) ts /= 90;	//Clock rate is set to 90000 as RFC3984
	if(*pData == '\0') { while(*pData == '\0') { pData++; hdrLen++; } pData++; hdrLen++; }
	dataLen = len - hdrLen;

	/* 统计 */
	DWORD tick = PA_GetTickCount();
	pStatVar->dwNumRecvedBytes += dataLen;
	pStatVar->dwNumRecvedPacks ++;
	if(tick - pStatVar->dwStartTickCount >= 1000)
	{
		int index = pStatVar->index;
		pStatVar->dwaNumRecvedBytesHistory[index] = pStatVar->dwNumRecvedBytes;
		pStatVar->dwaNumLostPacksHistory[index] = pStatVar->dwNumLostPacks;

		index++;
		if(index >= HISTORY_DEPTH) index = 0;
		pStatVar->index = index;
		pStatVar->dwStartTickCount = tick;
		pStatVar->dwNumLostPacks = pStatVar->dwNumRecvedBytes = 0;
	}

	/* 转发或其它处理 */
	if(pCS->OnRecvVideo) pCS->OnRecvVideo(pBuff, len, pCS->m_pCBData);

	/*
	 * 1. 获取［当前］帧缓冲区指针 
	 */
	while( !(f = pCS->m_RecvBuffer.GetFrameToWrite()) ) 
	{
		dbg_msg("### Drop a frame in receiver threaqd\n");
		pCS->m_RecvBuffer.DequeueFrame(pCS->m_RecvBuffer.GetDataFrame());
	}
	if(!f) return 0;

	/* 
	 * 2. 检查是否掉包 
	 */
	if ((pCS->m_u16SN + 1 != nSQ) && (pCS->m_u16SN != 0) && (0 != nSQ))	//掉包
	{
		pStatVar->dwNumLostPacks += (nSQ - pCS->m_u16SN);
		pCS->m_bWaitIDR = TRUE;
		memset(f->pData, 0, f->len);
		f->len = 0;
		dbg_msg("Prev SQ: %d, Current SQ: %d\n", pCS->m_u16SN, nSQ);
	}
	pCS->m_u16SN = nSQ;


	nNalType = H264_Get_NalType(pData[0]);
	if(nNalType == 0x1C)	//FU-A
	{
		packet_mode = 1;

/*
   The FU indicator octet has the following format:

      +---------------+
      |0|1|2|3|4|5|6|7|
      +-+-+-+-+-+-+-+-+
      |F|NRI| Type=1C |
      +---------------+

   The FU header has the following format:

      +---------------+
      |0|1|2|3|4|5|6|7|
      +-+-+-+-+-+-+-+-+
      |S|E|R| NalType |
      +---------------+
*/
		nNalType = pData[1] & 0x1F;
		bFirstFrag = bFirstMB = bFirstISlice = FALSE;
		if((pData[1] & 0x80))
		{
			bFirstFrag = TRUE;
			if(nNalType == NAL_TYPE_SLICE || nNalType == NAL_TYPE_IDR) 
			{
				bFirstMB = 1;
				if(nNalType == NAL_TYPE_IDR) bFirstISlice = TRUE;
			}
			pData[1] = nNalType | (pData[0] & 0xE0);
			pData++;
			dataLen --;
		}
		else
		{
			pData += 2;
			dataLen -= 2;
		}
	}
	else if ((nNalType > 4 && nNalType < 11 || nNalType == 1)) //??保护代码. 增加对包类型的判断(>0)防止全0的码流
	{
		/* 
		* 3.检查当前包是否是一帧的开始 
		*/
		bFirstISlice = FALSE;
		if(nNalType == NAL_TYPE_SLICE || nNalType == NAL_TYPE_IDR)
		{
			int bitoff = 0, sliceType;
			unsigned char *prbsp = pData+1;
			bFirstMB = uev(&prbsp, &bitoff) == 0;
			if(bFirstMB)
			{
				sliceType = uev(&prbsp, &bitoff);
				if(sliceType == SLICE_I || sliceType == SLICE_I2)
				{
					bFirstISlice = TRUE;
				}
			}
		}
	}
	else
		return 0;

	if(nNalType == NAL_TYPE_SPS/* || bFirstISlice*/) pCS->m_bWaitIDR = FALSE;//First Slice of IDR	
	if(pCS->m_bWaitIDR) return 0;

	/* 
	 * 4.处理预录 
	 */
	if( pCS->m_pWriter && (pCS->m_fRecordStreams & RECORD_STREAM_VIDEO) && FALSE == pCS->m_bSearchedSPS )
	{
		if(pCS->m_bPrerecord)
		{
			FRAMENODE *fCach, *fCach2;
			fCach = pCS->m_RecvBuffer.GetPrerecordStartFrame();
			//fCach = NULL;
			while(fCach && fCach->nalType != NAL_TYPE_SPS) fCach = pCS->m_RecvBuffer.NextFrame(fCach);
			if(fCach)	//缓冲区中已有预录数据,写入预录帧, 在以后写入每个收到的帧
			{
				fCach2 = fCach;
				while(1)
				{
					do { fCach2 = pCS->m_RecvBuffer.NextFrame(fCach2); 	} while(fCach2 && fCach2->nalType != NAL_TYPE_SPS);
					if(fCach2 && pCS->m_RecvBuffer.GetNewestFrame() && (pCS->m_RecvBuffer.GetNewestFrame()->timeStamp - fCach2->timeStamp) > 20000) //缓存超过20",则试图找下一个I帧
						fCach = fCach2;
					else break;
				}

				pCS->m_pWriter->WriteTag(TAG_STARTTIME, (const void*)(fCach->time), sizeof(DWORD), pCS->m_pWriterData);
				pCS->m_dwStartTimestamp = fCach->timeStamp;
				while(fCach)
				{
					pCS->m_pWriter->Write(RECORD_STREAM_VIDEO, fCach->timeStamp, fCach->pData, fCach->len, 
							(pCS->m_rt<<30) | (fCach->nalType == NAL_TYPE_SPS ? STREAMFLAG_KEYFRAME : 0), 
							pCS->m_pWriterData);
					pCS->m_dwTotalWriteBytes += fCach->len;
					fCach = pCS->m_RecvBuffer.NextFrame(fCach);
				}
				pCS->m_bSearchedSPS = TRUE;
			}
		}
	}


/*                                     *
 *	准备工作完成，下面缓存数据和存文件 *
 *                                     */

	while(!pCS->m_Lock.Lock(10))
		if(pCS->m_SessPm.pRtpRecvStreamVideo->runState == RTP_RUNSTATE_STOP) return 0;


	/* 如果当前帧是完整帧，则存文件或允许解码，最另起一帧保存当前数据包 */
	if(f->len > 0 && (f->nalType != nNalType || bFirstMB))
	{
		//存文件
		if ( (pCS->m_fRecordStreams & RECORD_STREAM_VIDEO) && pCS->m_pWriter ) 
		{
			if(TRUE == pCS->m_bSearchedSPS)
			{
				pCS->m_pWriter->Write(RECORD_STREAM_VIDEO, f->timeStamp, f->pData, f->len, 
							(pCS->m_rt<<30) | (f->nalType == NAL_TYPE_SPS ? STREAMFLAG_KEYFRAME : 0), 
							pCS->m_pWriterData);
				pCS->m_dwTotalWriteBytes += f->len;
				if( pCS->s_dwMaxFileMB && (pCS->m_dwTotalWriteBytes >> 10) > pCS->s_dwMaxFileMB << 10	//以M为单位比较误差太大，改以KB为单位比较
						|| pCS->s_dwMaxFileDuration && (ts - pCS->m_dwStartTimestamp)/1000 > pCS->s_dwMaxFileDuration) 
				{
					pCS->StopRecord();
					LibNotify(SESSEVENT_RECORD_BREAKED, pCS, 0);
				}
				pCS->m_dwLastTimestamp = ts;
			}
			//非预录时的 录像同步点
			else if(NAL_TYPE_SPS == nNalType)	//否则等待SPS包
			{
				pCS->m_bSearchedSPS = TRUE;
				pCS->m_dwStartTimestamp = ts;
			}
		}

		//完整帧排队
		f->ready = 1;
		pCS->m_RecvBuffer.QueueFrame(f);
		if(f->isKeyFrame)	//丢掉最近关键帧以前的数据
			;//pCS->m_RecvBuffer.ReadFromLatestKeyFrame();

		//同步消费帧
		if(pCS->OnFrameReceived(RECORD_STREAM_VIDEO, f->pData, f->len, f->timeStamp, f->isKeyFrame?STREAMFLAG_KEYFRAME:0))
			pCS->m_RecvBuffer.DequeueFrame(f);


		//另起一帧
		f = pCS->m_RecvBuffer.GetFrameToWrite();
		if(!f) 
		{
			pCS->m_Lock.Unlock();
			return 0;
		}
	}

	//请求缓冲空间，不足则作丢包处理
	if(!pCS->m_RecvBuffer.ReserveSpace(f->len + dataLen + 4)) 
	{ 
		pCS->m_bWaitIDR = TRUE;
		f->len = 0;
		pCS->m_Lock.Unlock(); 
		return 0; 
	}

	pCS->m_Lock.Unlock();

	/* 缓冲数据，第一包则初始帧头 */
	if(f->len == 0) 
	{
		f->timeStamp = ts;
		f->nalType = nNalType;
		if(nNalType == NAL_TYPE_SPS) 
		{
			f->time = time(NULL);
			f->isKeyFrame = 1;
		}
		else
			f->isKeyFrame = 0;
	}
	if(!packet_mode || bFirstFrag)
	{
		memcpy(f->pData + f->len, pCS->m_BufferHead, 4);
		f->len += 4;
	}
	memcpy(f->pData + f->len, pData, dataLen);
	f->len += dataLen;

	return 0;
}

int __STDCALL _OnRecvAudio(unsigned char * pBuff,  unsigned int len, void *pParam)
{
	DWORD ts;
	BYTE *pData;
	CTransferSession *pCS = (CTransferSession *)pParam;
	UINT dataLen;

	/* 转发 或 其它 */
	if(pCS->OnRecvAudio) pCS->OnRecvAudio(pBuff, len, pCS->m_pCBData);

	if(pCS->m_SessPm.transport == TRANSPORT_TCP)
	{
		MEDIA_HDR_S *pHdr = (MEDIA_HDR_S*)pBuff;
		ts = ntohl(pHdr->pts);
		pData = pBuff + sizeof(MEDIA_HDR_S);
		dataLen = len - sizeof(MEDIA_HDR_S);
	}
	else
	{
		RTP_HDR_S *pHdr = (RTP_HDR_S*)pBuff;
		ts = ntohl(pHdr->ts);
		pData = pBuff + sizeof(RTP_HDR_S);
		dataLen = len - sizeof(RTP_HDR_S);
	}
	
	ts = ((ts * 90) & 0xFFFFFFFF) / 90;	//模拟设备端视视时间戳先溢出

	//while(!pCS->m_Lock.Lock(10))
	//	if(pCS->m_SessPm.pRtpRecvStreamAudio->runState == RTP_RUNSTATE_RUNNING) return 0;
	if(!pCS->m_Lock.Lock(50)) return 0;

	if( (pCS->m_fRecordStreams & RECORD_STREAM_AUDIO) && pCS->m_pWriter )
	{
		pCS->m_pWriter->Write(RECORD_STREAM_AUDIO, 
							ts, 
							pData, 
							dataLen, //pData[2]*2 + 4,		//Refer to voice.h
							STREAMFLAG_KEYFRAME,	//always be keyframe
							pCS->m_pWriterData
						);
	}
	pCS->m_Lock.Unlock();

	pCS->OnFrameReceived(RECORD_STREAM_AUDIO, pData, dataLen, ts, STREAMFLAG_KEYFRAME);

	return 0;
}

WRITER *CTransferSession::GetWriter()
{
	return m_pWriter;
}

UINT CTransferSession::GetSessionState()
{
	return m_SessState;
}

UINT CTransferSession::GetSessionPlayState()
{
	UINT state = 0;
	if(m_SessState != SESSSTATE_STOP)
	{
		if(m_fRecordStreams) state |= PLAYSTATE_RECORD;
	}
	return state;
}

void CTransferSession::GetVideoStat(DWORD *pRate, DWORD *pLost)
{
	DWORD totalBytes = 0, totalLost = 0;

	for(int i=0; i<HISTORY_DEPTH; i++)
	{
		totalBytes += m_VideoStatVar.dwaNumRecvedBytesHistory[i];
		totalLost += m_VideoStatVar.dwaNumLostPacksHistory[i];
	}
	*pRate = totalBytes / 5;
	*pLost = totalLost / 5;
}

int CTransferSession::SetRotation(ROTATIONTYPE rt)
{
	CString str;
	switch(rt)
	{
	case ROTATION_NONE: str.Format("rotate -chn %d -rotation none", m_Chn); break;
	case ROTATION_HFLIP: str.Format("rotate -chn %d -rotation hflip", m_Chn); break;
	case ROTATION_VFLIP: str.Format("rotate -chn %d -rotation vflip", m_Chn); break;
	case ROTATION_TURNOVER: str.Format("rotate -chn %d -rotation turnover", m_Chn); break;
	default:
		return 0;
	}
	int rlt;
	if( (rlt = m_pConn->ExecCmd(str)) == 0)
	{
		m_rt = rt;
		return 0;
	}
	return rlt;
}

ROTATIONTYPE CTransferSession::GetRotation()
{
	if(m_SessState == SESSSTATE_CONNECTING)
	{
		CString str;
		char sRot[10];
		KEYVAL kv = { "rotation", KEYVALTYPE_STRING, &sRot, 10 };
		str.Format("-chn %d\r\n-list\r\n\r\n", m_Chn);

		if(m_pConn->QueryCmd("rotate", str, &kv, 1) == 0)
		{
			if(PA_StrCaseCmp(sRot, "vflip") == 0) m_rt = ROTATION_VFLIP;
			else if(PA_StrCaseCmp(sRot, "hflip") == 0) m_rt = ROTATION_HFLIP;
			else if(PA_StrCaseCmp(sRot, "turnover") == 0) m_rt = ROTATION_TURNOVER;
			else //if(PA_StrCaseCmp(sRot, "none") == 0) 
				m_rt = ROTATION_NONE;
		}
	}
	return m_rt;
}

void CTransferSession::StartAudioReceiving()
{
	m_Lock.Lock();
	if(!m_SessPm.pRtpRecvStreamAudio)//!m_pAS && !m_pw16AudioDecbuf && m_AudioAction != AUDIO_NONE)
	{
		StartReceiverThread(&m_SessPm, m_AudioType, m_UdpPort);
	}
	m_Lock.Unlock();
}
void CTransferSession::StopAudioReceiving()
{
	//m_Lock.Lock();
	if(m_SessPm.pRtpRecvStreamAudio)
	{
		HI_RTP_Recv_Destroy(m_SessPm.pRtpRecvStreamAudio);
		SAFE_FREE(m_SessPm.pRtpRecvStreamAudio);
	}
	//m_Lock.Unlock();
}
int CTransferSession::CTPInitSess(UINT chn, UINT transport, char sessId[20])
{
	CString str;
	str.Format("-chn %d\r\n-transport %s\r\n\r\n", chn, TransName[transport]);

	KEYVAL kv[] = {
		{ "sessid", KEYVALTYPE_STRING,sessId, 20 },
	};

	return m_pConn->QueryCmd("initsess", str, kv, sizeof(kv)/sizeof(KEYVAL));
}
int CTransferSession::CTPStopSess(const char *sessid)
{
	CString str;
	str.Format("-sessid %s\r\n\r\n", sessid);
	return m_pConn->ExecCmd("stopsess", str);
}

int CTransferSession::StartReceiverThread(SESSPARAM *pSessPm, MEDIASTREAM stream, int UdpPort/*for TRANSPORT_UDP only*/)
{
    RTP_RECV_S     *pRtpRecvS;
	int len;
	CString str;
	PA_SOCKET	sock;
	struct sockaddr_in sa;
	unsigned long mcip;
	BOOL	bAStream = (stream <= STREAM_AUDIO_MAX && stream >= STREAM_AUDIO_MIN);

	str.Format("-sessid %s\r\n-media %s\r\n\r\n", pSessPm->sessId, (const char*)StreamName(stream));

	/*
	 * 创建 SOCKET, 并通知服务器发送目标 *
	 */
	if(m_pConn->IsPassive())
	{
		pSessPm->transport = TRANSPORT_TCP;
		int rlt = m_pConn->RequestNewConn(&sock);
		if(rlt) return rlt;
	}
	else
		sock = socket(AF_INET, pSessPm->transport == TRANSPORT_TCP ? SOCK_STREAM : SOCK_DGRAM, 0);
	switch(pSessPm->transport)
	{
	case TRANSPORT_UDP: 
		len = sizeof(sa);
		PA_GetPeerName(m_pConn->hSocket, (struct sockaddr*)&sa, &len);
		sa.sin_port = htons(UdpPort);
		//sendto(sock, str, str.GetLength(), 0, psa, sizeof(*psa));	//Hole Punching
		break;
	case TRANSPORT_TCP:
		if(!m_pConn->IsPassive())
		{
			len = sizeof(sa);
			PA_GetPeerName(m_pConn->hSocket, (struct sockaddr*)&sa, &len);
			if(connect(sock, (struct sockaddr*)&sa, sizeof(sa)) < 0) { PA_SocketClose(sock); return FALSE; }
		}
		if(ExecCmd(sock, "require", str, NULL) != 0) { PA_SocketClose(sock); return FALSE; }
		break;

	case TRANSPORT_MULTICAST:	//多播会话，在 InitSess() 中通知，在此直接准备接收数据
		{
			char ip[17];
			int port;
			KEYVAL kv[] = { 
				{ "mcip", KEYVALTYPE_STRING,  ip, 17 },
				{ "mcport", KEYVALTYPE_INT, &port }
			};
			if(QueryCmd(m_pConn->hSocket, "require", str, kv, sizeof(kv)/sizeof(KEYVAL), 0) == 0)
			{
				sa.sin_family = AF_INET;
				sa.sin_addr.s_addr = htonl(INADDR_ANY);
				sa.sin_port = htons(port);
				memset(sa.sin_zero, 0, sizeof(sa.sin_zero));

				if(bind(sock, (const struct sockaddr*)&sa, sizeof(sa)) == 0)
				{
					struct ip_mreq im;
					len = sizeof(sa);
					PA_GetSockName(sock, (struct sockaddr*)&sa, &len);
					mcip = inet_addr(ip);
					im.imr_multiaddr.s_addr = mcip;
					im.imr_interface.s_addr = sa.sin_addr.s_addr;
					if(setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, (const char*)&im, sizeof(im)) == 0) break;
				}
			}
		}
		PA_SocketClose(sock);
		return FALSE;
		break;

	default:
		PA_SocketClose(sock);
		return FALSE;
	}

    int opt = 0x40000;
    setsockopt(sock, SOL_SOCKET, SO_RCVBUF, (const char*)(&opt), sizeof(int));

	/* 
	 * 创建 Receiver 并启动接收线程 
	 */
    pRtpRecvS = (RTP_RECV_S*)calloc(sizeof(RTP_RECV_S), 1);

	pRtpRecvS->sock = sock;
	pRtpRecvS->rtp_on_recv = bAStream ? pSessPm->OnRecvAudio : pSessPm->OnRecvVideo;
    pRtpRecvS->ext_args = (int*)pSessPm->pCBParam;
    pRtpRecvS->runState    = RTP_RUNSTATE_STOP;
	pRtpRecvS->transType = pSessPm->transport;
	if( pSessPm->transport == TRANSPORT_MULTICAST )
	{
		pRtpRecvS->mcgrpip = mcip;
	}
	pRtpRecvS->srcfilter = FALSE;

	strcpy(pRtpRecvS->sessId, pSessPm->sessId);
	if(pSessPm->transport == TRANSPORT_UDP)
	{
		pRtpRecvS->sPunch = (char*)malloc(str.GetLength() + 1);
		strcpy(pRtpRecvS->sPunch, str);
		//Hole punching					
		memcpy(&pRtpRecvS->sa_punch, &sa, sizeof(sa));
		sendto(sock, pRtpRecvS->sPunch, strlen(pRtpRecvS->sPunch), 0, (struct sockaddr*)&sa, sizeof(sa));
	}

	if(bAStream) { 
		pSessPm->pRtpRecvStreamAudio = pRtpRecvS; 
		pRtpRecvS->st = RECORD_STREAM_AUDIO; 
	}
	else {
		pSessPm->pRtpRecvStreamVideo = pRtpRecvS;
		pRtpRecvS->st = RECORD_STREAM_VIDEO;
	}

	HI_RTP_Recv_Start(pRtpRecvS);

	return TRUE;
}

BOOL CTransferSession::OnFrameReceived(UINT strm, BYTE *pData, UINT len, DWORD ts, DWORD flags)
{
	return TRUE;
}
