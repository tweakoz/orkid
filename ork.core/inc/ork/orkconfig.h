////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunknown-pragmas"
#pragma GCC diagnostic ignored "-Wpedantic"
#pragma GCC diagnostic ignored "-Wparentheses"

////////////////////////////////////////////////////////////////

#define ORK_CONFIG_OPENGL
#define INCLUDE_UNIT_TEST

///////////////////////////////////////////////////////////////////////////////

#define ThreadLocal __thread
#define MEMALIGN(x) __attribute__((aligned(x)))
#define ORK_CONFIG_DEFAULT_SERIALIZE_XML

///////////////////////////////////////////////////////////////////////////////

# if defined(BUILDING_ORKID)

    # if defined(LINUX) or defined(__APPLE__) // macos, linux

        #define ORK_API __attribute__ ((visibility("default")))
        //#define ORK_API_PUSH __attribute__ ((visibility("default")))
        //#define ORK_API_PUSH_PUBLIC _Pragma("GCC visibility push(default)")
        //#define ORK_API_PUSH_PRIVATE _Pragma("GCC visibility push(hidden)")
        //#define ORK_API_POP _Pragma("GCC visibility pop")

    #elif defined(_WIN32) // windows

        #define ORK_API __declspec(dllexport)

    #else

        #error

    #endif

#elif defined(USING_ORKID)

    # if defined(LINUX) or defined(__APPLE__) // macos, linux

        #define ORK_API __attribute__ ((visibility("default")))
        //#define ORK_API_PUSH_PUBLIC _Pragma("GCC visibility push(default)")
        //#define ORK_API_PUSH_PRIVATE _Pragma("GCC visibility push(hidden)")
        //#define ORK_API_POP _Pragma("GCC visibility pop")

    #elif defined(_WIN32) // windows

        #define ORK_API __declspec(dllimport)

    #else
    
        #error

    #endif

#else

  #error

#endif



