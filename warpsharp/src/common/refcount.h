#ifndef _REFCOUNT_H_
#define _REFCOUNT_H_

//////////////////////////////////////////////////////////////////////////////

template<class TYPE> class Ref {
	TYPE *_ptr;

	void IncRef() const
	{ if(_ptr != NULL) _ptr->IncRef(); }

	void DecRef() const
	{ if(_ptr != NULL) _ptr->DecRef(); }

public:
	Ref() : _ptr(NULL) {}

	Ref(TYPE *ptr) : _ptr(ptr)
	{ IncRef(); }

	Ref(const Ref& ref) : _ptr(ref._ptr)
	{ IncRef(); }

	template<class OTHER>
	Ref(OTHER *ptr) : _ptr(dynamic_cast<TYPE *>(ptr))
	{ IncRef(); }

	template<class OTHER>
	Ref(const Ref<OTHER>& ref) : _ptr(dynamic_cast<TYPE *>(ref.operator->()))
	{ IncRef(); }

	void operator=(const Ref& ref)
	{ ref.IncRef(); DecRef(); _ptr = ref._ptr; }

	~Ref()
	{ DecRef(); }

	TYPE *operator->() const
	{ return _ptr; }

	operator void *() const
	{ return _ptr; }

	bool operator!() const
	{ return _ptr == NULL || !*_ptr; }
};

//////////////////////////////////////////////////////////////////////////////

class SingleCounter {
public:
	typedef int CountType;

	static bool Increment(CountType& count)
	{ return !!++count; }

	static bool Decrement(CountType& count)
	{ return !!--count; }
};

class MultiCounter {
public:
	typedef LONG CountType;

	static bool Increment(CountType& count)
	{ return !!InterlockedIncrement(&count); }

	static bool Decrement(CountType& count)
	{ return !!InterlockedDecrement(&count); }
};

#ifdef _MT
typedef MultiCounter DefaultCounter;
#else
typedef SingleCounter DefaultCounter;
#endif

//////////////////////////////////////////////////////////////////////////////

template<class TYPE, class COUNTER = DefaultCounter>
class RefCount {
	typename COUNTER::CountType _count;

protected:
	RefCount() : _count(0) {}
	RefCount(const RefCount&) : _count(0) {}
	RefCount& operator=(const RefCount&) { return *this; }

public:
	bool operator!() const
	{ return false; }

	void IncRef()
	{ COUNTER::Increment(_count); }

	void DecRef()
	{ if(!COUNTER::Decrement(_count)) delete static_cast<TYPE *>(this); }
};

//////////////////////////////////////////////////////////////////////////////

#endif // _REFCOUNT_H_
