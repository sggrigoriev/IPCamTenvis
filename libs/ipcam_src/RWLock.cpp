#include <RWLock.h>

#ifdef WIN32
CReadWriteLock::CReadWriteLock() 
{
    m_currentLevel = LOCK_LEVEL_NONE;
    m_readerCount    =  m_writeCount = 0;
	m_writerId = 0;
    //m_unlockEvent  = ::CreateEvent( NULL, TRUE, FALSE, NULL );
	m_unlockEvent  = ::CreateEvent( NULL, FALSE, FALSE, NULL );
    m_accessMutex  = ::CreateMutex( NULL, FALSE, NULL );
    ::InitializeCriticalSection( &m_csStateChange );
}
CReadWriteLock::~CReadWriteLock() 
{
    ::DeleteCriticalSection( &m_csStateChange );
    if ( m_accessMutex ) ::CloseHandle( m_accessMutex );
    if ( m_unlockEvent ) ::CloseHandle( m_unlockEvent );
}

bool CReadWriteLock::_Lock( int level, DWORD timeout) 
{
    bool  bresult    = true;
    DWORD waitResult = 0;
    
    waitResult = ::WaitForSingleObject( m_accessMutex, timeout );
    if ( waitResult != WAIT_OBJECT_0 )  return false;

    if ( level == LOCK_LEVEL_READ && m_currentLevel != LOCK_LEVEL_WRITE )
    {
        ::EnterCriticalSection( &m_csStateChange );
        m_currentLevel = level;
        m_readerCount += 1;
        ::ResetEvent( m_unlockEvent );
        ::LeaveCriticalSection( &m_csStateChange );
    }
    else if ( level == LOCK_LEVEL_READ && 
                m_currentLevel == LOCK_LEVEL_WRITE )
    {
        waitResult = ::WaitForSingleObject( m_unlockEvent, timeout );
        if ( waitResult == WAIT_OBJECT_0 )
        {
            ::EnterCriticalSection( &m_csStateChange );
            m_currentLevel = level;
            m_readerCount += 1;
            ::ResetEvent( m_unlockEvent );
            ::LeaveCriticalSection( &m_csStateChange );
        }
        else bresult = false;
    }
    else if ( level == LOCK_LEVEL_WRITE && 
                m_currentLevel == LOCK_LEVEL_NONE )
    {
        ::EnterCriticalSection( &m_csStateChange );
        m_currentLevel = level;
		m_writerId = GetCurrentThreadId();
		m_writeCount = 1;
        ::ResetEvent( m_unlockEvent );
        ::LeaveCriticalSection( &m_csStateChange );
    }
    else if ( level == LOCK_LEVEL_WRITE && 
                m_currentLevel != LOCK_LEVEL_NONE )
    {
		DWORD id = GetCurrentThreadId();
		if(id == m_writerId) m_writeCount++;
		else
		{
			waitResult = ::WaitForSingleObject( m_unlockEvent, timeout );
			if ( waitResult == WAIT_OBJECT_0 )
			{
				::EnterCriticalSection( &m_csStateChange );
				m_currentLevel = level;
				m_writerId = GetCurrentThreadId();
				m_writeCount = 1;
				::ResetEvent( m_unlockEvent );
				::LeaveCriticalSection( &m_csStateChange );
			}
			else bresult = false;
		}
    }

    ::ReleaseMutex( m_accessMutex );
    return bresult;

} // lock()
    
void CReadWriteLock::Unlock() 
{ 
    ::EnterCriticalSection( &m_csStateChange );
    if ( m_currentLevel == LOCK_LEVEL_READ )
    {
        m_readerCount --;
        if ( m_readerCount == 0 ) 
        {
            m_currentLevel = LOCK_LEVEL_NONE;
            ::SetEvent (m_unlockEvent);
        }
    }
    else if ( m_currentLevel == LOCK_LEVEL_WRITE )
    {
		m_writeCount--;
		if(m_writeCount == 0)
		{
			m_currentLevel = LOCK_LEVEL_NONE;
			m_writerId = 0;
			::SetEvent ( m_unlockEvent );
		}
    }
    ::LeaveCriticalSection( &m_csStateChange );
}

#elif defined(__LINUX__)

CReadWriteLock::CReadWriteLock()
{
	pthread_rwlock_init(&m_lock, NULL);
}
CReadWriteLock::~CReadWriteLock()
{
	pthread_rwlock_destroy(&m_lock);
}

bool CReadWriteLock::LockR(DWORD timeout)
{
	if(timeout == INFINITE)
		return pthread_rwlock_rdlock(&m_lock) == 0;
	else
	{
		while(!pthread_rwlock_tryrdlock(&m_lock) && timeout > 0)
		{
			usleep(10000);
			if(timeout > 10) timeout -= 10;
			else return false;
		}
		return true;
	}
}
bool CReadWriteLock::LockW(DWORD timeout)
{
	if(timeout == INFINITE)
		return pthread_rwlock_wrlock(&m_lock);
	else
	{
		while(!pthread_rwlock_trywrlock(&m_lock) && timeout > 0)
		{
			usleep(10000);
			if(timeout > 10) timeout -= 10;
			else return false;
		}
		return true;
	}
}

void CReadWriteLock::Unlock()
{
	pthread_rwlock_unlock(&m_lock);
}

//------------------------------------------------------------------------
CSpinLock::CSpinLock()
{
	PA_SpinInit(m_spin);
}

CSpinLock::~CSpinLock()
{
	PA_SpinUninit(m_spin);
}

bool CSpinLock::Lock()
{
	return PA_SpinLock(m_spin) == 0;
}

void CSpinLock::Unlock()
{
	PA_SpinUnlock(m_spin);
}

CMutexLock::CMutexLock()
{
	PA_MutexInit(m_hMutex);
}
CMutexLock::~CMutexLock()
{
	PA_MutexUninit(m_hMutex);
}
bool CMutexLock::Lock()
{
	return PA_MutexLock(m_hMutex) == 0;
}
bool CMutexLock::Lock(DWORD tryMs)
{
	if(!PA_MutexTryLock(m_hMutex))
	{
		PA_Sleep(tryMs);
		return (!PA_MutexTryLock(m_hMutex));
	}
	return false;
}
void CMutexLock::Unlock()
{
	PA_MutexUnlock(m_hMutex);
}

#endif
