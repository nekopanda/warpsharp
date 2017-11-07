#ifndef _LIBRARY_H_
#define _LIBRARY_H_

#include <fstream>
#include "refcount.h"

//////////////////////////////////////////////////////////////////////////////

class LibraryObject;
typedef Ref<LibraryObject> LibraryObjectRef;

class LibraryObject : public RefCount<LibraryObject> {
public:
	virtual ~LibraryObject() {}
	virtual bool operator!() = 0;
	virtual HINSTANCE GetHandle() = 0;
	virtual void *GetAddress(const char *name) = 0;
	virtual const char *GetPath() = 0;
};

//////////////////////////////////////////////////////////////////////////////

class Library : public LibraryObject {
	HINSTANCE _handle;
	std::string _path;

public:
	Library(const char *path) : _handle(LoadLibrary(path)) {}

	~Library()
	{ if(_handle != NULL) FreeLibrary(_handle); }

	bool operator!()
	{ return _handle == NULL; }

	HINSTANCE GetHandle()
	{ return _handle; }

	void *GetAddress(const char *name)
	{ return GetProcAddress(_handle, name); }

	const char *GetPath() {
		if(_path.empty()) {
			char path[MAX_PATH];
			GetModuleFileName(_handle, path, sizeof(path));
			_path = path;
		}
		return _path.c_str();
	}
};

//////////////////////////////////////////////////////////////////////////////

class CopiedLibrary : public LibraryObject {
	HINSTANCE _handle;
	std::string _full, _temp;

	static bool Copy(const char *from, const char *to) {
		std::ifstream ifs(from, std::ios_base::binary);
		std::ofstream ofs(to, std::ios_base::binary);

		if(!ifs.is_open() || !ofs.is_open())
			return false;

		std::ifstream::char_type data;
		while(ifs.get(data) && ofs.put(data));

		return true;
	}

public:
	CopiedLibrary(const char *path) : _handle(LoadLibrary(path)) {
		if(_handle == NULL)
			return;

		char full[MAX_PATH];
		GetModuleFileName(_handle, full, sizeof(full));
		_full = full;

		FreeLibrary(_handle);
		_handle = NULL;
		_temp = tmpnam(NULL);

		if(Copy(_full.c_str(), _temp.c_str()))
			_handle = LoadLibrary(_temp.c_str());
	}

	~CopiedLibrary() {
		if(_handle != NULL) FreeLibrary(_handle);
		if(!_temp.empty()) DeleteFile(_temp.c_str());
	}

	bool operator!()
	{ return _handle == NULL; }

	HINSTANCE GetHandle()
	{ return _handle; }

	void *GetAddress(const char *name)
	{ return GetProcAddress(_handle, name); }

	const char *GetPath()
	{ return _full.c_str(); }
};

//////////////////////////////////////////////////////////////////////////////

#endif // _LIBRARY_H_
