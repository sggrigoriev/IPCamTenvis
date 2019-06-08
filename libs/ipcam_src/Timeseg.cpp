#include <Timeseg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


extern int ScatterResStrings(UINT sId, char *buf, int len, char *pStr[], int size);

//�ַ�����ʽ: (+|_)T;Mask;HH:MM:SS~HH:MM:SS,HH:MM:SS~HH:MM:SS ...
//			  Mask ���� 16 ���Ʊ�ʾ
static BOOL str2time(const char *str, TIMEOFDAY *ptod)
{
	int h,m,s;
	if(sscanf(str, "%d:%d:%d", &h, &m, &s) != 3) return FALSE;
	ptod->hour = h; ptod->min = m; ptod->sec = s;
	return TRUE;
}
BOOL str2timeseg(const char *s, TIMESEG *pts)
{
	if(*s == '_') pts->plus = 0;
	else if(*s == '+') pts->plus = 1;
	else return FALSE;

	s++;
	pts->type = atoi(s);

	if(pts->type != EVERYDAY)
	{
		if( !(s = strchr(s, ';')) ) return FALSE;
		pts->dwMask = strtoul(++s, NULL, 0);
	}

	pts->nTimeRange = 0;
	if( !(s = strchr(s, ';')) ) return TRUE;

	s++;
	memset(pts->timeRange, 0, sizeof(pts->timeRange));
	while(pts->nTimeRange < MAXTIMESEG)
	{
		if(!str2time(s, &pts->timeRange[pts->nTimeRange].from)) break;
		if(! (s = strchr(s, '-')) ) break;
		s++;
		if(!str2time(s, &pts->timeRange[pts->nTimeRange].to)) break;
		pts->nTimeRange++;
		if( !(s = strchr(s, ',')) ) break;
		s ++;
	}
	return TRUE;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-compare"
BOOL timeInSeg(struct tm *ptm, const TIMESEG *pts)
{
	int i;
	if(pts->type != EVERYDAY)
	{
		switch(pts->type)
		{
		case EVERYWEEK: if( (pts->dwMask & (1 << ptm->tm_wday)) == 0 ) return FALSE; break;
		case EVERYMONTH: if( (pts->dwMask & (1 << (ptm->tm_mday-1))) == 0 ) return FALSE; break;
		default:
			if(pts->type - INMONTH != ptm->tm_mon) return FALSE;
			break;
		}
	}
	if(pts->nTimeRange == 0) return TRUE;
	for(i=0; i<pts->nTimeRange; i++)
	{
		int t, tf, tt;
		t = (ptm->tm_hour << 16) + (ptm->tm_min << 8) + ptm->tm_sec;
		tf = (pts->timeRange[i].from.hour << 16) + (pts->timeRange[i].from.min << 8) + pts->timeRange[i].from.sec;
		tt = (pts->timeRange[i].to.hour << 16) + (pts->timeRange[i].to.min << 8) + pts->timeRange[i].to.sec;
		if(t >= tf && t < tt)
			return TRUE;
	}
	return FALSE;
}

#pragma GCC diagnostic ignored "-Wparentheses"
BOOL timeValid(struct tm *ptm, const TIMESEG *pts, int size)
{
	int i;
	BOOL valid = FALSE;
	for(i=0; i<size; i++)
	{
		if(timeInSeg(ptm, &pts[i])) 
			if(pts[i].plus) valid = TRUE; 
			else return FALSE;
	}
	return valid;
}

CString Time2Str(const TIMEOFDAY* ptod)
{
	CString rlt;
	rlt.Format("%02d:%02d:%02d", ptod->hour, ptod->min, ptod->sec);
	return rlt;
}

CString mask2str(DWORD dwMask, int n, const char **name = NULL)
{
	CString sRlt;
	int iFirst1Bit = -1, bPrevBitIs1 = 0;
	dwMask &= ~(1<<n);
	for(int i=0; i<=n; i++)
	{
		if( (1<<i) & dwMask )
		{
			if( !bPrevBitIs1 ) 
			{
				iFirst1Bit = i;
				bPrevBitIs1 = 1;
			}
		}
		else
		{
			if(bPrevBitIs1)
			{
				CString tmp;
				if(sRlt != "") sRlt += ",";
				if(iFirst1Bit + 1 == i) 
				{
					if(name) tmp = name[i-1];
					else tmp.Format("%d", i);
				}
				else 
				{
					if(name) tmp.Format("%s ~ %s", name[iFirst1Bit], name[i-1]);
					else tmp.Format("%d-%d", iFirst1Bit + 1, i);
				}
				sRlt += tmp;
			}
			bPrevBitIs1 = 0;
		}
	}
	return sRlt;
}

//Timeseg to representation String
CString Timeseg2Str(const TIMESEG *pts)
{
	CString rlt, strTmp;
	if(pts->plus) rlt = "+ ";
	else rlt = "_ ";

	if(pts->type == EVERYDAY) strTmp.Format("%d", (int)pts->type);
	else strTmp.Format("%d; 0x%X", (int)pts->type, pts->dwMask);
	rlt += strTmp;

	if(pts->nTimeRange)
	{
		rlt += "; ";
		for(int i=0; i < pts->nTimeRange; i++)
		{
			strTmp.Format("%s-%s", (const char*)Time2Str(&pts->timeRange[i].from), (const char*)Time2Str(&pts->timeRange[i].to));
			rlt += strTmp;
			if(i < pts->nTimeRange - 1) rlt += ", ";
		}
	}
	return rlt;
}

#pragma GCC diagnostic pop
