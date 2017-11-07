//////////////////////////////////////////////////////////////////////////////
#include <intrin.h>
#include <mmintrin.h>
#include <xmmintrin.h>
#include <emmintrin.h>

#ifdef __ICL
#include <pmmintrin.h>
#include <tmmintrin.h>
#else
#include <intrin.h>
#endif

#define CPU_MMX        0x000001    /* mmx */
#define CPU_MMXEXT     0x000002    /* mmx-ext*/
#define CPU_SSE        0x000004    /* sse */
#define CPU_SSE2       0x000008    /* sse 2 */
#define CPU_3DNOW      0x000010    /* 3dnow! */
#define CPU_3DNOWEXT   0x000020    /* 3dnow! ext */
#define CPU_SSE3       0x000080    /* sse 3 */
#define CPU_SSSE3      0x000100    /* ssse 3 */
#define SHUF16(x15,x14,x13,x12,x11,x10,x9,x8,x7,x6,x5,x4,x3,x2,x1,x0) {x0,x1,x2,x3,x4,x5,x6,x7,x8,x9,x10,x11,x12,x13,x14,x15}
#define SHUF8(x7,x6,x5,x4,x3,x2,x1,x0) {(x0<<1), (x0<<1)+1, (x1<<1), (x1<<1)+1, (x2<<1), (x2<<1)+1, (x3<<1), (x3<<1)+1, (x4<<1), (x4<<1)+1, (x5<<1), (x5<<1)+1, (x6<<1), (x6<<1)+1, (x7<<1), (x7<<1)+1}

static int _mm_flags_detected = 0;
static int _mm_flags = 0;

inline void cpu_cpuid(int op, int *_eax, int *_ebx, int *_ecx, int *_edx){
	int CPUInfo[4] = { 0x0, 0x0, 0x0, 0x0};

	__cpuid(CPUInfo, op);
	*_eax = CPUInfo[0];
	*_ebx = CPUInfo[1];
	*_ecx = CPUInfo[2];
	*_edx = CPUInfo[3];
#if 0
	__asm	{
		mov		eax,op
		cpuid
		mov		edi,_eax
		mov     [edi],eax
		mov		edi,_ebx
		mov     [edi],ebx
		mov		edi,_ecx
		mov     [edi],ecx
		mov		edi,_edx
		mov     [edi],edx
	}
#endif
}

inline int cpu_detect(void){
	if(_mm_flags_detected)
		return _mm_flags;

    int cpu = 0;
    int eax, ebx, ecx, edx;
    int	b_amd;

	_mm_flags_detected = 1;
    cpu_cpuid( 0, &eax, &ebx, &ecx, &edx);
    if( eax == 0 )
        return 0;

    b_amd   = (ebx == 0x68747541) && (ecx == 0x444d4163) && (edx == 0x69746e65);

    cpu_cpuid( 1, &eax, &ebx, &ecx, &edx );
    if( (edx&0x00800000) == 0 )
        return 0;

    cpu = CPU_MMX;
    if( (edx&0x02000000) )
        cpu |= CPU_MMXEXT|CPU_SSE;

    if( (edx&0x04000000) )
        cpu |= CPU_SSE2;

    if( (ecx&0x00000001) )
        cpu |= CPU_SSE3;
    if( (ecx&0x00000200) )
        cpu |= CPU_SSSE3;

    cpu_cpuid( 0x80000000, &eax, &ebx, &ecx, &edx );
	if( eax < 0x80000001 ){
		_mm_flags = cpu;
        return cpu;
	}

    cpu_cpuid( 0x80000001, &eax, &ebx, &ecx, &edx );
    if( edx&0x80000000 )
        cpu |= CPU_3DNOW;
    if( b_amd && (edx&0x00400000) )
        cpu |= CPU_MMXEXT;

	_mm_flags = cpu;
    return cpu;
}










template<class PLUGIN_TABLE>
class AviUtlPluginObject : public RefCount<AviUtlPluginObject<PLUGIN_TABLE> > {
	LibraryObjectRef _library;
	const char *_name;
	PLUGIN_TABLE *_table;

	enum State { INIT, PROC, EXIT };
	State _state;

protected:
	AviUtlPluginObject(const char *path, const char *name, bool copy, const char *api)
		: _name(name), _table(NULL), _state(INIT)
	{
		if(copy)
			_library = new CopiedLibrary(path);
		else
			_library = new Library(path);

		typedef PLUGIN_TABLE *(__stdcall *GET_PLUGIN_TABLE)();
		if(!_library) return;

		void *func = _library->GetAddress(api);
		if(func == NULL) return;

		_table = static_cast<GET_PLUGIN_TABLE>(func)();
	}

	virtual bool OnInit() = 0;
	virtual bool OnExit() = 0;

public:
	typedef PLUGIN_TABLE PLUGIN_TABLE;

	virtual ~AviUtlPluginObject()
	{}

	bool operator!() const
	{ return _table == NULL; }

	bool Init() {
		if(_table == NULL)
			return false;

		if(_state == INIT && OnInit())
			_state = PROC;

		return _state == PROC;
	}

	bool Exit() {
		if(_table == NULL)
			return false;

		if(_state == PROC && OnExit())
			_state = EXIT;

		return _state == EXIT;
	}

	const LibraryObjectRef& GetLibrary() const
	{ return _library; }

	const char *GetFuncName() const
	{ return _name; }

	const char *GetInformation() const
	{ return (_table->information != NULL) ? _table->information : ""; }

	PLUGIN_TABLE *GetPluginTable() const
	{ return _table; }
};

//////////////////////////////////////////////////////////////////////////////
class ConvertYUY2ToAviUtlYC : public GenericVideoFilter {
	static int ConvertMMX(PIXEL_YC *yc, const BYTE *yuv, int width) {
		static const	__int64	_MULY = 0x000004AD000004AD;	//	1197,	1197
		static const	__int64	_MULC = 0x1249000012490000;	//	4681,	4681
		static const	__int64	_SUBY = 0x0000012B0000012B;	//	299,	299
		static const	__int64	_SUBC = 0x000923DC000923DC;	//	599004,	599004

#if !(defined(_M_X64) || defined(USE_SIMD_INTRINSICS))
		if(width /= 4) __asm {
				xor			esi,esi
				mov			ecx,width
				mov			eax,yuv
				mov			ebx,yc
				pxor		mm7,mm7

		loopX:
				movq		mm0,[eax+esi*8]
				pshufw		mm2,mm0,	(0<<6)+(0<<4)+(3<<2)+(2<<0)
				inc			esi

				;yuv4バイト分を取得
				punpcklbw	mm0,mm7		;yuv[3,2,1,0] cr, y, cb, y
				punpcklbw	mm2,mm7		;yuv[7,6,5,4] cr, y, cb, y
				movq		mm1,mm0
				movq		mm3,mm2

				;yuv2yc変換
				pmaddwd		mm0,_MULY	;yuv[2], yuv[0]
				psrad		mm0,6
				psubd		mm0,_SUBY
				pmaddwd		mm2,_MULY	;yuv[6], yuv[4]
				psrad		mm2,6
				psubd		mm2,_SUBY

				pmaddwd		mm1,_MULC	;yuv[3], yuv[1]
				psubd		mm1,_SUBC
				psrad		mm1,8
				pmaddwd		mm3,_MULC	;yuv[7], yuv[5]
				psubd		mm3,_SUBC
				psrad		mm3,8

				packssdw	mm0,mm1		;yuv[3], yuv[1], yuv[2], yuv[0]
				packssdw	mm2,mm3		;yuv[7], yuv[5], yuv[6], yuv[4]

					;yc[0].y	=	yuv[0]
					;yc[0].cb	=	yuv[1]
					;yc[0].cr	=	yuv[3]
					;yc[1].y	=	yuv[2]
				pshufw		mm4,mm0,(1<<6)+(3<<4)+(2<<2)+(0<<0)

					;yc[1].cb	=	yuv[1]
					;yc[1].cr	=	yuv[3]
					;yc[2].y	=	yuv[4]
					;yc[2].cb	=	yuv[5]
				pshufw		mm5,mm2,(0<<6)+(2<<4)+(0<<2)+(0<<0)
				punpckhdq	mm5,mm0		;yuv[3], yuv[1], yuv[4], yuv[5]
				pshufw		mm5,mm5,(0<<6)+(1<<4)+(3<<2)+(2<<0)

					;yc[2].cr	=	yuv[7]
					;yc[3].y	=	yuv[6]
					;yc[3].cb	=	yuv[5]
					;yc[3].cr	=	yuv[7]
				pshufw		mm6,mm2,(3<<6)+(2<<4)+(1<<2)+(3<<0)

				movntq		[ebx],mm4
				movntq		[ebx+8],mm5
				movntq		[ebx+16],mm6
				add			ebx,24
				dec			ecx
				jnz			loopX
		}
#else
		if(width /= 4) {
			__m128i mm0, mm1, mm2, mm3, mm4, mm5, mm6, mm7, mem1, mem2;

			mm7 = _mm_setzero_si128();
			for (int x = 0; x < width; x++) {
				mm0 = _mm_loadl_epi64((__m128i*)&yuv[x*8]);
				mm2 = _mm_shufflelo_epi16(mm0, (0<<6)+(0<<4)+(3<<2)+(2<<0));

				//yuv4バイト分を取得
				mm0 = _mm_unpacklo_epi8(mm0, mm7);              //yuv[3,2,1,0] cr, y, cb, y
				mm2 = _mm_unpacklo_epi8(mm2, mm7);              //yuv[7,6,5,4] cr, y, cb, y
				mm1 = mm0;
				mm3 = mm2;

				//yuv2yc変換
				mem1 = _mm_loadl_epi64((__m128i*)_MULY);
				mem2 = _mm_loadl_epi64((__m128i*)_SUBY);
				mm0 = _mm_madd_epi16(mm0, mem1);                //yuv[2], yuv[0]
				mm0 = _mm_srai_epi32(mm0, 6);
				mm0 = _mm_sub_epi32(mm0, mem2);
				mm2 = _mm_madd_epi16(mm2, mem1);                //yuv[6], yuv[4]
				mm2 = _mm_srai_epi32(mm2, 6);
				mm2 = _mm_sub_epi32(mm2, mem2);

				mem1 = _mm_loadl_epi64((__m128i*)_MULC);
				mem2 = _mm_loadl_epi64((__m128i*)_SUBC);
				mm1 = _mm_madd_epi16(mm1, mem1);                //yuv[3], yuv[1]
				mm1 = _mm_sub_epi32(mm1, mem2);
				mm1 = _mm_srai_epi32(mm1, 8);
				mm3 = _mm_madd_epi16(mm3, mem1);                //yuv[7], yuv[5]
				mm3 = _mm_sub_epi32(mm3, mem2);
				mm3 = _mm_srai_epi32(mm3, 8);

				mm0 = _mm_packs_epi16(mm0, mm1);                //yuv[3], yuv[1], yuv[2], yuv[0]
				mm2 = _mm_packs_epi16(mm2, mm3);                //yuv[7], yuv[5], yuv[6], yuv[4]

					//yc[0].y	=	yuv[0]
					//yc[0].cb	=	yuv[1]
					//yc[0].cr	=	yuv[3]
					//yc[1].y	=	yuv[2]

				mm4 = _mm_shufflelo_epi16(mm0, (1<<6)+(3<<4)+(2<<2)+(0<<0));

					//yc[1].cb	=	yuv[1]
					//yc[1].cr	=	yuv[3]
					//yc[2].y	=	yuv[4]
					//yc[2].cb	=	yuv[5]

				mm5 = _mm_shufflelo_epi16(mm2, (0<<6)+(2<<4)+(0<<2)+(0<<0));
				mm5 = _mm_unpacklo_epi32(mm5, mm0);             //punpckhdq mm -> punpckldq xmm + psrlq xmm
				mm5 = _mm_srli_si128(mm5, 8);                   //yuv[3], yuv[1], yuv[4], yuv[5]
				mm5 = _mm_shufflelo_epi16(mm5, (0<<6)+(1<<4)+(3<<2)+(2<<0));

					//yc[2].cr	=	yuv[7]
					//yc[3].y	=	yuv[6]
					//yc[3].cb	=	yuv[5]
					//yc[3].cr	=	yuv[7]
				mm6 = _mm_shufflelo_epi16(mm2, (3<<6)+(2<<4)+(1<<2)+(3<<0));

				_mm_storel_epi64((__m128i*)((char*)yc), mm4);
				_mm_storel_epi64((__m128i*)((char*)yc+8), mm5);
				_mm_storel_epi64((__m128i*)((char*)yc+16), mm6);
				yc += 4;
			}
		}
#endif

		return width * 4;
	}

	static int ConvertSSE3(PIXEL_YC *yc, const BYTE *yuv, int width) {
		static const __m128i	_MULY = {0xAD, 0x04, 0x00, 0x00, 0xAD, 0x04, 0x00, 0x00, 0xAD, 0x04, 0x00, 0x00, 0xAD, 0x04, 0x00, 0x00};
		static const __m128i	_MULC = {0x00, 0x00, 0x49, 0x12, 0x00, 0x00, 0x49, 0x12, 0x00, 0x00, 0x49, 0x12, 0x00, 0x00, 0x49, 0x12};
		static const __m128i	_SUBY = {0x2B, 0x01, 0x00, 0x00, 0x2B, 0x01, 0x00, 0x00, 0x2B, 0x01, 0x00, 0x00, 0x2B, 0x01, 0x00, 0x00};
		static const __m128i	_SUBC = {0xDC, 0x23, 0x09, 0x00, 0xDC, 0x23, 0x09, 0x00, 0xDC, 0x23, 0x09, 0x00, 0xDC, 0x23, 0x09, 0x00};

		static const __m128i    _SHUF_2 = SHUF8(6,2,5,4,1,5,4,0);
		static const __m128i    _SHUF_3 = SHUF8(0,0,0,0,7,6,3,7);
		static const __m128i    _SHUF_4 = SHUF8(0,0,0,0,1,5,4,0);
		static const __m128i    _SHUF_5 = SHUF8(7,6,3,7,6,2,5,4);

#if !(defined(_M_X64) || defined(USE_SIMD_INTRINSICS))
		if(width /= 8) __asm {
				mov			ecx,width
				mov			eax,yuv
				mov			ebx,yc
				pxor		xmm7,xmm7

		loopX:
				;lddqu
				lddqu		xmm0,xmmword ptr [eax]
				movdqa		xmm2,xmm0

				;yuv4バイト分を取得
				punpcklbw	xmm0,xmm7		;yuv[ 7, 6, 5, 4, 3, 2, 1, 0]
				punpckhbw	xmm2,xmm7		;yuv[15,14,13,12,11,10, 9, 8]
				movdqa		xmm1,xmm0
				movdqa		xmm3,xmm2

				;yuv2yc変換
				pmaddwd		xmm0,_MULY		;yuv[ 6, 4, 2, 0]
				psrad		xmm0,6
				psubd		xmm0,_SUBY
				pmaddwd		xmm2,_MULY		;yuv[14,12,10, 8]
				psrad		xmm2,6
				psubd		xmm2,_SUBY

				pmaddwd		xmm1,_MULC		;yuv[ 7, 5, 3, 1]
				psubd		xmm1,_SUBC
				psrad		xmm1,8
				pmaddwd		xmm3,_MULC		;yuv[15,13,11, 9]
				psubd		xmm3,_SUBC
				psrad		xmm3,8

				packssdw	xmm0,xmm1		;yuv[ 7, 5, 3, 1], yuv[ 6, 4, 2, 0]
				packssdw	xmm2,xmm3		;yuv[15,13,11, 9], yuv[14,12,10, 8]

				movdqa		xmm4,xmm0
				movdqa		xmm5,xmm2

				;//SHUF8(6,2,5,4,1,5,4,0);
				pshufb		xmm4,_SHUF_2	;yuv[ 5, 4, 3, 1, 2, 3, 1, 0]

					;yc[0].y	=	yuv[0]
					;yc[0].cb	=	yuv[1]
					;yc[0].cr	=	yuv[3]
					;yc[1].y	=	yuv[2]
					;yc[1].cb	=	yuv[1]
					;yc[1].cr	=	yuv[3]
					;yc[2].y	=	yuv[4]
					;yc[2].cb	=	yuv[5]

				;//SHUF8(0,0,0,0,7,6,3,7);
				pshufb		xmm0,_SHUF_3	;yuv[-,-,-,-, 7, 5, 6, 7]
				;//SHUF8(0,0,0,0,1,5,4,0);
				pshufb		xmm2,_SHUF_4	;yuv[-,-,-,-,10,11, 9, 8]

					;yc[2].cr	=	yuv[7]
					;yc[3].y	=	yuv[6]
					;yc[3].cb	=	yuv[5]
					;yc[3].cr	=	yuv[7]
					;yc[4].y	=	yuv[8]
					;yc[4].cb	=	yuv[9]
					;yc[4].cr	=	yuv[11]
					;yc[5].y	=	yuv[10]

				;//SHUF8(7,6,3,7,6,2,5,4);
				pshufb		xmm5,_SHUF_5	;yuv[15,13,14,15,13,12,11, 9]
					;yc[5].cb	=	yuv[9]
					;yc[5].cr	=	yuv[11]
					;yc[6].y	=	yuv[12]
					;yc[6].cb	=	yuv[13]
					;yc[6].cr	=	yuv[15]
					;yc[7].y	=	yuv[14]
					;yc[7].cb	=	yuv[13]
					;yc[7].cr	=	yuv[15]

				movdqu		xmmword ptr [ebx],xmm4
				movdqu		xmmword ptr [ebx+16],xmm0
				movdqu		xmmword ptr [ebx+24],xmm2
				movdqu		xmmword ptr [ebx+32],xmm5
				add			eax,16
				add			ebx,48
				dec			ecx
				jnz			loopX
		}
#else
		if(width /= 8) {
			__m128i xmm0, xmm1, xmm2, xmm3, xmm4, xmm5, xmm7;

			xmm7 = _mm_setzero_si128();
			for (int x = 0; x < width; x++) {
				xmm0 = _mm_lddqu_si128((__m128i*)&yuv[x*16]);
				xmm2 = xmm0;

				//yuv4バイト分を取得
				xmm0 = _mm_unpacklo_epi8(xmm0, xmm7);           //yuv[ 7, 6, 5, 4, 3, 2, 1, 0]
				xmm2 = _mm_unpackhi_epi8(xmm2, xmm7);	        //yuv[15,14,13,12,11,10, 9, 8]
				xmm1 = xmm0;
				xmm3 = xmm2;

				//yuv2yc変換
				xmm0 = _mm_madd_epi16(xmm0, _MULY);	            //yuv[ 6, 4, 2, 0]
				xmm0 = _mm_srai_epi32(xmm0, 6);
				xmm0 = _mm_sub_epi32(xmm0, _SUBY);
				xmm2 = _mm_madd_epi16(xmm2, _MULY);             //yuv[14,12,10, 8]
				xmm2 = _mm_srai_epi32(xmm2, 6);
				xmm2 = _mm_sub_epi32(xmm2, _SUBY);

				xmm1 = _mm_madd_epi16(xmm1, _MULC);	            //yuv[ 7, 5, 3, 1]
				xmm1 = _mm_sub_epi32(xmm1, _SUBC);
				xmm1 = _mm_srai_epi32(xmm1, 8);
				xmm3 = _mm_madd_epi16(xmm3, _MULC);             //yuv[15,13,11, 9]
				xmm3 = _mm_sub_epi32(xmm3, _SUBC);
				xmm3 = _mm_srai_epi32(xmm3, 8);

				xmm0 = _mm_packs_epi32(xmm0, xmm1);             //yuv[ 7, 5, 3, 1], yuv[ 6, 4, 2, 0]
				xmm2 = _mm_packs_epi32(xmm2, xmm3);             //yuv[15,13,11, 9], yuv[14,12,10, 8]

				xmm4 = xmm0;
				xmm5 = xmm2;

				//SHUF8(6,2,5,4,1,5,4,0);
				xmm4 = _mm_shuffle_epi8(xmm4, _SHUF_2);         //yuv[ 5, 4, 3, 1, 2, 3, 1, 0]

					//yc[0].y	=	yuv[0]
					//yc[0].cb	=	yuv[1]
					//yc[0].cr	=	yuv[3]
					//yc[1].y	=	yuv[2]
					//yc[1].cb	=	yuv[1]
					//yc[1].cr	=	yuv[3]
					//yc[2].y	=	yuv[4]
					//yc[2].cb	=	yuv[5]

				//SHUF8(0,0,0,0,7,6,3,7);
				xmm0 = _mm_shuffle_epi8(xmm0, _SHUF_3);         //yuv[-,-,-,-, 7, 5, 6, 7]
				;//SHUF8(0,0,0,0,1,5,4,0);
				xmm2 = _mm_shuffle_epi8(xmm2, _SHUF_4);         //yuv[-,-,-,-,10,11, 9, 8]

					//yc[2].cr	=	yuv[7]
					//yc[3].y	=	yuv[6]
					//yc[3].cb	=	yuv[5]
					//yc[3].cr	=	yuv[7]
					//yc[4].y	=	yuv[8]
					//yc[4].cb	=	yuv[9]
					//yc[4].cr	=	yuv[11]
					//yc[5].y	=	yuv[10]

				//SHUF8(7,6,3,7,6,2,5,4);
				xmm5 = _mm_shuffle_epi8(xmm5, _SHUF_5);         //yuv[15,13,14,15,13,12,11, 9]

					//yc[5].cb	=	yuv[9]
					//yc[5].cr	=	yuv[11]
					//yc[6].y	=	yuv[12]
					//yc[6].cb	=	yuv[13]
					//yc[6].cr	=	yuv[15]
					//yc[7].y	=	yuv[14]
					//yc[7].cb	=	yuv[13]
					//yc[7].cr	=	yuv[15]

				_mm_storeu_si128((__m128i*)((char*)yc), xmm4);
				_mm_storeu_si128((__m128i*)((char*)yc+16), xmm0);
				_mm_storeu_si128((__m128i*)((char*)yc+24), xmm2);
				_mm_storeu_si128((__m128i*)((char*)yc+32), xmm5);

				yc += 8;
			}
		}
#endif

		return width * 8;
	}

	ConvertYUY2ToAviUtlYC(const PClip& clip, IScriptEnvironment *env)
		: GenericVideoFilter(clip)
	{
		vi.width *= 3;
		child->SetCacheHints(CACHE_NOTHING, 0);
	}

	static AVSValue __cdecl Create(AVSValue args, void *user_data, IScriptEnvironment *env) {
		PClip child(args[0].AsClip());
		const VideoInfo& vi = child->GetVideoInfo();

		if(!vi.IsYUY2())
			env->ThrowError("%s: YUY2専用", GetName());

		return new ConvertYUY2ToAviUtlYC(child, env);
	}

public:
	static const char *GetName()
	{ return "ConvertYUY2ToAviUtlYC"; }

	static void PluginInit(IScriptEnvironment *env)
	{ env->AddFunction(GetName(), "c", Create, 0); }

	static bool Convert(PIXEL_YC *yc, int ycPitch, const BYTE *yuv, int yuvPitch, int width, int height, long cpu) {
		bool emms = false;

		if(width * sizeof(PIXEL_YC) == ycPitch && width * 2 == yuvPitch) {
			emms = Convert(yc, yuv, width * height, cpu);
		}
		else {
			for(int h = height; h; --h) {
				emms = Convert(yc, yuv, width, cpu);
				reinterpret_cast<BYTE *&>(yc) += ycPitch;
				yuv += yuvPitch;
			}
		}

		return emms;
	}

	static bool Convert(PIXEL_YC *yc, const BYTE *yuv, int width, long cpu) {
		bool emms = false;

		if(cpu & CPU_SSSE3) {
			int w = ConvertSSE3(yc, yuv, width);
			width -= w;
			yuv += w * 2;
			yc += w;
			emms = true;
		}
		if(cpu & CPU_MMXEXT) {
			int w = ConvertMMX(yc, yuv, width);
			width -= w;
			yuv += w * 2;
			yc += w;
			emms = true;
		}

		for(int w = width / 2; w; --w) {
			yc[0].y             = (short)(((yuv[0] * 1197         ) >> 6) - 299);
			yc[1].y             = (short)(((yuv[2] * 1197         ) >> 6) - 299);
			yc[0].cb = yc[1].cb = (short)(( yuv[1] * 4681 - 599004) >> 8       );
			yc[0].cr = yc[1].cr = (short)(( yuv[3] * 4681 - 599004) >> 8       );
			yuv += 4;
			yc += 2;
		}


		return emms;
	}

	PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment *env) {
		PVideoFrame srcFrame(child->GetFrame(n, env));
		PVideoFrame dstFrame(env->NewVideoFrame(vi, sizeof(PIXEL_YC)));

		bool flag = Convert(reinterpret_cast<PIXEL_YC *>(dstFrame->GetWritePtr()), dstFrame->GetPitch(),
			srcFrame->GetReadPtr(), srcFrame->GetPitch(), vi.width / 3, vi.height, cpu_detect());//env->GetCPUFlags());

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

class ConvertAviUtlYCToYUY2 : public GenericVideoFilter {

	static int ConvertMMX(BYTE *yuv, const PIXEL_YC *yc, int width) {
	//		yuv[0] = Clamp((int)((yc[0].y  * 219 + 65919 ) >> 12), 0, 255);
	//		yuv[2] = Clamp((int)((yc[1].y  * 219 + 65919 ) >> 12), 0, 255);
	//		yuv[1] = Clamp((int)((yc[0].cb * 7   + 16450 ) >> 7 ), 0, 255);
	//		yuv[3] = Clamp((int)((yc[0].cr * 7   + 16450 ) >> 7 ), 0, 255);
		static const	__int64	_MULY = 0x00DB0000000000DB;	//	219,	219
		static const	__int64	_MULC = 0x0000000700070000;	//	7,	7
		static const	__int64	_ADDY = 0x0001017F0001017F;	//	65919,	65919
		static const	__int64	_ADDC = 0x0000404200004042;	//	16450,	16450

#if !(defined(_M_X64) || defined(USE_SIMD_INTRINSICS))
		if(width /= 4) __asm {
				mov			ecx,width
				mov			eax,yuv
				mov			ebx,yc
				pxor		mm7,mm7

		loopX:
				movq		mm0,[ebx]		;yc[1].y,	yc[0].cr,	yc[0].cb,	yc[0].y
				movq		mm1,mm0
				movq		mm2,[ebx+12]	;yc[3].y,	yc[2].cr,	yc[2].cb,	yc[2].y
				movq		mm3,mm2

				;yc2yuv変換
				pmaddwd		mm0,_MULY	;yc[1].y, yc[0].y
				paddd		mm0,_ADDY
				psrad		mm0,12

				pmaddwd		mm2,_MULY	;yc[3].y, yc[2].y
				paddd		mm2,_ADDY
				psrad		mm2,12


				pmaddwd		mm1,_MULC	;yc[0].cr, yc[0].cb
				paddd		mm1,_ADDC
				psrad		mm1,7
				pmaddwd		mm3,_MULC	;yc[2].cr, yc[2].cb
				paddd		mm3,_ADDC
				psrad		mm3,7

				;yuv用にパックド・バイトに変更
				packssdw	mm0,mm2		;yc[3].y, yc[2].y, yc[1].y, yc[0].y
				packssdw	mm1,mm3		;yc[2].cr, yc[2].cb, yc[0].cr, yc[0].cb
				packuswb	mm0,mm7		;
				packuswb	mm1,mm7		;
				punpcklbw	mm0,mm1

					;yuv[0]		=	yc[0].y		0
					;yuv[1]		=	yc[0].cb	1
					;yuv[2]		=	yc[1].y		2
					;yuv[3]		=	yc[0].cr	3
					;yuv[4]		=	yc[2].y		0
					;yuv[5]		=	yc[2].cb	1
					;yuv[6]		=	yc[3].y		2
					;yuv[7]		=	yc[2].cr	3

				movntq		[eax],mm0
				add			eax,8
				add			ebx,24
				dec			ecx
				jnz			loopX
		}
#else
		if(width /= 4) {
			__m128i mm0, mm1, mm2, mm3, mm7, mem1, mem2;

			mm7 = _mm_setzero_si128();
			for (int x = 0; x < width; x++) {
				mm0 = _mm_loadl_epi64((__m128i*)yc);
				mm1 = mm0;
				mm2 = _mm_loadl_epi64((__m128i*)(yc+2));
				mm3 = mm2;

				//yc2yuv変換
				mem1 = _mm_loadl_epi64((__m128i*)_MULY);
				mem2 = _mm_loadl_epi64((__m128i*)_ADDY);
				mm0 = _mm_madd_epi16(mm0, mem1);                //yc[1].y, yc[0].y
				mm0 = _mm_add_epi32(mm0, mem2);
				mm0 = _mm_srai_epi32(mm0, 12);
				mm2 = _mm_madd_epi16(mm2, mem1);                //yc[3].y, yc[2].y
				mm2 = _mm_add_epi32(mm2, mem2);
				mm2 = _mm_srai_epi32(mm2, 12);

				mem1 = _mm_loadl_epi64((__m128i*)_MULC);
				mem2 = _mm_loadl_epi64((__m128i*)_ADDC);
				mm1 = _mm_madd_epi16(mm1, mem1);                //yc[0].cr, yc[0].cb
				mm1 = _mm_add_epi32(mm1, mem2);
				mm1 = _mm_srai_epi32(mm1, 7);
				mm3 = _mm_madd_epi16(mm3, mem1);                //yc[2].cr, yc[2].cb
				mm3 = _mm_add_epi32(mm3, mem2);
				mm3 = _mm_srai_epi32(mm3, 7);

				//yuv用にパックド・バイトに変更
				mm0 = _mm_packs_epi32(mm0, mm2);                //yc[3].y, yc[2].y, yc[1].y, yc[0].y
				mm1 = _mm_packs_epi32(mm1, mm3);                //yc[2].cr, yc[2].cb, yc[0].cr, yc[0].cb
				mm0 = _mm_packus_epi16(mm0, mm7);
				mm1 = _mm_packus_epi16(mm1, mm7);
				mm0 = _mm_unpacklo_epi8(mm0, mm1);

					//yuv[0]		=	yc[0].y		0
					//yuv[1]		=	yc[0].cb	1
					//yuv[2]		=	yc[1].y		2
					//yuv[3]		=	yc[0].cr	3
					//yuv[4]		=	yc[2].y		0
					//yuv[5]		=	yc[2].cb	1
					//yuv[6]		=	yc[3].y		2
					//yuv[7]		=	yc[2].cr	3

				_mm_storel_epi64((__m128i*)yuv, mm0);
				yuv += 8;
				yc += 4;
			}
		}
#endif

		return width * 4;
	}

	static int ConvertSSE3(BYTE *yuv, const PIXEL_YC *yc, int width) {
	//		yuv[0] = Clamp((int)((yc[0].y  * 219 + 65919 ) >> 12), 0, 255);
	//		yuv[2] = Clamp((int)((yc[1].y  * 219 + 65919 ) >> 12), 0, 255);
	//		yuv[1] = Clamp((int)((yc[0].cb * 7   + 16450 ) >> 7 ), 0, 255);
	//		yuv[3] = Clamp((int)((yc[0].cr * 7   + 16450 ) >> 7 ), 0, 255);
		static const __m128i	_MULY = {0xDB, 0x00, 0x00, 0x00, 0x00, 0x00, 0xDB, 0x00, 0xDB, 0x00, 0x00, 0x00, 0x00, 0x00, 0xDB, 0x00};
		static const __m128i	_MULC = {0x00, 0x00, 0x07, 0x00, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 0x00, 0x07, 0x00, 0x00, 0x00};
		static const __m128i	_ADDY = {0x7F, 0x01, 0x01, 0x00, 0x7F, 0x01, 0x01, 0x00, 0x7F, 0x01, 0x01, 0x00, 0x7F, 0x01, 0x01, 0x00};
		static const __m128i	_ADDC = {0x42, 0x40, 0x00, 0x00, 0x42, 0x40, 0x00, 0x00, 0x42, 0x40, 0x00, 0x00, 0x42, 0x40, 0x00, 0x00};

#if !(defined(_M_X64) || defined(USE_SIMD_INTRINSICS))
		if(width /= 8) __asm {
				mov			ecx,width
				mov			eax,yuv
				mov			ebx,yc
				pxor		xmm7,xmm7

		loopX:
				lddqu		xmm0,xmmword ptr [ebx   ]		; 2b, 2y, 1r, 1b, 1y, 0r, 0b, 0y
				lddqu		xmm1,xmmword ptr [ebx+12]		; 4b, 4y, 3r, 3b, 3y, 2r, 2b, 2y
				punpcklqdq	xmm0,xmm1						; 3y, 2r, 2b, 2y, 1y, 0r, 0b, 0y
				movdqa		xmm1,xmm0
				lddqu		xmm2,xmmword ptr [ebx+24]
				lddqu		xmm3,xmmword ptr [ebx+36]
				punpcklqdq	xmm2,xmm3
				movdqa		xmm3,xmm2

				;yc2yuv変換
				pmaddwd		xmm0,_MULY	;yc[3].y, yc[2].y, yc[1].y, yc[0].y
				paddd		xmm0,_ADDY
				psrad		xmm0,12

				pmaddwd		xmm2,_MULY	;yc[7].y, yc[6].y, yc[5].y, yc[4].y
				paddd		xmm2,_ADDY
				psrad		xmm2,12

				pmaddwd		xmm1,_MULC	;yc[1].cr, yc[1].cb, yc[0].cr, yc[0].cb
				paddd		xmm1,_ADDC
				psrad		xmm1,7
				pmaddwd		xmm3,_MULC	;yc[2].cr, yc[2].cb
				paddd		xmm3,_ADDC
				psrad		xmm3,7

				;yuv用にパックド・バイトに変更
				packssdw	xmm0,xmm2		;yc[3].y, yc[2].y, yc[1].y, yc[0].y
				packssdw	xmm1,xmm3		;yc[2].cr, yc[2].cb, yc[0].cr, yc[0].cb
				packuswb	xmm0,xmm7		;
				packuswb	xmm1,xmm7		;
				punpcklbw	xmm0,xmm1

					;yuv[ 0]		=	0y
					;yuv[ 1]		=	0b
					;yuv[ 2]		=	1y
					;yuv[ 3]		=	0r
					;yuv[ 4]		=	2y
					;yuv[ 5]		=	2b
					;yuv[ 6]		=	3y
					;yuv[ 7]		=	2r
					;yuv[ 8]		=	4y
					;yuv[ 9]		=	4b
					;yuv[10]		=	5y
					;yuv[11]		=	4r
					;yuv[12]		=	6y
					;yuv[13]		=	6b
					;yuv[14]		=	7y
					;yuv[15]		=	6r

				movdqu		xmmword ptr [eax],xmm0
				add			eax,16
				add			ebx,48
				dec			ecx
				jnz			loopX
		}
#else
		if(width /= 8) {
			__m128i xmm0, xmm1, xmm2, xmm3, xmm7;

			xmm7 = _mm_setzero_si128();
			for (int x = 0; x < width; x++) {
				xmm0 = _mm_lddqu_si128((__m128i*)yc);       // 2b, 2y, 1r, 1b, 1y, 0r, 0b, 0y
				xmm1 = _mm_lddqu_si128((__m128i*)(yc+2));   // 4b, 4y, 3r, 3b, 3y, 2r, 2b, 2y 
				xmm0 = _mm_unpacklo_epi64(xmm0, xmm1);      // 3y, 2r, 2b, 2y, 1y, 0r, 0b, 0y
				xmm1 = xmm0;
				xmm2 = _mm_lddqu_si128((__m128i*)(yc+4));
				xmm3 = _mm_lddqu_si128((__m128i*)(yc+6));
				xmm2 = _mm_unpacklo_epi64(xmm2, xmm3);
				xmm3 = xmm2;

				//yc2yuv変換
				xmm0 = _mm_madd_epi16(xmm0, _MULY);	        //yc[3].y, yc[2].y, yc[1].y, yc[0].y
				xmm0 = _mm_add_epi32(xmm0, _ADDY);
				xmm0 = _mm_srai_epi32(xmm0, 12);

				xmm2 = _mm_madd_epi16(xmm2, _MULY);         //yc[7].y, yc[6].y, yc[5].y, yc[4].y
				xmm2 = _mm_add_epi32(xmm2, _ADDY);
				xmm2 = _mm_srai_epi32(xmm2, 12);

				xmm1 = _mm_madd_epi16(xmm1, _MULC);	        //yc[1].cr, yc[1].cb, yc[0].cr, yc[0].cb
				xmm1 = _mm_add_epi32(xmm1, _ADDC);
				xmm1 = _mm_srai_epi32(xmm1, 7);
				xmm3 = _mm_madd_epi16(xmm3, _MULC);	        //yc[2].cr, yc[2].cb
				xmm3 = _mm_add_epi32(xmm3, _ADDC);
				xmm3 = _mm_srai_epi32(xmm3, 7);

				//yuv用にパックド・バイトに変更
				xmm0 = _mm_packs_epi32(xmm0, xmm2);         //yc[3].y, yc[2].y, yc[1].y, yc[0].y
				xmm1 = _mm_packs_epi32(xmm1, xmm3);         //yc[2].cr, yc[2].cb, yc[0].cr, yc[0].cb
				xmm0 = _mm_packus_epi16(xmm0, xmm7);
				xmm1 = _mm_packus_epi16(xmm1, xmm7);
				xmm0 = _mm_unpacklo_epi8(xmm0, xmm1);

					//yuv[ 0]		=	0y
					//yuv[ 1]		=	0b
					//yuv[ 2]		=	1y
					//yuv[ 3]		=	0r
					//yuv[ 4]		=	2y
					//yuv[ 5]		=	2b
					//yuv[ 6]		=	3y
					//yuv[ 7]		=	2r
					//yuv[ 8]		=	4y
					//yuv[ 9]		=	4b
					//yuv[10]		=	5y
					//yuv[11]		=	4r
					//yuv[12]		=	6y
					//yuv[13]		=	6b
					//yuv[14]		=	7y
					//yuv[15]		=	6r

				_mm_storeu_si128((__m128i*)yuv, xmm0);
				yc += 8;
				yuv += 16;
			}
		}
#endif

		return width * 8;
	}

	static int Clamp(int n, int l, int h)
	{ return (n < l) ? l : (n > h) ? h : n; }

	ConvertAviUtlYCToYUY2(const PClip& clip, IScriptEnvironment *env)
		: GenericVideoFilter(clip)
	{
		vi.width /= 3;
		child->SetCacheHints(CACHE_NOTHING, 0);
	}

	static AVSValue __cdecl Create(AVSValue args, void *user_data, IScriptEnvironment *env) {
		PClip child(args[0].AsClip());
		const VideoInfo& vi = child->GetVideoInfo();

		if(!vi.IsYUY2())
			env->ThrowError("%s: YUY2専用", GetName());

		return new ConvertAviUtlYCToYUY2(child, env);
	}

public:
	static const char *GetName()
	{ return "ConvertAviUtlYCToYUY2"; }

	static void PluginInit(IScriptEnvironment *env)
	{ env->AddFunction(GetName(), "c", Create, 0); }

	static bool Convert(BYTE *yuv, int yuvPitch, const PIXEL_YC *yc, int ycPitch, int width, int height, long cpu) {
		bool emms = false;

		if(width * 2 == yuvPitch && width * sizeof(PIXEL_YC) == ycPitch) {
			emms = Convert(yuv, yc, width * height, cpu);
		}
		else {
			for(int h = height; h; --h) {
				emms = Convert(yuv, yc, width, cpu);
				yuv += yuvPitch;
				reinterpret_cast<const BYTE *&>(yc) += ycPitch;
			}
		}

		return emms;
	}

	static bool Convert(BYTE *yuv, const PIXEL_YC *yc, int width, long cpu) {
		bool emms = false;

		if(cpu & CPU_SSSE3) {
			int w = ConvertSSE3(yuv, yc, width);
			width -= w;
			yc += w;
			yuv += w * 2;
			emms = true;
		}
		if(cpu & CPU_MMXEXT) {
			int w = ConvertMMX(yuv, yc, width);
			width -= w;
			yc += w;
			yuv += w * 2;
			emms = true;
		}

		for(int w = width / 2; w; --w) {
			yuv[0] = Clamp((int)((yc[0].y  * 219 + 65919 ) >> 12), 0, 255);
			yuv[2] = Clamp((int)((yc[1].y  * 219 + 65919 ) >> 12), 0, 255);
			yuv[1] = Clamp((int)((yc[0].cb * 7   + 16450 ) >> 7 ), 0, 255);
			yuv[3] = Clamp((int)((yc[0].cr * 7   + 16450 ) >> 7 ), 0, 255);
			yc += 2;
			yuv += 4;
		}

		return emms;
	}

	PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment *env) {
		PVideoFrame srcFrame(child->GetFrame(n, env));
		PVideoFrame dstFrame(env->NewVideoFrame(vi));

		bool flag = Convert(dstFrame->GetWritePtr(), dstFrame->GetPitch(),
			reinterpret_cast<const PIXEL_YC *>(srcFrame->GetReadPtr()),
			srcFrame->GetPitch(), vi.width, vi.height, cpu_detect());//env->GetCPUFlags());

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
