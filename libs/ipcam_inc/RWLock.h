#ifndef __RWLock_h__
#define __RWLock_h__

#include "platform_adpt.h"

#ifdef WIN32

/* Thanks for luckysym
	http://blog.csdn.net/luckysym/archive/2007/06/29/1671731.aspx
 */

class EXTERN CReadWriteLock
{
public:
    CReadWriteLock() ;
    ~CReadWriteLock() ;

	enum { LOCK_LEVEL_NONE, LOCK_LEVEL_READ, LOCK_LEVEL_WRITE };

public:
	inline bool LockR(DWORD timeout = INFINITE) { return _Lock(LOCK_LEVEL_READ, timeout); }
	inline bool LockW(DWORD timeout = INFINITE) { return _Lock(LOCK_LEVEL_WRITE, timeout); }
	void Unlock() ;
protected:
    int    m_currentLevel;
    int    m_readerCount, m_writeCount;
	DWORD  m_writerId;
    HANDLE m_unlockEvent; 
    HANDLE m_accessMutex;
    CRITICAL_SECTION m_csStateChange;
	bool _Lock( int level, DWORD timeout = INFINITE);
};  

#define CSpinLock CCriticalSection
#define CMutexLock CMutex
#define CLockHelper CSingleLock

#elif defined(__LINUX__)

#define INFINITE 0xFFFFFFFF

class EXTERN CReadWriteLock
{
public:
    CReadWriteLock();
    ~CReadWriteLock();

public:
	bool LockR(DWORD timeout = INFINITE);
	bool LockW(DWORD timeout = INFINITE);
	void Unlock() ;
protected:
	pthread_rwlock_t m_lock;
};  

//---------------------------------------------------
class CLock
{
public:
	virtual bool Lock() = 0;
	virtual void Unlock() = 0;
};

class CSpinLock : public CLock
{
public:
	CSpinLock();
	~CSpinLock();
	
	bool Lock();
	void Unlock();
	
protected:
	PA_SPIN	m_spin;
};

class CMutexLock : public CLock
{
public:
	CMutexLock();
	~CMutexLock();
	
	bool Lock();
	bool Lock(DWORD tryMs);
	void Unlock();
	
protected:
	PA_MUTEX	m_hMutex;
};

class CLockHelper
{
public:
	CLockHelper(CLock *pLock, BOOL bLock) { m_pLock = pLock; m_bLock = bLock; if(m_bLock) pLock->Lock(); }
	~CLockHelper() { if(m_bLock) m_pLock->Unlock(); }
protected:
	CLock *m_pLock;
	BOOL m_bLock;
};

#endif

#endif
