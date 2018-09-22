#ifndef __CSTRING_H__
#define __CSTRING_H__

#ifndef WIN32

class CString 
{
public:
	CString();
	CString(const char *str);
	CString(const char *str, int size);
	CString(CString &str);
	~CString();
	
public:
	CString& Format(const char* fmt, ...);
	char *GetBuffer(int nMinBufferLen);
	void ReleaseBuffer(int nNewLength = -1);

	inline unsigned int GetLength()
	{
		return m_nLength;
	}
	
	inline operator char *()
	{
		return GetBuffer(-1);
	}
	
	CString operator +(const char *str);
	CString operator +(CString &str);
	CString& operator =(const char* str);
	CString& operator =(CString &str);
	CString& operator +=(const char *str);
	CString& operator +=(CString &str);
	bool operator == (const char *str);
	bool operator == (CString &str);
	bool operator != (const char *str);
	bool operator != (CString &str);
	bool operator < (const char *str);
	bool operator < (CString &str);
	bool operator > (const char *str);
	bool operator > (CString &str);
	
protected:
	unsigned int	m_nSize, m_nLength;
	struct StringData {
		int nRef;
		char *pszData;
	};
	StringData *m_pData;
	
private:
	void _Alloc(int size, bool bShrink = false);
	void _Release();
	CString& _Assign(CString& str);
};
#endif

#endif
