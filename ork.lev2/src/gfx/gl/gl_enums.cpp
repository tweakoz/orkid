////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#include <ork/lev2/gfx/gfxenv.h>
#include <ork/kernel/string/deco.inl>

#include "gl.h"

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////

std::string _glTypeToString(GLenum type){
    std::string type_str;
    switch (type) {
      case GL_FLOAT:
        type_str = "float";
        break;
      case GL_FLOAT_VEC2:
        type_str = "vec2";
        break;
      case GL_FLOAT_VEC3:
        type_str = "vec3";
        break;
      case GL_FLOAT_VEC4:
        type_str = "vec4";
        break;
      case GL_FLOAT_MAT4:
        type_str = "mat4";
        break;
      case GL_FLOAT_MAT3:
        type_str = "mat3";
        break;
      case GL_FLOAT_MAT2:
        type_str = "mat2";
        break;
      case GL_SAMPLER_2D:
        type_str = "sampler2D";
        break;
      case GL_SAMPLER_3D:
        type_str = "sampler3D";
        break;
      case GL_SAMPLER_CUBE: 
        type_str = "samplerCube";
        break;
      case GL_SAMPLER_2D_SHADOW:
        type_str = "sampler2DShadow";
        break;
      case GL_SAMPLER_2D_ARRAY:
        type_str = "sampler2DArray";
        break;
      case GL_NONE:
        type_str = "none";
        break;
      default:
        type_str = FormatString("unknown<%x>", type);
        break;
    }
    return type_str;
}

///////////////////////////////////////////////////////////////////////////////
} //namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////
