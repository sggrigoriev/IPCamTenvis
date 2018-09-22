#include "platform_adpt.h"
#include <DvsConn.h>
#include <md5.h>
#include <errdefs.h>

//////////////////////////////////////////////////////////////////////////////////////////////
////////#///#/////////////#///////////////////////////////////////////////////////////////////
////////#///#/////////////#////////////#//////####////////////////////////////////////////////
////////#///#//####////####///####///######//#////#///////////////////////////////////////////
////////#///#//#///#//#///#//#///#/////#/////######///////////////////////////////////////////
////////#///#//#///#//#///#//#///#/////#/////#////////////////////////////////////////////////
/////////###///####////####///###/#/////##////####////////////////////////////////////////////
///////////////#//////////////////////////////////////////////////////////////////////////////
///////////////#//////////////////////////////////////////////////////////////////////////////
///////////////#//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////

static int GetReply(PA_SOCKET sk, CString& strReply);
static int SendPacket(PA_SOCKET sk, const char *buf, int len);
static int SendFile(PA_SOCKET sk, FILE *fp, UPDATECALLBACK pFunc, void* data);

#pragma pack(push, 1)
typedef struct _PackedFileHeader2 {
	unsigned char	tag[4];	//"PK2"
	unsigned int	product;		//target device type
	DWORD			pubtime;
	unsigned char	ver[4];
	unsigned char	tver1[4], tver2[4];
	unsigned int	fnum;
} PACKEDFILEHEADER;

typedef struct _subhdr {
	BYTE tag[4];
	BYTE md5[16];
	DWORD len;
} SUBHEADER;
#pragma pack(pop)

static const char *sProduct[] = { 
	"IPCAM/1-Channel DVS with TVP5150",
	"4-Channel DVS",
	"4-Channel DVR",
	"4-Channel DVS with TW2835"
};
#define XORSTRSIZE	8
static const unsigned char xorstr[] = { 0xa1, 0x83, 0x24, 0x78, 0xb3, 0x41, 0x43, 0x56 };
//RETURN: <  0:  error
//	      >=  0:  product id
int VerifyFile(const char *sfile, CString& product, CString& ver, CString& pub)
{
	int rlt = -1;
	PACKEDFILEHEADER fhdr;
	FILE *fp;
	time_t pubtime;
	
	fp = fopen(sfile, "rb");
	if(!fp) { return rlt; }

	fread(&fhdr, sizeof(fhdr), 1, fp);
	if(memcmp(fhdr.tag, "PK2", 4)) { fclose(fp); return -1; }
	pubtime = fhdr.pubtime;
	struct tm *ptm = localtime(&pubtime);
	pub.Format("%d-%02d-%02d %02d:%02d:%02d", ptm->tm_year+1900, ptm->tm_mon+1, ptm->tm_mday, ptm->tm_hour, ptm->tm_min, ptm->tm_sec);
	switch(fhdr.product & 0x0000FFFF)
	{
	case 0: product = "Hi3510"; break;
	case 3512: product = "Hi3512"; break;
	case 365: product = "DM365"; break;
	case 8126: product = "GM8126"; break;
	case 3518: product = "Hi3518"; break;
	default: fclose(fp); return 0;
	}
	ver.Format("%d.%d.%d.%d", fhdr.ver[0], fhdr.ver[1], fhdr.ver[2], fhdr.ver[3]);


	MD5_CTX md5ctx;
	BYTE buf[4000], digest[16];
	SUBHEADER subhdr;

	for(int i=0; i<fhdr.fnum; i++)
	{
		MD5Init(&md5ctx);
		fread(&subhdr, sizeof(subhdr), 1, fp);
		if(memcmp(subhdr.tag, "FILE", 4) == 0 || memcmp(subhdr.tag, "CMD", 4) == 0)
		{
			int rlen, total;
			rlt = 0;
			if(memcmp(subhdr.tag, "FILE", 4) == 0)
			{
				int fnlen, flen;
				fread(&fnlen, 1, sizeof(int), fp);
				fread(buf + sizeof(int), 1, fnlen, fp);
				fread(&flen, 1, sizeof(int), fp);

				if(fnlen + flen + 2*sizeof(int) != subhdr.len) { rlt = -1; break; }
				total = flen;

				memcpy(buf, &fnlen, sizeof(int));
				memcpy(buf + sizeof(int) + fnlen, &flen, sizeof(int));
				MD5Update(&md5ctx, buf, 2*sizeof(int) + fnlen);
			}
			else
				total  = subhdr.len;

			while(total > 0)
			{
				rlen = total > 4000 ? 4000 : total;
				rlen = fread(buf, 1, rlen, fp);
				if(rlen <= 0) break;
				for(int k=0; k<rlen; k++)
					buf[k] ^= xorstr[k%XORSTRSIZE];
				MD5Update(&md5ctx, buf, rlen);
				total -= rlen;
			}
			if(total == 0)
			{
				MD5Final(digest, &md5ctx);
				if(memcmp(digest, subhdr.md5, 16)) rlt = -1;
			}
			if(rlt < 0) break;
		}
		else
			continue;
	}

	fclose(fp);
	if(rlt < 0) return rlt;
	else return fhdr.product;
}

//Return:	0 - OK;
#define INVALID_RESPONSE	-3
#define REMOTEHOST_HASNO_RESPONSE	-2
#define REMOTEHOST_CLOSED	-4
#define SENDDATA_ERROR	-5
#define CANNOT_CONNECT	-6
#define RESPONSE_MESSAGE	1
#define RESPONSE_OK			0
int GetReply(PA_SOCKET sk, CString& strReply)
{
	int len;
	char buf[200];
	
	fd_set rdfs;
	struct timeval tv;
retry:
	FD_ZERO(&rdfs);
	FD_SET(sk, &rdfs);
	tv.tv_sec = 8;	//DVS端写入文件时可能花较长时间
	tv.tv_usec = 0;
	if(select(sk+1, &rdfs, NULL, NULL, &tv) == 0)
	{
		return REMOTEHOST_HASNO_RESPONSE;
	}
	else
	if( ( len = recv(sk, buf, 199, 0) ) > 0)
	{
		buf[len] = '\0';
		char *p = buf;
parse:
		if(strncmp(p, "REPLY ", 6)) return INVALID_RESPONSE;
		strReply = p+6;
		if(strncmp(p+6, "OK", 2) == 0) return RESPONSE_OK;
		else if(PA_StrNCaseCmp(p+6, "Continue", 8) == 0)
		{
			if(p[15]) { p += 15; goto parse; }
			goto retry;
		}
		else return RESPONSE_MESSAGE;
	}
	else if(len == 0)
	{
		return REMOTEHOST_CLOSED;
	}
	return PA_SOCKET_ERROR;
}

//Return: 0 - OK;
int SendPacket(PA_SOCKET sk, const char *buf, int len)
{
	int rlt = -1;
	CString str;

	rlt = send(sk, buf, len, 0);
	if( rlt == len )
	{
		return GetReply(sk, str);
	}
	else
	{
		return SENDDATA_ERROR;
	}
}

//该函数返回后，pConn 代表的连接不再有效
DWORD CTPUpdate(DVSCONN *pConn, const char *file, UPDATECALLBACK pFunc, void* data)
{
	struct sockaddr_in svrAddr;
	CString sTmp;
	DWORD rlt;
	PA_SOCKET sk;
	PACKEDFILEHEADER hdr;
	FILE *fp;
	fp = fopen(file, "rb");
	if(!fp) return E_CANNOTOPENFILE;
	fread(&hdr, sizeof(hdr), 1, fp);

	if(hdr.product != pConn->devInfo.uTypeId && pConn->devInfo.uTypeId > 10)
	{
		fclose(fp);
		return E_WRONG_TARGETPLATFORM;
	}
	memcpy(&svrAddr, &pConn->devAddr, sizeof(svrAddr));
	if( (rlt = pConn->ExecCmd(hdr.product==0?"quit":"exit", sTmp)) != 0) 
	{
		fclose(fp);
		return rlt;
	}

	//pConn->Disconnect();
	if(pFunc) pFunc(UE_WAIT, NULL, data);

	PA_Sleep(3000);
	sk = socket(AF_INET, SOCK_STREAM, 0);
/*
	if(pConn->pAgent)
	{
		char host[260], *p;
		strcpy(host, pConn->cHost);
		p = strchr(host, ':');
		if(p) 
		{
			svrAddr.sin_port = htons(atoi(p+1));
			*p = '\0';
			if(!ResolveHost(host, &svrAddr.sin_addr.s_addr))
			{
				return E_CTP_HOST;
			}
		}
		else
			svrAddr.sin_port = htons(8001);
	}
*/
	for(int i=0; i<10; i++)
	{
		int err;
		if(connect(sk, (const struct sockaddr*)&svrAddr, sizeof(svrAddr)) == 0)
		{
			err = SendPacket(sk, "CMD 10\r\necho hello", 18);
			if(err == 0) break;
			else
			{
				if(err == PA_SOCKET_ERROR) err = PA_SocketGetError();
				if(err == ECONNRESET || err == ECONNREFUSED)
				{
					PA_SocketClose(sk);
					PA_Sleep(1000);
					sk = socket(AF_INET, SOCK_STREAM, 0);
					continue;
				}
				else
				{
					PA_SocketClose(sk);
					fclose(fp);
					return E_CONN;
				}
			}
		}
		else
			PA_Sleep(1000);
	}


	rlt = 0;
	while(hdr.fnum)
		if( (rlt = SendFile(sk, fp, pFunc, data)) != 0) break;
		else hdr.fnum --;
	fclose(fp);

	PA_SocketClose(sk);

	return rlt;
}

//Return: 0 - OK; Otherwise - something wrong
int SendFile(PA_SOCKET sk, FILE *fp, UPDATECALLBACK pFunc, void* data)
{
	int rlt = 0;
	CString ss, sfmt;
	SUBHEADER subhdr;
	int i, len;
	int percent = -1;

	char *buf = (char*)malloc(2000);

	fread(&subhdr, sizeof(subhdr), 1, fp);
	if(memcmp(subhdr.tag, "FILE", 4) == 0)
	{
		int fnlen, flen;
		char tgtname[256];
		fread(&fnlen, 1, sizeof(int), fp);
		fread(tgtname, 1, fnlen, fp);
		fread(&flen, 1, sizeof(int), fp);

		sprintf(buf, "%s[%dB]", tgtname, flen);
		if(pFunc) pFunc(UE_BOF, buf, data);

		len = sprintf(buf+1000, "%s %d", tgtname, flen);
		len = sprintf(buf, "FILE %d\r\n%s", len, buf+1000);
		if( (rlt = SendPacket(sk, buf, len)) == 0)
		{
			int sent, rlen;
			MD5_CTX mdctx;

			sent = 0;
			MDInit(&mdctx);
			while(sent < flen)
			{	//长度是XORSTRSIZE的倍数
				#define DATASIZE 1400/XORSTRSIZE*XORSTRSIZE
				rlen = (flen-sent) > DATASIZE ? DATASIZE : (flen-sent);
				len = sprintf(buf, "DATA %d\r\n", rlen);
				rlen = fread(buf + len, 1, rlen, fp);
				sent += rlen;

				for(i=0; i<rlen; i++) buf[len+i] ^= xorstr[i%XORSTRSIZE];
				MDUpdate(&mdctx, (unsigned char*)(buf+len), rlen);

				rlt = SendPacket(sk, buf, rlen + len);
				if(rlt) break;

				int pc = 100*sent/flen;
				if(pc != percent)
				{
					percent = pc;
					if(pFunc) pFunc(UE_PROGRESS, (char*)(percent), data);
				}
			}
			if(rlt == 0)
			{
				BYTE digest[16];
				MDFinal(digest, &mdctx);
				strcpy(buf, "MD5 16\r\n");
				len = strlen(buf);
				memcpy(buf+len, digest, 16);
				rlt = SendPacket(sk, buf, len + 16);
			}
		}
	}
	else if(memcmp(subhdr.tag, "CMD", 4) == 0)
	{
		len = sprintf(buf, "CMD %d\r\n", subhdr.len);
		fread(buf+len, 1, subhdr.len, fp);
		for(i=0; i<subhdr.len; i++)
			buf[len+i] ^= xorstr[i%XORSTRSIZE];
		if(pFunc) pFunc(UE_EXEC, buf+len, data);
		SendPacket(sk, buf, len + subhdr.len);
	}
	else
	{
		fseek(fp, subhdr.len, SEEK_CUR);
	}

	free(buf);
	if(!rlt) 
	{
		if(pFunc) pFunc(UE_EOF, NULL, data);
	}
	return rlt;
}

