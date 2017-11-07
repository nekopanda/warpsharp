#ifndef _CONSOLE_H_
#define _CONSOLE_H_

#include <vector>
#include <iostream>

//////////////////////////////////////////////////////////////////////////////

template<class CH, class TR = std::char_traits<CH> >
class basic_win32console_streambuf : public std::basic_streambuf<CH, TR> {
	HANDLE _handle;
	int _count;

	void output(const char *str, int len)
	{ WriteConsoleA(_handle, str, len, NULL, NULL); }

	void output(const wchar_t *str, int len)
	{ WriteConsoleW(_handle, str, len, NULL, NULL); }

public:
	basic_win32console_streambuf(DWORD handle = STD_OUTPUT_HANDLE)
		: _handle(GetStdHandle(handle)), _count(0) {}

	~basic_win32console_streambuf()
	{ if(_count > 0) FreeConsole(); }

protected:
	int_type overflow(int_type c) {
		if(c == traits_type::eof())
			return traits_type::eof();

		if(!_count)
			_count = AllocConsole() ? 1 : -1;

		char_type buffer[] = { c };
		output(buffer, 1);

		return traits_type::not_eof(c);
	}
};

//////////////////////////////////////////////////////////////////////////////

template<class CH, class TR = std::char_traits<CH> >
class basic_win32debug_streambuf : public std::basic_streambuf<CH, TR> {
	enum { SIZE = 256 };
	char_type buffer[SIZE];

	static void output(const char *str)
	{ OutputDebugStringA(str); }

	static void output(const wchar_t *str)
	{ OutputDebugStringW(str); }

	void reset()
	{ setp(buffer, buffer + SIZE - 1); }

public:
	basic_win32debug_streambuf()
	{ reset(); }

	~basic_win32debug_streambuf()
	{ sync(); }

protected:
	int_type overflow(int_type c) {
		if(c == traits_type::eof())
			return traits_type::eof();

		sync();
		sputc(c);

		return traits_type::not_eof(c);
	}

	int sync() {
		if(pptr() != pbase()) {
			*pptr() = '\0';
			output(buffer);
			reset();
		}
		return 0;
	}
};

//////////////////////////////////////////////////////////////////////////////

typedef basic_win32console_streambuf<TCHAR> win32console_streambuf;
typedef basic_win32debug_streambuf<TCHAR> win32debug_streambuf;

//////////////////////////////////////////////////////////////////////////////
#ifdef IMPLEMENT

static win32console_streambuf dout_streambuf;
//static win32debug_streambuf dout_streambuf;
std::ostream dout(&dout_streambuf);

#else

extern std::ostream dout;

#endif
//////////////////////////////////////////////////////////////////////////////

#endif // _CONSOLE_H_
