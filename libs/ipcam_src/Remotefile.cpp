#include <Ctp.h>
#include <FileStruct.h>
#include <errdefs.h>
#include <fcntl.h>
#include <ReadWriter.h>
#include <vector>
#include <DvsConn.h>
#include "Remotefile.h"

#ifndef offsetof
#define offsetof(s, m) ((intptr_t)&(((s*)0)->m)) /*TODO GSG*/
#endif
//--------------------------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////////////
///////////////////                               ////////////////////////////////
///////////////////       Remote File Reader      ////////////////////////////////
///////////////////                               ////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////
#ifndef MAX_PATH
#define MAX_PATH 256
#endif

typedef struct _KFIndex {
	DWORD timeStamp;
	DWORD offset;
} KEYFRAME_INDEX;
typedef struct _tagFRAMEHEADER {
	DWORD strmType, len, ts, flag;
} FRAMEHEADER;
typedef struct _tagPFPacket {
	DWORD offset, length;
	FRAMEHEADER frmhdr;
	BYTE  data[1];
} PFPACKET;

#define RRS_RUN		0
#define RRS_STOP	1
#define RRS_ERR		2
typedef struct _tagRRDATA {
	DVSCONN	*pConn;
	char	sid[20];
	PA_SOCKET	sock;
	PA_HTHREAD hRcvThd;
	DWORD	status;

	FILEINFO fileInfo;
	char sTmpFile[MAX_PATH];

	DWORD tsMax;
	std::vector<KEYFRAME_INDEX> *pvKfi;

	BOOL bEof;
	FILE *fp;
	int flen;
	PA_MUTEX hFileMutex;

	FRAMEHEADER fhdrLA;	//look ahead frame-header
	FRAMEHEADER fhdrLast;	//Last downloaded frame's header
	//
	PROGRESSCALLBACK	cb_dwnld;
	void 			*cb_data;
	UINT	iDnldProgress;	//千分比
} RRDATA;

PA_THREAD_RETTYPE __STDCALL RecvThread(void *lpParameter)
{
#define BUFSIZE (sizeof(PFPACKET) + 200*1024)
	char *buf;
	int wp = 0, len;
	PFPACKET *pPfp;
	RRDATA *pRRData = (RRDATA*)lpParameter;

	buf = (char*)malloc(BUFSIZE);
	pPfp = (PFPACKET*)buf;
	int i=0;
	while( pRRData->status == RRS_RUN )
	{
		int fwpos, frpos, packLen;
		fd_set rfds, efds;
		struct timeval tv;

		FD_ZERO(&rfds); FD_ZERO(&efds);
		FD_SET(pRRData->sock, &rfds); FD_SET(pRRData->sock, &efds);
		tv.tv_sec = 4; tv.tv_usec = 0;
		if(select(pRRData->sock+1, &rfds, NULL, &efds, &tv) <= 0) 
		{ 
			pRRData->status = RRS_ERR; 
			break; 
		}
		if(FD_ISSET(pRRData->sock, &efds)) 
		{
			pRRData->status = RRS_ERR; 
			break; 
		}

		if( (len = recv(pRRData->sock, buf + wp, BUFSIZE - wp, 0)) == 0 ) 
		{ 
			pRRData->status = RRS_STOP; 
			break; 
		}
		if(len < 0) 
		{ 
			pRRData->status = RRS_ERR; 
			break; 
		}

		wp += len;
		while(1) //缓冲区数据存文件，每次一帧直到无完整帧
		{
			i++;
			//if(i>3000) { i=0; Sleep(30000); }
			char *pPFAddr = (char*)pPfp;
			if	( (pPFAddr + 24 > buf + wp)	//不足头部长度
					||	(pPFAddr + (packLen = pPfp->length + offsetof(PFPACKET, frmhdr)) > buf + wp)	//数据未读完
				)
			{
				memcpy(buf, pPfp, buf + wp - pPFAddr);
				wp -= pPFAddr - buf;
				pPfp = (PFPACKET*)buf;
				break;
			}

			PA_MutexLock(pRRData->hFileMutex);
			frpos = ftell(pRRData->fp);
			fseek(pRRData->fp, 0, SEEK_END);
			fwrite(&pPfp->frmhdr, 1, pPfp->length, pRRData->fp);
			fseek(pRRData->fp, frpos, 0);

			memcpy(&pRRData->fhdrLast, &(((PFPACKET*)pPFAddr)->frmhdr), sizeof(FRAMEHEADER));

			pRRData->tsMax = pPfp->frmhdr.ts;
			fwpos = pRRData->flen;
			pRRData->flen += pPfp->length;
			if(pPfp->frmhdr.strmType == RECORD_STREAM_VIDEO && (pPfp->frmhdr.flag & STREAMFLAG_KEYFRAME))
			{
				KEYFRAME_INDEX ki = { pPfp->frmhdr.ts, fwpos };
				pRRData->pvKfi->push_back(ki);
				dbg_msg("offset = %d, timeStamp = %d\n", ki.offset, ki.timeStamp);
			}
			PA_MutexUnlock(pRRData->hFileMutex);

			if(pRRData->fileInfo.dwDuration)
			{
				int progress = (1000*pRRData->tsMax) / pRRData->fileInfo.dwDuration + 1;
				if(progress != pRRData->iDnldProgress)
				{
					pRRData->iDnldProgress = progress;
					if(pRRData->cb_dwnld) 
						pRRData->cb_dwnld(1, progress, pRRData->tsMax, pRRData->cb_data);
				}
			}
			pPfp = (PFPACKET*)(pPFAddr + packLen);
		}
	}
	pRRData->bEof = TRUE;
	free(buf);
	return 0;
}

DWORD RemoteFile_GetFileInfo(FILEINFO *pFi, void *pData)
{
	RRDATA *pRrd = (RRDATA*)pData;
	memcpy(pFi, &pRrd->fileInfo, sizeof(FILEINFO));
	return 0;
}

DWORD RemoteFile_GetDownloadProgress(DWORD *ts, DWORD *len, void *pData)
{
	RRDATA *pRrd = (RRDATA*)pData;
	*ts = pRrd->fhdrLast.ts;
	*len = pRrd->flen;
	return 0;
}

DWORD RemoteFile_BeginReading(const char *sFileName, void **ppData)
{
	char sid[20];
	DWORD rlt;
	int flen;
	DVSCONN *pConn;
	REMOTEREADERPARAM *pRRP;
	RRDATA *pRRData;
	
	pRRP = *(REMOTEREADERPARAM**)ppData;
	pConn = pRRP->pConn;
	*ppData = NULL;
	rlt = pConn->CTPFileSession(sFileName, sid, &flen);
	if(rlt) return rlt;

	PA_SOCKET sock;
	sock = socket(AF_INET, SOCK_STREAM, 0);
	if(connect(sock, (const struct sockaddr*)&pConn->devAddr, sizeof(pConn->devAddr)))
	{
		PA_SocketClose(sock);
		return E_CTP_CONNECT;
	}

	pRRData = (RRDATA*)calloc(sizeof(RRDATA), 1);
	pRRData->pConn = pConn;
	pRRData->cb_dwnld = pRRP->cb_dwnld;
	pRRData->cb_data = pRRP->cb_data;
	strcpy(pRRData->sid, sid);
	pRRData->sock = sock;
	//pRRData->status = RRS_RUN;
	pRRData->fileInfo.nBitsPerSample = 16;
	pRRData->fileInfo.nSamplesPerSec = 8000;
	pRRData->fileInfo.nChannels = 1;
	pRRData->fileInfo.dwDuration = 99999000;	//A large value seems impossible
	pRRData->fileInfo.tmStart = 0;
	
	*ppData = pRRData;

	KEYVAL kv[] = {
		{ "nSamplesPerSec", KEYVALTYPE_INT, &pRRData->fileInfo.nSamplesPerSec },
		{ "nBitPerSample", KEYVALTYPE_INT, &pRRData->fileInfo.nBitsPerSample },
		{ "nChannels", KEYVALTYPE_INT, &pRRData->fileInfo.nChannels },
		{ "dwDuration", KEYVALTYPE_INT, &pRRData->fileInfo.dwDuration },
		{ "tmStart", KEYVALTYPE_INT, &pRRData->fileInfo.tmStart }
	};
	CString str;
	str.Format("-sessid %s\r\n\r\n", sid);
	if( (rlt = QueryCmd(sock, "playfile", str, kv, sizeof(kv)/sizeof(KEYVAL), PF_DONTINITVALS), NULL) )
	{
		PA_SocketClose(sock);
		free(pRRData);
		return rlt;
	}
	pRRData->fileInfo.tmEnd = pRRData->fileInfo.tmStart + pRRData->fileInfo.dwDuration/1000;

	char tmpPath[MAX_PATH];
	FILE *fp;
#ifdef WIN32
	GetTempPath(MAX_PATH, tmpPath);
	GetTempFileName(tmpPath, "TPF", 0, tmpPath);
#else
	strcpy(tmpPath, "download.tmp");
#endif
	fp = fopen(tmpPath, "w+b");
	if(!fp) 
	{
		PA_SocketClose(sock);
		free(pRRData);
		return E_CANNOTOPENFILE;
	}
	pRRData->fp = fp;
	strcpy(pRRData->sTmpFile, tmpPath);
	pRRData->pvKfi = new std::vector<KEYFRAME_INDEX>;
	PA_MutexInit(pRRData->hFileMutex);

	pRRData->hRcvThd = PA_ThreadCreate(RecvThread, pRRData);
	return E_OK;
}
DWORD RemoteFile_LookAhead(DWORD *streamType, DWORD *timeStamp, DWORD *flag, DWORD *size, void *data)
{
	RRDATA *pRRData = (RRDATA*)data;
	DWORD fpos;

	PA_MutexLock(pRRData->hFileMutex);
	fpos = ftell(pRRData->fp);
	if(pRRData->flen <= fpos+1)
	{
		PA_MutexUnlock(pRRData->hFileMutex);
		if(pRRData->status == RRS_RUN) 
			return E_WAITDATA;
		else 
			return E_EOF;
	}
	else if(pRRData->fhdrLA.strmType == 0)
	{
		if(fread(&pRRData->fhdrLA, sizeof(FRAMEHEADER), 1, pRRData->fp) != 1) 
		{
			PA_MutexUnlock(pRRData->hFileMutex);
			return E_EOF;
		}
	}
	*streamType = pRRData->fhdrLA.strmType;
	*timeStamp = pRRData->fhdrLA.ts;
	*flag = pRRData->fhdrLA.flag;
	*size = pRRData->fhdrLA.len;
	PA_MutexUnlock(pRRData->hFileMutex);
	return E_OK;
}

DWORD RemoteFile_Read(DWORD *streamType, DWORD *timeStamp, BYTE *buf, /*INOUT*/DWORD *len, DWORD *flag, void *data)
{
	RRDATA *pRRData = (RRDATA*)data;
	DWORD fpos;

	PA_MutexLock(pRRData->hFileMutex);
	fpos = ftell(pRRData->fp);
	if(pRRData->flen <= fpos+1)
	{
		PA_MutexUnlock(pRRData->hFileMutex);
		return E_WAITDATA;
	}

	FRAMEHEADER fhr, *phdr;
	if(pRRData->fhdrLA.strmType == 0)
	{
		fread(&fhr, sizeof(FRAMEHEADER), 1, pRRData->fp);
		phdr = &fhr;
	}
	else
		phdr = &pRRData->fhdrLA;
	if(*len < phdr->len)
	{
		*len = phdr->len;
		PA_MutexUnlock(pRRData->hFileMutex);
		return E_BUFFERTOOSMALL;
	}
	*streamType = phdr->strmType;
	*len = phdr->len;
	*timeStamp = phdr->ts;
	*flag = phdr->flag;
	fread(buf, 1, *len, pRRData->fp);
	pRRData->fhdrLA.strmType = 0;		//LookAhead缓冲无效
	PA_MutexUnlock(pRRData->hFileMutex);
	return 0;
}
DWORD RemoteFile_SeekKeyFrame(DWORD timeStamp, void *data)
{
	RRDATA *pRRData = (RRDATA*)data;
	DWORD rlt;

	PA_MutexLock(pRRData->hFileMutex);

	pRRData->fhdrLA.strmType = 0;		//LookAhead缓冲无效

	if(timeStamp > pRRData->tsMax)
	{
		KEYFRAME_INDEX &ki = pRRData->pvKfi->at(pRRData->pvKfi->size()-1);
		fseek(pRRData->fp, ki.offset, 0);
		rlt = ki.timeStamp;
	}
	else if(pRRData->pvKfi->size() == 0) 
	{
		rewind(pRRData->fp);
		rlt = 0;
	}
	else
	{
		int low, hi, mid;
		KEYFRAME_INDEX kfi;
		mid = low = 0; hi = pRRData->pvKfi->size() - 1;
		while(low < hi)
		{
			 mid = (low + hi)/2;
			 kfi = pRRData->pvKfi->at(mid);
			if(kfi.timeStamp > timeStamp) hi = mid - 1;
			else if(kfi.timeStamp < timeStamp) low = mid + 1;
			else break;
		}
		hi = pRRData->pvKfi->size() - 1;
		if(pRRData->pvKfi->at(mid).timeStamp < timeStamp) 
			for( ; mid < hi && pRRData->pvKfi->at(mid).timeStamp < timeStamp; mid++);
		for(; mid > 0; mid--)
		{
			kfi = pRRData->pvKfi->at(mid);
			if(kfi.timeStamp < timeStamp) 
				break;
		}
		
		fseek(pRRData->fp, pRRData->pvKfi->at(mid).offset, 0);
		rlt = pRRData->pvKfi->at(mid).timeStamp;
	}

	PA_MutexUnlock(pRRData->hFileMutex);
	return rlt;
}
void RemoteFile_EndReading(void *pData)
{
	RRDATA *pRRData = (RRDATA*)pData;
	CString str;
	str.Format("-sessid %s\r\n\r\n", pRRData->sid);
	pRRData->pConn->ExecCmd("stopfilesess", str);
	pRRData->status = RRS_STOP;
	PA_ThreadWaitUntilTerminate(pRRData->hRcvThd);
	PA_ThreadCloseHandle(pRRData->hRcvThd);
	PA_MutexUninit(pRRData->hFileMutex);
	//UninitFileSegs(&pRRData->fss);
	PA_SocketClose(pRRData->sock);
	fclose(pRRData->fp);
	unlink(pRRData->sTmpFile);
	delete pRRData->pvKfi;
	free(pData);
}

READER RemoteFile_Reader = {
	NULL,						//Probe
	RemoteFile_BeginReading,	
	RemoteFile_GetFileInfo,						//GetFileInfo
	RemoteFile_GetDownloadProgress,
	RemoteFile_LookAhead,						//LookAhead
	RemoteFile_Read,
	NULL,						//ReadTag
	RemoteFile_SeekKeyFrame,
	RemoteFile_EndReading
};

READER* GetDefaultRemoteReader()
{
	return &RemoteFile_Reader;
}
