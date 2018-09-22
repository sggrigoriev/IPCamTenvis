/***********************************************************************************
*              Copyright 2006 - 2006, Hisilicon Tech. Co., Ltd.
*                           ALL RIGHTS RESERVED
* FileName: hi_rtp.h
* Description: The RTP module.
*
* History:
* Version   Date         Author     DefectNum    Description
* 1.1       2006-05-10   T41030     NULL         Create this file.
***********************************************************************************/

#ifndef __HI_RTP_H__
#define __HI_RTP_H__

#ifdef __cplusplus
#if __cplusplus
extern "C"{
#endif
#endif /* __cplusplus */

#include "platform_adpt.h"

/*
#define HI_ERR_RTP_INVALID_PARA 1
#define HI_ERR_RTP_SOCKET       2
#define HI_ERR_RTP_SEND         3
#define HI_ERR_RTP_REACHMAX     4
#define HI_ERR_RTP_NOT_ENOUGHMEM 5
#define HI_CHECK_TCPIP_PORT(port) ((port > 0 ) && (port < 35536))
*/

#define H264_STARTCODE_LEN      4 /* 00 00 00 01 */

#define SET_BITFIELD(field, val, mask, shift) \
   do { \
     (field) &= ~(mask); \
     (field) |= (((val) << (shift)) & (mask)); \
   } while (0)


#define RTP_VERSION    2

/*RTP Payload type define*/
typedef enum hiRTP_PT_E
{
    RTP_PT_ULAW             = 0,        /* mu-law */
    RTP_PT_GSM              = 3,        /* GSM */
    RTP_PT_G723             = 4,        /* G.723 */
    RTP_PT_ALAW             = 8,        /* a-law */
    RTP_PT_G722             = 9,        /* G.722 */
    RTP_PT_S16BE_STEREO     = 10,       /* linear 16, 44.1khz, 2 channel */
    RTP_PT_S16BE_MONO       = 11,       /* linear 16, 44.1khz, 1 channel */
    RTP_PT_MPEGAUDIO        = 14,       /* mpeg audio */
    RTP_PT_JPEG             = 26,       /* jpeg */
    RTP_PT_H261             = 31,       /* h.261 */
    RTP_PT_MPEGVIDEO        = 32,       /* mpeg video */
    RTP_PT_MPEG2TS          = 33,       /* mpeg2 TS stream */
    RTP_PT_H263             = 34,       /* old H263 encapsulation */
                            
    //RTP_PT_PRIVATE          = 96,       
    RTP_PT_H264             = 96,       /* hisilicon define as h.264 */
    RTP_PT_G726             = 97,       /* hisilicon define as G.726 */
    RTP_PT_ADPCM            = 98,       /* hisilicon define as ADPCM */

    RTP_PT_INVALID          = 127
}RTP_PT_E;



/* op-codes */
#define RTP_OP_PACKETFLAGS  1               /* opcode datalength = 1 */

#define RTP_OP_CODE_DATA_LENGTH     1

/* flags for opcode RTP_OP_PACKETFLAGS */
#define RTP_FLAG_LASTPACKET 0x00000001      /* last packet in stream */
#define RTP_FLAG_KEYFRAME   0x00000002      /* keyframe packet */

#ifndef BYTE_ORDER
#define BYTE_ORDER LITTLE_ENDIAN
#endif

/* total 12Bytes */
typedef struct hiRTP_HDR_S
{

#if (BYTE_ORDER == LITTLE_ENDIAN)
    /* byte 0 */
    WORD cc      :4;   /* CSRC count */
    WORD x       :1;   /* header extension flag */
    WORD p       :1;   /* padding flag */
    WORD version :2;   /* protocol version */

    /* byte 1 */
    WORD pt      :7;   /* payload type */
    WORD marker  :1;   /* marker bit */
#elif (BYTE_ORDER == BIG_ENDIAN)
    /* byte 0 */
    WORD version :2;   /* protocol version */
    WORD p       :1;   /* padding flag */
    WORD x       :1;   /* header extension flag */
    WORD cc      :4;   /* CSRC count */
    /*byte 1*/
    WORD marker  :1;   /* marker bit */
    WORD pt      :7;   /* payload type *//
#else
    #error YOU MUST DEFINE BYTE_ORDER == LITTLE_ENDIAN OR BIG_ENDIAN !  
#endif


    /* bytes 2, 3 */
    WORD seqno  :16;   /* sequence number */

    /* bytes 4-7 */
    UINT ts;            /* timestamp in ms */
  
    /* bytes 8-11 */
    UINT ssrc;          /* synchronization source */
} RTP_HDR_S;

#define RTP_HDR_SET_VERSION(pHDR, val)  ((pHDR)->version = val)
#define RTP_HDR_SET_P(pHDR, val)        ((pHDR)->p       = val)
#define RTP_HDR_SET_X(pHDR, val)        ((pHDR)->x       = val) 
#define RTP_HDR_SET_CC(pHDR, val)       ((pHDR)->cc      = val)

#define RTP_HDR_SET_M(pHDR, val)        ((pHDR)->marker  = val)
#define RTP_HDR_SET_PT(pHDR, val)       ((pHDR)->pt      = val)

#define RTP_HDR_SET_SEQNO(pHDR, _sn)    ((pHDR)->seqno  = (_sn))
#define RTP_HDR_SET_TS(pHDR, _ts)       ((pHDR)->ts     = (_ts))

#define RTP_HDR_SET_SSRC(pHDR, _ssrc)    ((pHDR)->ssrc  = _ssrc)

#define RTP_HDR_LEN  sizeof(RTP_HDR_S)

#define RTP_DATA_MAX_LENGTH 2048

/*RTP打包类型*/
typedef enum hiRTP_PACKET_TYPE
{
    RTP_PACKET_TYPE_DEFAULT = 0,
    RTP_PACKET_TYPE_STAP = 1,
    RTP_PACKET_TYPE_BUTT
}RTP_PACKET_TYPE_E;

#define STAP_HDR_LEN 3
typedef struct hiSTAP_RTPPLAYLOAD
{
    BYTE header;
    BYTE nal_size[2]; 
    BYTE data[RTP_DATA_MAX_LENGTH];
}STAP_RTPPLAYLOAD_S;


/* RTP Header Extension  */
typedef struct hiRTP_HDR_EXT_S
{
    WORD ext_type;         /* defined by profile */
    WORD len;              /* extension length in 32-bit word */
} RTP_HDR_EXT_S;


typedef struct RTPOpCode
{
    WORD op_code;                /* RTP_OP_PACKETFLAGS */
    WORD op_code_data_length;    /* RTP_OP_CODE_DATA_LENGTH */
    UINT op_code_data [RTP_OP_CODE_DATA_LENGTH];
} RTP_OPCODE_S;


typedef struct hiRTP_DATA_S
{
    UINT     data_length;
    BYTE      data [RTP_DATA_MAX_LENGTH];/* actual length determined by data_length */
} RTP_DATA_S;

typedef enum hiRTP_STREAM_TYPE_E
{
    RTP_STREAM_TYPE_SND     = 0,
    RTP_STREAM_TYPE_RCV     = 1,
    RTP_STREAM_TYPE_SNDRCV  = 2
}RTP_STREAM_TYPE_E;


typedef enum hiRTP_RUNSTATE_E
{
    RTP_RUNSTATE_STOP    = 0,
	RTP_RUNSTATE_WAIT	 = 1,
    RTP_RUNSTATE_RUNNING = 2
}RTP_RUNSTATE_E;



typedef enum hiRTP_TRANSPORT_TYPE_E
{
	RTP_TRANSPORT_UDP = 0,
	RTP_TRANSPORT_TCP,
	RTP_TRANSPORT_MULTICAST,
	RTP_TRANSPORT_BUTT
} RTP_TRANSPORT_TYPE_E;


typedef enum hiPACK_TYPE_E
{
    PACK_TYPE_RAW = 0, /*海思媒体包头类型MEDIA_HDR_S，头长8个字节，用于TCP传输*/
    PACK_TYPE_RTP,     /*普通RTP打包方式，头是12个字节*/
    PACK_TYPE_RTP_STAP,/* STAP-A打包方式，加了3个字节的净荷头，头是15个字节*/
    PACK_TYPE_BUTT     
} PACK_TYPE_E;

typedef enum hiAV_TYPE_E
{
    AV_TYPE_VIDEO = 0,
    AV_TYPE_AUDIO,
    AV_TYPE_AV,
    AV_TYPE_BUTT
} AV_TYPE_E;

#pragma pack(push, 1)
typedef struct hiMEDIA_HDR_S        
{                                                                                                
    /* word 0 */ 
	BYTE  syncByte1;	//同步字节(0xAA, 0x55)
	BYTE  syncByte2;
	BYTE	pt;
    BYTE  marker:1;
    BYTE  avType:7;

	/* word 1 */
    WORD seqno;
    WORD len;             
                            
    /* word 1 */          
    UINT pts;             
} MEDIA_HDR_S; 
#pragma pack(pop)

typedef struct hiRTP_STATS_S
{
    UINT64  sent_packet;    /* number of packets send */
    UINT64  sent_byte;      /* bytes sent */
    UINT64  sent_error;     /* error times when send */
    UINT64  recv_packet;    /* number of packets received */
    UINT64  recv_byte;      /* bytes of payload received */
    UINT64  unavaillable;   /* packets not availlable when they were queried */
    UINT64  bad;            /* packets that did not appear to be RTP */
    UINT64  discarded;      /* incoming packets discarded because the queue exceeds its max size */
    UINT64  timeout_cnt;
} RTP_STATS_S;

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* __cplusplus */

#endif
