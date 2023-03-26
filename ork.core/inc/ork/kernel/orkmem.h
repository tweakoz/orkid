////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
//////////////////////////////////////////////////////////////// 

#pragma once

#include <ork/orktypes.h>

#if 1 //! defined(_XBOX)
#define USESTDNEW
#endif

/////////////////////////////////////////////////
// msvc leak detector
/////////////////////////////////////////////////
#if defined(USESTDNEW)
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#ifdef _WIN32
#include <crtdbg.h>
#endif
#endif
/////////////////////////////////////////////////
/////////////////////////////////////////////////

// CONFIG
#if !defined(SDK_FINALROM) && !defined(FINAL)
# define ORK_DEBUG_MEMORY_API
#endif

// END CONFIG //////////////////////////////////

#undef __ORKMEM_ALLOC_NO_THROW
#undef __ORKMEM_COMPAT_NO_THROW
#ifdef GCC
# define __ORKMEM_ALLOC_NO_THROW
# define __ORKMEM_COMPAT_NO_THROW
#elif defined(WII)
# define __ORKMEM_ALLOC_NO_THROW
# define __ORKMEM_COMPAT_NO_THROW _MSL_NO_THROW
#else
# define __ORKMEM_ALLOC_NO_THROW
# define __ORKMEM_COMPAT_NO_THROW
#endif

#include <stddef.h>

#if defined(ORK_DEBUG_MEMORY_API)
# define __DEBUG_ARGS const char *filename, int lineno
# define __DEBUG_ARGS_APPEND , const char *filename, int lineno
# define __GENERATE_DEBUG_ARGS __FILE__, __LINE__
# define __GENERATE_DEBUG_ARGS_APPEND , __FILE__, __LINE__
# define __PASS_DEBUG_ARGS filename, lineno
# define __PASS_DEBUG_ARGS_APPEND , filename, lineno
void OrkCheckMemory();
void OrkDumpMemory();
#else
# define __DEBUG_ARGS
# define __DEBUG_ARGS_APPEND
# define __GENERATE_DEBUG_ARGS
# define __GENERATE_DEBUG_ARGS_APPEND
# define __PASS_DEBUG_ARGS
# define __PASS_DEBUG_ARGS_APPEND
# define OrkCheckMemory()
# define OrkDumpMemory()
#endif

///////////////////////////////////////////////////////////////////////////////////////////
// WE ARE USING THESE IN PRODIGY (March, 2009)
///////////////////////////////////////////////////////////////////////////////////////////
#ifdef USESTDNEW
//#define DEBUG_NEW new(_NORMAL_BLOCK, __FILE__, __LINE__)
//#define new new(_NORMAL_BLOCK, __FILE__, __LINE__)
//#define OrkNew new
//#define OrkNewAry new[]
#define OrkNew new
#define OrkNewAry new[]
#define OrkDelete(ptr) delete(ptr)
#define OrkDeleteAry(ptr) delete[](ptr)
#else
void *operator new(size_t size __DEBUG_ARGS_APPEND) __ORKMEM_ALLOC_NO_THROW;
void *operator new[](size_t size __DEBUG_ARGS_APPEND) __ORKMEM_ALLOC_NO_THROW;
void operator delete(void *data __DEBUG_ARGS_APPEND);
void operator delete[](void *data __DEBUG_ARGS_APPEND);

void *operator new(size_t size) __ORKMEM_ALLOC_NO_THROW;
void *operator new[](size_t size) __ORKMEM_ALLOC_NO_THROW;
void operator delete(void *data);
void operator delete[](void *data);

#define OrkNew new(__GENERATE_DEBUG_ARGS)
#define OrkDelete(ptr) delete(ptr __GENERATE_DEBUG_ARGS_APPEND)
#define OrkNewAry new[](__GENERATE_DEBUG_ARGS)
#define OrkDeleteAry(ptr) delete[](ptr __GENERATE_DEBUG_ARGS_APPEND)
#endif

void *OrkAlloc(size_t size __DEBUG_ARGS_APPEND);
void *OrkAllocAligned(size_t size, size_t alignment __DEBUG_ARGS_APPEND);
void OrkFree(void *data __DEBUG_ARGS_APPEND);

class AllocationLabel
{
public:
	AllocationLabel(const char *label) {}
};

///////////////////////////////////////////////////////////////////////////////////////////
// IMPORTANT!!!
//
// THESE ARE DEPRECATED. WE PREFER EXPLICIT MEMORY PATTERNS OVER IMPLICIT ONES.
//
// TALK TO NASA OR TWEAK IF YOU DO NOT UNDERSTAND.
///////////////////////////////////////////////////////////////////////////////////////////
namespace alloc {
  struct ork       { static const ork       marker; };
  struct list      { static const list      marker; };
  struct templist  { static const templist  marker; };
  struct stack     { static const stack     marker; };
  struct tempstack { static const tempstack marker; };
  struct system    { static const system    marker; };
  struct manager   { static const manager   marker; };
}

#ifndef USESTDNEW
void *operator new(size_t size, const alloc::ork       & __DEBUG_ARGS_APPEND) __ORKMEM_ALLOC_NO_THROW;
void *operator new(size_t size, const alloc::list      & __DEBUG_ARGS_APPEND) __ORKMEM_ALLOC_NO_THROW;
void *operator new(size_t size, const alloc::templist  & __DEBUG_ARGS_APPEND) __ORKMEM_ALLOC_NO_THROW;
void *operator new(size_t size, const alloc::stack     & __DEBUG_ARGS_APPEND) __ORKMEM_ALLOC_NO_THROW;
void *operator new(size_t size, const alloc::tempstack & __DEBUG_ARGS_APPEND) __ORKMEM_ALLOC_NO_THROW;
void *operator new(size_t size, const alloc::system    & __DEBUG_ARGS_APPEND) __ORKMEM_ALLOC_NO_THROW;

void *operator new[](size_t size, const alloc::ork       & __DEBUG_ARGS_APPEND) __ORKMEM_ALLOC_NO_THROW;
void *operator new[](size_t size, const alloc::list      & __DEBUG_ARGS_APPEND) __ORKMEM_ALLOC_NO_THROW;
void *operator new[](size_t size, const alloc::templist  & __DEBUG_ARGS_APPEND) __ORKMEM_ALLOC_NO_THROW;
void *operator new[](size_t size, const alloc::stack     & __DEBUG_ARGS_APPEND) __ORKMEM_ALLOC_NO_THROW;
void *operator new[](size_t size, const alloc::tempstack & __DEBUG_ARGS_APPEND) __ORKMEM_ALLOC_NO_THROW;
void *operator new[](size_t size, const alloc::system    & __DEBUG_ARGS_APPEND) __ORKMEM_ALLOC_NO_THROW;

void operator delete(void *data, const alloc::ork       & __DEBUG_ARGS_APPEND);
void operator delete(void *data, const alloc::list      & __DEBUG_ARGS_APPEND);
void operator delete(void *data, const alloc::templist  & __DEBUG_ARGS_APPEND);
void operator delete(void *data, const alloc::stack     & __DEBUG_ARGS_APPEND);
void operator delete(void *data, const alloc::tempstack & __DEBUG_ARGS_APPEND);
void operator delete(void *data, const alloc::system    & __DEBUG_ARGS_APPEND);

void operator delete[](void *data, const alloc::ork       & __DEBUG_ARGS_APPEND);
void operator delete[](void *data, const alloc::list      & __DEBUG_ARGS_APPEND);
void operator delete[](void *data, const alloc::templist  & __DEBUG_ARGS_APPEND);
void operator delete[](void *data, const alloc::stack     & __DEBUG_ARGS_APPEND);
void operator delete[](void *data, const alloc::tempstack & __DEBUG_ARGS_APPEND);
void operator delete[](void *data, const alloc::system    & __DEBUG_ARGS_APPEND);
#endif
