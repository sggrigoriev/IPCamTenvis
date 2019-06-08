#ifndef WIN32

#include "CString.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

CString::CString()
{
	m_nSize = m_nLength = 0; 
	m_pData = NULL; 
}
CString::CString(const char *str)
{
	m_nSize = m_nLength = 0; 
	m_pData = NULL; 
	
	if(str)
	{
		_Alloc(strlen(str)+1);

		memcpy(m_pData->pszData, str, m_nSize);
		m_nLength = m_nSize - 1;
	}
}
CString::CString(const char *str, int size)
{
	m_nSize = m_nLength = 0; 
	m_pData = NULL; 
	
	if(str)
	{
		_Alloc(size+1);

		strncpy(m_pData->pszData, str, size);
		m_pData->pszData[size] = '\0';
		m_nLength = strlen(m_pData->pszData);
	}
}
CString::CString(CString &str)
{
	m_nSize = m_nLength = 0; 
	m_pData = NULL; 
	
	_Assign(str);
}

CString::~CString()
{
	_Release();
}

CString& CString::operator = (const char *str)
{
	_Release();
	
	CString tmp(str);
	*this = tmp;
	return *this;
}

CString& CString::operator = (CString& str)
{
	_Assign(str);
	return *this;
}

CString CString::operator + (const char *str)
{
	CString tmp;
	int len2 = str ? strlen(str) : 0;
	
	if( GetLength() + len2 )
	{
		tmp._Alloc(GetLength() + len2 + 1);

		if(GetLength())
		{
			memcpy(tmp.m_pData->pszData, m_pData->pszData, m_nLength);
			tmp.m_nLength = m_nLength;
		}
		if(len2)
		{
			memcpy(tmp.m_pData->pszData + tmp.m_nLength, str, len2);
			tmp.m_nLength += len2;
		}
		tmp.m_pData->pszData[tmp.m_nLength] = '\0';
	}
	return tmp;
}

CString CString::operator + (CString& str)
{
	return *this + (const char*)str;
}

CString& CString::operator += (const char *str)
{
	int len = str ? strlen(str) : 0;
		
	if(len)
	{
		_Alloc(m_nLength + len + 1);
		memcpy(m_pData->pszData + m_nLength, str, len);
		m_nLength += len;
		m_pData->pszData[m_nLength] = '\0';
	}
	return *this;
}

CString& CString::operator += (CString& str)
{
	return *this += (const char*)str;
}

bool CString::operator != (const char *str)
{
	if( !str || !*str )
	{
		if(m_nLength == 0 ) return true;
	}
	else if(m_pData && m_pData->pszData)
	{
		return strcmp(str, m_pData->pszData) == 0;
	}
	return false;
}

bool CString::operator != (CString &str)
{
	if(m_nLength == str.m_nLength)
	{
		if(m_nLength == 0) return true;
		return strcmp(m_pData->pszData, str.m_pData->pszData);
	}
	return false;
}

bool CString::operator == (const char *str)
{
	return !(*this != str);
}
bool CString::operator == (CString &str)
{
	return !(*this != str);
}
bool CString::operator > (const char *str)
{
	if( !str || !*str )
	{
		if(m_nLength) return true;
	}
	else if(m_pData && m_pData->pszData)
	{
		return strcmp(m_pData->pszData, str) > 0;
	}
	return false;	
}
bool CString::operator > (CString &str)
{
	if( str.GetLength() == 0 )
	{
		if(m_nLength) return true;
	}
	else if(m_pData && m_pData->pszData)
	{
		return strcmp(m_pData->pszData, str) > 0;
	}
	return false;		
}

bool CString::operator < (const char *str)
{
	if(str && *str)
	{
		if(m_nLength == 0) return true;
		return strcmp(m_pData->pszData, str) < 0;
	}
	return false;
}
bool CString::operator < (CString &str)
{
	if(str.GetLength())
	{
		if(m_nLength == 0) return true;
		return strcmp(m_pData->pszData, str) < 0;
	}
	return false;
}
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-compare"
void CString::_Alloc(int size, bool bShrink)
{
	if(!m_pData) m_pData = (StringData*)calloc(sizeof(StringData), 1);
	if(m_pData->pszData)
	{
		if(m_nSize < size)
		{
			m_nSize = (unsigned int)size;
			m_pData->pszData = (char*)realloc(m_pData->pszData, m_nSize);
		}
		else if(bShrink)
		{
			m_nSize = (unsigned int)size;
			m_pData->pszData = (char*)realloc(m_pData->pszData, m_nSize);
			if(m_nLength >= m_nSize)
				m_nLength = m_nSize - 1;
		}
	}
	else
	{
		m_nSize = size;
		if(m_nSize) m_pData->pszData = (char*)malloc(m_nSize);
		m_nLength = 0;
		m_pData->nRef = 1;
}	}
#pragma GCC diagnostic pop

void CString::_Release()
{
	if(m_pData)
	{
		m_pData->nRef --;
		if(m_pData->nRef == 0)
		{	
			if(m_pData->pszData) free(m_pData->pszData);
			m_pData->pszData = NULL;
			
			free(m_pData);
			m_pData = NULL;
		}
	}
	m_nSize = m_nLength = 0;
}

CString& CString::_Assign(CString& str)
{
	if(!str.m_pData) str._Alloc(128);
	if(str.m_pData == m_pData) return *this;

	_Release();
	m_pData = str.m_pData;
	m_pData->nRef++;
	m_nSize = str.m_nSize;
	m_nLength = str.m_nLength;
	return *this;
}

CString& CString::Format(const char *fmt, ...)
{
	va_list args;

	if(!m_pData) _Alloc(0);
	else _Release();
	
	va_start(args, fmt);
	m_nLength = vasprintf(&m_pData->pszData, fmt, args);
	m_nSize = m_nLength + 1;

	return *this;
}

char *CString::GetBuffer(int nMinSize)
{
	if(nMinSize < 0)
		_Alloc(m_nLength+1);
	else
		_Alloc(nMinSize+1);

	m_pData->pszData[m_nLength] = '\0';
	return m_pData->pszData;
}

void CString::ReleaseBuffer(int nNewSize)
{
	if(nNewSize < 0)
		_Alloc(m_nLength+1);
	else
		_Alloc(nNewSize, true);
}

#endif

#if 0
int main()
{
	CString s2("abcd", 2);
	CString s = s2;
	printf("%s\n", (const char*)(s+s2));
	printf("s==s2: %d, s>s2: %d, s<s2: %d\n", s==s2, s>s2, s<s2);
	return 0;
}
#endif

