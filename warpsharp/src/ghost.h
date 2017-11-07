//////////////////////////////////////////////////////////////////////////////

template<int STEP>
class ByteToFixedWord : public GenericSlicer {
public:
	ByteToFixedWord(PSlicer slicer) : GenericSlicer(slicer)
	{ si.bpp = sizeof(short); }

	void GetSlice(int y, BYTE *slice) {
		const BYTE *src = child->GetCached(y, true);
		short *dst = reinterpret_cast<short *>(slice);

		for(int w = si.width; w; --w) {
			int p = *src;
			*dst = (p << 4) | (p >> 4);
			src += STEP; dst += 1;
		}
	}

	void SetCacheSize(int size) {
		cache = SliceCache::New(size, si.RowSize(), si.height);
		child->SetCacheSize(1);
	}
};

template<int STEP>
class FixedWordToByte : public GenericSlicer {
public:
	FixedWordToByte(PSlicer slicer) : GenericSlicer(slicer)
	{ si.bpp = STEP; }

	void GetSlice(int y, BYTE *slice) {
		const short *src = reinterpret_cast<const short *>(child->GetCached(y, true));
		BYTE *dst = slice;

		for(int w = si.width; w; --w) {
			int p = *src >> 4;
			*dst = (p < 0) ? 0 : (p > 255) ? 255 : p;
			src += 1; dst += STEP;
		}
	}

	void SetCacheSize(int size) {
		cache = SliceCache::New(size, si.RowSize(), si.height);
		child->SetCacheSize(1);
	}
};

//////////////////////////////////////////////////////////////////////////////

class GhostTiming : public GenericSlicer {
	int _sign, _integer, _decimal, _strength;

	enum {
		SHIFT = (sizeof(int) - sizeof(short)) * 8 - 1,
		SCALE = 1 << SHIFT,
	};

	static void Proc(short *dst, int width, int sign, int integer, int decimal, int strength) {
		if(sign > 0)
			dst += width - 1;

		short *src1 = dst - integer;
		short *src0 = src1 + sign;
		int inverse = SCALE - decimal;

		integer *= sign;

		if(integer == 0)
			src0 = src1;

		for(int w = width - integer; w; --w) {
			*dst += (((*src1 * decimal + *src0 * inverse) >> SHIFT) * strength) >> SHIFT;
			src1 -= sign;
			src0 -= sign;
			dst -= sign;
		}

//		int side = (src0->y * strength) >> SHIFT;

//		for(int w = integer; w; --w) {
//			dst->y += side;
//			dst -= sign;
//		}
	}

public:
	GhostTiming(PSlicer slicer, double position, double strength)
		: GenericSlicer(slicer)
	{
		assert(si.bpp == sizeof(short));

		_sign = (position < 0) ? -1 : 1;
		_integer = position;
		_decimal = (position - _integer) * _sign * SCALE;

		if(_decimal)
			_integer += _sign;
		else
			_decimal = SCALE;

		_strength = strength * SCALE;
	}

	void GetSlice(int y, BYTE *slice) {
		child->GetSlice(y, slice);
		short *dst = reinterpret_cast<short *>(slice);

		if(_decimal == SCALE)
			Proc(dst, si.width, _sign, _integer, SCALE, _strength);
		else
			Proc(dst, si.width, _sign, _integer, _decimal, _strength);
	}
};

//////////////////////////////////////////////////////////////////////////////

class EraseGhost : public GenericVideoFilter {
	PSlicer _target;

	EraseGhost(const PClip& clip, AVSValue args, IScriptEnvironment *env)
		: GenericVideoFilter(clip)
	{
		_target = new SourceSlicer(vi);

		if(vi.IsYV12())
			_target = new ByteToFixedWord<1>(_target);
		else if(vi.IsYUY2())
			_target = new ByteToFixedWord<2>(_target);

		int size = args.ArraySize() / 2;

		for(int i = 0; i < size; ++i) {
			double pos = args[i * 2 + 0].AsFloat();
			double str = args[i * 2 + 1].AsFloat() / 1024.0;

			if(str != 0 && pos > -vi.width && pos < vi.width)
				_target = new GhostTiming(_target, pos, str);
		}

		if(vi.IsYV12())
			_target = new FixedWordToByte<1>(_target);
		else if(vi.IsYUY2())
			_target = new FixedWordToByte<2>(_target);

		_target = new TargetSlicer(_target);
		_target->SetCacheSize(1);

		child->SetCacheHints(CACHE_NOTHING, 0);
	}

	static AVSValue __cdecl Create(AVSValue args, void *user_data, IScriptEnvironment *env) {
		PClip child(args[0].AsClip());
		const VideoInfo& vi = child->GetVideoInfo();
		AVSValue array(args[1]);

		if(!vi.IsYUV())
			env->ThrowError("%s: YUV専用", GetName());

		if(array.ArraySize() == 0)
			return child;

		return new EraseGhost(child, array, env);
	}

public:
	static const char *GetName()
	{ return "EraseGhost"; }

	static void PluginInit(IScriptEnvironment *env)
	{ env->AddFunction(GetName(), "cf*", Create, 0); }

	PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment *env) {
		PVideoFrame frame(child->GetFrame(n, env));
		env->MakeWritable(&frame);

		BYTE *ptr = frame->GetWritePtr();
		int pitch = frame->GetPitch();

		_target->ClearCache();
		_target->SetSourceFrame(ptr, pitch);
		_target->SetTargetFrame(ptr, pitch);

		for(int y = 0; y < vi.height; ++y)
			_target->GetCached(y, true);

		return frame;
	}
};

//////////////////////////////////////////////////////////////////////////////

class SearchGhost {
	static AVSValue __cdecl Create(AVSValue args, void *user_data, IScriptEnvironment *env) {
		PClip child(args[0].AsClip());
		const VideoInfo& vi = child->GetVideoInfo();

		if(!vi.IsYUV())
			env->ThrowError("%s: YUV専用", GetName());

		int frame     = std::min(std::max(args[1].AsInt(0),             0),    vi.num_frames - 1);
		int top       = std::min(std::max(args[2].AsInt(0),             0),    vi.height - 1);
		int bottom    = std::min(std::max(args[3].AsInt(vi.height - 1), top),  vi.height - 1);
		int left      = std::min(std::max(args[4].AsInt(0),             0),    vi.width - 1);
		int right     = std::min(std::max(args[5].AsInt(vi.width / 2),  left), vi.width - 1);
		int search    = args[6].AsInt(28);
		int threshold = args[7].AsInt(16);

		std::string deflog(GetName());
		deflog += ".txt";
		const char *log = args[8].AsString(deflog.c_str());

		PVideoFrame source(child->GetFrame(frame, env));
		std::vector<int> pattern(vi.width, 0);

		for(int y = top; y <= bottom; ++y) {
			const BYTE *p = source->GetReadPtr() + source->GetPitch() * y;
			int s = vi.IsYUY2();

			for(int x = 0; x < vi.width; ++x) {
				int t = p[x << s];
				pattern[x] += (t << 4) | (t >> 4);
			}
		}

		int div = bottom - top + 1;

		for(int x = 0; x < vi.width; ++x)
			pattern[x] /= div;

		int index0 = 0;
		int high = pattern[index0];

		for(int x = 1; x < vi.width / 2; ++x) {
			int v = pattern[x] - pattern[x - 1];

			if(v > high) {
				index0 = x;
				high = v;
			}
		}

		if(pattern[index0] < 16)
			return child;

		std::ofstream ofs(log, std::ios_base::app);

		ofs << "# " << GetName() << "("
			<< frame << ", "
			<< top << "," << bottom << ", "
			<< left << "," << right << ", "
			<< search << "," << threshold << ", "
			<< "\"" << log << "\")" << std::endl;

		ofs << EraseGhost::GetName() << "(";

		left = std::max(left, index0 + 1);
		right = std::max(right, index0 + 1);

		for(int x = vi.width - 1; x > index0; --x)
			pattern[x] = pattern[x - 1] - pattern[x];

		std::vector<AVSValue> vargs;
		vargs.push_back(child);

		for(int y = 0; y < search; ++y) {
			int index1 = 0;
			high = 0;

			for(int x = right; x >= left; --x) {
				int v = abs(pattern[x]);

				if(v > high) {
					index1 = x;
					high = v;
				}
			}

			if(high < threshold)
				break;

			double position = index1 - index0;
			double strength = pattern[index1] * 1024 / pattern[index0];
			pattern[index1] = 0;

			if(y) ofs << ", ";
			ofs << position << "," << strength;

			vargs.push_back(position);
			vargs.push_back(strength);
		}

		ofs << ")" << std::endl;

		return env->Invoke(EraseGhost::GetName(), AVSValue(&vargs[0], vargs.size()));
	}

public:
	static const char *GetName()
	{ return "SearchGhost"; }

	static void PluginInit(IScriptEnvironment *env)
	{ env->AddFunction(GetName(), "c[frame]i[top]i[bottom]i[left]i[right]i[search]i[threshold]i[log]s", Create, 0); }
};

//////////////////////////////////////////////////////////////////////////////

class EraseGhostV : public GenericVideoFilter {
	std::vector<int> _pattern;
	int _strength;

	EraseGhostV(const PClip& clip, int strength, const char *pattern, IScriptEnvironment *env)
		: GenericVideoFilter(clip), _pattern(vi.width, 0x4000), _strength(strength)
	{
		std::ifstream ifs(pattern, std::ios_base::binary);
		ifs.read(reinterpret_cast<char *>(&_pattern[0]), _pattern.size() * sizeof(_pattern[0]));

		child->SetCacheHints(CACHE_NOTHING, 0);
	}

	static AVSValue __cdecl Create(AVSValue args, void *user_data, IScriptEnvironment *env) {
		PClip child(args[0].AsClip());
		const VideoInfo& vi = child->GetVideoInfo();
		int strength = args[1].AsInt(256);
		const char *pattern = args[2].AsString("");

		if(!vi.IsYUV())
			env->ThrowError("%s: YUV専用", GetName());

		if(strength == 0)
			return child;

		return new EraseGhostV(child, strength, pattern, env);
	}

public:
	static const char *GetName()
	{ return "EraseGhostV"; }

	static void PluginInit(IScriptEnvironment *env)
	{ env->AddFunction(GetName(), "c[strength]i[pattern]s", Create, 0); }

	PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment *env) {
		int str = _strength;
		int org = (256 - _strength) * 0x4000;

		PVideoFrame frame(child->GetFrame(n, env));
		env->MakeWritable(&frame);

		BYTE *ptr = frame->GetWritePtr();
		int align = frame->GetPitch() - vi.RowSize();
		int step = vi.IsYUY2() + 1;

		for(int h = vi.height; h; --h) {
			int *pat = &_pattern[0];

			for(int w = vi.width; w; --w) {
				int t = *ptr;
				t = (t * org + t * *pat * str) >> 22;
				*ptr = (t > 255) ? 255 : t;
				ptr += step; pat += 1;
			}

			ptr += align;
		}

		return frame;
	}
};

//////////////////////////////////////////////////////////////////////////////

class SearchGhostV {
	static AVSValue __cdecl Create(AVSValue args, void *user_data, IScriptEnvironment *env) {
		PClip child(args[0].AsClip());
		const VideoInfo& vi = child->GetVideoInfo();

		if(!vi.IsYUV())
			env->ThrowError("%s: YUV専用", GetName());

		int frame  = std::min(std::max(args[1].AsInt(0),             0),   vi.num_frames - 1);
		int top    = std::min(std::max(args[2].AsInt(0),             0),   vi.height - 1);
		int bottom = std::min(std::max(args[3].AsInt(vi.height - 1), top), vi.height - 1);
		int depth  = std::min(std::max(args[4].AsInt(1),             1),   vi.num_frames - frame);

		std::string deflog(GetName());
		deflog += ".bin";
		const char *log = args[5].AsString(deflog.c_str());

		std::vector<int> pattern(vi.width, 0);

		for(int i = 0; i < depth; ++i) {
			PVideoFrame src(child->GetFrame(frame + i, env));

			for(int y = top; y <= bottom; ++y) {
				const BYTE *p = src->GetReadPtr() + src->GetPitch() * y;
				int s = vi.IsYUY2();

				for(int x = 0; x < vi.width; ++x) {
					int t = p[x << s];
					pattern[x] += (t << 4) | (t >> 4);
				}
			}
		}

		int high = 0;
		int div = (bottom - top + 1) * depth;

		for(int x = 0; x < vi.width; ++x) {
			pattern[x] /= div;
			high = std::max(high, pattern[x]);
		}

		int total = 0;
		int count = 0;

		for(int x = 0; x < vi.width; ++x) {
			if(high / 4 < pattern[x]) {
				total += pattern[x];
				count += 1;
			}
		}

		int average = total / std::max(count, 1);

		for(int x = 0; x < vi.width; ++x) {
			int n = (average << 14) / std::max(pattern[x], 1);

			if(n < 0x1000 || n > 0x7fe1)
				n = 0x4000;

			pattern[x] = n;
		}

		std::ofstream ofs(log, std::ios_base::binary);
		ofs.write(reinterpret_cast<char *>(&pattern[0]), pattern.size() * sizeof(pattern[0]));
		ofs.flush();

		AVSValue argsGhost[] = { child, 256, log };
		return env->Invoke(EraseGhostV::GetName(), AVSValue(argsGhost, sizeof(argsGhost) / sizeof(*argsGhost)));
	}

public:
	static const char *GetName()
	{ return "SearchGhostV"; }

	static void PluginInit(IScriptEnvironment *env)
	{ env->AddFunction(GetName(), "c[frame]i[top]i[bottom]i[depth]i[pattern]s", Create, 0); }
};

//////////////////////////////////////////////////////////////////////////////
