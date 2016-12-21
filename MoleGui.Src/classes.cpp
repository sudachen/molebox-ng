
/*

  Copyright (C) 2009, Alexey Sudachen, https://goo.gl/lJNXya.

*/

#define _TEGGOSTRIP__EXPR__
#define _TEGGOSTRIP__FILE__

#if defined _WIN32 && !defined _WINDOWS_
# if !defined _X86_
#   define _X86_ 1
# endif
# if !defined WINVER
#   define WINVER 0x400
# endif
#endif

#include <windows.h>
#include <winternl.h>
#include "xnt.h"
#include "sources/detect_compiler.h"

#define _X86_ASSEMBLER

/*
namespace aux {
  CXX_EXTERNC void *CXX_STDCALL Lalloc(u32_t sz);
  CXX_EXTERNC void *CXX_STDCALL LallocZ(u32_t sz);
  CXX_EXTERNC void  CXX_STDCALL Lfree(void *p);
  CXX_EXTERNC void  CXX_STDCALL LfreePA(void *p, u32_t cnt);
}
*/

#define _TEGGO_ADLER_HERE
#define _TEGGO_CODECOP_HERE
#define _TEGGO_CRC32_HERE
#define _TEGGO_SPECIFIC_HERE
#define _TEGGO_FORMAT_HERE
#define _TEGGO_GENIO_HERE
#define _TEGGO_INSTANCE_HERE
#define _TEGGO_LZPLUS_DECODER_HERE
#define _TEGGO_STRING_HERE
#define _TEGGO_SYSUTIL_HERE
#define _TEGGO_XHASH_HERE
#define _TEGGO_NEWDES_HERE
#define _TEGGO_HOOKMGR_HERE
#define _TEGGO_X86CODE_HERE
#define _TEGGO_THREADS_HERE
#define _TEGGO_DISABLE_DBGOUTPUT
#define _TEGGO_THREADS_HERE
#define _TEGGO_STREAMS_HERE
#define _TEGGO_ZLIB_HERE
#define _TEGGO_COLLECTION_HERE
#include "Sources/_specific.h"
#include "Sources/_adler32.h"
#include "Sources/_codecop.inl"
#include "Sources/_crc32.h"
#include "Sources/_specific.inl"
#include "Sources/format.h"
#include "Sources/genericio.inl"
#include "Sources/hinstance.h"
#include "Sources/lz+decoder.inl"
#include "Sources/string.inl"
#include "Sources/sysutil.h"
#include "Sources/x86code.h"
#include "Sources/threads.h"
#include "Sources/streams.inl"
#include "Sources/zlib.inl"
#include "Sources/collection.inl"
#include "blowfish.c"

