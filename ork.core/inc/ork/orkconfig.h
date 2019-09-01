////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#define ORK_CONFIG_OPENGL
#define ORK_CONFIG_QT
#define USE_FCOLLADA

///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
#if defined(ORKMSVC2005FASDBG)

#define _SECURE_SCL 0
#define _HAS_ITERATOR_DEBUGGING 0

#ifndef WIN32
#define WIN32
#endif

#define _LIB
#define _ORKID
#define _MSVC2005
#define TIXML_USE_STL
#define TIXML_WRITE
#define TIXML_ORKID_FILEIO
#define _CRT_SECURE_NO_DEPRECATE
#endif

#if defined(ORKMSVC2005DBG)

#define _SECURE_SCL 1
#ifndef WIN32
#define WIN32
#endif

#define _LIB
#define _ORKID
#define _MSVC2005
#define TIXML_USE_STL
#define TIXML_WRITE
#define TIXML_ORKID_FILEIO
#define _CRT_SECURE_NO_DEPRECATE
#endif
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
#if defined(ORKMSVC2005OPT)
//_SCEEPSPGU_TARGET_PC;_SCEEPSPGU_DEBUG;_SCEEPSPGUM_TARGET_PC;_SCEEPSPGUM_DEBUG;_SCEEPSPGUU_TARGET_PC;_SCEEPSPGUU_DEBUG
//#define _SECURE_SCL 1
#define WIN32
#define _LIB
#define _ORKID
#define _MSVC2005
#define TIXML_USE_STL
#define TIXML_WRITE
#define TIXML_ORKID_FILEIO
#define _CRT_SECURE_NO_DEPRECATE
#endif
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////

// nasa - added this to enable/disable the compiling of unit test code
#define INCLUDE_UNIT_TEST

///////////////////////////////////////////////////////////////////////////////
// Build Configuration

#define _CONSOLE_BUILD_LEVEL 1
#define _BUILD_LEVEL 2 // (memdbg) (editors)
#define MEMORYMANAGER_LOGGING 1

///////////////////////////////////////////////////////////////////////////////

#define safe_delete(x)                                                                                                             \
  if (0 != x)                                                                                                                      \
    delete x;

///////////////////////////////////////////////////////////////////////////////
// Graphics Enables

#define USE_ARB_EXTENSIONS // Use ARB

//////////////////////////////////////////////////////////////////////////////
// OS Selection (Win32/MSVC/.NET)

#if defined(_MSVC2002) || defined(_MSVC2003) || defined(_MSVC2005) || defined(_MSVCXBOX)
#define _MSVC
#endif

#if defined(_MSVC)

#pragma warning(disable : 4099) // STL
#pragma warning(disable : 4786) // STL
#pragma warning(disable : 4355) // STL
#pragma warning(disable : 4995) // STL
#pragma warning(disable : 4800) // 'unsigned int' : forcing value to bool 'true' or 'false' (performance warning)
#pragma warning(disable : 4995) // '_OLD_IOSTREAMS_ARE_DEPRECATED': name was marked as #pragma deprecated
#pragma warning(disable : 4100) // warning C4100: 'pClass' : unreferenced formal parameter
#pragma warning(disable : 4127) // warning C4127: conditional expression is constant
#pragma warning(disable : 4512) // warning C4127: conditional expression is constant
#pragma warning(disable : 4201) // warning C4127: conditional expression is constant
#pragma warning(disable : 4189) // local variable is initialized but not referenced
#pragma warning(disable : 4505) // unreferenced local function has been removed
#pragma warning(disable : 4503) // decorated name length exceeded, name was truncated
#endif

#if defined(GCC)
#if defined(_IOS)
#define ThreadLocal
#else
#define ThreadLocal __thread
#endif
#elif defined(_MSVC)
#define ThreadLocal __declspec(thread)
#endif

///////////////////////////////////////////////////////////////////////////////
// Plugin Support

///////////////////////////////////////////////////////////////////////////////
#if defined(_XBOX) && defined(_MSVC)
///////////////////////////////////////////////////////////////////////////////
#define _DOTNET2003
#define MEMALIGN(x) __declspec(align(x))
///////////////////////////////////////////////////////////////////////////////
#elif defined(_WIN32) && defined(_MSVC)
///////////////////////////////////////////////////////////////////////////////
#define VC_EXTRALEAN
#define MEMALIGN(x) __declspec(align(x))
#define snprintf _snprintf
#elif (defined(_PS2) || defined(_PSP)) && defined(GCC)
#define MEMALIGN(x) __attribute__((aligned(x)))
#elif defined(IX) || defined(NITRO) || defined(WII)
#define MEMALIGN(x) __attribute__((aligned(x)))
#if defined(_LINUX)
#endif
#endif

#if defined(_XBOX_VER)
#define __BIG_ENDIAN__
#define ORK_CONFIG_DIRECT3D
#elif defined(IX) || defined(_WIN32)
#define ORK_CONFIG_FLOAT64
#endif
#if defined(_WIN32)
#define ORK_CONFIG_DIRECT3D
#endif
///////////////////////////////////////////////////////////////////////////////
#if defined(NITRO)
#define ORK_CONFIG_DEFAULT_SERIALIZE_BIN
#else
#define ORK_CONFIG_DEFAULT_SERIALIZE_XML
#endif
///////////////////////////////////////////////////////////////////////////////
