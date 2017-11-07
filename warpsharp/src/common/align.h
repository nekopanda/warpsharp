#ifndef _ALIGN_H_
#define _ALIGN_H_

#include <memory>
#include <malloc.h>

//////////////////////////////////////////////////////////////////////////////

template<class T, int A>
class aligned_allocator {
public:
	typedef size_t size_type;
	typedef ptrdiff_t difference_type;
	typedef T *pointer;
	typedef const T *const_pointer;
	typedef T& reference;
	typedef const T& const_reference;
	typedef T value_type;

	enum { alignment = A };

	template<class T> struct rebind
	{ typedef aligned_allocator<T, A> other; };

	aligned_allocator() {}
	aligned_allocator(const aligned_allocator&) {}
	template<class T> aligned_allocator(const aligned_allocator<T, A>&) {}

	pointer address(reference x) const
	{ return &x; }

	const_pointer address(const_reference x) const
	{ return &x; }

	pointer allocate(size_type n, const void *hint = NULL)
	{ return static_cast<T *>(_aligned_malloc(n * sizeof(T), A)); }

	void deallocate(pointer p, size_type n)
	{ _aligned_free(p); }

	void construct(pointer p, const_reference val)
	{ new(p) T(val); }

	void destroy(pointer p)
	{ p->~T(); }

	size_type max_size() const
	{ return size_t(-1) / sizeof(T); }
};

//////////////////////////////////////////////////////////////////////////////

#endif // _ALIGN_H_
