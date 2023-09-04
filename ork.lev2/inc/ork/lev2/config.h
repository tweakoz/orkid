////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once 

//#define ENABLE_IGL
#define USE_ORKSL_LANG
// #define ENABLE_VULKAN from CMakeLists.txt
// #define ENABLE_GLFW from CMakeLists.txt

#if defined(ORK_ARCHITECTURE_X86_64)
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
//#define ENABLE_ALSA
 //#define ENABLE_PORTAUDIO 
 #define ENABLE_PIPEWIRE 
#else 
#define ENABLE_PORTAUDIO
#endif
