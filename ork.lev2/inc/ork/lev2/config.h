////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2021, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once 

#if defined(ORK_ARCHITECTURE_X86_64)
//#define ENABLE_IGL
  #if defined(LINUX)
    #define OPENGL_46
  #else
    #define OPENGL_41
  #endif
#else
  #define OPENGL_40
#endif

#if defined(LINUX) and defined(ORK_ARCHITECTURE_X86_64)
#define ENABLE_OPENVR
#define ENABLE_ISPC
#endif

#if defined(LINUX)
#define ENABLE_ALSA
#else 
#define ENABLE_PORTAUDIO
#endif
