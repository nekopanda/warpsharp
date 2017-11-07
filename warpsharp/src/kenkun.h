//////////////////////////////////////////////////////////////////////////////

#if !(defined(_M_X64) || defined(USE_SIMD_INTRINSICS))
#define _KenKunNRT_Init(rows, dest, rowSize, params) \
	__asm push		eax				\
	__asm push		ebx				\
	__asm push		ecx				\
	__asm push		edx				\
	__asm push		esi				\
	__asm push		edi				\
	__asm mov		esi, rows		\
	__asm mov		edi, dest		\
	__asm mov		ecx, rowSize	\
	__asm mov		ebx, params		\
	__asm _KenKunNRT_Loop:			\

#define _KenKunNRT_Begin() \
	__asm mov		edx, esi		\
	__asm mov		eax, [edx+0]	\
	__asm pxor		mm7, mm7		\
	__asm movd		mm2, [eax+ecx]	\
	__asm mov		eax, [edx+8]	\
	__asm punpcklbw	mm2, mm7		\
	__asm movd		mm1, [eax+ecx]	\
	__asm mov		eax, [edx+4]	\
	__asm punpcklbw	mm1, mm7		\
	__asm movd		mm0, [eax+ecx]	\
	__asm add		edx, 12			\
	__asm punpcklbw	mm0, mm7		\
	__asm paddsw	mm1, mm2		\
	__asm paddsw	mm1, mm0		\

#define _KenKunNRT_Proc() \
	__asm mov		eax, [edx+0]	\
	__asm pxor		mm5, mm5		\
	__asm movd		mm4, [eax+ecx]	\
	__asm mov		eax, [edx+8]	\
	__asm punpcklbw	mm4, mm5		\
	__asm movd		mm3, [eax+ecx]	\
	__asm mov		eax, [edx+4]	\
	__asm punpcklbw	mm3, mm5		\
	__asm movd		mm2, [eax+ecx]	\
	__asm add		edx, 12			\
	__asm punpcklbw	mm2, mm5		\
	__asm paddsw	mm3, mm4		\
	__asm movq		mm4, mm2		\
	__asm paddsw	mm3, mm2		\
	__asm psubsw	mm4, mm0		\
	__asm psubsw	mm3, mm1		\
	__asm movq		mm5, mm4		\
	__asm movq		mm6, mm3		\
	__asm psraw		mm4, 15			\
	__asm psraw		mm3, 15			\
	__asm pxor		mm4, mm5		\
	__asm pxor		mm3, mm6		\
	__asm psubusw	mm4, [ebx]		\
	__asm psubusw	mm3, [ebx]		\
	__asm paddsw	mm4, mm4		\
	__asm paddsw	mm4, mm3		\
	__asm paddsw	mm4, mm4		\
	__asm psubsw	mm2, mm0		\
	__asm movq		mm5, mm2		\
	__asm psraw		mm2, 15			\
	__asm pxor		mm5, mm2		\
	__asm psubusw	mm5, mm4		\
	__asm pxor		mm5, mm2		\
	__asm paddsw	mm7, mm5		\

#define _KenKunNRT_End() \
	__asm pxor		mm5, mm2		\
	__asm paddsw	mm7, mm5		\
	__asm pmulhw	mm7, [ebx+8]	\
	__asm paddsw	mm0, mm7		\
	__asm packuswb	mm0, mm0		\
	__asm movd		[edi+ecx], mm0	\
	__asm add		ecx, 4			\

#define _KenKunNRT_Exit() \
	__asm jnz		_KenKunNRT_Loop	\
	__asm pop		edi				\
	__asm pop		esi				\
	__asm pop		edx				\
	__asm pop		ecx				\
	__asm pop		ebx				\
	__asm pop		eax				\

#else
#define _KenKunNRT_Init(rows, dest, rowSize, params) \
	__m128i mm0, mm1, mm2, mm3, mm4, mm5, mm6, mm7;							\
	int i;																	\
	int j = rowSize;														\
																			\
	while (j < 0) {															\

#define _KenKunNRT_Begin() \
		i = 0;																\
		mm7 = _mm_setzero_si128();											\
		mm2 = _mm_cvtsi32_si128(*(int*)(rows[i*3+0]+j));					\
		mm2 = _mm_unpacklo_epi8(mm2, mm7);									\
		mm1 = _mm_cvtsi32_si128(*(int*)(rows[i*3+2]+j));					\
		mm1 = _mm_unpacklo_epi8(mm1, mm7);									\
		mm0 = _mm_cvtsi32_si128(*(int*)(rows[i*3+1]+j));					\
		mm0 = _mm_unpacklo_epi8(mm0, mm7);									\
		i++;																\
		mm1 = _mm_adds_epi16(mm1, mm2);										\
		mm1 = _mm_adds_epi16(mm1, mm0);										\

#define _KenKunNRT_Proc() \
		mm5 = _mm_setzero_si128();											\
		mm4 = _mm_cvtsi32_si128(*(int*)(rows[i*3+0]+j));					\
		mm4 = _mm_unpacklo_epi8(mm4, mm5);									\
		mm3 = _mm_cvtsi32_si128(*(int*)(rows[i*3+2]+j));					\
		mm3 = _mm_unpacklo_epi8(mm3, mm5);									\
		mm2 = _mm_cvtsi32_si128(*(int*)(rows[i*3+1]+j));					\
		mm2 = _mm_unpacklo_epi8(mm2, mm5);									\
		i++;																\
		mm3 = _mm_adds_epi16(mm3, mm4);										\
		mm4 = mm2;															\
		mm3 = _mm_adds_epi16(mm3, mm2);										\
		mm4 = _mm_subs_epi16(mm4, mm0);										\
		mm3 = _mm_subs_epi16(mm3, mm1);										\
		mm5 = mm4;															\
		mm6 = mm3;															\
		mm4 = _mm_srai_epi16(mm4, 15);										\
		mm3 = _mm_srai_epi16(mm3, 15);										\
		mm4 = _mm_xor_si128(mm4, mm5);										\
		mm3 = _mm_xor_si128(mm3, mm6);										\
		mm4 = _mm_subs_epu16(mm4, _mm_loadl_epi64((__m128i*)params));		\
		mm3 = _mm_subs_epu16(mm3, _mm_loadl_epi64((__m128i*)params));		\
		mm4 = _mm_adds_epi16(mm4, mm4);										\
		mm4 = _mm_adds_epi16(mm4, mm3);										\
		mm4 = _mm_adds_epi16(mm4, mm4);										\
		mm2 = _mm_subs_epi16(mm2, mm0);										\
		mm5 = mm2;															\
		mm2 = _mm_srai_epi16(mm2, 15);										\
		mm5 = _mm_xor_si128(mm5, mm2);										\
		mm5 = _mm_subs_epu16(mm5, mm4);										\
		mm5 = _mm_xor_si128(mm5, mm2);										\
		mm7 = _mm_adds_epi16(mm7, mm5);										\

#define _KenKunNRT_End() \
		mm5 = _mm_xor_si128(mm5, mm2);										\
		mm7 = _mm_adds_epi16(mm7, mm5);										\
		mm7 = _mm_mulhi_epi16(mm7, _mm_loadl_epi64((__m128i*)(params+1)));	\
		mm0 = _mm_adds_epi16(mm0, mm7);										\
		mm0 = _mm_packus_epi16(mm0, mm0);									\
		*(int*)(dest+j) = _mm_cvtsi128_si32(mm0);							\
		j += 4;																\

#define _KenKunNRT_Exit() \
	}
#endif

template<int RADIUS> struct KenKunNRT_MMX {};

template<> struct KenKunNRT_MMX<1> {
	static void Proc(const BYTE **rows, BYTE *dest, int rowSize, __int64 *params) {
		_KenKunNRT_Init(rows, dest, rowSize, params)
		_KenKunNRT_Begin()
		_KenKunNRT_Proc()
		_KenKunNRT_Proc()
		_KenKunNRT_End()
		_KenKunNRT_Exit()
	}
};

template<> struct KenKunNRT_MMX<2> {
	static void Proc(const BYTE **rows, BYTE *dest, int rowSize, __int64 *params) {
		_KenKunNRT_Init(rows, dest, rowSize, params)
		_KenKunNRT_Begin()
		_KenKunNRT_Proc()
		_KenKunNRT_Proc()
		_KenKunNRT_Proc()
		_KenKunNRT_Proc()
		_KenKunNRT_End()
		_KenKunNRT_Exit()
	}
};

template<> struct KenKunNRT_MMX<3> {
	static void Proc(const BYTE **rows, BYTE *dest, int rowSize, __int64 *params) {
		_KenKunNRT_Init(rows, dest, rowSize, params)
		_KenKunNRT_Begin()
		_KenKunNRT_Proc()
		_KenKunNRT_Proc()
		_KenKunNRT_Proc()
		_KenKunNRT_Proc()
		_KenKunNRT_Proc()
		_KenKunNRT_Proc()
		_KenKunNRT_End()
		_KenKunNRT_Exit()
	}
};

#undef _KenKunNRT_Init
#undef _KenKunNRT_Begin
#undef _KenKunNRT_Proc
#undef _KenKunNRT_End
#undef _KenKunNRT_Exit

//////////////////////////////////////////////////////////////////////////////

class KenKunNRT : public GenericVideoFilter {
	enum {
		MAX_RADIUS = 3,
		MAX_KERNEL = MAX_RADIUS * 2 + 1
	};
	int _strength, _radius, _threshold;

	KenKunNRT(const PClip& clip, int strength, int radius, int threshold, IScriptEnvironment *env)
		: GenericVideoFilter(clip)
		, _strength (std::min(std::max(strength,  0), 256))
		, _radius   (std::min(std::max(radius,    1), int(MAX_RADIUS)))
		, _threshold(std::min(std::max(threshold, 0), 256))
	{ child->SetCacheHints(CACHE_RANGE, _radius * 2); }

	static AVSValue __cdecl Create(AVSValue args, void *user_data, IScriptEnvironment *env) {
		PClip child(args[0].AsClip());
		const VideoInfo& vi = child->GetVideoInfo();
		int strength = args[1].AsInt(256);
		int radius = args[2].AsInt(2);
		int threshold = args[3].AsInt(20);

		if(!vi.IsYUV())
			env->ThrowError("%s: YUV専用", GetName());

		if(!(env->GetCPUFlags() & CPUF_MMX))
			env->ThrowError("%s: MMX専用", GetName());

		if(strength == 0)
			return child;

		return new KenKunNRT(child, strength, radius, threshold, env);
	}

	static void Proc(int plane, const PVideoFrame& target, const PVideoFrame *frames, int strength, int radius, int threshold) {
		int kernel = radius * 2 + 1;
		__int64 params[] = {
			0x0001000100010001 * threshold,
			0x0001000100010001 * ((strength * (1ui64 << 55) / kernel + (1ui64 << 46)) >> 47),
		};
		const BYTE *rows[MAX_KERNEL][3];
		int pitches[MAX_KERNEL];
		int rowSize = -(frames[0]->GetRowSize(plane) & ~3);
		int height = frames[0]->GetHeight(plane);

		for(int i = 0; i < kernel; ++i) {
			rows[i][0] = rows[i][1] = rows[i][2] = frames[i]->GetReadPtr(plane) - rowSize;
			pitches[i] = frames[i]->GetPitch(plane);
		}

		BYTE *dest = target->GetWritePtr(plane) - rowSize;
		int pitch = target->GetPitch(plane);

		for(int y = 0; y < height; ++y) {
			for(int i = 0; i < kernel; ++i) {
				rows[i][0] = rows[i][1];
				rows[i][1] = rows[i][2];

				if(y + 1 < height)
					rows[i][2] += pitches[i];
			}

			switch(radius) {
			case 1: KenKunNRT_MMX<1>::Proc(rows[0], dest, rowSize, params); break;
			case 2: KenKunNRT_MMX<2>::Proc(rows[0], dest, rowSize, params); break;
			case 3: KenKunNRT_MMX<3>::Proc(rows[0], dest, rowSize, params); break;
			}

			dest += pitch;
		}
	}

public:
	static const char *GetName()
	{ return "KenKunNRT"; }

	static void PluginInit(IScriptEnvironment *env)
	{ env->AddFunction(GetName(), "c[strength]i[radius]i[threshold]i", Create, 0); }

	PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment *env) {
		PVideoFrame frames[MAX_KERNEL];

		for(int i = 0; i < _radius; ++i)
			frames[i + 1] = child->GetFrame(std::max(n + i - _radius, 0), env);

		frames[0] = child->GetFrame(n, env);

		for(int i = 0; i < _radius; ++i)
			frames[i + 1 + _radius] = child->GetFrame(std::min(n + i + 1, vi.num_frames - 1), env);

		PVideoFrame target(env->NewVideoFrame(vi));
		Proc(PLANAR_Y, target, frames, _strength, _radius, _threshold);

		if(vi.IsYV12()) {
			Proc(PLANAR_U, target, frames, _strength, _radius, _threshold);
			Proc(PLANAR_V, target, frames, _strength, _radius, _threshold);
		}

#if !defined(_M_X64)
		__asm emms
#endif
		return target;
	}
};

//////////////////////////////////////////////////////////////////////////////

#if !(defined(_M_X64) || defined(USE_SIMD_INTRINSICS))
#define _KenKunNR_Init(rows, dest, rowSize, params) \
	__asm push		eax				\
	__asm push		ebx				\
	__asm push		ecx				\
	__asm push		edx				\
	__asm push		esi				\
	__asm push		edi				\
	__asm mov		esi, rows		\
	__asm mov		edi, dest		\
	__asm mov		ecx, rowSize	\
	__asm mov		ebx, params		\
	__asm _KenKunNR_Loop:			\

#define _KenKunNR_Exit() \
	__asm jnz		_KenKunNR_Loop	\
	__asm pop		edi				\
	__asm pop		esi				\
	__asm pop		edx				\
	__asm pop		ecx				\
	__asm pop		ebx				\
	__asm pop		eax				\

#define _KenKunNR_Proc() \
	__asm psubsw	mm1, mm0	\
	__asm psubsw	mm4, mm0	\
	__asm movq		mm2, mm1	\
	__asm movq		mm5, mm4	\
	__asm psraw		mm2, 15		\
	__asm psraw		mm5, 15		\
	__asm pxor		mm1, mm2	\
	__asm pxor		mm4, mm5	\
	__asm movq		mm3, mm1	\
	__asm movq		mm6, mm4	\
	__asm psubusw	mm3, [ebx]	\
	__asm psubusw	mm6, [ebx]	\
	__asm paddsw	mm3, mm3	\
	__asm paddsw	mm6, mm6	\
	__asm psubusw	mm1, mm3	\
	__asm psubusw	mm4, mm6	\
	__asm pxor		mm1, mm2	\
	__asm pxor		mm4, mm5	\
	__asm paddsw	mm7, mm1	\
	__asm paddsw	mm7, mm4	\

#define _KenKunNR_Next() \
	__asm mov		eax, [edx]	\
	__asm add		edx, 4		\

#define _KenKunNR_Begin() \
	__asm mov		edx, esi		\
	__asm mov		eax, [edx]		\
	__asm pxor		mm7, mm7		\
	__asm movd		mm0, [eax+ecx]	\
	__asm add		edx, 4			\
	__asm punpcklbw	mm0, mm7		\

#define _KenKunNR_Get(off) \
	__asm movd		mm1, [eax+ecx-off]	\
	__asm pxor		mm6, mm6			\
	__asm movd		mm4, [eax+ecx+off]	\
	__asm punpcklbw	mm1, mm6			\
	__asm punpcklbw	mm4, mm6			\

#define _KenKunNR_Get0() \
	__asm movd		mm1, [eax+ecx]	\
	__asm pxor		mm6, mm6		\
	__asm mov		eax, [edx]		\
	__asm add		edx, 4			\
	__asm movd		mm4, [eax+ecx]	\
	__asm punpcklbw	mm1, mm6		\
	__asm punpcklbw	mm4, mm6		\

#define _KenKunNR_End() \
	__asm pmulhw	mm7, [ebx+8]	\
	__asm paddsw	mm0, mm7		\
	__asm packuswb	mm0, mm0		\
	__asm movd		[edi+ecx], mm0	\
	__asm add		ecx, 4			\

#define _KenKunNR_BeginY() \
	__asm mov		edx, esi		\
	__asm mov		eax, [edx]		\
	__asm add		edx, 4			\
	__asm movq		mm0, [eax+ecx]	\
	__asm pxor		mm7, mm7		\
	__asm psllw		mm0, 8			\
	__asm psrlw		mm0, 8			\

#define _KenKunNR_GetY(off) \
	__asm movq		mm1, [eax+ecx-off]	\
	__asm movq		mm4, [eax+ecx+off]	\
	__asm psllw		mm1, 8				\
	__asm psllw		mm4, 8				\
	__asm psrlw		mm1, 8				\
	__asm psrlw		mm4, 8				\

#define _KenKunNR_GetY0() \
	__asm movq		mm1, [eax+ecx]	\
	__asm mov		eax, [edx]		\
	__asm add		edx, 4			\
	__asm movq		mm4, [eax+ecx]	\
	__asm psllw		mm1, 8			\
	__asm psllw		mm4, 8			\
	__asm psrlw		mm1, 8			\
	__asm psrlw		mm4, 8			\

#define _KenKunNR_EndY() \
	__asm pmulhw	mm7, [ebx+8]	\
	__asm paddsw	mm0, mm7		\
	__asm movq		[edi+ecx], mm0	\

#define _KenKunNR_BeginC() \
	__asm mov		edx, esi		\
	__asm mov		eax, [edx]		\
	__asm add		edx, 4			\
	__asm movq		mm0, [eax+ecx]	\
	__asm pxor		mm7, mm7		\
	__asm psrlw		mm0, 8			\

#define _KenKunNR_GetC(off) \
	__asm movq		mm1, [eax+ecx-off]	\
	__asm movq		mm4, [eax+ecx+off]	\
	__asm psrlw		mm1, 8				\
	__asm psrlw		mm4, 8				\

#define _KenKunNR_GetC0() \
	__asm movq		mm1, [eax+ecx]	\
	__asm mov		eax, [edx]		\
	__asm add		edx, 4			\
	__asm movq		mm4, [eax+ecx]	\
	__asm psrlw		mm1, 8			\
	__asm psrlw		mm4, 8			\

#define _KenKunNR_EndC() \
	__asm pmulhw	mm7, [ebx+16]	\
	__asm paddsw	mm0, mm7		\
	__asm psllw		mm0, 8			\
	__asm por		mm0, [edi+ecx]	\
	__asm movq		[edi+ecx], mm0	\
	__asm add		ecx, 8			\

#else
#define _KenKunNR_Init(rows, dest, rowSize, params) \
	__m128i mm0, mm1, mm2, mm3, mm4, mm5, mm6, mm7;							\
	int i, j;																	\
	int k = rowSize;														\
																			\
	while (k < 0) {															\

#define _KenKunNR_Exit() \
	}

#define _KenKunNR_Proc() \
		mm1 = _mm_subs_epi16(mm1, mm0);										\
		mm4 = _mm_subs_epi16(mm4, mm0);										\
		mm2 = mm1;															\
		mm5 = mm4;															\
		mm2 = _mm_srai_epi16(mm2, 15);										\
		mm5 = _mm_srai_epi16(mm5, 15);										\
		mm1 = _mm_xor_si128(mm1, mm2);										\
		mm4 = _mm_xor_si128(mm4, mm5);										\
		mm3 = mm1;															\
		mm6 = mm4;															\
		mm3 = _mm_subs_epu16(mm3, _mm_loadl_epi64((__m128i*)params));		\
		mm6 = _mm_subs_epu16(mm6, _mm_loadl_epi64((__m128i*)params));		\
		mm3 = _mm_adds_epi16(mm3, mm3);										\
		mm6 = _mm_adds_epi16(mm6, mm6);										\
		mm1 = _mm_subs_epu16(mm1, mm3);										\
		mm4 = _mm_subs_epu16(mm4, mm6);										\
		mm1 = _mm_xor_si128(mm1, mm2);										\
		mm4 = _mm_xor_si128(mm4, mm5);										\
		mm7 = _mm_adds_epi16(mm7, mm1);										\
		mm7 = _mm_adds_epi16(mm7, mm4);										\

#define _KenKunNR_Next() \
		j = i; i++;															\

#define _KenKunNR_Begin() \
		i = 0; j = 0;														\
		mm7 = _mm_setzero_si128();											\
		mm0 = _mm_cvtsi32_si128(*(int*)(rows[j]+k));						\
		i++;																\
		mm0 = _mm_unpacklo_epi8(mm0, mm7);									\

#define _KenKunNR_Get(off) \
		mm1 = _mm_cvtsi32_si128(*(int*)(rows[j]+k-off));					\
		mm6 = _mm_setzero_si128();											\
		mm4 = _mm_cvtsi32_si128(*(int*)(rows[j]+k+off));					\
		mm1 = _mm_unpacklo_epi8(mm1, mm6);									\
		mm4 = _mm_unpacklo_epi8(mm4, mm6);									\

#define _KenKunNR_Get0() \
		mm1 = _mm_cvtsi32_si128(*(int*)(rows[j]+k));						\
		mm6 = _mm_setzero_si128();											\
		j = i; i++;															\
		mm4 = _mm_cvtsi32_si128(*(int*)(rows[j]+k));						\
		mm1 = _mm_unpacklo_epi8(mm1, mm6);									\
		mm4 = _mm_unpacklo_epi8(mm4, mm6);									\

#define _KenKunNR_End() \
		mm7 = _mm_mulhi_epi16(mm7, _mm_loadl_epi64((__m128i*)(params+1)));	\
		mm0 = _mm_adds_epi16(mm0, mm7);										\
		mm0 = _mm_packus_epi16(mm0, mm0);									\
		*(int*)(dest+k) = _mm_cvtsi128_si32(mm0);							\
		k += 4;																\

#define _KenKunNR_BeginY() \
		i = 0; j = 0;														\
		mm0 = _mm_loadl_epi64((__m128i*)(rows[j]+k));						\
		i++;																\
		mm7 = _mm_setzero_si128();											\
		mm0 = _mm_slli_epi16(mm0, 8);										\
		mm0 = _mm_srli_epi16(mm0, 8);										\

#define _KenKunNR_GetY(off) \
		mm1 = _mm_loadl_epi64((__m128i*)(rows[j]+k-off));					\
		mm4 = _mm_loadl_epi64((__m128i*)(rows[j]+k+off));					\
		mm1 = _mm_slli_epi16(mm1, 8);										\
		mm4 = _mm_slli_epi16(mm4, 8);										\
		mm1 = _mm_srli_epi16(mm1, 8);										\
		mm4 = _mm_srli_epi16(mm4, 8);										\

#define _KenKunNR_GetY0() \
		mm1 = _mm_loadl_epi64((__m128i*)(rows[j]+k));						\
		j = i; i++;															\
		mm4 = _mm_loadl_epi64((__m128i*)(rows[j]+k));						\
		mm1 = _mm_slli_epi16(mm1, 8);										\
		mm4 = _mm_slli_epi16(mm4, 8);										\
		mm1 = _mm_srli_epi16(mm1, 8);										\
		mm4 = _mm_srli_epi16(mm4, 8);										\

#define _KenKunNR_EndY() \
		mm7 = _mm_mulhi_epi16(mm7, _mm_loadl_epi64((__m128i*)(params+1)));	\
		mm0 = _mm_adds_epi16(mm0, mm7);										\
		_mm_storel_epi64((__m128i*)(dest+k), mm0);							\

#define _KenKunNR_BeginC() \
		i = 0; j = 0;														\
		mm0 = _mm_loadl_epi64((__m128i*)(rows[j]+k));						\
		i++;																\
		mm7 = _mm_setzero_si128();											\
		mm0 = _mm_srli_epi16(mm0, 8);										\

#define _KenKunNR_GetC(off) \
		mm1 = _mm_loadl_epi64((__m128i*)(rows[j]+k-off));					\
		mm4 = _mm_loadl_epi64((__m128i*)(rows[j]+k+off));					\
		mm1 = _mm_srli_epi16(mm1, 8);										\
		mm4 = _mm_srli_epi16(mm4, 8);										\

#define _KenKunNR_GetC0() \
		mm1 = _mm_loadl_epi64((__m128i*)(rows[j]+k));						\
		j = i; i++;															\
		mm4 = _mm_loadl_epi64((__m128i*)(rows[j]+k));						\
		mm1 = _mm_srli_epi16(mm1, 8);										\
		mm4 = _mm_srli_epi16(mm4, 8);										\

#define _KenKunNR_EndC() \
		mm7 = _mm_mulhi_epi16(mm7, _mm_loadl_epi64((__m128i*)(params+2)));	\
		mm0 = _mm_adds_epi16(mm0, mm7);										\
		mm0 = _mm_slli_epi16(mm0, 8);										\
		mm0 = _mm_or_si128(mm0, _mm_loadl_epi64((__m128i*)(dest+k)));		\
		_mm_storel_epi64((__m128i*)(dest+k), mm0);							\
		k += 8;																\

#endif

template<int TYPE, int RADIUS> struct KenKunNR_MMX {};

template<> struct KenKunNR_MMX<VideoInfo::CS_YV12, 1> {
	enum {
		PixelsY = 9,
		PixelsC = 5,
	};

	static void ProcY(const BYTE **rows, BYTE *dest, int rowSize, __int64 *params) {
		_KenKunNR_Init(rows, dest, rowSize, params)

		_KenKunNR_Begin()
		_KenKunNR_Get(1) _KenKunNR_Proc() _KenKunNR_Next()
		_KenKunNR_Get(1) _KenKunNR_Proc()
		_KenKunNR_Get0() _KenKunNR_Proc()
		_KenKunNR_Get(1) _KenKunNR_Proc()
		_KenKunNR_End()

		_KenKunNR_Exit()
	}

	static void ProcC(const BYTE **rows, BYTE *dest, int rowSize, __int64 *params) {
		_KenKunNR_Init(rows, dest, rowSize, params)

		_KenKunNR_Begin()
		_KenKunNR_Get(1) _KenKunNR_Proc() _KenKunNR_Next()
		_KenKunNR_Get0() _KenKunNR_Proc()
		_KenKunNR_End()

		_KenKunNR_Exit()
	}
};

template<> struct KenKunNR_MMX<VideoInfo::CS_YV12, 2> {
	enum {
		PixelsY = 21,
		PixelsC = 9,
	};

	static void ProcY(const BYTE **rows, BYTE *dest, int rowSize, __int64 *params) {
		_KenKunNR_Init(rows, dest, rowSize, params)

		_KenKunNR_Begin()
		_KenKunNR_Get(1) _KenKunNR_Proc()
		_KenKunNR_Get(2) _KenKunNR_Proc() _KenKunNR_Next()
		_KenKunNR_Get(1) _KenKunNR_Proc()
		_KenKunNR_Get(2) _KenKunNR_Proc()
		_KenKunNR_Get0() _KenKunNR_Proc()
		_KenKunNR_Get(1) _KenKunNR_Proc()
		_KenKunNR_Get(2) _KenKunNR_Proc() _KenKunNR_Next()
		_KenKunNR_Get(1) _KenKunNR_Proc()
		_KenKunNR_Get0() _KenKunNR_Proc()
		_KenKunNR_Get(1) _KenKunNR_Proc()
		_KenKunNR_End()

		_KenKunNR_Exit()
	}

	static void ProcC(const BYTE **rows, BYTE *dest, int rowSize, __int64 *params) {
		_KenKunNR_Init(rows, dest, rowSize, params)

		_KenKunNR_Begin()
		_KenKunNR_Get(1) _KenKunNR_Proc() _KenKunNR_Next()
		_KenKunNR_Get(1) _KenKunNR_Proc()
		_KenKunNR_Get0() _KenKunNR_Proc()
		_KenKunNR_Get(1) _KenKunNR_Proc()
		_KenKunNR_End()

		_KenKunNR_Exit()
	}
};

template<> struct KenKunNR_MMX<VideoInfo::CS_YV12, 3> {
	enum {
		PixelsY = 37,
		PixelsC = 13,
	};

	static void ProcY(const BYTE **rows, BYTE *dest, int rowSize, __int64 *params) {
		_KenKunNR_Init(rows, dest, rowSize, params)

		_KenKunNR_Begin()
		_KenKunNR_Get(1) _KenKunNR_Proc()
		_KenKunNR_Get(2) _KenKunNR_Proc()
		_KenKunNR_Get(3) _KenKunNR_Proc() _KenKunNR_Next()
		_KenKunNR_Get(1) _KenKunNR_Proc()
		_KenKunNR_Get(2) _KenKunNR_Proc()
		_KenKunNR_Get(3) _KenKunNR_Proc()
		_KenKunNR_Get0() _KenKunNR_Proc()
		_KenKunNR_Get(1) _KenKunNR_Proc()
		_KenKunNR_Get(2) _KenKunNR_Proc()
		_KenKunNR_Get(3) _KenKunNR_Proc() _KenKunNR_Next()
		_KenKunNR_Get(1) _KenKunNR_Proc()
		_KenKunNR_Get(2) _KenKunNR_Proc()
		_KenKunNR_Get0() _KenKunNR_Proc()
		_KenKunNR_Get(1) _KenKunNR_Proc()
		_KenKunNR_Get(2) _KenKunNR_Proc() _KenKunNR_Next()
		_KenKunNR_Get(1) _KenKunNR_Proc()
		_KenKunNR_Get0() _KenKunNR_Proc()
		_KenKunNR_Get(1) _KenKunNR_Proc()
		_KenKunNR_End()

		_KenKunNR_Exit()
	}

	static void ProcC(const BYTE **rows, BYTE *dest, int rowSize, __int64 *params) {
		_KenKunNR_Init(rows, dest, rowSize, params)

		_KenKunNR_Begin()
		_KenKunNR_Get(1) _KenKunNR_Proc()
		_KenKunNR_Get(2) _KenKunNR_Proc() _KenKunNR_Next()
		_KenKunNR_Get(1) _KenKunNR_Proc()
		_KenKunNR_Get0() _KenKunNR_Proc()
		_KenKunNR_Get(1) _KenKunNR_Proc() _KenKunNR_Next()
		_KenKunNR_Get0() _KenKunNR_Proc()
		_KenKunNR_End()

		_KenKunNR_Exit()
	}
};

template<> struct KenKunNR_MMX<VideoInfo::CS_YUY2, 1> {
	enum {
		PixelsY = 9,
		PixelsC = 5,
	};

	static void Proc(const BYTE **rows, BYTE *dest, int rowSize, __int64 *params) {
		_KenKunNR_Init(rows, dest, rowSize, params)

		_KenKunNR_BeginY()
		_KenKunNR_GetY(2) _KenKunNR_Proc() _KenKunNR_Next()
		_KenKunNR_GetY(2) _KenKunNR_Proc()
		_KenKunNR_GetY0() _KenKunNR_Proc()
		_KenKunNR_GetY(2) _KenKunNR_Proc()
		_KenKunNR_EndY()

		_KenKunNR_BeginC()
		_KenKunNR_GetC(4) _KenKunNR_Proc() _KenKunNR_Next()
		_KenKunNR_GetC0() _KenKunNR_Proc()
		_KenKunNR_EndC()

		_KenKunNR_Exit()
	}
};

template<> struct KenKunNR_MMX<VideoInfo::CS_YUY2, 2> {
	enum {
		PixelsY = 21,
		PixelsC = 11,
	};

	static void Proc(const BYTE **rows, BYTE *dest, int rowSize, __int64 *params) {
		_KenKunNR_Init(rows, dest, rowSize, params)

		_KenKunNR_BeginY()
		_KenKunNR_GetY(2) _KenKunNR_Proc()
		_KenKunNR_GetY(4) _KenKunNR_Proc() _KenKunNR_Next()
		_KenKunNR_GetY(2) _KenKunNR_Proc()
		_KenKunNR_GetY(4) _KenKunNR_Proc()
		_KenKunNR_GetY0() _KenKunNR_Proc()
		_KenKunNR_GetY(2) _KenKunNR_Proc()
		_KenKunNR_GetY(4) _KenKunNR_Proc() _KenKunNR_Next()
		_KenKunNR_GetY(2) _KenKunNR_Proc()
		_KenKunNR_GetY0() _KenKunNR_Proc()
		_KenKunNR_GetY(2) _KenKunNR_Proc()
		_KenKunNR_EndY()

		_KenKunNR_BeginC()
		_KenKunNR_GetC(4) _KenKunNR_Proc() _KenKunNR_Next()
		_KenKunNR_GetC(4) _KenKunNR_Proc()
		_KenKunNR_GetC0() _KenKunNR_Proc()
		_KenKunNR_GetC(4) _KenKunNR_Proc() _KenKunNR_Next()
		_KenKunNR_GetC0() _KenKunNR_Proc()
		_KenKunNR_EndC()

		_KenKunNR_Exit()
	}
};

template<> struct KenKunNR_MMX<VideoInfo::CS_YUY2, 3> {
	enum {
		PixelsY = 37,
		PixelsC = 19,
	};

	static void Proc(const BYTE **rows, BYTE *dest, int rowSize, __int64 *params) {
		_KenKunNR_Init(rows, dest, rowSize, params)

		_KenKunNR_BeginY()
		_KenKunNR_GetY(2) _KenKunNR_Proc()
		_KenKunNR_GetY(4) _KenKunNR_Proc()
		_KenKunNR_GetY(6) _KenKunNR_Proc() _KenKunNR_Next()
		_KenKunNR_GetY(2) _KenKunNR_Proc()
		_KenKunNR_GetY(4) _KenKunNR_Proc()
		_KenKunNR_GetY(6) _KenKunNR_Proc()
		_KenKunNR_GetY0() _KenKunNR_Proc()
		_KenKunNR_GetY(2) _KenKunNR_Proc()
		_KenKunNR_GetY(4) _KenKunNR_Proc()
		_KenKunNR_GetY(6) _KenKunNR_Proc() _KenKunNR_Next()
		_KenKunNR_GetY(2) _KenKunNR_Proc()
		_KenKunNR_GetY(4) _KenKunNR_Proc()
		_KenKunNR_GetY0() _KenKunNR_Proc()
		_KenKunNR_GetY(2) _KenKunNR_Proc()
		_KenKunNR_GetY(4) _KenKunNR_Proc() _KenKunNR_Next()
		_KenKunNR_GetY(2) _KenKunNR_Proc()
		_KenKunNR_GetY0() _KenKunNR_Proc()
		_KenKunNR_GetY(2) _KenKunNR_Proc()
		_KenKunNR_EndY()

		_KenKunNR_BeginC()
		_KenKunNR_GetC(4) _KenKunNR_Proc()
		_KenKunNR_GetC(8) _KenKunNR_Proc() _KenKunNR_Next()
		_KenKunNR_GetC(4) _KenKunNR_Proc()
		_KenKunNR_GetC0() _KenKunNR_Proc()
		_KenKunNR_GetC(4) _KenKunNR_Proc() _KenKunNR_Next()
		_KenKunNR_GetC(4) _KenKunNR_Proc()
		_KenKunNR_GetC0() _KenKunNR_Proc()
		_KenKunNR_GetC(4) _KenKunNR_Proc() _KenKunNR_Next()
		_KenKunNR_GetC0() _KenKunNR_Proc()
		_KenKunNR_EndC()

		_KenKunNR_Exit()
	}
};

#undef _KenKunNR_Proc
#undef _KenKunNR_Next
#undef _KenKunNR_Init
#undef _KenKunNR_Exit

#undef _KenKunNR_Begin
#undef _KenKunNR_Get
#undef _KenKunNR_Get0
#undef _KenKunNR_End

#undef _KenKunNR_BeginY
#undef _KenKunNR_GetY
#undef _KenKunNR_GetY0
#undef _KenKunNR_EndY

#undef _KenKunNR_BeginC
#undef _KenKunNR_GetC
#undef _KenKunNR_GetC0
#undef _KenKunNR_EndC

//////////////////////////////////////////////////////////////////////////////

class KenKunNR : public GenericVideoFilter {
	enum {
		MAX_RADIUS = 3,
		MAX_KERNEL = MAX_RADIUS * 2 + 1,
	};

	std::vector<BYTE, aligned_allocator<BYTE, 16> > _buffer;
	int _strength, _radius, _threshold;

	KenKunNR(const PClip& clip, int strength, int radius, int threshold, IScriptEnvironment *env)
		: GenericVideoFilter(clip)
		, _strength (std::min(std::max(strength,  0), 256))
		, _radius   (std::min(std::max(radius,    1), int(MAX_RADIUS)))
		, _threshold(std::min(std::max(threshold, 0), 256))
	{ child->SetCacheHints(CACHE_NOTHING, 0); }

	static AVSValue __cdecl Create(AVSValue args, void *user_data, IScriptEnvironment *env) {
		PClip child(args[0].AsClip());
		const VideoInfo& vi = child->GetVideoInfo();
		int strength = args[1].AsInt(256);
		int radius = args[2].AsInt(2);
		int threshold = args[3].AsInt(6);

		if(!vi.IsYUV())
			env->ThrowError("%s: YUV専用", GetName());

		if(!(env->GetCPUFlags() & CPUF_MMX))
			env->ThrowError("%s: MMX専用", GetName());

		if(strength == 0)
			return child;

		return new KenKunNR(child, strength, radius, threshold, env);
	}

	static void CopyYUY2(const BYTE *src, BYTE *dst, int rowSize, int margin) {
		memcpy(dst, src, rowSize);
		BYTE *s0 = dst, *s1 = dst + rowSize - 4;

		for(int i = 0; i < margin; i += 4) {
			BYTE *d0 = dst - 4 - i, *d1 = dst + rowSize + i;

			d0[0] = d0[2] = s0[0];
			d0[1] = s0[1];
			d0[3] = s0[3];

			d1[0] = d1[2] = s1[2];
			d1[1] = s1[1];
			d1[3] = s1[3];
		}
	}

	void ProcYUY2(const PVideoFrame& dstFrame, const PVideoFrame& srcFrame) {
		static const int pixels[][2] = {
			{ 1, 1 },
			{ KenKunNR_MMX<VideoInfo::CS_YUY2, 1>::PixelsY, KenKunNR_MMX<VideoInfo::CS_YUY2, 1>::PixelsC },
			{ KenKunNR_MMX<VideoInfo::CS_YUY2, 2>::PixelsY, KenKunNR_MMX<VideoInfo::CS_YUY2, 2>::PixelsC },
			{ KenKunNR_MMX<VideoInfo::CS_YUY2, 3>::PixelsY, KenKunNR_MMX<VideoInfo::CS_YUY2, 3>::PixelsC },
		};
		__int64 params[] = {
			0x0001000100010001 * _threshold,
			0x0001000100010001 * ((_strength * (1ui64 << 55) / pixels[_radius][0] + (1ui64 << 46)) >> 47),
			0x0001000100010001 * ((_strength * (1ui64 << 55) / pixels[_radius][1] + (1ui64 << 46)) >> 47),
		};

		const BYTE *src = srcFrame->GetReadPtr();
		int srcPitch = srcFrame->GetPitch();

		BYTE *dst = dstFrame->GetWritePtr();
		int dstPitch = dstFrame->GetPitch();

		int rowSize = srcFrame->GetRowSize() & ~7;
		int kernel = _radius * 2 + 1;
		int margin = (_radius + 1) / 2 * 4;
		int rowSize2 = rowSize + margin * 2;

		if(_buffer.empty())
			_buffer.resize(rowSize2 * kernel);

		BYTE *rows[MAX_KERNEL];
		rows[0] = &_buffer[margin];

		for(int i = 1; i < kernel; ++i)
			rows[i] = rows[i - 1] + rowSize2;

		for(int i = 0; i < _radius && i < vi.height; ++i) {
			CopyYUY2(src, rows[_radius + i + 1], rowSize, margin);
			src += srcPitch;
		}

		for(int y = 0; y < vi.height; ++y) {
			BYTE *last = rows[0];

			for(int i = 1; i < kernel; ++i)
				rows[i - 1] = rows[i];

			rows[kernel - 1] = last;

			if(y + _radius < vi.height) {
				CopyYUY2(src, last, rowSize, margin);
				src += srcPitch;
			}

			const BYTE *rows2[MAX_KERNEL];
			rows2[0] = rows[_radius] + rowSize;

			for(int i = 1; i <= _radius; ++i) {
				rows2[i * 2 - 1] = rows[_radius - std::min(i, y)] + rowSize;
				rows2[i * 2 + 0] = rows[_radius + std::min(i, vi.height - y - 1)] + rowSize;
			}

			switch(_radius) {
			case 1: KenKunNR_MMX<VideoInfo::CS_YUY2, 1>::Proc(rows2, dst + rowSize, -rowSize, params); break;
			case 2: KenKunNR_MMX<VideoInfo::CS_YUY2, 2>::Proc(rows2, dst + rowSize, -rowSize, params); break;
			case 3: KenKunNR_MMX<VideoInfo::CS_YUY2, 3>::Proc(rows2, dst + rowSize, -rowSize, params); break;
			}

			dst += dstPitch;
		}
	}

	static void CopyYV12(const BYTE *src, BYTE *dst, int rowSize, int margin) {
		memcpy(dst, src, rowSize);
		memset(dst - margin, *dst, margin);
		memset(dst + rowSize, dst[rowSize - 1], margin);
	}

	void ProcYV12(const PVideoFrame& dstFrame, const PVideoFrame& srcFrame, int plane) {
		static const int pixels[][2] = {
			{ 1, 1 },
			{ KenKunNR_MMX<VideoInfo::CS_YV12, 1>::PixelsY, KenKunNR_MMX<VideoInfo::CS_YV12, 1>::PixelsC },
			{ KenKunNR_MMX<VideoInfo::CS_YV12, 2>::PixelsY, KenKunNR_MMX<VideoInfo::CS_YV12, 2>::PixelsC },
			{ KenKunNR_MMX<VideoInfo::CS_YV12, 3>::PixelsY, KenKunNR_MMX<VideoInfo::CS_YV12, 3>::PixelsC },
		};
		__int64 params[] = {
			0x0001000100010001 * _threshold,
			0x0001000100010001 * ((_strength * (1ui64 << 55) / pixels[_radius][plane != PLANAR_Y] + (1ui64 << 46)) >> 47),
		};

		const BYTE *src = srcFrame->GetReadPtr(plane);
		int srcPitch = srcFrame->GetPitch(plane);

		BYTE *dst = dstFrame->GetWritePtr(plane);
		int dstPitch = dstFrame->GetPitch(plane);

		int rowSize = srcFrame->GetRowSize(plane) & ~3;
		int kernel = _radius * 2 + 1;
		int margin = (_radius + 3) & ~3;
		int rowSize2 = rowSize + margin * 2;
		int height = srcFrame->GetHeight(plane);

		if(_buffer.empty())
			_buffer.resize(rowSize2 * kernel);

		BYTE *rows[MAX_KERNEL];
		rows[0] = &_buffer[margin];

		for(int i = 1; i < kernel; ++i)
			rows[i] = rows[i - 1] + rowSize2;

		for(int i = 0; i < _radius && i < height; ++i) {
			CopyYV12(src, rows[_radius + i + 1], rowSize, margin);
			src += srcPitch;
		}

		for(int y = 0; y < height; ++y) {
			BYTE *last = rows[0];

			for(int i = 1; i < kernel; ++i)
				rows[i - 1] = rows[i];

			rows[kernel - 1] = last;

			if(y + _radius < height) {
				CopyYV12(src, last, rowSize, margin);
				src += srcPitch;
			}

			const BYTE *rows2[MAX_KERNEL];
			rows2[0] = rows[_radius] + rowSize;

			for(int i = 1; i <= _radius; ++i) {
				rows2[i * 2 - 1] = rows[_radius - std::min(i, y)] + rowSize;
				rows2[i * 2 + 0] = rows[_radius + std::min(i, height - y - 1)] + rowSize;
			}

			if(plane == PLANAR_Y) {
				switch(_radius) {
				case 1: KenKunNR_MMX<VideoInfo::CS_YV12, 1>::ProcY(rows2, dst + rowSize, -rowSize, params); break;
				case 2: KenKunNR_MMX<VideoInfo::CS_YV12, 2>::ProcY(rows2, dst + rowSize, -rowSize, params); break;
				case 3: KenKunNR_MMX<VideoInfo::CS_YV12, 3>::ProcY(rows2, dst + rowSize, -rowSize, params); break;
				}
			}
			else {
				switch(_radius) {
				case 1: KenKunNR_MMX<VideoInfo::CS_YV12, 1>::ProcC(rows2, dst + rowSize, -rowSize, params); break;
				case 2: KenKunNR_MMX<VideoInfo::CS_YV12, 2>::ProcC(rows2, dst + rowSize, -rowSize, params); break;
				case 3: KenKunNR_MMX<VideoInfo::CS_YV12, 3>::ProcC(rows2, dst + rowSize, -rowSize, params); break;
				}
			}

			dst += dstPitch;
		}
	}

public:
	static const char *GetName()
	{ return "KenKunNR"; }

	static void PluginInit(IScriptEnvironment *env)
	{ env->AddFunction(GetName(), "c[strength]i[radius]i[threshold]i", Create, 0); }

	PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment *env) {
		PVideoFrame srcFrame(child->GetFrame(n, env));
		PVideoFrame *dstFrame = &srcFrame, newFrame;

		if(!srcFrame->IsWritable()) {
			newFrame = env->NewVideoFrame(vi);
			dstFrame = &newFrame;
		}

		if(vi.IsYUY2()) {
			ProcYUY2(*dstFrame, srcFrame);
		}
		else if(vi.IsYV12()) {
			ProcYV12(*dstFrame, srcFrame, PLANAR_Y);
			ProcYV12(*dstFrame, srcFrame, PLANAR_U);
			ProcYV12(*dstFrame, srcFrame, PLANAR_V);
		}

#if !defined(_M_X64)
		__asm emms
#endif
		return *dstFrame;
	}
};

//////////////////////////////////////////////////////////////////////////////
