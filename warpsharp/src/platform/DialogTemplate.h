#ifndef _DIALOG_TEMPLATE_H_
#define _DIALOG_TEMPLATE_H_

#include "../common/refcount.h"

#include <iostream>
#include <vector>

#include <comutil.h>
#include <CommCtrl.h>

namespace DialogTemplate {

//////////////////////////////////////////////////////////////////////////////

class Frame;
typedef Ref<Frame> FrameRef;

class Frame : public RefCount<Frame> {
public:
	virtual ~Frame() {}
	virtual int GetItems() = 0;
	virtual int GetWidth() = 0;
	virtual int GetHeight() = 0;
	virtual void Write(std::ostream& os, int x, int y) = 0;
};

//////////////////////////////////////////////////////////////////////////////

class Layout;
typedef Ref<Layout> LayoutRef;

class Layout : public Frame {
protected:
	std::vector<FrameRef> _frames;

public:
	void AddFrame(const FrameRef& frame)
	{ _frames.push_back(frame); }

	int GetItems() {
		int i, j;
		for(i = 0, j = 0; i < _frames.size(); ++i)
			j += _frames[i]->GetItems();
		return j;
	}
};

class Cols;
typedef Ref<Cols> ColsRef;

class Cols : public Layout {
public:
	int GetWidth() {
		int i, j;
		for(i = 0, j = 0; i < _frames.size(); ++i)
			j += _frames[i]->GetWidth();
		return j;
	}

	int GetHeight() {
		int i, j;
		for(i = 0, j = 0; i < _frames.size(); ++i)
			j = std::max(j, _frames[i]->GetHeight());
		return j;
	}

	void Write(std::ostream& os, int x, int y) {
		for(int i = 0; i < _frames.size(); x += _frames[i++]->GetWidth())
			_frames[i]->Write(os, x, y);
	}
};

class Rows;
typedef Ref<Rows> RowsRef;

class Rows : public Layout {
public:
	int GetWidth() {
		int i, j;
		for(i = 0, j = 0; i < _frames.size(); ++i)
			j = std::max(j, _frames[i]->GetWidth());
		return j;
	}

	int GetHeight() {
		int i, j;
		for(i = 0, j = 0; i < _frames.size(); ++i)
			j += _frames[i]->GetHeight();
		return j;
	}

	void Write(std::ostream& os, int x, int y) {
		for(int i = 0; i < _frames.size(); y += _frames[i++]->GetHeight())
			_frames[i]->Write(os, x, y);
	}
};

//////////////////////////////////////////////////////////////////////////////

class Dialog;
typedef Ref<Dialog> DialogRef;

class Dialog : public Rows {
	bstr_t _title;
	DWORD _style;

	short _pointsize;
	bstr_t _font;

protected:
	virtual void InitTemplate(DLGTEMPLATE& dlg, int x, int y) {
		dlg.style = _style;
		dlg.dwExtendedStyle = 0;
		dlg.cdit = GetItems();
		dlg.x = x;
		dlg.y = y;
		dlg.cx = GetWidth();
		dlg.cy = GetHeight();
	}

	virtual void WriteMenu(std::ostream& os)
	{ WORD wZero = 0; os.write((char *)&wZero, sizeof(wZero)); }

	virtual void WriteClass(std::ostream& os)
	{ WORD wZero = 0; os.write((char *)&wZero, sizeof(wZero)); }

	virtual void WriteTitle(std::ostream& os, const bstr_t& title)
	{ os.write((char *)title.operator wchar_t*(), (title.length() + 1) * sizeof(wchar_t)); }

	virtual void WriteFont(std::ostream& os, short pointsize, const bstr_t& font) {
		os.write((char *)&pointsize, sizeof(pointsize));
		os.write((char *)font.operator wchar_t*(), (font.length() + 1) * sizeof(wchar_t));
	}

public:
	Dialog(const char *title, DWORD style = WS_CAPTION | WS_POPUP | WS_SYSMENU | DS_MODALFRAME)
		: _title(title), _style(style), _pointsize(0) {}

	void SetStyle(int flag)
	{ _style |= flag; }

	void SetFont(int pointsize, const char *font)
	{ SetStyle(DS_SETFONT); _pointsize = pointsize; _font = font; }

	void Write(std::ostream& os, int x, int y) {
		DLGTEMPLATE dlg;
		InitTemplate(dlg, x, y);
		os.write((char *)&dlg, sizeof(dlg));

		WriteMenu(os);
		WriteClass(os);
		WriteTitle(os, _title);

		if(_style & DS_SETFONT)
			WriteFont(os, _pointsize, _font);

		for(int i = 4 - os.tellp() & 3; i; --i)
			os.put(0);

		Rows::Write(os, 0, 0);
	}
};

//////////////////////////////////////////////////////////////////////////////

class Item;
typedef Ref<Item> ItemRef;

class Item : public Frame {
	bstr_t _title;
	DWORD _style;
	WORD _id;
	short _w, _h;

protected:
	virtual void InitTemplate(DLGITEMTEMPLATE& item, int x, int y) {
		item.style = _style;
		item.dwExtendedStyle = 0;
		item.x = x;
		item.y = y;
		item.cx = GetWidth();
		item.cy = GetHeight();
		item.id = _id;
	}

	virtual void WriteClass(std::ostream& os) = 0;

	virtual void WriteTitle(std::ostream& os, const bstr_t& title)
	{ os.write((char *)title.operator wchar_t*(), (title.length() + 1) * sizeof(wchar_t)); }

	virtual void WriteExtra(std::ostream& os)
	{ WORD wZero = 0; os.write((char *)&wZero, sizeof(wZero)); }

public:
	Item(const char *title, int id, int w, int h, DWORD style)
		: _title(title), _style(style), _id(id), _w(w), _h(h) {}

	void SetStyle(int flag)
	{ _style |= flag; }

	int GetItems() { return 1; }
	int GetWidth() { return _w; }
	int GetHeight() { return _h; }

	void Write(std::ostream& os, int x, int y) {
		DLGITEMTEMPLATE item;
		InitTemplate(item, x, y);
		os.write((char *)&item, sizeof(item));

		WriteClass(os);
		WriteTitle(os, _title);
		WriteExtra(os);

		for(int i = 4 - os.tellp() & 3; i; --i)
			os.put(0);
	}
};

//////////////////////////////////////////////////////////////////////////////

class CommonItem;
typedef Ref<CommonItem> CommonItemRef;

class CommonItem : public Item {
	DWORD _class;

	void WriteClass(std::ostream& os)
	{ os.write((char *)&_class, sizeof(_class)); }

public:
	CommonItem(int klass, const char *title, int id, int w, int h, DWORD style)
		: Item(title, id, w, h, style), _class(klass << 16 | 0x0000ffff) {}
};

class Button;
typedef Ref<Button> ButtonRef;

class Button : public CommonItem {
public:
	Button(const char *title, int id, int w, int h = 10, DWORD style = WS_CHILD | WS_VISIBLE)
		: CommonItem(0x0080, title, id, w, h, style) {}
};

class Check;
typedef Ref<Button> CheckRef;

class Check : public CommonItem {
public:
	Check(const char *title, int id, int w, int h = 8, DWORD style = WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX)
		: CommonItem(0x0080, title, id, w, h, style) {}
};

class Edit;
typedef Ref<Edit> EditRef;

class Edit : public CommonItem {
public:
	Edit(const char *title, int id, int w, int h = 8, DWORD style = WS_CHILD | WS_VISIBLE)
		: CommonItem(0x0081, title, id, w, h, style) {}
};

class Static;
typedef Ref<Static> StaticRef;

class Static : public CommonItem {
public:
	Static(const char *title, int id, int w, int h = 8, DWORD style = WS_CHILD | WS_VISIBLE)
		: CommonItem(0x0082, title, id, w, h, style) {}
};

class ListBox;
typedef Ref<ListBox> ListBoxRef;

class ListBox : public CommonItem {
public:
	ListBox(const char *title, int id, int w, int h = 8, DWORD style = WS_CHILD | WS_VISIBLE)
		: CommonItem(0x0083, title, id, w, h, style) {}
};

class ScrollBar;
typedef Ref<ScrollBar> ScrollBarRef;

class ScrollBar : public CommonItem {
public:
	ScrollBar(const char *title, int id, int w, int h = 8, DWORD style = WS_CHILD | WS_VISIBLE)
		: CommonItem(0x0084, title, id, w, h, style) {}
};

class ComboBox;
typedef Ref<ComboBox> ComboBoxRef;

class ComboBox : public CommonItem {
	void InitTemplate(DLGITEMTEMPLATE& item, int x, int y)
	{ CommonItem::InitTemplate(item, x, y); item.cy *= 16; }

public:
	ComboBox(const char *title, int id, int w, int h = 12, DWORD style = WS_CHILD | WS_VISIBLE)
		: CommonItem(0x0085, title, id, w, h, style) {}
};

//////////////////////////////////////////////////////////////////////////////

class ControlItem;
typedef Ref<ControlItem> ControlItemRef;

class ControlItem : public Item {
	bstr_t _class;

	void WriteClass(std::ostream& os)
	{ os.write((char *)_class.operator wchar_t*(), (_class.length() + 1) * sizeof(wchar_t)); }

public:
	ControlItem(const char *klass, const char *title, int id, int w, int h, DWORD style)
		: Item(title, id, w, h, style), _class(klass) {}
};

class TrackBar;
typedef Ref<TrackBar> TrackBarRef;

class TrackBar : public ControlItem {
public:
	TrackBar(const char *title, int id, int w, int h = 8, DWORD style = WS_CHILD | WS_VISIBLE)
		: ControlItem(TRACKBAR_CLASS, title, id, w, h, style) {}
};

//////////////////////////////////////////////////////////////////////////////

} // namespace DialogTemplate

#endif // _DIALOG_TEMPLATE_H_
