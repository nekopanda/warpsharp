#ifndef _THREAD_H_
#define _THREAD_H_

#include <process.h>
#include "refcount.h"

//////////////////////////////////////////////////////////////////////////////

class Thread;
typedef Ref<Thread> ThreadRef;

class ThreadProc {
public:
	virtual ~ThreadProc() {}
	virtual void OnThreadProc(Thread *thread) = 0;
};

class Thread : public RefCount<Thread> {
	ThreadProc *_proc;
	bool _abort;

	uintptr_t _thread;
	unsigned _index;

	static unsigned __stdcall Callback(void *arg)
	{ static_cast<Thread *>(arg)->Callback(); return 0; }

	void Callback()
	{ _proc->OnThreadProc(this); }

public:
	Thread(ThreadProc *proc) : _proc(proc), _abort(false)
	{ _thread = _beginthreadex(NULL, 0, Callback, this, 0, &_index); }

	~Thread() {
		if(!!*this) {
			_abort = true;
			WaitForSingleObject(GetHandle(), INFINITE);
			CloseHandle(GetHandle());
		}
	}

	bool IsAbort() const
	{ return _abort; }

	bool operator!() const
	{ return _thread == 0; }

	HANDLE GetHandle() const
	{ return (HANDLE)_thread; }

	DWORD GetIndex() const
	{ return _index; }
};

//////////////////////////////////////////////////////////////////////////////

class Event;
typedef Ref<Event> EventRef;

class Event : public RefCount<Event> {
	HANDLE _handle;

public:
	Event() : _handle(CreateEvent(NULL, FALSE, FALSE, NULL)) {}
	~Event() { CloseHandle(_handle); }

	void Signal()
	{ ::SetEvent(_handle); }

	bool Wait(DWORD time = INFINITE)
	{ return WaitForSingleObject(_handle, time) != WAIT_TIMEOUT; }

	bool operator!() const
	{ return _handle == NULL; }
};

//////////////////////////////////////////////////////////////////////////////

class CriticalSection {
	CRITICAL_SECTION _cs;

public:
	CriticalSection()
	{ InitializeCriticalSection(&_cs); }

	~CriticalSection()
	{ DeleteCriticalSection(&_cs); }

	void Enter()
	{ EnterCriticalSection(&_cs); }

	void Leave()
	{ LeaveCriticalSection(&_cs); }
};

class CriticalLock {
	CriticalSection& _cs;

public:
	CriticalLock(CriticalSection& cs) : _cs(cs)
	{ _cs.Enter(); }

	~CriticalLock()
	{ _cs.Leave(); }
};

//////////////////////////////////////////////////////////////////////////////

#endif // _THREAD_H_
