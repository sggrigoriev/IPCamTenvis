#ifndef __iwparse_h__
#define __iwparse_h__

#ifdef __cplusplus
extern "C" {
#endif


#define IE_WPA1		0x01
#define IE_WPA2		0x02

#define CIPHER_TKIP	0x01
#define CIPHER_AES	0x02
#define CIPHER_CCMP	0x02

#define AUTH_PSK	0x01

struct iw_ap {
	char essid[32];
	char protocol[32];
	int  mode;
	int  channel;
	int  quality;
	int  encrypted;	// 0 | 1
	int  bitrate;

	// IE
	int wpav;	// 0 or combination of IE_WPA1, IE_WPA2
	int cipher;
	int auth;	// psk
};



#ifdef __cplusplus
}
#endif

#endif

