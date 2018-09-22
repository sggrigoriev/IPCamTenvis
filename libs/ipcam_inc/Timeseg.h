#ifndef __timeseg_h__
#define __timeseg_h__

#include "basetype.h"
#include "CString.h"
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _tagTIMEOFDAY { unsigned char hour, min, sec; } TIMEOFDAY;
struct time_range { TIMEOFDAY from, to; };
#define EVERYDAY	0
#define EVERYWEEK	1
#define EVERYMONTH	2
#define INMONTH		3
#define MAXTIMESEG		10
typedef
struct _tagTIMESEG {
	unsigned short plus;
	unsigned short type;		//0-Every day; 1-Every week; 2-Every month; 3+X - In Xth-month
	unsigned long dwMask;
	unsigned int nTimeRange;
	struct time_range timeRange[MAXTIMESEG];
} TIMESEG;

BOOL str2timeseg(const char *s, TIMESEG *pts);
EXTERN BOOL timeInSeg(struct tm *ptm, const TIMESEG *pts);
EXTERN BOOL timeValid(struct tm *ptm, const TIMESEG *pts, int size);

#ifdef __cplusplus
}
#endif

CString Timeseg2Str(const TIMESEG *pts);

#endif
