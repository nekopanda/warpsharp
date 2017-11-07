//////////////////////////////////////////////////////////////////////////////

template<int TYPE>
class XsharpenT : public GenericVideoFilter {
	friend class Xsharpen;
	int _strength, _threshold;

	XsharpenT(const PClip& clip, int strength, int threshold, IScriptEnvironment *env)
		: GenericVideoFilter(clip), _strength(strength), _threshold(threshold)
	{ child->SetCacheHints(CACHE_NOTHING, 0); }

public:
	PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment *env) {
		using std::min;
		using std::max;
		enum { STEP = (TYPE == VideoInfo::CS_YUY2) + 1 };

		PVideoFrame srcFrame(child->GetFrame(n, env));
		PVideoFrame dstFrame(env->NewVideoFrame(vi));

		int rowSize = vi.RowSize();
		int srcPitch = srcFrame->GetPitch();
		int dstPitch = dstFrame->GetPitch();

		const BYTE *src = srcFrame->GetReadPtr();
		BYTE *dst = dstFrame->GetWritePtr();

		int strength = _strength;
		int inverse = 256 - strength;
		int threshold = _threshold;

		memcpy(dst, src, rowSize);
		src += srcPitch;
		dst += dstPitch;

		for(int h = vi.height - 2; h; --h) {
			struct { int lo, hi; } ranges[4], *range;
			int luma;

			range = &ranges[0];
			range->lo = range->hi = *src;

			luma = src[-srcPitch];
			range->lo = min(range->lo, luma);
			range->hi = max(range->hi, luma);

			luma = src[+srcPitch];
			range->lo = min(range->lo, luma);
			range->hi = max(range->hi, luma);

			range = &ranges[1];
			range->lo = range->hi = src[STEP];

			luma = src[STEP - srcPitch];
			range->lo = min(range->lo, luma);
			range->hi = max(range->hi, luma);

			luma = src[STEP + srcPitch];
			range->lo = min(range->lo, luma);
			range->hi = max(range->hi, luma);

			dst[0] = src[0];
			if(TYPE == VideoInfo::CS_YUY2)
				dst[1] = src[1];

			src += STEP;
			dst += STEP;

			for(int x = 1, w = vi.width - 2; w; ++x, --w) {
				range = &ranges[(x + 1) & 3];
				range->lo = range->hi = src[STEP];

				luma = src[STEP - srcPitch];
				range->lo = min(range->lo, luma);
				range->hi = max(range->hi, luma);

				luma = src[STEP + srcPitch];
				range->lo = min(range->lo, luma);
				range->hi = max(range->hi, luma);

				int lo = range->lo;
				int hi = range->hi;

				range = &ranges[(x + 0) & 3];
				lo = min(lo, range->lo);
				hi = max(hi, range->hi);

				range = &ranges[(x - 1) & 3];
				lo = min(lo, range->lo);
				hi = max(hi, range->hi);

				luma = *src;
				int lodiff = luma - lo;
				int hidiff = hi - luma;
				int diff, target;

				if(lodiff < hidiff) {
					diff = lodiff;
					target = lo;
				}
				else {
					diff = hidiff;
					target = hi;
				}

				if(diff < threshold)
					dst[0] = (luma * inverse + target * strength) >> 8;
				else
					dst[0] = src[0];

				if(TYPE == VideoInfo::CS_YUY2)
					dst[1] = src[1];

				src += STEP;
				dst += STEP;
			}

			dst[0] = src[0];
			if(TYPE == VideoInfo::CS_YUY2)
				dst[1] = src[1];

			src += STEP + srcPitch - rowSize;
			dst += STEP + dstPitch - rowSize;
		}

		memcpy(dst, src, rowSize);

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

class Xsharpen {
	static AVSValue __cdecl Create(AVSValue args, void *user_data, IScriptEnvironment *env) {
		PClip child(args[0].AsClip());
		const VideoInfo& vi = child->GetVideoInfo();
		int strength = args[1].AsInt(128);
		int threshold = args[2].AsInt(8);

		if(vi.IsYUY2())
			return new XsharpenT<VideoInfo::CS_YUY2>(child, strength, threshold, env);
		else if(vi.IsYV12())
			return new XsharpenT<VideoInfo::CS_YV12>(child, strength, threshold, env);

		env->ThrowError("%s: YUVê—p", GetName());
		return AVSValue();
	}

public:
	static const char *GetName()
	{ return "Xsharpen"; }

	static void PluginInit(IScriptEnvironment *env)
	{ env->AddFunction(GetName(), "c[strength]i[threshold]i", Create, 0); }
};

//////////////////////////////////////////////////////////////////////////////
