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
      case GL_UNSIGNED_BYTE:
        type_str = "U8";
        break;
      case GL_UNSIGNED_SHORT:
        type_str = "U16";
        break;
      case GL_UNSIGNED_INT:
        type_str = "U32";
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

std::string _glBlendFuncTermToString(GLenum blendfunc){
    std::string blend_str;
    switch (blendfunc) {
      case GL_ZERO:
        blend_str = "ZERO";
        break;
      case GL_ONE:
        blend_str = "ONE";
        break;
      case GL_SRC_COLOR:
        blend_str = "SRC_COLOR";
        break;
      case GL_ONE_MINUS_SRC_COLOR:
        blend_str = "ONE_MINUS_SRC_COLOR";
        break;
      case GL_DST_COLOR:
        blend_str = "DST_COLOR";
        break;
      case GL_ONE_MINUS_DST_COLOR:
        blend_str = "ONE_MINUS_DST_COLOR";
        break;
      case GL_SRC_ALPHA:
        blend_str = "SRC_ALPHA";
        break;
      case GL_ONE_MINUS_SRC_ALPHA:
        blend_str = "ONE_MINUS_SRC_ALPHA";
        break;
      case GL_DST_ALPHA:
        blend_str = "DST_ALPHA";
        break;
      case GL_ONE_MINUS_DST_ALPHA:
        blend_str = "ONE_MINUS_DST_ALPHA";
        break;
      case GL_CONSTANT_COLOR:
        blend_str = "CONSTANT_COLOR";
        break;
      case GL_ONE_MINUS_CONSTANT_COLOR:
        blend_str = "ONE_MINUS_CONSTANT_COLOR";
        break;
      case GL_CONSTANT_ALPHA:
        blend_str = "CONSTANT_ALPHA";
        break;
      case GL_ONE_MINUS_CONSTANT_ALPHA:
        blend_str = "ONE_MINUS_CONSTANT_ALPHA";
        break;
      case GL_SRC_ALPHA_SATURATE:
        blend_str = "SRC_ALPHA_SATURATE";
        break;
      default:
        blend_str = FormatString("unknown<%x>", blendfunc);
        break;
    }
    return blend_str;
}

std::string _glBlendOpToString(GLenum blendop){
    std::string blend_str;
    switch (blendop) {
      case GL_FUNC_ADD:
        blend_str = "FUNC_ADD";
        break;
      case GL_FUNC_SUBTRACT:
        blend_str = "FUNC_SUBTRACT";
        break;
      case GL_FUNC_REVERSE_SUBTRACT:
        blend_str = "FUNC_REVERSE_SUBTRACT";
        break;
      case GL_MIN:
        blend_str = "MIN";
        break;
      case GL_MAX:
        blend_str = "MAX";
        break;
      default:
        blend_str = FormatString("unknown<%x>", blendop);
        break;
    }
    return blend_str;
}

std::string _glDepthFuncToString(GLenum depthfunc){
    std::string depth_str;
    switch (depthfunc) {
      case GL_NEVER:
        depth_str = "NEVER";
        break;
      case GL_LESS:
        depth_str = "LESS";
        break;
      case GL_EQUAL:
        depth_str = "EQUAL";
        break;
      case GL_LEQUAL:
        depth_str = "LEQUAL";
        break;
      case GL_GREATER:
        depth_str = "GREATER";
        break;
      case GL_NOTEQUAL:
        depth_str = "NOTEQUAL";
        break;
      case GL_GEQUAL:
        depth_str = "GEQUAL";
        break;
      case GL_ALWAYS:
        depth_str = "ALWAYS";
        break;
      default:
        depth_str = FormatString("unknown<%x>", depthfunc);
        break;
    }
    return depth_str;
}

std::string _glStencilFuncToString(GLenum stencilfunc){
    std::string stencil_str;
    switch (stencilfunc) {
      case GL_NEVER:
        stencil_str = "NEVER";
        break;
      case GL_LESS:
        stencil_str = "LESS";
        break;
      case GL_EQUAL:
        stencil_str = "EQUAL";
        break;
      case GL_LEQUAL:
        stencil_str = "LEQUAL";
        break;
      case GL_GREATER:
        stencil_str = "GREATER";
        break;
      case GL_NOTEQUAL:
        stencil_str = "NOTEQUAL";
        break;
      case GL_GEQUAL:
        stencil_str = "GEQUAL";
        break;
      case GL_ALWAYS:
        stencil_str = "ALWAYS";
        break;
      default:
        stencil_str = FormatString("unknown<%x>", stencilfunc);
        break;
    }
    return stencil_str;
}

std::string _glCullModeToString(GLenum cullfacemode){
    std::string cull_str;
    switch (cullfacemode) {
      case GL_FRONT:
        cull_str = "FRONT";
        break;
      case GL_BACK:
        cull_str = "BACK";
        break;
      case GL_FRONT_AND_BACK:
        cull_str = "FRONT_AND_BACK";
        break;
      default:
        cull_str = FormatString("unknown<%x>", cullfacemode);
        break;
    }
    return cull_str;
}

std::string _glFaceWindingToString(GLenum cullfacemode){
    std::string cull_str;
    switch (cullfacemode) {
      case GL_CW:
        cull_str = "CW";
        break;
      case GL_CCW:
        cull_str = "CCW";
        break;
      default:
        cull_str = FormatString("unknown<%x>", cullfacemode);
        break;
    }
    return cull_str;
}

///////////////////////////////////////////////////////////////////////////////
} //namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////
