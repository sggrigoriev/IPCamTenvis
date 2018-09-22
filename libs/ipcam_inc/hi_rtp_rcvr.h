#ifndef __hi_rtp_rcvr_h__
#define __hi_rtp_rcvr_h__

#include "platform_adpt.h"
#include "hi_rtp.h"

struct hiRTP_RECV_S;
typedef  int (__STDCALL *RTP_ON_RECV_CB)(unsigned char *pBuf,  unsigned int size, void *pParam); 

typedef struct hiRTP_RECV_S
{
	PA_HTHREAD	thd;		/* thread to recv cmd */

	PA_SOCKET       sock;       /*send/recv socket*/

	UINT		st;		//RECORD_STREAM_xxx
	RTP_PT_E	pt;         /*payload type*/

	WORD              last_sn;    /*last recv sn*/
	UINT              last_ts;    /*last recv ts*/

	struct sockaddr_in	sa_punch;	//打洞包和 "Keep NAT-map Alive" 包的发送目标
	char				*sPunch;	//在WAIT状态发送的打洞包, 在创建时由调用者设置

	RTP_ON_RECV_CB      rtp_on_recv;

	volatile int/*RTP_RUNSTATE_E*/      runState;
	/*volatile*/ RTP_TRANSPORT_TYPE_E transType;

	unsigned long mcgrpip;	//SongZC, 20061129
	BOOL 	srcfilter;	//if not source specific multicast, filter packet manually

	char 		sessId[20];	//

	RTP_STATS_S	stats;

	int                 *ext_args;
} RTP_RECV_S;

int HI_RTP_Recv_Create(/*IO*/ RTP_RECV_S ** ppRtpStream, int sock/*接收数据套接字*/, 
						  IN RTP_ON_RECV_CB _on_recv_fh, int *ext_args);

int HI_RTP_Recv_Start(IN RTP_RECV_S * pRtpStream);

int HI_RTP_Recv_Stop(/*IO*/ RTP_RECV_S * pRtpStream);

int HI_RTP_Recv_Destroy(/*IO*/ RTP_RECV_S *pRtpStream);


#endif
