//////////////////////////////////////////////////////////////////////////////
#include <intrin.h>

class AutoDeint : public GenericVideoFilter {
	static void DeintMMX(BYTE *dstPtr, const BYTE **curTbl, const BYTE **preTbl, int rowSize) {
#if !(defined(_M_X64) || defined(USE_SIMD_INTRINSICS))
		__asm {
			push		eax
			push		ebx
			push		ecx
			push		edx
			push		esi

			mov			eax, dstPtr
			mov			ebx, curTbl
			mov			ecx, rowSize
			mov			edx, preTbl

		loopX:
			mov			esi, [ebx+0]
			pxor		mm7, mm7
			movd		mm6, [esi+ecx]

			mov			esi, [ebx+4]
			punpcklbw	mm6, mm7
			movd		mm3, [esi+ecx]

			mov			esi, [ebx+8]
			punpcklbw	mm3, mm7
			movd		mm0, [esi+ecx]

			mov			esi, [ebx+12]
			punpcklbw	mm0, mm7
			movd		mm5, [esi+ecx]

			mov			esi, [ebx+16]
			punpcklbw	mm5, mm7
			movd		mm2, [esi+ecx]

			mov			esi, [edx+8]
			punpcklbw	mm2, mm7
			movd		mm1, [esi+ecx]

			paddsw		mm2, mm6
			punpcklbw	mm1, mm7

			movq		mm4, mm5
			paddsw		mm5, mm3
			psubsw		mm4, mm3
			psraw		mm5, 1
			movq		mm3, mm4
			psraw		mm4, 15
			pxor		mm4, mm3
			psraw		mm2, 1
			movq		mm7, mm0
			psubsw		mm0, mm2
			movq		mm6, mm7
			movq		mm3, mm0
			psraw		mm0, 15
			paddsw		mm6, mm1
			pxor		mm0, mm3
			psraw		mm6, 1
			paddsw		mm0, mm4
			movq		mm4, mm5
			psubsw		mm7, mm1
			psubsw		mm4, mm2
			movq		mm3, mm7
			movq		mm2, mm4
			psraw		mm7, 15
			psraw		mm4, 15
			pxor		mm7, mm3
			pxor		mm4, mm2
			psubsw		mm0, mm7
			pcmpgtw		mm0, mm4
			pand		mm6, mm0
			pandn		mm0, mm5
			por			mm6, mm0

			mov			esi, [edx+12]
			pxor		mm4, mm4
			movd		mm0, [esi+ecx]

			mov			esi, [edx+16]
			punpcklbw	mm0, mm4
			movd		mm2, [esi+ecx]

			mov			esi, [edx+4]
			punpcklbw	mm2, mm4
			movd		mm3, [esi+ecx]

			mov			esi, [edx+0]
			punpcklbw	mm3, mm4
			psubsw		mm0, mm3
			movd		mm3, [esi+ecx]

			punpcklbw	mm3, mm4
			paddsw		mm2, mm3

			movq		mm3, mm0
			psraw		mm0, 15
			psraw		mm2, 1
			pxor		mm0, mm3
			psubsw		mm1, mm2
			movq		mm4, mm5
			movq		mm3, mm1
			psubsw		mm4, mm2
			psraw		mm1, 15
			movq		mm2, mm4
			pxor		mm1, mm3
			psraw		mm4, 15
			paddsw		mm1, mm0
			pxor		mm4, mm2
			psubsw		mm1, mm7
			pcmpgtw		mm1, mm4
			pand		mm6, mm1
			pandn		mm1, mm5
			por			mm6, mm1

			packuswb	mm6, mm6
			movd		[eax+ecx], mm6
			add			ecx, 4
			jnz			loopX

			pop			esi
			pop			edx
			pop			ecx
			pop			ebx
			pop			eax
		}
#else
		__m128i mm0, mm1, mm2, mm3, mm4, mm5, mm6, mm7;
		int i = rowSize;

		while (i < 0) {
			mm7 = _mm_setzero_si128();
			mm6 = _mm_cvtsi32_si128(*(int*)(curTbl[0]+i));
			mm6 = _mm_unpacklo_epi8(mm6, mm7);

			mm3 = _mm_cvtsi32_si128(*(int*)(curTbl[1]+i));
			mm3 = _mm_unpacklo_epi8(mm3, mm7);

			mm0 = _mm_cvtsi32_si128(*(int*)(curTbl[2]+i));
			mm0 = _mm_unpacklo_epi8(mm0, mm7);

			mm5 = _mm_cvtsi32_si128(*(int*)(curTbl[3]+i));
			mm5 = _mm_unpacklo_epi8(mm5, mm7);

			mm2 = _mm_cvtsi32_si128(*(int*)(curTbl[4]+i));
			mm2 = _mm_unpacklo_epi8(mm2, mm7);

			mm1 = _mm_cvtsi32_si128(*(int*)(preTbl[2]+i));
			mm1 = _mm_unpacklo_epi8(mm1, mm7);

			mm2 = _mm_adds_epi16(mm2, mm6);

			mm4 = mm5;
			mm5 = _mm_adds_epi16(mm5, mm3);
			mm4 = _mm_subs_epi16(mm4, mm3);
			mm5 = _mm_srai_epi16(mm5, 1);
			mm3 = mm4;
			mm4 = _mm_srai_epi16(mm4, 15);
			mm4 = _mm_xor_si128(mm4, mm3);
			mm2 = _mm_srai_epi16(mm2, 1);
			mm7 = mm0;
			mm0 = _mm_subs_epi16(mm0, mm2);
			mm6 = mm7;
			mm3 = mm0;
			mm0 = _mm_srai_epi16(mm0, 15);
			mm6 = _mm_adds_epi16(mm6, mm1);
			mm0 = _mm_xor_si128(mm0, mm3);
			mm6 = _mm_srai_epi16(mm6, 1);
			mm0 = _mm_adds_epi16(mm0, mm4);
			mm4 = mm5;
			mm7 = _mm_subs_epi16(mm7, mm1);
			mm4 = _mm_subs_epi16(mm4, mm2);
			mm3 = mm7;
			mm2 = mm4;
			mm7 = _mm_srai_epi16(mm7, 15);
			mm4 = _mm_srai_epi16(mm4, 15);
			mm7 = _mm_xor_si128(mm7, mm3);
			mm4 = _mm_xor_si128(mm4, mm2);
			mm0 = _mm_subs_epi16(mm0, mm7);
			mm0 = _mm_cmpgt_epi16(mm0, mm4);
			mm6 = _mm_and_si128(mm6, mm0);
			mm0 = _mm_andnot_si128(mm0, mm5);
			mm6 = _mm_or_si128(mm6, mm0);

			mm4 = _mm_setzero_si128();
			mm0 = _mm_cvtsi32_si128(*(int*)(preTbl[3]+i));
			mm0 = _mm_unpacklo_epi8(mm0, mm4);

			mm2 = _mm_cvtsi32_si128(*(int*)(preTbl[4]+i));
			mm2 = _mm_unpacklo_epi8(mm2, mm4);

			mm3 = _mm_cvtsi32_si128(*(int*)(preTbl[1]+i));
			mm3 = _mm_unpacklo_epi8(mm3, mm4);
			mm0 = _mm_subs_epi16(mm0, mm3);

			mm3 = _mm_cvtsi32_si128(*(int*)(preTbl[0]+i));
			mm3 = _mm_unpacklo_epi8(mm3, mm4);
			mm2 = _mm_adds_epi16(mm2, mm3);

			mm3 = mm0;
			mm0 = _mm_srai_epi16(mm0, 15);
			mm2 = _mm_srai_epi16(mm2, 1);
			mm0 = _mm_xor_si128(mm0, mm3);
			mm1 = _mm_subs_epi16(mm1, mm2);
			mm4 = mm5;
			mm3 = mm1;
			mm4 = _mm_subs_epi16(mm4, mm2);
			mm1 = _mm_srai_epi16(mm1, 15);
			mm2 = mm4;
			mm1 = _mm_xor_si128(mm1, mm3);
			mm4 = _mm_srai_epi16(mm4, 15);
			mm1 = _mm_adds_epi16(mm1, mm0);
			mm4 = _mm_xor_si128(mm4, mm2);
			mm1 = _mm_subs_epi16(mm1, mm7);
			mm1 = _mm_cmpgt_epi16(mm1, mm4);
			mm6 = _mm_and_si128(mm6, mm1);
			mm1 = _mm_andnot_si128(mm1, mm5);
			mm6 = _mm_or_si128(mm6, mm1);

			mm6 = _mm_packus_epi16(mm6, mm6);	//src only [31:0]
			*(int*)(dstPtr+i) = _mm_cvtsi128_si32(mm6);
			i += 4;
		}
#endif

	}

	static void BlendMMX(BYTE *dstPtr, const BYTE **ptrTbl, int rowSize) {
#if !(defined(_M_X64) || defined(USE_SIMD_INTRINSICS))
		__asm {
			push		eax
			push		ebx
			push		ecx
			push		esi

			mov			eax, dstPtr
			mov			ebx, ptrTbl
			mov			ecx, rowSize

		loopX:
			mov			esi, [ebx+16]
			pxor		mm5, mm5
			movd		mm0, [esi+ecx]

			mov			esi, [ebx+12]
			punpcklbw	mm0, mm5
			movd		mm1, [esi+ecx]

			mov			esi, [ebx+8]
			punpcklbw	mm1, mm5
			movd		mm2, [esi+ecx]

			mov			esi, [ebx+4]
			punpcklbw	mm2, mm5
			movd		mm3, [esi+ecx]

			mov			esi, [ebx]
			punpcklbw	mm3, mm5
			movd		mm4, [esi+ecx]

			movq		mm7, mm1
			punpcklbw	mm4, mm5
			movq		mm6, mm2
			psubsw		mm0, mm1
			paddsw		mm7, mm3
			psubsw		mm1, mm2
			paddsw		mm7, mm6
			psubsw		mm2, mm3
			paddsw		mm7, mm6
			psubsw		mm3, mm4
			psraw		mm7, 2
			movq		mm4, mm1
			movq		mm5, mm0
			psubsw		mm6, mm7
			pxor		mm5, mm1
			pxor		mm4, mm2
			psraw		mm5, 15
			psraw		mm4, 15
			pand		mm0, mm5
			pand		mm1, mm4
			movq		mm5, mm2
			pand		mm2, mm4
			pxor		mm5, mm3
			movq		mm4, mm1
			psraw		mm5, 15
			psraw		mm1, 15
			pand		mm3, mm5
			pxor		mm1, mm4
			movq		mm5, mm2
			movq		mm4, mm0
			psraw		mm2, 15
			psraw		mm0, 15
			pxor		mm2, mm5
			pxor		mm0, mm4
			movq		mm5, mm3
			movq		mm4, mm2
			psraw		mm3, 15
			pcmpgtw		mm2, mm1
			pxor		mm3, mm5
			pand		mm1, mm2
			paddsw		mm0, mm3
			pandn		mm2, mm4
			movq		mm5, mm0
			por			mm1, mm2
			pcmpgtw		mm0, mm1
			movq		mm4, mm6
			pand		mm1, mm0
			psraw		mm4, 15
			pandn		mm0, mm5
			pxor		mm6, mm4
			por			mm0, mm1
			psubusw		mm6, mm0
			pxor		mm6, mm4
			paddsw		mm7, mm6

			packuswb	mm7, mm7
			movd		[eax+ecx], mm7
			add			ecx, 4
			jnz			loopX

			pop			esi
			pop			ecx
			pop			ebx
			pop			eax
		}
#else
		__m128i mm0, mm1, mm2, mm3, mm4, mm5, mm6, mm7;
		int i = rowSize;

		while (i < 0) {
			mm5 = _mm_setzero_si128();
			mm0 = _mm_cvtsi32_si128(*(int*)(ptrTbl[4]+i));
			mm0 = _mm_unpacklo_epi8(mm0, mm5);

			mm1 = _mm_cvtsi32_si128(*(int*)(ptrTbl[3]+i));
			mm1 = _mm_unpacklo_epi8(mm1, mm5);

			mm2 = _mm_cvtsi32_si128(*(int*)(ptrTbl[2]+i));
			mm2 = _mm_unpacklo_epi8(mm2, mm5);

			mm3 = _mm_cvtsi32_si128(*(int*)(ptrTbl[1]+i));
			mm3 = _mm_unpacklo_epi8(mm3, mm5);

			mm4 = _mm_cvtsi32_si128(*(int*)(ptrTbl[0]+i));
			mm4 = _mm_unpacklo_epi8(mm4, mm5);

			mm7 = mm1;
			mm6 = mm2;
			mm0 = _mm_subs_epi16(mm0, mm1);
			mm7 = _mm_adds_epi16(mm7, mm3);
			mm1 = _mm_subs_epi16(mm1, mm2);
			mm7 = _mm_adds_epi16(mm7, mm6);
			mm2 = _mm_subs_epi16(mm2, mm3);
			mm7 = _mm_adds_epi16(mm7, mm6);
			mm3 = _mm_subs_epi16(mm3, mm4);
			mm7 = _mm_srai_epi16(mm7, 2);
			mm4 = mm1;
			mm5 = mm0;
			mm6 = _mm_subs_epi16(mm6, mm7);
			mm5 = _mm_xor_si128(mm5, mm1);
			mm4 = _mm_xor_si128(mm4, mm2);
			mm5 = _mm_srai_epi16(mm5, 15);
			mm4 = _mm_srai_epi16(mm4, 15);
			mm0 = _mm_and_si128(mm0, mm5);
			mm1 = _mm_and_si128(mm1, mm4);
			mm5 = mm2;
			mm2 = _mm_and_si128(mm2, mm4);
			mm5 = _mm_xor_si128(mm5, mm3);
			mm4 = mm1;
			mm5 = _mm_srai_epi16(mm5, 15);
			mm1 = _mm_srai_epi16(mm1, 15);
			mm3 = _mm_and_si128(mm3, mm5);
			mm1 = _mm_xor_si128(mm1, mm4);
			mm5 = mm2;
			mm4 = mm0;
			mm2 = _mm_srai_epi16(mm2, 15);
			mm0 = _mm_srai_epi16(mm0, 15);
			mm2 = _mm_xor_si128(mm2, mm5);
			mm0 = _mm_xor_si128(mm0, mm4);
			mm5 = mm3;
			mm4 = mm2;
			mm3 = _mm_srai_epi16(mm3, 15);
			mm2 = _mm_cmpgt_epi16(mm2, mm1);
			mm3 = _mm_xor_si128(mm3, mm5);
			mm1 = _mm_and_si128(mm1, mm2);
			mm0 = _mm_adds_epi16(mm0, mm3);
			mm2 = _mm_andnot_si128(mm2, mm4);
			mm5 = mm0;
			mm1 = _mm_or_si128(mm1, mm2);
			mm0 = _mm_cmpgt_epi16(mm0, mm1);
			mm4 = mm6;
			mm1 = _mm_and_si128(mm1, mm0);
			mm4 = _mm_srai_epi16(mm4, 15);
			mm0 = _mm_andnot_si128(mm0, mm5);

			mm6 = _mm_xor_si128(mm6, mm4);
			mm0 = _mm_or_si128(mm0, mm1);
			mm6 = _mm_subs_epu16(mm6, mm0);
			mm6 = _mm_xor_si128(mm6, mm4);
			mm7 = _mm_adds_epi16(mm7, mm6);

			mm7 = _mm_packus_epi16(mm7, mm7);
			*(int*)(dstPtr+i) = _mm_cvtsi128_si32(mm7);
			i += 4;
		}
#endif
	}

	//----------------------------------------------------------------------------

	int _mode;
	bool _same;

	AutoDeint(const PClip& clip, const char *mode, IScriptEnvironment *env)
		: GenericVideoFilter(clip), _mode(0), _same(false)
	{
		if(strcmp(mode, "blend") == 0) {
			_mode = 1;
			_same = true;
		}

		if(_same)
			child->SetCacheHints(CACHE_NOTHING, 0);
		else
			child->SetCacheHints(CACHE_RANGE, 1);
	}

	static AVSValue __cdecl Create(AVSValue args, void *user_data, IScriptEnvironment *env) {
		PClip child(args[0].AsClip());
		const VideoInfo& vi = child->GetVideoInfo();
		const char *mode = args[1].AsString("");

		if(!vi.IsYUV())
			env->ThrowError("%s: YUV専用", GetName());

		if(!(env->GetCPUFlags() & CPUF_MMX))
			env->ThrowError("%s: MMX専用", GetName());

		return new AutoDeint(child, mode, env);
	}

public:
	static const char *GetName()
	{ return "AutoDeint"; }

	static void PluginInit(IScriptEnvironment *env)
	{ env->AddFunction(GetName(), "c[mode]s", Create, 0); }

	PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment *env) {
		PVideoFrame dstFrame(env->NewVideoFrame(vi));
		PVideoFrame curFrame(child->GetFrame(n, env));
		PVideoFrame preFrame(curFrame);
		if(n > 0 && !_same) preFrame = child->GetFrame(n - 1, env);

		static const int plane[] = { PLANAR_Y, PLANAR_U, PLANAR_V };
		int planes = vi.IsYUY2() ? 1 : 3;
		int parity = child->GetParity(n);

		for(int i = 0; i < planes; ++i) {
			int p = plane[i];

			switch(_mode) {
			case 0:
				Deint(
					dstFrame->GetWritePtr(p), dstFrame->GetPitch(p),
					curFrame->GetReadPtr(p), curFrame->GetPitch(p),
					preFrame->GetReadPtr(p), preFrame->GetPitch(p),
					dstFrame->GetRowSize(p), dstFrame->GetHeight(p), parity);
				break;
			case 1:
				Blend(
					dstFrame->GetWritePtr(p), dstFrame->GetPitch(p),
					curFrame->GetReadPtr(p), curFrame->GetPitch(p),
					preFrame->GetReadPtr(p), preFrame->GetPitch(p),
					dstFrame->GetRowSize(p), dstFrame->GetHeight(p), parity);
				break;
			}
		}

		return dstFrame;
	}

	static void Deint(
		BYTE *dstPtr, int dstPitch,
		const BYTE *curPtr, int curPitch,
		const BYTE *prePtr, int prePitch,
		int rowSize, int height, int parity)
	{
		rowSize &= ~3;
		dstPtr += rowSize;

		const BYTE *curTbl[5], *preTbl[5];
		curTbl[1] = curTbl[2] = curTbl[3] = curPtr + rowSize;
		preTbl[1] = preTbl[2] = preTbl[3] = prePtr + rowSize;
		curTbl[4] = curTbl[3] + curPitch;
		preTbl[4] = preTbl[3] + prePitch;

		for(; height; --height) {
			curTbl[0] = curTbl[1]; curTbl[1] = curTbl[2]; curTbl[2] = curTbl[3]; curTbl[3] = curTbl[4];
			preTbl[0] = preTbl[1]; preTbl[1] = preTbl[2]; preTbl[2] = preTbl[3]; preTbl[3] = preTbl[4];

			if(height > 2) {
				curTbl[4] = curTbl[3] + curPitch;
				preTbl[4] = preTbl[3] + prePitch;
			}

			if(parity ^= 1)
				DeintMMX(dstPtr, curTbl, preTbl, -rowSize);
			else
				memcpy(dstPtr - rowSize, curTbl[2] - rowSize, rowSize);

			dstPtr += dstPitch;
		}

#if !(defined(_M_X64) || defined(USE_SIMD_INTRINSICS))
		_mm_empty();
#endif
	}

	static void Blend(
		BYTE *dstPtr, int dstPitch,
		const BYTE *curPtr, int curPitch,
		const BYTE *prePtr, int prePitch,
		int rowSize, int height, int parity)
	{
		rowSize &= ~3;
		dstPtr += rowSize;

		const BYTE *ptr[] = { prePtr, curPtr };
		int pitch[] = { prePitch, curPitch };

		const BYTE *tbl[5];
		tbl[1] = ptr[parity] + rowSize;
		tbl[2] = ptr[parity ^ 1] + rowSize;
		tbl[3] = tbl[1];
		tbl[4] = tbl[2] + pitch[parity ^ 1];

		for(; height; --height) {
			tbl[0] = tbl[1]; tbl[1] = tbl[2];
			tbl[2] = tbl[3]; tbl[3] = tbl[4];

			if(height > 2)
				tbl[4] = tbl[2] + pitch[parity] * 2;
			else
				tbl[4] = tbl[2];

			BlendMMX(dstPtr, tbl, -rowSize);
			dstPtr += dstPitch;
			parity ^= 1;
		}

#if !(defined(_M_X64) || defined(USE_SIMD_INTRINSICS))
		_mm_empty();
#endif
	}
};

//////////////////////////////////////////////////////////////////////////////

class Auto24FPS : public GenericVideoFilter {
	static void AnalyzeMMX(
		const BYTE *curPtr, int curPitch,
		const BYTE *prePtr, int prePitch,
		int rowSize, int threshold,
		int& curCount, int& preCount)
	{
		int width = rowSize / 4;
		WORD count[4];
#if !(defined(_M_X64) || defined(USE_SIMD_INTRINSICS))
		WORD thr[] = { threshold, threshold, threshold, threshold };

		__asm {
			push		ebx
			push		ecx
			push		edx
			push		esi
			push		edi

			pxor		mm7, mm7
			movq		mm5, thr
			mov			esi, curPtr
			mov			edi, prePtr
			mov			ecx, curPitch
			mov			ebx, esi
			mov			edx, esi
			sub			ebx, ecx
			add			edx, ecx
			mov			ecx, width

		loopX:
			pxor		mm2, mm2
			movd		mm6, [ebx]
			movd		mm3, [edx]
			movd		mm0, [esi]
			movd		mm1, [edi]
			punpcklbw	mm6, mm2
			punpcklbw	mm3, mm2
			paddsw		mm6, mm3
			punpcklbw	mm0, mm2
			punpcklbw	mm1, mm2

			psraw		mm6, 1
			movq		mm4, mm0
			add			ebx, 4
			psubsw		mm4, mm1
			add			edx, 4
			movq		mm2, mm4
			add			esi, 4
			psraw		mm4, 15
			add			edi, 4
			pxor		mm4, mm2
			psubsw		mm0, mm6
			psubsw		mm1, mm6
			psubsw		mm4, mm5
			movq		mm2, mm0
			movq		mm3, mm1
			psraw		mm0, 15
			psraw		mm1, 15
			pxor		mm0, mm2
			pxor		mm1, mm3
			pcmpgtw		mm0, mm4
			pcmpgtw		mm1, mm4

			movq		mm2, mm0
			punpcklwd	mm0, mm1
			punpckhwd	mm2, mm1
			psubsw		mm7, mm0
			psubsw		mm7, mm2

			dec			ecx
			jnz			loopX
			movq		count, mm7

			pop			edi
			pop			esi
			pop			edx
			pop			ecx
			pop			ebx
		}
#else
		__m128i mm0, mm1, mm2, mm3, mm4, mm5, mm6, mm7;
		const BYTE *cur_p = curPtr;
		const BYTE *pre_p = prePtr;

		mm7 = _mm_setzero_si128();
		mm5 = _mm_set1_epi16((WORD)threshold);
		for (int i = width; i > 0; i--) {
			mm2 = _mm_setzero_si128();
			mm6 = _mm_cvtsi32_si128(*(int*)(cur_p-curPitch));
			mm3 = _mm_cvtsi32_si128(*(int*)(cur_p+curPitch));
			mm0 = _mm_cvtsi32_si128(*(int*)cur_p);
			mm1 = _mm_cvtsi32_si128(*(int*)pre_p);
			mm6 = _mm_unpacklo_epi8(mm6, mm2);
			mm3 = _mm_unpacklo_epi8(mm3, mm2);
			mm6 = _mm_adds_epi16(mm6, mm3);
			mm0 = _mm_unpacklo_epi8(mm0, mm2);
			mm1 = _mm_unpacklo_epi8(mm1, mm2);
			cur_p += 4;
			pre_p += 4;

			mm6 = _mm_srai_epi16(mm6, 1);
			mm4 = mm0;
			mm4 = _mm_subs_epi16(mm4, mm1);
			mm2 = mm4;
			mm4 = _mm_srai_epi16(mm4, 15);
			mm4 = _mm_xor_si128(mm4, mm2);
			mm0 = _mm_subs_epi16(mm0, mm6);
			mm1 = _mm_subs_epi16(mm1, mm6);
			mm4 = _mm_subs_epi16(mm4, mm5);
			mm2 = mm0;
			mm3 = mm1;
			mm0 = _mm_srai_epi16(mm0, 15);
			mm1 = _mm_srai_epi16(mm1, 15);
			mm0 = _mm_xor_si128(mm0, mm2);
			mm1 = _mm_xor_si128(mm1, mm3);
			mm0 = _mm_cmpgt_epi16(mm0, mm4);
			mm1 = _mm_cmpgt_epi16(mm1, mm4);

			mm2 = mm0;
			mm0 = _mm_unpacklo_epi16(mm0, mm1);
			mm2 = _mm_unpackhi_epi16(mm2, mm1);	// unpacklwd -> unpackhi & shr
			mm2 = _mm_srli_si128(mm2, 8);
			mm7 = _mm_subs_epi16(mm7, mm0);
			mm7 = _mm_subs_epi16(mm7, mm2);
		}
		_mm_storel_epi64((__m128i*)count, mm7);
#endif

		curCount += rowSize - count[0] - count[2];
		preCount += rowSize - count[1] - count[3];
	}

	//----------------------------------------------------------------------------

	static void Analyze(
		const BYTE *curPtr, int curPitch,
		const BYTE *prePtr, int prePitch,
		int rowSize, int height, int parity,
		int threshold, int& curCount, int& preCount)
	{
		curPtr += parity * curPitch;
		prePtr += parity * prePitch;
		height /= 2;

		for(; height; --height) {
			AnalyzeMMX(curPtr, curPitch, prePtr, prePitch, rowSize, threshold, curCount, preCount);
			curPtr += curPitch * 2;
			prePtr += prePitch * 2;
		}

#if !(defined(_M_X64) || defined(USE_SIMD_INTRINSICS))
		_mm_empty();
#endif
	}

	static void Copy(
		BYTE *dstPtr, int dstPitch,
		const BYTE *curPtr, int curPitch,
		const BYTE *prePtr, int prePitch,
		int rowSize, int height, int parity)
	{
		const BYTE *ptr[] = { curPtr, prePtr };
		int pitch[] = { curPitch, prePitch };

		for(; height; --height) {
			memcpy(dstPtr, ptr[parity ^= 1], rowSize);
			dstPtr += dstPitch;
			ptr[0] += pitch[0];
			ptr[1] += pitch[1];
		}
	}

	//----------------------------------------------------------------------------

	struct Diff {
		int a, b;
		Diff(int n = -1) : a(n), b(n) {}
		bool operator!() const { return a < 0 || b < 0; }
	};

	struct Drop {
		char a, b;
		Drop() : a(-1), b(-1) {}
		bool operator!() const { return a < 0 || b < 0; }
	};

	std::vector<Diff> _diffs;
	std::vector<Drop> _drops;

	PClip _cropped;
	int _frames, _threshold, _mode;
	bool _drop, _debug;

	int DropFrame(int n, IScriptEnvironment *env) {
		int drop = AnalyzeDropFrame(n, env);
		if(n % 5 >= drop) ++n;
		return n;
	}

	int AnalyzeDropFrame(int n, IScriptEnvironment *env) {
		Drop drop = AnalyzeDrop(n, env);
		if(drop.a == drop.b) return drop.a;

		Drop prev = AnalyzeDrop(n - 5, NULL);
		if(!!prev && prev.a == drop.a) return drop.a;

		return drop.b;
	}

	Drop AnalyzeDrop(int n, IScriptEnvironment *env) {
		n /= 5;
		if(n < 0 || n >= _drops.size())
			return Drop();

		Drop& drop = _drops[n];
		if(!!drop || env == NULL) return drop;

		Diff diffs[13], *d = diffs + 4;
		n *= 5;

		for(int i = -4; i < 9; ++i)
			d[i] = AnalyzeDiff(n + i, env);

		int aa[5], bb[5];

		for(int i = 0; i < 5; ++i) {
			aa[i] = d[i-4].b + d[i-3].b + d[i-2].a + d[i-2].b + d[i-1].a + d[i-0].a;
			bb[i] = d[i+0].a + d[i+1].b + d[i+2].b + d[i+3].a + d[i+3].b + d[i+4].a;
		}

		drop.a = drop.b = 0;

		for(int i = 1; i < 5; ++i) {
			if(aa[i] < aa[drop.a]) drop.a = i;
			if(bb[i] < bb[drop.b]) drop.b = i;
		}

		return drop;
	}

	Diff AnalyzeDiff(int n, IScriptEnvironment *env) {
		if(n < 0 || n >= _frames)
			return Diff(0);

		Diff& diff = _diffs[n];
		if(!!diff || env == NULL) return diff;

		const PClip& child = _cropped;
		const VideoInfo& vi = _cropped->GetVideoInfo();

		PVideoFrame curFrame(child->GetFrame(n, env));
		PVideoFrame preFrame(curFrame);
		if(n > 0) preFrame = child->GetFrame(n - 1, env);

		diff.a = diff.b = 0;

		static const int plane[] = { PLANAR_Y, PLANAR_U, PLANAR_V };
		int planes = vi.IsYUY2() ? 1 : 3;
		int parity = child->GetParity(n);

		for(int i = 0; i < planes; ++i) {
			int p = plane[i];

			Analyze(
				curFrame->GetReadPtr(p), curFrame->GetPitch(p),
				preFrame->GetReadPtr(p), preFrame->GetPitch(p),
				curFrame->GetRowSize(p), curFrame->GetHeight(p), parity,
				_threshold, diff.b, diff.a);
		}

		return diff;
	}

	bool AnalyzeProc(int n, IScriptEnvironment *env) {
		Diff diff = AnalyzeDiff(n, env);
		return diff.a > diff.b;
	}

	//----------------------------------------------------------------------------

	Auto24FPS(const PClip& clip, bool drop, int threshold, int border, const char *mode, bool debug, IScriptEnvironment *env)
		: GenericVideoFilter(clip), _frames(vi.num_frames), _threshold(threshold), _mode(0), _drop(drop), _debug(debug)
	{
		if(border < 2)
			border = 2;
		else
			border = (border + 1) & ~1;

		int w = (vi.width - border * 2) & ~1;
		int h = (vi.height - border * 2) & ~1;

		AVSValue args[] = { child, border, border, w, h };
		_cropped = env->Invoke("Crop", AVSValue(args, sizeof(args) / sizeof(*args))).AsClip();

		if(strcmp(mode, "blend") == 0)
			_mode = 1;

		_diffs.resize(_frames);
		_drops.resize(_frames / 5 + 1);

		if(_drop) {
			vi.num_frames = vi.num_frames * 4 / 5;
			vi.SetFPS(vi.fps_numerator * 4, vi.fps_denominator * 5);
		}

		child->SetCacheHints(CACHE_RANGE, (_drop || _debug) ? 12 : 1);
	}

	static AVSValue __cdecl Create(AVSValue args, void *user_data, IScriptEnvironment *env) {
		PClip child(args[0].AsClip());
		const VideoInfo& vi = child->GetVideoInfo();
		bool drop = args[1].AsBool(true);
		int threshold = args[2].AsInt(24);
		int border = args[3].AsInt(16);
		const char *mode = args[4].AsString("");
		bool debug = args[5].AsBool(false);

		if(!vi.IsYUV())
			env->ThrowError("%s: YUV専用", GetName());

		if(!(env->GetCPUFlags() & CPUF_MMX))
			env->ThrowError("%s: MMX専用", GetName());

		return new Auto24FPS(child, drop, threshold, border, mode, debug, env);
	}

public:
	static const char *GetName()
	{ return "Auto24FPS"; }

	static void PluginInit(IScriptEnvironment *env)
	{ env->AddFunction(GetName(), "c[drop]b[threshold]i[border]i[mode]s[debug]b", Create, 0); }

	PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment *env) {
		if(_drop) n = DropFrame(n + n / 4, env);
		bool proc = AnalyzeProc(n, env);

		if(_debug) dout
			<< GetName()
			<< ": frame=" << n
			<< " a=" << _diffs[n].a
			<< " b=" << _diffs[n].b
			<< " proc=" << proc
			<< " drop=" << (n % 5 == AnalyzeDropFrame(n, env))
			<< std::endl;

		PVideoFrame curFrame(child->GetFrame(n, env));
		PVideoFrame preFrame(curFrame);

		if(!proc && _mode == 0)
			return curFrame;

		if(proc && n > 0)
			preFrame = child->GetFrame(n - 1, env);

		PVideoFrame dstFrame(env->NewVideoFrame(vi));

		static const int plane[] = { PLANAR_Y, PLANAR_U, PLANAR_V };
		int planes = vi.IsYUY2() ? 1 : 3;
		int parity = child->GetParity(n);

		for(int i = 0; i < planes; ++i) {
			int p = plane[i];

			switch(_mode) {
			case 0:
				Copy(
					dstFrame->GetWritePtr(p), dstFrame->GetPitch(p),
					curFrame->GetReadPtr(p), curFrame->GetPitch(p),
					preFrame->GetReadPtr(p), preFrame->GetPitch(p),
					dstFrame->GetRowSize(p), dstFrame->GetHeight(p), parity);
				break;
			case 1:
				AutoDeint::Blend(
					dstFrame->GetWritePtr(p), dstFrame->GetPitch(p),
					curFrame->GetReadPtr(p), curFrame->GetPitch(p),
					preFrame->GetReadPtr(p), preFrame->GetPitch(p),
					dstFrame->GetRowSize(p), dstFrame->GetHeight(p), parity);
				break;
			}
		}

		return dstFrame;
	}
};

//////////////////////////////////////////////////////////////////////////////
