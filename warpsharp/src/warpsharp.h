//////////////////////////////////////////////////////////////////////////////
#include <intrin.h>

class Cubic4Table {
	int table[1024];

public:
	Cubic4Table(double A, bool mmx_table) {
		for(int i=0; i<256; i++) {
			double d = (double)i / 256.0;
			int y1, y2, y3, y4, ydiff;

			// Coefficients for all four pixels *must* add up to 1.0 for
			// consistent unity gain.
			//
			// Two good values for A are -1.0 (original VirtualDub bicubic filter)
			// and -0.75 (closely matches Photoshop).

			y1 = (int)floor(0.5 + (        +     A*d -       2.0*A*d*d +       A*d*d*d) * 16384.0);
			y2 = (int)floor(0.5 + (+ 1.0             -     (A+3.0)*d*d + (A+2.0)*d*d*d) * 16384.0);
			y3 = (int)floor(0.5 + (        -     A*d + (2.0*A+3.0)*d*d - (A+2.0)*d*d*d) * 16384.0);
			y4 = (int)floor(0.5 + (                  +           A*d*d -       A*d*d*d) * 16384.0);

			// Normalize y's so they add up to 16384.

			ydiff = (16384 - y1 - y2 - y3 - y4)/4;
			assert(ydiff > -16 && ydiff < 16);

			y1 += ydiff;
			y2 += ydiff;
			y3 += ydiff;
			y4 += ydiff;

			if (mmx_table) {
				table[i*4 + 0] = (y2<<16) | (y1 & 0xffff);
				table[i*4 + 1] = (y4<<16) | (y3 & 0xffff);
				table[i*4 + 2] = table[i*4 + 3] = 0;
			} else {
				table[i*4 + 0] = y1;
				table[i*4 + 1] = y2;
				table[i*4 + 2] = y3;
				table[i*4 + 3] = y4;
			}
		}
	}

	const int *GetTable() const
	{ return table; }
};

//////////////////////////////////////////////////////////////////////////////

class WarpSharpBump : public GenericVideoFilter {
	static int Proc_YUY2_MMX(BYTE *dst, const BYTE *src, int pitch, int width, int threshold) {
		threshold = 0xffff - threshold;
		WORD thresh64[] = { threshold, threshold, threshold, threshold };
		static const __int64 _00ff00ff00ff00ff = 0x00ff00ff00ff00ff;

#if !(defined(_M_X64) || defined(USE_SIMD_INTRINSICS))
		if(width &= ~3) __asm {
			mov			esi,src
			mov			edi,dst
			mov			ecx,pitch
			mov			edx,ecx
			mov			eax,width
			neg			edx
			movq		mm6,thresh64
			movq		mm7,_00ff00ff00ff00ff
			lea			eax,[esi+eax*2]
		bump1:
			movq		mm0,[esi-2]
			movq		mm1,[esi+edx]
			pand		mm0,mm7
			pand		mm1,mm7

			movq		mm2,[esi+2]
			movq		mm3,[esi+ecx]
			pand		mm2,mm7
			pand		mm3,mm7
			psubw		mm0,mm2
			psubw		mm1,mm3

			movq		mm2,[esi+edx-2]
			movq		mm3,[esi+ecx-2]
			pand		mm2,mm7
			pand		mm3,mm7
			paddw		mm0,mm2
			paddw		mm1,mm2
			paddw		mm0,mm3
			psubw		mm1,mm3

			movq		mm2,[esi+edx+2]
			movq		mm3,[esi+ecx+2]
			pand		mm2,mm7
			pand		mm3,mm7
			psubw		mm0,mm2
			paddw		mm1,mm2
			psubw		mm0,mm3 // mm0 = dx
			psubw		mm1,mm3 // mm1 = dy

			movq		mm2,mm0
			movq		mm3,mm1
			psraw		mm2,15
			psraw		mm3,15
			pxor		mm0,mm2
			pxor		mm1,mm3
			psubw		mm0,mm2 // mm0 = abs(dx)
			psubw		mm1,mm3 // mm1 = abs(dy)

			movq		mm2,mm0
			movq		mm3,mm0
			paddw		mm2,mm1
			psubusw		mm3,mm1
			movq		mm4,mm2
			psubusw		mm1,mm0
			paddw		mm2,mm2
			por			mm1,mm3 // mm1 = abs(dy - dx)
			paddw		mm2,mm4 // mm2 = 3 * (dx + dy)

			paddw		mm1,mm2
			add			esi,8
			paddusw		mm1,mm6
			add			edi,8
			psubusw		mm1,mm6 // mm1 = min(level, threshold)
			cmp			esi,eax
			movq		[edi-8],mm1
			jl			bump1
		}
#else
		if(width &= ~3) {
			__m128i mm0, mm1, mm2, mm3, mm4, mm6, mm7;

			mm6 = _mm_loadl_epi64((__m128i*)&thresh64);
			mm7 = _mm_loadl_epi64((__m128i*)&_00ff00ff00ff00ff);
			for (int x = 0; x < width/4; x++) {
				mm0 = _mm_loadl_epi64((__m128i*)(src-2));
				mm1 = _mm_loadl_epi64((__m128i*)(src-pitch));
				mm0 = _mm_and_si128(mm0, mm7);
				mm1 = _mm_and_si128(mm1, mm7);

				mm2 = _mm_loadl_epi64((__m128i*)(src+2));
				mm3 = _mm_loadl_epi64((__m128i*)(src+pitch));
				mm2 = _mm_and_si128(mm2, mm7);
				mm3 = _mm_and_si128(mm3, mm7);
				mm0 = _mm_sub_epi16(mm0, mm2);
				mm1 = _mm_sub_epi16(mm1, mm3);

				mm2 = _mm_loadl_epi64((__m128i*)(src-pitch-2));
				mm3 = _mm_loadl_epi64((__m128i*)(src+pitch-2));
				mm2 = _mm_and_si128(mm2, mm7);
				mm3 = _mm_and_si128(mm3, mm7);
				mm0 = _mm_add_epi16(mm0, mm2);
				mm1 = _mm_add_epi16(mm1, mm2);
				mm0 = _mm_add_epi16(mm0, mm3);
				mm1 = _mm_sub_epi16(mm1, mm3);

				mm2 = _mm_loadl_epi64((__m128i*)(src-pitch+2));
				mm3 = _mm_loadl_epi64((__m128i*)(src+pitch+2));
				mm2 = _mm_and_si128(mm2, mm7);
				mm3 = _mm_and_si128(mm3, mm7);
				mm0 = _mm_sub_epi16(mm0, mm2);
				mm1 = _mm_add_epi16(mm1, mm2);
				mm0 = _mm_sub_epi16(mm0, mm3);  // mm0 = dx
				mm1 = _mm_sub_epi16(mm1, mm3);  // mm1 = dy

				mm2 = mm0;
				mm3 = mm1;
				mm2 = _mm_srai_epi16(mm2, 15);
				mm3 = _mm_srai_epi16(mm3, 15);
				mm0 = _mm_xor_si128(mm0, mm2);
				mm1 = _mm_xor_si128(mm1, mm3);
				mm0 = _mm_sub_epi16(mm0, mm2);  // mm0 = abs(dx)
				mm1 = _mm_sub_epi16(mm1, mm3);  // mm1 = abs(dy)

				mm2 = mm0;
				mm3 = mm0;
				mm2 = _mm_add_epi16(mm2, mm1);
				mm3 = _mm_subs_epu16(mm3, mm1);
				mm4 = mm2;
				mm1 = _mm_subs_epu16(mm1, mm0);
				mm2 = _mm_add_epi16(mm2, mm2);
				mm1 = _mm_or_si128(mm1, mm3);   // mm1 = abs(dy - dx)
				mm2 = _mm_add_epi16(mm2, mm4);  // mm2 = 3 * (dx + dy)

				mm1 = _mm_add_epi16(mm1, mm2);
				src += 8;
				mm1 = _mm_adds_epu16(mm1, mm6);
				mm1 = _mm_subs_epu16(mm1, mm6); // mm1 = min(level, threshold)
				_mm_storel_epi64((__m128i*)dst, mm1);
				dst += 8;
			}
		}
#endif
		return width;
	}

	static int Proc_YV12_MMX(BYTE *dst, const BYTE *src, int pitch, int width, int threshold) {
		threshold = 0xffff - threshold;
		WORD thresh64[] = { threshold, threshold, threshold, threshold };

#if !(defined(_M_X64) || defined(USE_SIMD_INTRINSICS))
		if(width &= ~3) __asm {
			mov			esi,src
			mov			edi,dst
			mov			ecx,pitch
			mov			edx,ecx
			mov			eax,width
			neg			edx
			movq		mm6,thresh64
			pxor		mm7,mm7
			add			eax,esi
		bump1:
			movd		mm0,[esi-1]
			movd		mm1,[esi+edx]
			punpcklbw	mm0,mm7
			punpcklbw	mm1,mm7

			movd		mm2,[esi+1]
			movd		mm3,[esi+ecx]
			punpcklbw	mm2,mm7
			punpcklbw	mm3,mm7
			psubw		mm0,mm2
			psubw		mm1,mm3

			movd		mm2,[esi+edx-1]
			movd		mm3,[esi+ecx-1]
			punpcklbw	mm2,mm7
			punpcklbw	mm3,mm7
			paddw		mm0,mm2
			paddw		mm1,mm2
			paddw		mm0,mm3
			psubw		mm1,mm3

			movd		mm2,[esi+edx+1]
			movd		mm3,[esi+ecx+1]
			punpcklbw	mm2,mm7
			punpcklbw	mm3,mm7
			psubw		mm0,mm2
			paddw		mm1,mm2
			psubw		mm0,mm3 // mm0 = dx
			psubw		mm1,mm3 // mm1 = dy

			movq		mm2,mm0
			movq		mm3,mm1
			psraw		mm2,15
			psraw		mm3,15
			pxor		mm0,mm2
			pxor		mm1,mm3
			psubw		mm0,mm2 // mm0 = abs(dx)
			psubw		mm1,mm3 // mm1 = abs(dy)

			movq		mm2,mm0
			movq		mm3,mm0
			paddw		mm2,mm1
			psubusw		mm3,mm1
			movq		mm4,mm2
			psubusw		mm1,mm0
			paddw		mm2,mm2
			por			mm1,mm3 // mm1 = abs(dy - dx)
			paddw		mm2,mm4 // mm2 = 3 * (dx + dy)

			paddw		mm1,mm2
			add			esi,4
			paddusw		mm1,mm6
			add			edi,8
			psubusw		mm1,mm6 // mm1 = min(level, threshold)
			cmp			esi,eax
			movq		[edi-8],mm1
			jl			bump1
		}
#else
		if(width &= ~3) {
			__m128i mm0, mm1, mm2, mm3, mm4, mm6, mm7;

			mm6 = _mm_loadl_epi64((__m128i*)&thresh64);
			mm7 = _mm_setzero_si128();
			for (int x = 0; x < width/2; x++) {
				mm0 = _mm_cvtsi32_si128(*(src-1));
				mm1 = _mm_cvtsi32_si128(*(src-pitch));
				mm0 = _mm_unpacklo_epi8(mm0, mm7);
				mm1 = _mm_unpacklo_epi8(mm1, mm7);

				mm2 = _mm_cvtsi32_si128(*(src+1));
				mm3 = _mm_cvtsi32_si128(*(src+pitch));
				mm2 = _mm_unpacklo_epi8(mm2, mm7);
				mm3 = _mm_unpacklo_epi8(mm3, mm7);
				mm0 = _mm_sub_epi16(mm0, mm2);
				mm1 = _mm_sub_epi16(mm1, mm3);

				mm2 = _mm_cvtsi32_si128(*(src-pitch-1));
				mm3 = _mm_cvtsi32_si128(*(src+pitch-1));
				mm2 = _mm_unpacklo_epi8(mm2, mm7);
				mm3 = _mm_unpacklo_epi8(mm3, mm7);
				mm0 = _mm_add_epi16(mm0, mm2);
				mm1 = _mm_add_epi16(mm1, mm2);
				mm0 = _mm_add_epi16(mm0, mm3);
				mm1 = _mm_sub_epi16(mm1, mm3);

				mm2 = _mm_cvtsi32_si128(*(src-pitch+1));
				mm3 = _mm_cvtsi32_si128(*(src+pitch+1));
				mm2 = _mm_unpacklo_epi8(mm2, mm7);
				mm3 = _mm_unpacklo_epi8(mm3, mm7);
				mm0 = _mm_sub_epi16(mm0, mm2);
				mm1 = _mm_add_epi16(mm1, mm2);
				mm0 = _mm_sub_epi16(mm0, mm3);  // mm0 = dx
				mm1 = _mm_sub_epi16(mm1, mm3);  // mm1 = dy

				mm2 = mm0;
				mm3 = mm1;
				mm2 = _mm_srai_epi16(mm2, 15);
				mm3 = _mm_srai_epi16(mm3, 15);
				mm0 = _mm_xor_si128(mm0, mm2);
				mm1 = _mm_xor_si128(mm1, mm3);
				mm0 = _mm_sub_epi16(mm0, mm2);  // mm0 = abs(dx)
				mm1 = _mm_sub_epi16(mm1, mm3);  // mm1 = abs(dy)

				mm2 = mm0;
				mm3 = mm0;
				mm2 = _mm_add_epi16(mm2, mm1);
				mm3 = _mm_subs_epu16(mm3, mm1);
				mm4 = mm2;
				mm1 = _mm_subs_epu16(mm1, mm0);
				mm2 = _mm_add_epi16(mm2, mm2);
				mm1 = _mm_or_si128(mm1, mm3);   // mm1 = abs(dy - dx)
				mm2 = _mm_add_epi16(mm2, mm4);  // mm2 = 3 * (dx + dy)

				mm1 = _mm_add_epi16(mm1, mm2);
				src += 4;
				mm1 = _mm_adds_epu16(mm1, mm6);
				mm1 = _mm_subs_epu16(mm1, mm6); // mm1 = min(level, threshold)
				_mm_storel_epi64((__m128i*)dst, mm1);
				dst += 8;
			}
		}
#endif

		return width;
	}

	static void Proc(BYTE *dst, const BYTE *src, int step, int pitch, int width, int threshold) {
		for(int w = width; w; --w) {
			int dx, dy;

			dx = src[-step - pitch] + src[-step] + src[-step + pitch]
			   - src[+step - pitch] - src[+step] - src[+step + pitch];

			dy = src[-step - pitch] + src[-pitch] + src[+step - pitch]
			   - src[-step + pitch] - src[+pitch] - src[+step + pitch];

			dx = abs(dx);
			dy = abs(dy);

			int level = 3 * (dx + dy) + abs(dy - dx);

			if(level > threshold)
				level = threshold;

			*reinterpret_cast<WORD *>(dst) = level;
			src += step;
			dst += 2;
		}
	}

	static bool Proc_YUY2(BYTE *dst, const BYTE *src, int pitch, int width, int threshold, long cpu) {
		bool emms = false;

		if(cpu & CPUF_MMX) {
			int w = Proc_YUY2_MMX(dst, src, pitch, width, threshold);
			width -= w;
			src += w * 2;
			dst += w * 2;
			emms = true;
		}

		Proc(dst, src, 2, pitch, width, threshold);
		return emms;
	}

	static bool Proc_YV12(BYTE *dst, const BYTE *src, int pitch, int width, int threshold, long cpu) {
		bool emms = false;

		if(cpu & CPUF_MMX) {
			int w = Proc_YV12_MMX(dst, src, pitch, width, threshold);
			width -= w;
			src += w;
			dst += w * 2;
			emms = true;
		}

		Proc(dst, src, 1, pitch, width, threshold);
		return emms;
	}

	//----------------------------------------------------------------------------

	int _threshold, _type;

	WarpSharpBump(const PClip& clip, int threshold, IScriptEnvironment *env)
		: GenericVideoFilter(clip), _threshold(threshold)
	{
		if(vi.IsYUY2())
			_type = VideoInfo::CS_YUY2;
		else if(vi.IsYV12())
			_type = VideoInfo::CS_YV12;

		vi.pixel_type = VideoInfo::CS_YUY2;
	}

	static AVSValue __cdecl Create(AVSValue args, void *user_data, IScriptEnvironment *env) {
		PClip child(args[0].AsClip());
		const VideoInfo& vi = child->GetVideoInfo();
		int threshold = args[1].AsInt(255);

		if(!vi.IsYUV())
			env->ThrowError("%s: YUVêÍóp", GetName());

		return new WarpSharpBump(child, threshold, env);
	}

public:
	static const char *GetName()
	{ return "WarpSharpBump"; }

	static void PluginInit(IScriptEnvironment *env)
	{ env->AddFunction(GetName(), "c[threshold]i", Create, 0); }

	PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment *env) {
		PVideoFrame srcFrame(child->GetFrame(n, env));
		PVideoFrame dstFrame(env->NewVideoFrame(vi));

		const BYTE *src = srcFrame->GetReadPtr();
		BYTE *dst = dstFrame->GetWritePtr();

		int rowSize = vi.RowSize();
		int srcPitch = srcFrame->GetPitch();
		int dstPitch = dstFrame->GetPitch();

		long cpu = USE_NATIVE ? env->GetCPUFlags() : 0;
		bool flag = false;

		memset(dst, 0, rowSize);
		src += srcPitch;
		dst += dstPitch;

		for(int h = vi.height - 2; h; --h) {
			*reinterpret_cast<WORD *>(dst) = 0;
			*reinterpret_cast<WORD *>(dst + rowSize - 2) = 0;

			switch(_type) {
			case VideoInfo::CS_YUY2:
				flag = Proc_YUY2(dst + 2, src + 2, srcPitch, vi.width - 2, _threshold, cpu);
				break;
			case VideoInfo::CS_YV12:
				flag = Proc_YV12(dst + 2, src + 1, srcPitch, vi.width - 2, _threshold, cpu);
				break;
			}

			src += srcPitch;
			dst += dstPitch;
		}

		if(flag)
#if !(defined(_M_X64) || defined(USE_SIMD_INTRINSICS))
			_mm_empty();
#else
			{}
#endif
		memset(dst, 0, rowSize);

		return dstFrame;
	}
};

//////////////////////////////////////////////////////////////////////////////

class WarpSharpBlur : public GenericVideoFilter {
	static void DoRow(BYTE *dst, const BYTE *src, int w) {
		WORD *d = reinterpret_cast<WORD *>(dst);
		const WORD *s = reinterpret_cast<const WORD *>(src);

		d[0] = (s[0] * 11 + s[1] * 4 + s[2]            + 8) >> 4;
		d[1] = (s[0] *  5 + s[1] * 6 + s[2] * 4 + s[3] + 8) >> 4;

		for(w -= 4, d += 2; w; --w, ++s, ++d)
			*d = (s[0] + s[1] * 4 + s[2] * 6 + s[3] * 4 + s[4] + 8) >> 4;

		d[0] = (s[0] + s[1] * 4 + s[2] * 6 + s[3] *  5 + 8) >> 4;
		d[1] = (       s[1]     + s[2] * 4 + s[3] * 11 + 8) >> 4;
	}

	static bool DoCol(BYTE *dst, BYTE **tbl, int width, long cpu) {
		bool emms = false;
		int x = 0;

		if(cpu & CPUF_MMX) {
			x = DoColMMX(dst, tbl, width);
			emms = true;
		}

		WORD *d = reinterpret_cast<WORD *>(dst);
		WORD **t = reinterpret_cast<WORD **>(tbl);

		for(; x < width; ++x)
			d[x] = (t[0][x] + t[1][x] * 4 + t[2][x] * 6 + t[3][x] * 4 + t[4][x] + 8) >> 4;

		return emms;
	}

	static int DoColMMX(BYTE *dst, BYTE **tbl, int width) {
		static const __int64 _0008000800080008 = 0x0008000800080008;

#if !(defined(_M_X64) || defined(USE_SIMD_INTRINSICS))
		if(width &= ~3) __asm {
			push		ebp

			movq		mm7,_0008000800080008

			mov			edi,tbl
			mov			eax,[edi]
			mov			ebx,[edi+4]
			mov			ecx,[edi+8]
			mov			edx,[edi+12]
			mov			esi,[edi+16]
			mov			edi,dst

			mov			ebp,width
			shl			ebp,1
			neg			ebp

			sub			eax,ebp
			sub			ebx,ebp
			sub			ecx,ebp
			sub			edx,ebp
			sub			esi,ebp
			sub			edi,ebp

		xloop:
			movq		mm0,[eax+ebp]
			movq		mm1,[ebx+ebp]
			movq		mm2,[ecx+ebp]
			movq		mm3,[edx+ebp]
			movq		mm4,[esi+ebp]

			paddw		mm0,mm4
			paddw		mm1,mm3
			psllw		mm1,2
			movq		mm5,mm2
			paddw		mm2,mm2
			paddw		mm2,mm5
			paddw		mm2,mm2
			paddw		mm0,mm1
			paddw		mm0,mm2
			paddw		mm0,mm7
			psrlw		mm0,4

			movq		[edi+ebp],mm0
			add			ebp,8
			jne			xloop

			pop			ebp
		}
#else
		if(width &= ~3) {
			__m128i mm0, mm1, mm2, mm3, mm4, mm5, mm7;
			WORD *d = reinterpret_cast<WORD *>(dst);
			WORD **t = reinterpret_cast<WORD **>(tbl);

			mm7 = _mm_loadl_epi64((__m128i*)&_0008000800080008);
			for (int x = 0; x < width; x+=4) {
				mm0 = _mm_loadl_epi64((__m128i*)&t[0][x]);
				mm1 = _mm_loadl_epi64((__m128i*)&t[1][x]);
				mm2 = _mm_loadl_epi64((__m128i*)&t[2][x]);
				mm3 = _mm_loadl_epi64((__m128i*)&t[3][x]);
				mm4 = _mm_loadl_epi64((__m128i*)&t[4][x]);

				mm0 = _mm_add_epi16(mm0, mm4);
				mm1 = _mm_add_epi16(mm1, mm3);
				mm1 = _mm_slli_epi16(mm1, 2);
				mm5 = mm2;
				mm2 = _mm_add_epi16(mm2, mm2);
				mm2 = _mm_add_epi16(mm2, mm5);
				mm2 = _mm_add_epi16(mm2, mm2);
				mm0 = _mm_add_epi16(mm0, mm1);
				mm0 = _mm_add_epi16(mm0, mm2);
				mm0 = _mm_add_epi16(mm0, mm7);
				mm0 = _mm_srli_epi16(mm0, 4);
				_mm_storel_epi64((__m128i*)&d[x], mm0);
			}
		}
#endif

		return width;
	}

	//----------------------------------------------------------------------------

	enum {
		RADIUS = 2,
		KERNEL = RADIUS * 2 + 1,
	};

	std::vector<BYTE, aligned_allocator<BYTE, 16> > _buf;
	BYTE *_row[KERNEL];
	BYTE *_tbl[KERNEL][KERNEL];
	int _pass;
	int i;

	bool Proc(BYTE *dst, int dstPitch, const BYTE *src, int srcPitch, long cpu) {
		int rowSize = vi.RowSize();

		DoRow(_row[0], src, vi.width);

		for(i = 1; i <= RADIUS; ++i)
			memcpy(_row[i], _row[0], rowSize);

		for(; i < KERNEL - 1; ++i)
			DoRow(_row[i], src + srcPitch * (i - RADIUS), vi.width);

		bool emms = false;

		for(int h = vi.height; h; --h) {
			if(h > RADIUS)
				DoRow(_row[i], src + srcPitch * RADIUS, vi.width);
			else
				memcpy(_row[i], _row[i ? i - 1 : KERNEL - 1], rowSize);

			emms = DoCol(dst, _tbl[i], vi.width, cpu);

			if(++i >= KERNEL)
				i = 0;

			src += srcPitch;
			dst += dstPitch;
		}

		return emms;
	}

	WarpSharpBlur(const PClip& clip, int pass, IScriptEnvironment *env)
		: GenericVideoFilter(clip), _pass(pass)
	{
		int rowSize = (vi.RowSize() + 15) & ~15;
		_buf.resize(rowSize * KERNEL);

		for(int i = 0; i < KERNEL; ++i)
			_row[i] = &_buf[0] + i * rowSize;

		for(int i = 0; i < KERNEL; ++i)
			for(int j = 0; j < KERNEL; ++j)
				_tbl[i][j] = _row[(i + j + 1) % KERNEL];
	}

	static AVSValue __cdecl Create(AVSValue args, void *user_data, IScriptEnvironment *env) {
		PClip child(args[0].AsClip());
		const VideoInfo& vi = child->GetVideoInfo();
		int pass = args[1].AsInt(1);

		if(!vi.IsYUY2())
			env->ThrowError("%s: YUY2êÍóp", GetName());

		if(pass <= 0)
			return child;

		return new WarpSharpBlur(child, pass, env);
	}

public:
	static const char *GetName()
	{ return "WarpSharpBlur"; }

	static void PluginInit(IScriptEnvironment *env)
	{ env->AddFunction(GetName(), "c[pass]i", Create, 0); }

	PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment *env) {
		PVideoFrame srcFrame(child->GetFrame(n, env));
		const BYTE *src = srcFrame->GetReadPtr();
		int srcPitch = srcFrame->GetPitch();

		BYTE *dst = srcFrame->GetWritePtr();
		int dstPitch = srcFrame->GetPitch();
		PVideoFrame dstFrame(srcFrame);

		if(dst == NULL) {
			dstFrame = env->NewVideoFrame(vi);
			dst = dstFrame->GetWritePtr();
			dstPitch = dstFrame->GetPitch();
		}

		long cpu = USE_NATIVE ? env->GetCPUFlags() : 0;
		bool flag = false;

		for(int i = 0; i < _pass; ++i) {
			flag = Proc(dst, dstPitch, src, srcPitch, cpu);
			src = dst;
			srcPitch = dstPitch;
		}

		if(flag)
#if !(defined(_M_X64) || defined(USE_SIMD_INTRINSICS))
			_mm_empty();
#else
			{}
#endif
		return dstFrame;
	}
};

//////////////////////////////////////////////////////////////////////////////

template<int PixelType, bool UseMMX>
class WarpSharpT : public GenericVideoFilter {
	enum { STEP = (PixelType == VideoInfo::CS_YUY2) + 1 };

	static int InterpolateCubic(const BYTE *src, int pitch, const int *qh, const int *qv) {
		const BYTE *p0 = src;
		const BYTE *p1 = p0 + pitch;
		const BYTE *p2 = p1 + pitch;
		const BYTE *p3 = p2 + pitch;
		int luma;

		luma  = ((p0[0] * qh[0] + p0[STEP] * qh[1] + p0[STEP * 2] * qh[2] + p0[STEP * 3] * qh[3] + 128) >> 8) * qv[0];
		luma += ((p1[0] * qh[0] + p1[STEP] * qh[1] + p1[STEP * 2] * qh[2] + p1[STEP * 3] * qh[3] + 128) >> 8) * qv[1];
		luma += ((p2[0] * qh[0] + p2[STEP] * qh[1] + p2[STEP * 2] * qh[2] + p2[STEP * 3] * qh[3] + 128) >> 8) * qv[2];
		luma += ((p3[0] * qh[0] + p3[STEP] * qh[1] + p3[STEP * 2] * qh[2] + p3[STEP * 3] * qh[3] + 128) >> 8) * qv[3];
		luma = (luma + (1 << 19)) >> 20;

		if(luma < 0)
			luma = 0;
		else if(luma > 255)
			luma = 255;

		return luma;
	}

	//----------------------------------------------------------------------------

	friend class WarpSharp;

	PClip _bump;
	int _depth;
	Cubic4Table _cubic;
	WarpSharpT(const PClip& clip, int depth, int blur, int bump, double cubic, IScriptEnvironment *env)
		: GenericVideoFilter(clip), _depth(depth), _cubic(cubic, UseMMX)
	{
		AVSValue argsBump[] = { child, bump };
		_bump = env->Invoke(WarpSharpBump::GetName(), AVSValue(argsBump, sizeof(argsBump) / sizeof(*argsBump))).AsClip();

		AVSValue argsBlur[] = { _bump, blur };
		_bump = env->Invoke(WarpSharpBlur::GetName(), AVSValue(argsBlur, sizeof(argsBlur) / sizeof(*argsBlur))).AsClip();

		child->SetCacheHints(CACHE_RANGE, 1);
	}

public:
	PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment *env) {
		const int *cubic = _cubic.GetTable();

		PVideoFrame bumFrame(_bump->GetFrame(n, env));
		PVideoFrame srcFrame(child->GetFrame(n, env));
		PVideoFrame dstFrame(env->NewVideoFrame(vi));

		int depth = _depth;
		int srcRowSize = vi.RowSize();
		int bumRowSize = bumFrame->GetRowSize();
		int bumPitch = bumFrame->GetPitch();
		int srcPitch = srcFrame->GetPitch();
		int dstPitch = dstFrame->GetPitch();

		const BYTE *bum = bumFrame->GetReadPtr();
		const BYTE *src = srcFrame->GetReadPtr();
		BYTE *dst = dstFrame->GetWritePtr();

		memcpy(dst, src, srcRowSize);

		bum += bumPitch;
		src += srcPitch;
		dst += dstPitch;

		int lo_dispy = 0;
		int hi_dispy = (vi.height - 3) * 256 - 1;

		for(int h = vi.height - 2; h; --h) {
			dst[0] = src[0];
			if(PixelType == VideoInfo::CS_YUY2)
				dst[1] = src[1];

			bum += 2;
			src += STEP;
			dst += STEP;

			int lo_dispx = 0;
			int hi_dispx = (vi.width - 3) * 256 - 1;

			for(int w = vi.width - 2; w; --w) {
				int dispx =
					*reinterpret_cast<const WORD *>(bum - 2) -
					*reinterpret_cast<const WORD *>(bum + 2);

				int dispy =
					*reinterpret_cast<const WORD *>(bum - bumPitch) -
					*reinterpret_cast<const WORD *>(bum + bumPitch);

				dispx = (dispx * depth + 8) >> 4;
				dispy = (dispy * depth + 8) >> 4;

				if(dispx < lo_dispx)
					dispx = lo_dispx;
				else if(dispx > hi_dispx)
					dispx = hi_dispx;

				if(dispy < lo_dispy)
					dispy = lo_dispy;
				else if(dispy > hi_dispy)
					dispy = hi_dispy;

				const BYTE *src2 = src + ((dispx >> 8) - 1) * STEP + ((dispy >> 8) - 1) * srcPitch;
				const int *qh = cubic + (dispx & 255) * 4;
				const int *qv = cubic + (dispy & 255) * 4;

				dst[0] = InterpolateCubic(src2, srcPitch, qh, qv);
				if(PixelType == VideoInfo::CS_YUY2)
					dst[1] = src[1];

				bum += 2;
				src += STEP;
				dst += STEP;

				lo_dispx -= 256;
				hi_dispx -= 256;
			}

			dst[0] = src[0];
			if(PixelType == VideoInfo::CS_YUY2)
				dst[1] = src[1];

			bum += bumPitch - bumRowSize + 2;
			src += srcPitch - srcRowSize + STEP;
			dst += dstPitch - srcRowSize + STEP;

			lo_dispy -= 256;
			hi_dispy -= 256;
		}

		if(UseMMX)
#if !(defined(_M_X64) || defined(USE_SIMD_INTRINSICS))
			_mm_empty();
#else
			{}
#endif
		memcpy(dst, src, srcRowSize);

		if(PixelType == VideoInfo::CS_YV12) {
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

inline
int WarpSharpT<VideoInfo::CS_YUY2, true>::InterpolateCubic(
	const BYTE *src, int pitch, const int *qh, const int *qv)
{
	static const __int64
		_00ff00ff00ff00ff = 0x00ff00ff00ff00ff,
		_0000ffff0000ffff = 0x0000ffff0000ffff,
		_0000008000000080 = 0x0000008000000080,
		_0008000000000000 = 0x0008000000000000;

	int luma;

#if !(defined(_M_X64) || defined(USE_SIMD_INTRINSICS))
	__asm {
		mov			eax,src
		mov			edx,pitch

		movq		mm0,[eax]
		movq		mm7,_00ff00ff00ff00ff
		movq		mm1,[eax+edx]
		pand		mm0,mm7
		movq		mm2,[eax+edx*2]
		add			eax,edx
		pand		mm1,mm7
		movq		mm3,[eax+edx*2]
		pand		mm2,mm7
		pand		mm3,mm7

		mov			eax,qh
		movq		mm5,[eax]

		pmaddwd		mm0,mm5
		pmaddwd		mm1,mm5
		pmaddwd		mm2,mm5
		pmaddwd		mm3,mm5

		punpckldq	mm4,mm0
		punpckldq	mm5,mm1
		punpckldq	mm6,mm2
		punpckldq	mm7,mm3

		paddd		mm0,mm4
		paddd		mm1,mm5
		paddd		mm2,mm6
		paddd		mm3,mm7

		psrlq		mm0,32
		psrlq		mm1,32
		psrlq		mm2,32
		psrlq		mm3,32

		movq		mm4,_0000008000000080

		punpckldq	mm0,mm2
		punpckldq	mm1,mm3

		paddd		mm0,mm4
		paddd		mm1,mm4

		movq		mm4,_0000ffff0000ffff

		psrlq		mm0,8
		psllq		mm1,8

		pand		mm0,mm4
		pandn		mm4,mm1

		mov			eax,qv
		por			mm0,mm4

		pmaddwd		mm0,[eax]
		punpckldq	mm4,mm0
		paddd		mm0,mm4

		pxor		mm4,mm4
		paddd		mm0,_0008000000000000
		psrad		mm0,4

		pcmpgtw		mm4,mm0
		pandn		mm4,mm0
		packuswb	mm4,mm4
		psrlq		mm4,56
		movd		luma,mm4
	}
#else
	{
		__m128i mm0, mm1, mm2, mm3, mm4, mm5, mm6, mm7, mem;

		mm0 = _mm_loadl_epi64((__m128i*)src);
		mm7 = _mm_loadl_epi64((__m128i*)&_00ff00ff00ff00ff);
		mm1 = _mm_loadl_epi64((__m128i*)(src+pitch));
		mm0 = _mm_and_si128(mm0, mm7);
		mm2 = _mm_loadl_epi64((__m128i*)(src+pitch*2));
		mm1 = _mm_and_si128(mm1, mm7);
		mm3 = _mm_loadl_epi64((__m128i*)(src+pitch*3));
		mm2 = _mm_and_si128(mm2, mm7);
		mm3 = _mm_and_si128(mm3, mm7);

		mm5 = _mm_loadl_epi64((__m128i*)qh);

		mm0 = _mm_madd_epi16(mm0, mm5);
		mm1 = _mm_madd_epi16(mm1, mm5);
		mm2 = _mm_madd_epi16(mm2, mm5);
		mm3 = _mm_madd_epi16(mm3, mm5);

		mm4 = _mm_slli_epi64(mm0, 32);  //unpack -> move & shift
		mm5 = _mm_slli_epi64(mm1, 32);
		mm6 = _mm_slli_epi64(mm2, 32);
		mm7 = _mm_slli_epi64(mm3, 32);

		mm0 = _mm_add_epi32(mm0, mm4);
		mm1 = _mm_add_epi32(mm1, mm5);
		mm2 = _mm_add_epi32(mm2, mm6);
		mm3 = _mm_add_epi32(mm3, mm7);

		mm0 = _mm_srli_epi64(mm0, 32);
		mm1 = _mm_srli_epi64(mm1, 32);
		mm2 = _mm_srli_epi64(mm2, 32);
		mm3 = _mm_srli_epi64(mm3, 32);

		mm4 = _mm_loadl_epi64((__m128i*)&_0000008000000080);

		mm0 = _mm_unpacklo_epi32(mm0, mm2);
		mm1 = _mm_unpacklo_epi32(mm1, mm3);

		mm0 = _mm_add_epi32(mm0, mm4);
		mm1 = _mm_add_epi32(mm1, mm4);

		mm4 = _mm_loadl_epi64((__m128i*)&_0000ffff0000ffff);

		mm0 = _mm_srli_epi64(mm0, 8);
		mm1 = _mm_slli_epi64(mm1, 8);

		mm0 = _mm_and_si128(mm0, mm4);
		mm4 = _mm_andnot_si128(mm4, mm1);

		mem = _mm_loadl_epi64((__m128i*)qv);
		mm0 = _mm_or_si128(mm0, mm4);

		mm0 = _mm_madd_epi16(mm0, mem);
		mm4 = _mm_unpacklo_epi32(mm4, mm0);
		mm0 = _mm_add_epi32(mm0, mm4);

		mm4 = _mm_setzero_si128();
		mem = _mm_loadl_epi64((__m128i*)&_0008000000000000);
		mm0 = _mm_add_epi32(mm0, mem);
		mm0 = _mm_srai_epi32(mm0, 4);

		mm4 = _mm_cmpgt_epi16(mm4, mm0);
		mm4 = _mm_andnot_si128(mm4, mm0);
		mm4 = _mm_packus_epi16(mm4, mm4);
		mm4 = _mm_srli_si128(mm4, 11);
		luma = _mm_cvtsi128_si32(mm4);
	}
#endif

	return luma;
}

inline
int WarpSharpT<VideoInfo::CS_YV12, true>::InterpolateCubic(
	const BYTE *src, int pitch, const int *qh, const int *qv)
{
	static const __int64
		_0000ffff0000ffff = 0x0000ffff0000ffff,
		_0000008000000080 = 0x0000008000000080,
		_0008000000000000 = 0x0008000000000000;

	int luma;

#if !(defined(_M_X64) || defined(USE_SIMD_INTRINSICS))
	__asm {
		mov			eax,src
		mov			edx,pitch

		movd		mm0,[eax]
		pxor		mm7,mm7
		movd		mm1,[eax+edx]
		punpcklbw	mm0,mm7
		movd		mm2,[eax+edx*2]
		add			eax,edx
		punpcklbw	mm1,mm7
		movd		mm3,[eax+edx*2]
		punpcklbw	mm2,mm7
		punpcklbw	mm3,mm7

		mov			eax,qh
		movq		mm5,[eax]

		pmaddwd		mm0,mm5
		pmaddwd		mm1,mm5
		pmaddwd		mm2,mm5
		pmaddwd		mm3,mm5

		punpckldq	mm4,mm0
		punpckldq	mm5,mm1
		punpckldq	mm6,mm2
		punpckldq	mm7,mm3

		paddd		mm0,mm4
		paddd		mm1,mm5
		paddd		mm2,mm6
		paddd		mm3,mm7

		psrlq		mm0,32
		psrlq		mm1,32
		psrlq		mm2,32
		psrlq		mm3,32

		movq		mm4,_0000008000000080

		punpckldq	mm0,mm2
		punpckldq	mm1,mm3

		paddd		mm0,mm4
		paddd		mm1,mm4

		movq		mm4,_0000ffff0000ffff

		psrlq		mm0,8
		psllq		mm1,8

		pand		mm0,mm4
		pandn		mm4,mm1

		mov			eax,qv
		por			mm0,mm4

		pmaddwd		mm0,[eax]
		punpckldq	mm4,mm0
		paddd		mm0,mm4

		pxor		mm4,mm4
		paddd		mm0,_0008000000000000
		psrad		mm0,4

		pcmpgtw		mm4,mm0
		pandn		mm4,mm0
		packuswb	mm4,mm4
		psrlq		mm4,56
		movd		luma,mm4
	}
#else
	{
		__m128i mm0, mm1, mm2, mm3, mm4, mm5, mm6, mm7, mem;

		mm0 = _mm_loadl_epi64((__m128i*)src);
		mm7 = _mm_setzero_si128();
		mm1 = _mm_loadl_epi64((__m128i*)(src+pitch));
		mm0 = _mm_unpacklo_epi8(mm0, mm7);
		mm2 = _mm_loadl_epi64((__m128i*)(src+pitch*2));
		mm1 = _mm_unpacklo_epi8(mm1, mm7);
		mm3 = _mm_loadl_epi64((__m128i*)(src+pitch*3));
		mm2 = _mm_unpacklo_epi8(mm2, mm7);
		mm3 = _mm_unpacklo_epi8(mm3, mm7);

		mm5 = _mm_loadl_epi64((__m128i*)qh);

		mm0 = _mm_madd_epi16(mm0, mm5);
		mm1 = _mm_madd_epi16(mm1, mm5);
		mm2 = _mm_madd_epi16(mm2, mm5);
		mm3 = _mm_madd_epi16(mm3, mm5);

		mm4 = _mm_slli_epi64(mm0, 32);  //unpack -> move & shift
		mm5 = _mm_slli_epi64(mm1, 32);
		mm6 = _mm_slli_epi64(mm2, 32);
		mm7 = _mm_slli_epi64(mm3, 32);

		mm0 = _mm_add_epi32(mm0, mm4);
		mm1 = _mm_add_epi32(mm1, mm5);
		mm2 = _mm_add_epi32(mm2, mm6);
		mm3 = _mm_add_epi32(mm3, mm7);

		mm0 = _mm_srli_epi64(mm0, 32);
		mm1 = _mm_srli_epi64(mm1, 32);
		mm2 = _mm_srli_epi64(mm2, 32);
		mm3 = _mm_srli_epi64(mm3, 32);

		mm4 = _mm_loadl_epi64((__m128i*)&_0000008000000080);

		mm0 = _mm_unpacklo_epi32(mm0, mm2);
		mm1 = _mm_unpacklo_epi32(mm1, mm3);

		mm0 = _mm_add_epi32(mm0, mm4);
		mm1 = _mm_add_epi32(mm1, mm4);

		mm4 = _mm_loadl_epi64((__m128i*)&_0000ffff0000ffff);

		mm0 = _mm_srli_epi64(mm0, 8);
		mm1 = _mm_slli_epi64(mm1, 8);

		mm0 = _mm_and_si128(mm0, mm4);
		mm4 = _mm_andnot_si128(mm4, mm1);

		mem = _mm_loadl_epi64((__m128i*)qv);
		mm0 = _mm_or_si128(mm0, mm4);

		mm0 = _mm_madd_epi16(mm0, mem);
		mm4 = _mm_unpacklo_epi32(mm4, mm0);
		mm0 = _mm_add_epi32(mm0, mm4);

		mm4 = _mm_setzero_si128();
		mem = _mm_loadl_epi64((__m128i*)&_0008000000000000);
		mm0 = _mm_add_epi32(mm0, mem);
		mm0 = _mm_srai_epi32(mm0, 4);

		mm4 = _mm_cmpgt_epi16(mm4, mm0);
		mm4 = _mm_andnot_si128(mm4, mm0);
		mm4 = _mm_packus_epi16(mm4, mm4);
		mm4 = _mm_srli_si128(mm4, 11);
		luma = _mm_cvtsi128_si32(mm4);
	}
#endif

	return luma;
}

//////////////////////////////////////////////////////////////////////////////

class WarpSharp : public GenericVideoFilter {
	static AVSValue __cdecl Create(AVSValue args, void *user_data, IScriptEnvironment *env) {
		PClip child(args[0].AsClip());
		const VideoInfo& vi = child->GetVideoInfo();
		int depth = args[1].AsInt(128);
		int blur = args[2].AsInt(3);
		int bump = args[3].AsInt(128);
		double cubic = args[4].AsFloat(-0.6);
		long cpu = USE_NATIVE ? env->GetCPUFlags() : 0;

		if(depth == 0)
			return child;

		if(vi.IsYUY2()) {
			if(cpu & CPUF_MMX)
				return new WarpSharpT<VideoInfo::CS_YUY2, true>(child, depth, blur, bump, cubic, env);
			else
				return new WarpSharpT<VideoInfo::CS_YUY2, false>(child, depth, blur, bump, cubic, env);
		}
		else if(vi.IsYV12()) {
			if(cpu & CPUF_MMX)
				return new WarpSharpT<VideoInfo::CS_YV12, true>(child, depth, blur, bump, cubic, env);
			else
				return new WarpSharpT<VideoInfo::CS_YV12, false>(child, depth, blur, bump, cubic, env);
		}

		env->ThrowError("%s: YUVêÍóp", WarpSharp::GetName());
		return AVSValue();
	}

public:
	static const char *GetName()
	{ return "WarpSharp"; }

	static void PluginInit(IScriptEnvironment *env)
	{ env->AddFunction(GetName(), "c[depth]i[blur]i[bump]i[cubic]f", Create, 0); }
};

//////////////////////////////////////////////////////////////////////////////
