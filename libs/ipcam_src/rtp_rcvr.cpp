/***********************************************************************************
* FileName: hi_rtp_rcvr.c
* Description: The receiver model for audio and vedio data.
***********************************************************************************/

#include "hi_rtp.h"
#include "hi_rtp_rcvr.h"
#include "DvsConn.h"
#include "TransferSession.h"
//#include <jni.h>

#define RTP_MAX_PACKET_BUFF 8192//4096

#define MAX_PACKET_SIZE	RTP_MAX_PACKET_BUFF

/* MULTICAST:  sockPair -- NULL
   UNICAST:    sockPair -- 
*/
int HI_RTP_Recv_Create(/*IO*/ RTP_RECV_S ** ppRtpStream, int sock, IN RTP_ON_RECV_CB _on_recv_fh,
                          int *ext_args )
{
	RTP_RECV_S* pRecv;
	int socket_opt_value = 1;

	pRecv = (RTP_RECV_S*)malloc(sizeof(RTP_RECV_S));

	memset(pRecv, 0 , sizeof(RTP_RECV_S));
	pRecv->rtp_on_recv = _on_recv_fh;
	pRecv->ext_args = ext_args;
	pRecv->runState    = RTP_RUNSTATE_STOP;

	pRecv->sock = sock;

	socket_opt_value = 0x80000;
	if (setsockopt(pRecv->sock, SOL_SOCKET, SO_RCVBUF, (const char *)(&socket_opt_value) ,sizeof(int)) == -1) 
	{

	}

	*ppRtpStream = pRecv;		

	return 0;
}

static void RTP_RecvTcp(RTP_RECV_S *pRtpStream)
{
	BYTE RecvBuff[MAX_PACKET_SIZE];
	int nByteRecv, nExistedData = 0;
	MEDIA_HDR_S *pMdHdr;
	UINT waitCnt = 0;
	CTransferSession *pCS = (CTransferSession*)pRtpStream->ext_args;
#ifdef _DEBUG
	WORD seqno = 0;
#endif

	pRtpStream->runState = RTP_RUNSTATE_RUNNING ;
	while(pRtpStream->runState == RTP_RUNSTATE_RUNNING)
	{
		pMdHdr = (MEDIA_HDR_S*)RecvBuff;
		/* 
		 * RECEIVE DATA UNTIL AT LEAST A SLICE IS RECEIVED
		 */
		while(pRtpStream->runState == RTP_RUNSTATE_RUNNING)
		{
			if(nExistedData < sizeof(MEDIA_HDR_S) || nExistedData < sizeof(MEDIA_HDR_S) + ntohs(pMdHdr->len))
			{
				if(nExistedData == MAX_PACKET_SIZE)	//Buffer is full. Seems corrupted, discard all data
				{
					dbg_msg("Seems something wrong. clear buffer\n");
					nExistedData = 0;
					continue;
				}

				struct timeval tv;
				fd_set readfds, efds;

				tv.tv_sec = 0;
				tv.tv_usec = 500000;
				FD_ZERO(&readfds);
				FD_SET(pRtpStream->sock, &readfds);
				efds = readfds;
				if(select(pRtpStream->sock + 1, &readfds, NULL, &efds, &tv) <= 0)
				{
					waitCnt++;
					if((waitCnt & 1) == 0) pCS->m_dwIdleSec++;
					if(waitCnt >= 10)
					{
						LibNotify(SESSEVENT_NO_DATA, pRtpStream->ext_args, 0);
						waitCnt = 0;
					}
					continue;
				}

				nByteRecv = recv(pRtpStream->sock, (char*)RecvBuff + nExistedData, MAX_PACKET_SIZE - nExistedData, 0);
				if(nByteRecv <= 0)
				{
					//pRtpStream->runState = RTP_RUNSTATE_STOP;	//??
					//break;
					PA_Sleep(500);
					waitCnt++;
					if((waitCnt & 1) == 0) pCS->m_dwIdleSec++;
					if(waitCnt >= 10)
					{
						LibNotify(SESSEVENT_NO_DATA, pRtpStream->ext_args, 0);
						waitCnt = 0;
					}
					continue;
				}
				else
				{
					waitCnt = 0;
					pCS->m_dwIdleSec = 0;
					nExistedData += nByteRecv;
				}
			}
			else
				break;
		}


		/*
		 * PROCESS SLICE(s) IN BUFFER.
		 * THERE MAY BE MORE THAN ONE SLICE IN BUFFER
		 */
		while(pRtpStream->runState == RTP_RUNSTATE_RUNNING)
		{
			if( pMdHdr->syncByte1 == 0xAA && pMdHdr->syncByte2 == 0x55 && (pMdHdr->pt == RTP_PT_H264 || pMdHdr->pt == RTP_PT_ALAW || pMdHdr->pt == RTP_PT_G726))
			{
#ifdef _DEBUG
				if(ntohs(pMdHdr->seqno) != seqno + 1)
					if(ntohs(pMdHdr->seqno) == seqno+2) dbg_msg("Packet %d lost\n", seqno+1);
					else dbg_msg("Packet %d ~ %d lost\n", seqno+1, ntohs(pMdHdr->seqno)-1);
				seqno = ntohs(pMdHdr->seqno);
#endif
				UINT packLen = sizeof(MEDIA_HDR_S) + ntohs(pMdHdr->len);

				if(pRtpStream->rtp_on_recv)	pRtpStream->rtp_on_recv((unsigned char*)pMdHdr, packLen, pRtpStream->ext_args);

				nExistedData -= packLen;
				pMdHdr = (MEDIA_HDR_S*)((char*)pMdHdr + packLen);

				//Partial packet
				if(nExistedData < sizeof(MEDIA_HDR_S) || nExistedData < sizeof(MEDIA_HDR_S) + ntohs(pMdHdr->len))
				{
					memcpy(RecvBuff, pMdHdr, nExistedData);
					break;
				}
			}
			else	//重新定位到包头
			{
				dbg_msg("Corrupt package, re-locate the header\n");
				unsigned char *p = (unsigned char*)pMdHdr;
				unsigned char *rear = p + nExistedData - 3;
				for(; p < rear; p++)
				{
					if(*p == 0xAA && *(p+1) == 0x55 && (*(p+2) == RTP_PT_H264 || *(p+2) == RTP_PT_ALAW || *(p+2) == RTP_PT_G726) ) 
						break;
				}

				nExistedData -= (p - (unsigned char*)pMdHdr);
				pMdHdr = (MEDIA_HDR_S*)p;
				if(nExistedData < sizeof(MEDIA_HDR_S) || nExistedData < sizeof(MEDIA_HDR_S) + ntohs(pMdHdr->len))
				{
					memcpy(RecvBuff, pMdHdr, nExistedData);
					break;
				}
			}
		}
		//if(nExistedData) dbg_msg("Partial packet. nExistedData = %d\n", nExistedData);
	}//while
}

static void WaitForFirstPacket(RTP_RECV_S *pRtpStream)
{
	fd_set readfds;
	struct timeval tv;
	int nRet, waitCnt;
	CTransferSession *pCS = (CTransferSession*)pRtpStream->ext_args;

	//Hole punching
	waitCnt = 0;
	while(pRtpStream->runState == RTP_RUNSTATE_WAIT)
	{
		tv.tv_sec = 1;
		tv.tv_usec = 0;
		FD_ZERO(&readfds);
		FD_SET(pRtpStream->sock, &readfds);
		nRet = select(pRtpStream->sock + 1, &readfds, NULL, NULL, &tv);
		if(nRet > 0) break; 
		if(nRet == 0) 
		{ 	//Hole punching
			if(pRtpStream->sPunch) 
				sendto(pRtpStream->sock, pRtpStream->sPunch, strlen(pRtpStream->sPunch), 0, 
						(struct sockaddr*)&pRtpStream->sa_punch, sizeof(pRtpStream->sa_punch));

			//等待记时
			waitCnt++;
			if((waitCnt & 1) == 0) pCS->m_dwIdleSec++;
			if(waitCnt > 10) 
			{
				LibNotify(SESSEVENT_NO_DATA, pRtpStream->ext_args, 0);
				waitCnt = 0;
				//pRtpStream->runState = RTP_RUNSTATE_STOP;
			}
		}
	}
	if(pRtpStream->sPunch) { free(pRtpStream->sPunch); pRtpStream->sPunch = NULL; }
	pCS->m_dwIdleSec = 0;
}

static int DistOfSeqNo(int earlier, int later)
{
	int dist = later - earlier;
	if(dist < 0 && dist < -65335)
	{
		dist += 0x10000;
	}
	return dist;
}

//------------------------------------------------------------------------------
//
// Memory buffer used for re-order UDP packets
//

#define MAX_BUFFERED_PACKETS 10
#define BLOCK_SIZE	RTP_MAX_PACKET_BUFF

typedef
struct _tagMEMBLOCK {
	unsigned int used;
	unsigned int seq;
	unsigned int dataLen;
	unsigned char buff[BLOCK_SIZE];
} MEMBLOCK;

typedef struct _tagMemPool {
	BOOL flowed;
	int first, last;
	WORD expectedSeq;
	MEMBLOCK *index[MAX_BUFFERED_PACKETS];

	MEMBLOCK memBlock[MAX_BUFFERED_PACKETS];
} MEMPOOL;

MEMPOOL* MemPoolAlloc()
{
	MEMPOOL *pool = (MEMPOOL*)malloc(sizeof(MEMPOOL));
	if(!pool) return NULL;

	pool->first = pool->last = 0;
	pool->flowed = FALSE;
	memset(pool->index, 0, sizeof(pool->index));
	for(int i=0; i<MAX_BUFFERED_PACKETS; i++)
	{
		pool->memBlock[i].used = FALSE;
	}
	return pool;
}

void MemPoolFree(MEMPOOL *pPool)
{
	free(pPool);
}

MEMBLOCK* MemPoolGetFreeMemBlock(MEMPOOL* pPool)
{
	for(int i=0; i<MAX_BUFFERED_PACKETS; i++)
	{
		if(!pPool->memBlock[i].used)
		{
			return &pPool->memBlock[i];
		}
	}
	return NULL;
}

/// seq must be set before
/// return: 1 - queued
///			0 - not queued as the buffer is full
///			-1 - discard the packets dumplicated or arrived too late
int MemPoolQueueMemBlock(MEMPOOL* pPool, MEMBLOCK* pBlock)
{
	int seqdist;

	if(!pPool->flowed)
	{
		//第一个包先缓冲一下
		if(pPool->index[0] == NULL)	//first packet
		{
			pPool->index[0] = pBlock;
			pPool->last = 1;
			pPool->expectedSeq = pBlock->seq + 1;
			pBlock->used = TRUE;
			return TRUE;
		}
		else	//second packet
		{
			pPool->flowed = TRUE;
			seqdist = DistOfSeqNo(pPool->index[0]->seq, pBlock->seq);

			//第1、2个包乱序
			if(seqdist == -1)
			{
				pPool->index[1] = pPool->index[0];
				pPool->index[0] = pBlock;
				pPool->last = 2;
				pBlock->used = TRUE;
			}
			//如果第二个包与第一个包的SEQ差超出缓冲，则将第1个包丢掉
			else if(seqdist > MAX_BUFFERED_PACKETS-1)
			{
				pPool->index[0]->used = FALSE;
				pPool->index[0] = pBlock;
				pBlock->used = TRUE;
				pPool->flowed = FALSE;
				pPool->expectedSeq = pBlock->seq + 1;
			}
			else if(seqdist > 0) //缓存第2个包
			{
				pPool->index[seqdist] = pBlock;
				pBlock->used = TRUE;
				pPool->last = seqdist+1;
				if(seqdist == 1) pPool->expectedSeq++;
			}
			else
				return -1;

			return 1;
		}
	}
	else

	//应该已经调用MemPoolGetDataMemBlock 取走最前面的包, pPool->index[0] 为 NULL
	{
		seqdist = DistOfSeqNo(pPool->expectedSeq, pBlock->seq);
		if(seqdist < 0) return -1; //dumplicated packet
		if(seqdist >= MAX_BUFFERED_PACKETS)
		{
			int i, n=0;
			while(!pPool->index[n] && n < pPool->last) 
			{
				n++;
				pPool->expectedSeq++;
			}

			if(n)
			{
				for(i=n; i<pPool->last && pPool->index[i]; i++) pPool->expectedSeq++;
				for(i=n; i<pPool->last; i++)
				{
					pPool->index[i-n] = pPool->index[i];
					pPool->index[i] = NULL;
				}
				pPool->last -= n;
				return 0;
			}
			else //断网，丢了很多包
			{
				dbg_msg("Packet %d ~ %d lost\n", pPool->expectedSeq, pBlock->seq-1);
				pPool->index[0] = pBlock;
				pBlock->used = TRUE;
				pPool->expectedSeq = pBlock->seq + 1;
				pPool->first = 0;
				pPool->last = 1;
				pPool->flowed = FALSE;
				return 1;
			}
		}
		else if(!pPool->index[seqdist])
		{
			pPool->index[seqdist] = pBlock;
			pBlock->used = TRUE;
			if(seqdist+1 > pPool->last) pPool->last = seqdist+1;
			if(seqdist == 0)
			{
				int i = 0;
				while(pPool->index[i] && i < pPool->last)
				{
					pPool->expectedSeq++;
					i++;
				}
			}
			return 1;
		}
		else
			return -1;
	}
}

UINT MemPoolGetDataMemBlock(MEMPOOL* pPool, MEMBLOCK *ppMemBlks[MAX_BUFFERED_PACKETS])
{
	if(!pPool->flowed || !pPool->index[0])
		return 0;

	int i, n = 0;

	while(pPool->index[n] && n < pPool->last) 
	{
		ppMemBlks[n] = pPool->index[n];
		pPool->index[n]->used = FALSE;
		pPool->index[n] = NULL;
		n++;
		//pPool->expectedSeq++;
	}

	for(i=n; i < pPool->last; i++)
	{
		pPool->index[i-n] = pPool->index[i];
		pPool->index[i] = NULL;
	}
	pPool->last -= n;
	return n;
}
//------------------------------------------------------------------------------

void RTP_RecvUdp(RTP_RECV_S *pRtpStream)
{    
	struct sockaddr_in from;
	int from_len, nRet;
	fd_set readfds;
	struct timeval tv;
	unsigned long tick1, tick2;
	UINT waitCnt;
	CTransferSession *pCS = (CTransferSession*)pRtpStream->ext_args;
	MEMPOOL *pMemPool = MemPoolAlloc();
	MEMBLOCK *pMemBlk = MemPoolGetFreeMemBlock(pMemPool);

	WORD	expectedSeqNo;

	pRtpStream->runState = RTP_RUNSTATE_WAIT ;
	WaitForFirstPacket(pRtpStream);
	if(pRtpStream->runState == RTP_RUNSTATE_STOP) return;


	tick1 = PA_GetTickCount();
	waitCnt = 0;
	while(pRtpStream->runState != RTP_RUNSTATE_STOP)
	{
		/* Do receiving data */
		/* Keep NAT alive. */

		tick2 = PA_GetTickCount();
		if(tick2 - tick1 > 30000)	//30'
		{
			//Should send RTCP packet here
			sendto(pRtpStream->sock, "", 1, 0, (struct sockaddr*)&from, sizeof(from));
			tick1 = tick2;
		}

		/* Count the time of data break. Send SESSEVENT_NO_DATA notification if breaked more than 5" */
		tv.tv_sec = 0;
		tv.tv_usec = 500000;
		FD_ZERO(&readfds);
		FD_SET(pRtpStream->sock, &readfds);
		nRet = select(pRtpStream->sock + 1, &readfds, NULL, NULL, &tv);			
		if(nRet <= 0) 
		{ 
			waitCnt++; 
			if((waitCnt & 1) == 0) pCS->m_dwIdleSec++;
			if(waitCnt >= 10)
			{
				LibNotify(SESSEVENT_NO_DATA, pRtpStream->ext_args, 0);
			}

			++pRtpStream->stats.timeout_cnt; 
			continue; 
		}

		waitCnt = 0;
		pCS->m_dwIdleSec = 0;
		from_len = sizeof(from);
		pMemBlk = MemPoolGetFreeMemBlock(pMemPool);	//Never return NULL here.
		pMemBlk->dataLen = PA_RecvFrom(pRtpStream->sock, (char*)pMemBlk->buff, MAX_PACKET_SIZE, 0, (struct sockaddr*)&from, &from_len);


		if (pMemBlk->dataLen > 0 )
		{
			RTP_HDR_S *pHdr;
			int rlt, nMemBlk;
			MEMBLOCK *pMemBlks[MAX_BUFFERED_PACKETS];

			pHdr = (RTP_HDR_S*)pMemBlk->buff;
			pMemBlk->seq = ntohs(pHdr->seqno);

			/* 多播源过滤 */
			if(pRtpStream->srcfilter && from.sin_addr.s_addr != pRtpStream->mcgrpip)
				continue;

requeue:
			rlt = MemPoolQueueMemBlock(pMemPool, pMemBlk);
			if(rlt < 0) continue;
			nMemBlk = MemPoolGetDataMemBlock(pMemPool, pMemBlks);
			//dbg_msg("MemPoolGetDataMemBlock: %d\n", nMemBlk);
			if(nMemBlk == 0) continue;

			if(pRtpStream->runState == RTP_RUNSTATE_WAIT)
			{
				expectedSeqNo = pMemBlks[0]->seq;
				pRtpStream->runState = RTP_RUNSTATE_RUNNING;
			}

			pRtpStream->stats.discarded += pMemBlks[0]->seq - expectedSeqNo;

			for(int i=0; i<nMemBlk; i++)
			{
				//dbg_msg("process seq: %d\n", pMemBlks[i]->seq);
				/* Stat recved bytes */
				pRtpStream->stats.recv_byte += pMemBlks[i]->dataLen;
				pRtpStream->stats.recv_packet++;

				if(pRtpStream->rtp_on_recv)
					pRtpStream->rtp_on_recv(pMemBlks[i]->buff, pMemBlks[i]->dataLen, pRtpStream->ext_args);
			}
			expectedSeqNo = pMemBlks[nMemBlk-1]->seq + 1;

			if(pRtpStream->stats.recv_packet > 100 && 
					10 * pRtpStream->stats.discarded > pRtpStream->stats.recv_packet)
			{
				/* TODO: 如果丢包太多(10%), 请求降低码流 */
				memset(&pRtpStream->stats, 0, sizeof(pRtpStream->stats));
			}

			if(rlt == 0) goto requeue;
		} //if(pMemBlk->dataLen > 0)
	}//while(!...STOP)
	MemPoolFree(pMemPool);
}

void *__STDCALL HI_RTP_RecvHandle(void* args)
{
    RTP_RECV_S *pRtpStream  = (RTP_RECV_S*)args;
    CTransferSession *pCS = (CTransferSession*)pRtpStream->ext_args;
    UINT strm = pRtpStream->st;
  pCS->ReceiverThreadBeginning(strm);
	if(pRtpStream->transType == RTP_TRANSPORT_TCP)
	{
		RTP_RecvTcp(pRtpStream);
	}
	else //if(pRtpStream->transType == RTP_TRANSPORT_UDP)
	{
		RTP_RecvUdp(pRtpStream);
	}//if(... UDP) 
	pCS->ReceiverThreadEnding(strm);
    return NULL;
}

/* 开启接收服务 */                  
int HI_RTP_Recv_Start(IN RTP_RECV_S * pRtpStream)
{
	pRtpStream->thd = PA_ThreadCreate(HI_RTP_RecvHandle, pRtpStream);
	return 0;
}


int HI_RTP_Recv_Stop(/*IO*/ RTP_RECV_S * pRtpStream)
{
	if (pRtpStream)
	{
		pRtpStream->runState = RTP_RUNSTATE_STOP ;
		PA_ThreadWaitUntilTerminate(pRtpStream->thd);    
		PA_ThreadCloseHandle(pRtpStream->thd);
	}
	return 0;
}

int HI_RTP_Recv_Destroy(/*IO*/ RTP_RECV_S *pRtpStream)
{
	if (pRtpStream == NULL)
	{
		return 0;
	}

	if (pRtpStream->runState != RTP_RUNSTATE_STOP)
	{
		HI_RTP_Recv_Stop(pRtpStream);
	}
	PA_SocketClose(pRtpStream->sock);

	return 0;

}
