//////////////////////////////////////////////////////////////////////////////

class UVTimingBase {
protected:
	int _offsetU, _offsetV, _blendU, _blendV, _signU, _signV;
	bool _edge;

	enum {
		SHIFT = (sizeof(int) - sizeof(BYTE)) * 8 - 1,
		SCALE = 1 << SHIFT,
	};

	UVTimingBase(double u, double v, bool edge) {
		_edge = edge;
		_offsetU = u;
		_offsetV = v;
		_signU = (u < 0) ? -1 : 1;
		_signV = (v < 0) ? -1 : 1;
		_blendU = (u - _offsetU) * _signU * SCALE;
		_blendV = (v - _offsetV) * _signV * SCALE;
		if(_blendU) _offsetU += _signU; else _blendU = SCALE;
		if(_blendV) _offsetV += _signV; else _blendV = SCALE;
	}
};

//////////////////////////////////////////////////////////////////////////////

class UVTimingH : public GenericVideoFilter, public UVTimingBase {
	static void Proc(BYTE *dst, int step, int width, int offset, int blend, int sign, bool edge) {
		if(offset == 0)
			return;

		if(sign > 0)
			dst += (width - 1) * step;

		BYTE *src = dst - offset * step;
		int inv = SCALE - blend;

		offset *= sign;
		sign *= step;

		for(int w = width - offset; w; --w) {
			*dst = (*src * blend + *(src + sign) * inv) >> SHIFT;
			src -= sign;
			dst -= sign;
		}

		if(!edge)
			return;

		int clr = *(src + sign);

		for(int w = offset; w; --w) {
			*dst = clr;
			dst -= sign;
		}
	}

	UVTimingH(const PClip& clip, double u, double v, bool edge, IScriptEnvironment *env)
		: GenericVideoFilter(clip)
		, UVTimingBase(
			std::min(std::max(u, double(-vi.width)), double(vi.width)) / 2,
			std::min(std::max(v, double(-vi.width)), double(vi.width)) / 2, edge)
	{ child->SetCacheHints(CACHE_NOTHING, 0); }

	static AVSValue __cdecl Create(AVSValue args, void *user_data, IScriptEnvironment *env) {
		PClip child(args[0].AsClip());
		const VideoInfo& vi = child->GetVideoInfo();
		double u = args[1].AsFloat(0);
		double v = args[2].AsFloat(0);
		bool edge = args[3].AsBool(false);

		if(!vi.IsYUV())
			env->ThrowError("%s: YUVê—p", GetName());

		if(u == 0 && v == 0)
			return child;

		return new UVTimingH(child, u, v, edge, env);
	}

public:
	static const char *GetName()
	{ return "UVTimingH"; }

	static void PluginInit(IScriptEnvironment *env)
	{ env->AddFunction(GetName(), "c[u]f[v]f[edge]b", Create, 0); }

	PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment *env) {
		PVideoFrame frame(child->GetFrame(n, env));
		env->MakeWritable(&frame);

		if(vi.IsYUY2()) {
			BYTE *ptr = frame->GetWritePtr();
			int pitch = frame->GetPitch();
			int width = vi.width / 2;

			for(int h = vi.height; h; --h) {
				Proc(ptr + 1, 4, width, _offsetU, _blendU, _signU, _edge);
				Proc(ptr + 3, 4, width, _offsetV, _blendV, _signV, _edge);
				ptr += pitch;
			}
		}
		else if(vi.IsYV12()) {
			BYTE *ptrU = frame->GetWritePtr(PLANAR_U);
			BYTE *ptrV = frame->GetWritePtr(PLANAR_V);
			int pitchU = frame->GetPitch(PLANAR_U);
			int pitchV = frame->GetPitch(PLANAR_V);
			int width = vi.width / 2;

			for(int h = vi.height / 2; h; --h) {
				Proc(ptrU, 1, width, _offsetU, _blendU, _signU, _edge);
				Proc(ptrV, 1, width, _offsetV, _blendV, _signV, _edge);
				ptrU += pitchU;
				ptrV += pitchV;
			}
		}

		return frame;
	}
};

//////////////////////////////////////////////////////////////////////////////

class UVTimingV : public GenericVideoFilter, public UVTimingBase {
	static void Proc(BYTE *dst, int step, int pitch, int width, int height, int offset, int blend, int sign, bool edge) {
		if(offset == 0)
			return;

		if(sign > 0)
			dst += (height - 1) * pitch;

		BYTE *src = dst - offset * pitch;
		int inv = SCALE - blend;

		offset *= sign;
		sign *= pitch;

		for(int h = height - offset; h; --h) {
			for(int w = width; w; --w) {
				*dst = (*src * blend + *(src + sign) * inv) >> SHIFT;
				src += step;
				dst += step;
			}
			src -= width * step + sign;
			dst -= width * step + sign;
		}

		if(!edge)
			return;

		src += sign;

		for(int h = offset; h; --h) {
			for(int w = width; w; --w) {
				*dst = *src;
				src += step;
				dst += step;
			}
			src -= width * step;
			dst -= width * step + sign;
		}
	}

	UVTimingV(const PClip& clip, double u, double v, bool edge, IScriptEnvironment *env)
		: GenericVideoFilter(clip)
		, UVTimingBase(
			std::min(std::max(u, double(-vi.height)), double(vi.height)) / (vi.IsYV12() + 1),
			std::min(std::max(v, double(-vi.height)), double(vi.height)) / (vi.IsYV12() + 1), edge)
	{ child->SetCacheHints(CACHE_NOTHING, 0); }

	static AVSValue __cdecl Create(AVSValue args, void *user_data, IScriptEnvironment *env) {
		PClip child(args[0].AsClip());
		const VideoInfo& vi = child->GetVideoInfo();
		double u = args[1].AsFloat(0);
		double v = args[2].AsFloat(0);
		bool edge = args[3].AsBool(false);

		if(!vi.IsYUV())
			env->ThrowError("%s: YUVê—p", GetName());

		if(u == 0 && v == 0)
			return child;

		return new UVTimingV(child, u, v, edge, env);
	}

public:
	static const char *GetName()
	{ return "UVTimingV"; }

	static void PluginInit(IScriptEnvironment *env)
	{ env->AddFunction(GetName(), "c[u]f[v]f[edge]b", Create, 0); }

	PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment *env) {
		PVideoFrame frame(child->GetFrame(n, env));
		env->MakeWritable(&frame);

		if(vi.IsYUY2()) {
			BYTE *ptr = frame->GetWritePtr();
			int pitch = frame->GetPitch();
			int width = vi.width / 2;

			Proc(ptr + 1, 4, pitch, width, vi.height, _offsetU, _blendU, _signU, _edge);
			Proc(ptr + 3, 4, pitch, width, vi.height, _offsetV, _blendV, _signV, _edge);
		}
		else if(vi.IsYV12()) {
			BYTE *ptrU = frame->GetWritePtr(PLANAR_U);
			BYTE *ptrV = frame->GetWritePtr(PLANAR_V);
			int pitchU = frame->GetPitch(PLANAR_U);
			int pitchV = frame->GetPitch(PLANAR_V);
			int width = vi.width / 2;
			int height = vi.height / 2;

			Proc(ptrU, 1, pitchU, width, height, _offsetU, _blendU, _signU, _edge);
			Proc(ptrV, 1, pitchV, width, height, _offsetV, _blendV, _signV, _edge);
		}

		return frame;
	}
};

//////////////////////////////////////////////////////////////////////////////
