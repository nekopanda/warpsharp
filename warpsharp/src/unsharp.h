//////////////////////////////////////////////////////////////////////////////

template<int STEP>
class UnsharpV : public GenericSlicer {
	int _radius;

	void GetSlice0(int y, BYTE *buffer) {
		const BYTE *src = child->GetCached(0, true);
		DWORD *dst = reinterpret_cast<DWORD *>(buffer);
		int radius = _radius + 1;

		for(int w = si.width; w; --w) {
			*dst++ = *src * radius;
			src += STEP;
		}

		for(int y = 1; y < radius; ++y) {
			src = child->GetCached(y, true);
			dst = reinterpret_cast<DWORD *>(buffer);

			for(int w = si.width; w; --w) {
				*dst++ += *src;
				src += STEP;
			}
		}
	}

	void GetSlice1(int y, BYTE *buffer) {
		int y1 = y - _radius - 1;
		int y2 = y + _radius;

		if(y1 < 0)
			y1 = 0;

		if(y2 >= si.height)
			y2 = si.height - 1;

		const BYTE *src1 = child->GetCached(y1, false);
		const BYTE *src2 = child->GetCached(y2, true);

		const DWORD *pre = reinterpret_cast<const DWORD *>(GetCached(y - 1, false));
		DWORD *dst = reinterpret_cast<DWORD *>(buffer);

		for(int w = si.width; w; --w) {
			*dst++ = *pre++ - *src1 + *src2;
			src1 += STEP; src2 += STEP;
		}
	}

public:
	UnsharpV(PSlicer slicer, int radius)
		: GenericSlicer(slicer), _radius(radius)
	{
		si.width = si.RowSize() / STEP;
		si.bpp = sizeof(DWORD);
	}

	void GetSlice(int y, BYTE *buffer) {
		if(y)
			GetSlice1(y, buffer);
		else
			GetSlice0(y, buffer);
	}

	void SetCacheSize(int size) {
		if(size < 2) size = 2;
		cache = SliceCache::New(size, si.RowSize(), si.height);
		child->SetCacheSize(_radius * 2 + 2);
	}
};

//////////////////////////////////////////////////////////////////////////////

class Unsharp : public GenericSlicer {
	int _radius;

public:
	Unsharp(PSlicer slicer, int radius)
		: GenericSlicer(slicer), _radius(radius) {}

	void GetSlice(int y, BYTE *buffer) {
		const DWORD *left = reinterpret_cast<const DWORD *>(child->GetCached(y, true));
		const DWORD *right = left + 1;
		DWORD *dst = reinterpret_cast<DWORD *>(buffer);

		*dst = *left * (_radius + 1);

		for(int w = _radius; w; --w)
			*dst += *right++;

		++dst;

		for(int w = _radius; w; --w)
			*dst++ = dst[-1] - *left + *right++;

		for(int w = si.width - _radius * 2 - 1; w; --w)
			*dst++ = dst[-1] - *left++ + *right++;

		--right;

		for(int w = _radius; w; --w)
			*dst++ = dst[-1] - *left++ + *right;
	}

	void SetCacheSize(int size) {
		cache = SliceCache::New(size, si.RowSize(), si.height);
		child->SetCacheSize(1);
	}
};

//////////////////////////////////////////////////////////////////////////////

template<int TYPE>
class UnsharpMaskT : public GenericVideoFilter {
	friend class UnsharpMask;
	enum { STEP = (TYPE == VideoInfo::CS_YUY2) + 1 };

	PSlicer _unsharp;
	int _strength, _square, _threshold;

	UnsharpMaskT(const PClip& clip, int strength, int radius, int threshold, IScriptEnvironment *env)
		: GenericVideoFilter(clip), _strength(strength), _threshold(threshold)
	{
		radius = std::max(radius, 1);
		radius = std::min(radius, (vi.width - 1) / 2);
		radius = std::min(radius, (vi.height - 1) / 2);

		_square = radius * 2 + 1;
		_square *= _square;

		_unsharp = new SourceSlicer(vi);
		_unsharp = new UnsharpV<STEP>(_unsharp, radius);
		_unsharp = new Unsharp(_unsharp, radius);
		_unsharp->SetCacheSize(1);

		child->SetCacheHints(CACHE_NOTHING, 0);
	}

public:
	PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment *env) {
		PVideoFrame srcFrame(child->GetFrame(n, env));
		PVideoFrame dstFrame(env->NewVideoFrame(vi));

		const BYTE *src = srcFrame->GetReadPtr();
		BYTE *dst = dstFrame->GetWritePtr();

		int rowSize = vi.RowSize();
		int srcPitch = srcFrame->GetPitch();
		int dstPitch = dstFrame->GetPitch();

		int square = _square;
		int strength = _strength;
		int threshold = _threshold;

		_unsharp->ClearCache();
		_unsharp->SetSourceFrame(src, srcPitch);

		for(int y = 0; y < vi.height; ++y) {
			const DWORD *unsharp = reinterpret_cast<const DWORD *>(_unsharp->GetCached(y, true));

			for(int x = vi.width; x; --x) {
				int s = src[0];
				int t = s - *unsharp / square;

				if(abs(t) > threshold) {
					t = s + ((t * strength) >> 7);
					dst[0] = (t < 0) ? 0 : (t > 255) ? 255 : t;
				}
				else {
					dst[0] = s;
				}

				if(TYPE == VideoInfo::CS_YUY2)
					dst[1] = src[1];

				src += STEP;
				dst += STEP;
				unsharp += 1;
			}

			src += srcPitch - rowSize;
			dst += dstPitch - rowSize;
		}

		if(TYPE == VideoInfo::CS_YV12) {
		    env->BitBlt(
				dstFrame->GetWritePtr(PLANAR_V), dstFrame->GetPitch(PLANAR_V),
				srcFrame->GetReadPtr(PLANAR_V), srcFrame->GetPitch(PLANAR_V),
				srcFrame->GetRowSize(PLANAR_V), srcFrame->GetHeight(PLANAR_V));
		    env->BitBlt(
				dstFrame->GetWritePtr(PLANAR_U), dstFrame->GetPitch(PLANAR_U),
				srcFrame->GetReadPtr(PLANAR_U), srcFrame->GetPitch(PLANAR_U),
				srcFrame->GetRowSize(PLANAR_U), srcFrame->GetHeight(PLANAR_U));
		}

		return dstFrame;
	}
};

//////////////////////////////////////////////////////////////////////////////

class UnsharpMask {
	static AVSValue __cdecl Create(AVSValue args, void *user_data, IScriptEnvironment *env) {
		PClip child(args[0].AsClip());
		int strength = args[1].AsInt(64);
		int radius = args[2].AsInt(3);
		int threshold = args[3].AsInt(8);
		const VideoInfo& vi = child->GetVideoInfo();

		if(strength == 0)
			return child;

		if(vi.IsYUY2())
			return new UnsharpMaskT<VideoInfo::CS_YUY2>(child, strength, radius, threshold, env);
		else if(vi.IsYV12())
			return new UnsharpMaskT<VideoInfo::CS_YV12>(child, strength, radius, threshold, env);

		env->ThrowError("%s: YUVê—p", GetName());
		return AVSValue();
	}

public:
	static const char *GetName()
	{ return "UnsharpMask"; }

	static void PluginInit(IScriptEnvironment *env)
	{ env->AddFunction(GetName(), "c[strength]i[radius]i[threshold]i", Create, 0); }
};

//////////////////////////////////////////////////////////////////////////////
