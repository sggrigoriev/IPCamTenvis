#ifndef __Remotefile_h__
#define __Remotefile_h__

//------------ Remote File Reader -------------
#include "FileAVSyncReader.h"

struct _DVSCONN;
typedef struct _tagRRParam {
	PROGRESSCALLBACK cb_dwnld;	//Downloading progress indication
	void 		*cb_data;
	struct _DVSCONN *pConn;
} REMOTEREADERPARAM;

#endif
