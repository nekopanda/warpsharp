//////////////////////////////////////////////////////////////////////////////

struct SlicerInfo {
	int width, height, bpp;

	SlicerInfo() {}

	SlicerInfo(int w, int h, int b)
		: width(w), height(h), bpp(b) {}

	SlicerInfo(const VideoInfo& vi)
		: width(vi.width), height(vi.height), bpp(vi.BytesFromPixels(1)) {}

	int RowSize() const
	{ return width * bpp; }
};

class ISlicer : public RefCount<ISlicer> {
public:
	virtual const SlicerInfo& GetSlicerInfo() = 0;
	virtual void GetSlice(int y, BYTE *slice) = 0;
	virtual const BYTE *GetCached(int y, bool prior) = 0;
	virtual void SetSourceFrame(const BYTE *frame, int pitch) = 0;
	virtual void SetTargetFrame(BYTE *frame, int pitch) = 0;
	virtual void SetCacheSize(int size) = 0;
	virtual void ClearCache() = 0;
};

typedef Ref<ISlicer> PSlicer;

//////////////////////////////////////////////////////////////////////////////

class SliceCache;
typedef Ref<SliceCache> SliceCacheRef;

class SliceCache : public RefCount<SliceCache> {
	struct Slice : public Node<Slice> {
		int index;
		BYTE *buffer;
	};

	Node<Slice> _node;
	std::vector<Slice> _slice;
	std::vector<Slice *> _table;
	std::vector<BYTE, aligned_allocator<BYTE, 16> > _image;

	SliceCache(int cache, int pitch, int height) {
		pitch = (pitch + 15) & ~15;

		_slice.resize(cache);
		_table.assign(height, NULL);
		_image.resize(pitch * cache);

		for(int i = 0; i < cache; ++i) {
			Slice *slice = &_slice[i];
			slice->index = -1;
			slice->buffer = &_image[pitch * i];
			_node.LinkPrev(slice);
		}
	}

public:
	static SliceCacheRef New(int cache, int pitch, int height)
	{ return (cache > 0) ? new SliceCache(cache, pitch, height) : SliceCacheRef(); }

	bool GetCached(int y, bool prior, BYTE *& buffer) {
		Slice *slice = _table[y];
		bool result = (slice != NULL);

		if(!result) {
			slice = _node.GetPrev();

			if(slice->index >= 0)
				_table[slice->index] = NULL;

			slice->index = y;
			_table[y] = slice;
		}

		if(prior) {
			slice->Unlink();
			_node.LinkNext(slice);
		}

		buffer = slice->buffer;
		return result;
	}

	void ClearCache() {
		Slice *slice = _node.GetNext();

		while(slice != &_node) {
			if(slice->index >= 0) {
				_table[slice->index] = NULL;
				slice->index = -1;
			}
			slice = slice->GetNext();
		}
	}
};

//////////////////////////////////////////////////////////////////////////////

class GenericSlicer : public ISlicer {
protected:
	PSlicer child;
	SlicerInfo si;
	SliceCacheRef cache;

public:
	GenericSlicer(const PSlicer& slicer)
		: child(slicer), si(child->GetSlicerInfo()) {}

	const SlicerInfo& GetSlicerInfo()
	{ return si; }

	void GetSlice(int y, BYTE *slice)
	{ child->GetSlice(y, slice); }

	const BYTE *GetCached(int y, bool prior) {
		assert(!!cache);
		BYTE *slice;

		if(!cache->GetCached(y, prior, slice))
			GetSlice(y, slice);

		return slice;
	}

	void SetSourceFrame(const BYTE *frame, int pitch)
	{ child->SetSourceFrame(frame, pitch); }

	void SetTargetFrame(BYTE *frame, int pitch)
	{ child->SetTargetFrame(frame, pitch); }

	void SetCacheSize(int size) {
		cache = SliceCache::New(size, si.RowSize(), si.height);
		child->SetCacheSize(0);
	}

	void ClearCache() {
		if(!!cache)
			cache->ClearCache();

		child->ClearCache();
	}
};

//////////////////////////////////////////////////////////////////////////////

class SourceSlicer : public ISlicer {
	SlicerInfo _si;
	const BYTE *_frame;
	int _pitch;

public:
	SourceSlicer(const SlicerInfo& si)
		: _si(si), _frame(NULL), _pitch(0) {}

	const SlicerInfo& GetSlicerInfo()
	{ return _si; }

	void GetSlice(int y, BYTE *slice)
	{ memcpy(slice, _frame + y * _pitch, _si.RowSize()); }

	const BYTE *GetCached(int y, bool prior)
	{ return _frame + y * _pitch; }

	void SetSourceFrame(const BYTE *frame, int pitch)
	{ _frame = frame; _pitch = pitch; }

	void SetTargetFrame(BYTE *frame, int pitch) {}

	void SetCacheSize(int size) {}

	void ClearCache() {}
};

//////////////////////////////////////////////////////////////////////////////

class TargetSlicer : public ISlicer {
	PSlicer _child;
	SlicerInfo _si;
	BYTE *_frame;
	int _pitch;
	std::vector<BYTE *> _table;

public:
	TargetSlicer(const PSlicer& slicer)
		: _child(slicer), _si(slicer->GetSlicerInfo())
		, _frame(NULL), _pitch(0), _table(_si.height, static_cast<BYTE *>(NULL)) {}

	const SlicerInfo& GetSlicerInfo()
	{ return _si; }

	void GetSlice(int y, BYTE *slice)
	{ _child->GetSlice(y, slice); }

	const BYTE *GetCached(int y, bool prior) {
		if(_table[y] == NULL)
			GetSlice(y, _table[y] = _frame + y * _pitch);

		return _table[y];
	}

	void SetSourceFrame(const BYTE *frame, int pitch)
	{ _child->SetSourceFrame(frame, pitch); }

	void SetTargetFrame(BYTE *frame, int pitch)
	{ _frame = frame; _pitch = pitch; }

	void SetCacheSize(int size)
	{ _child->SetCacheSize(0); }

	void ClearCache() {
		_table.assign(_si.height, NULL);
		_child->ClearCache();
	}
};

//////////////////////////////////////////////////////////////////////////////
