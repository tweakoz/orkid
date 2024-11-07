////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "statedebug.h"

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////

void _FtxGlDebugger::_validateRaster() {

  using namespace ftxui;
  node_vect_t NODES;

  /////////////////////////////////////
  // query blending state
  /////////////////////////////////////

  GLboolean enabled;
  GLint blendSrcRGB, blendSrcA;
  GLint blendDstRGB, blendDstA;
  GLint blendOpRGB, blendOpA;
  GLfloat blendColor[4];
  glGetBooleanv(GL_BLEND, &enabled);
  glGetIntegerv(GL_BLEND_SRC_RGB, &blendSrcRGB);
  glGetIntegerv(GL_BLEND_DST_RGB, &blendDstRGB);
  glGetIntegerv(GL_BLEND_SRC_ALPHA, &blendSrcA);
  glGetIntegerv(GL_BLEND_DST_ALPHA, &blendDstA);
  glGetIntegerv(GL_BLEND_EQUATION_RGB, &blendOpRGB);
  glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &blendOpA);
  glGetFloatv(GL_BLEND_COLOR, blendColor);

  std::string src_rgb = _glBlendFuncTermToString(blendSrcRGB);
  std::string dst_rgb = _glBlendFuncTermToString(blendDstRGB);
  std::string src_a = _glBlendFuncTermToString(blendSrcA);
  std::string dst_a = _glBlendFuncTermToString(blendDstA);
  std::string op_rgb = _glBlendOpToString(blendOpRGB);
  std::string op_a = _glBlendOpToString(blendOpA);

  std::string blendstr_ena;
  blendstr_ena = enabled ? "ON" : "OFF";
  std::string blendstr_color = FormatString("R: %f G: %f B: %f A: %f", blendColor[0], blendColor[1], blendColor[2], blendColor[3]);
  std::string blendstr_modergb = FormatString("SrcRGB: %-8s DstRGB: %-8s OpRGB: %-8s", src_rgb.c_str(), dst_rgb.c_str(), op_rgb.c_str());  
  std::string blendstr_modea   = FormatString("SrcA:   %-8s DstA:   %-8s OpA:   %-8s", src_a.c_str(), dst_a.c_str(), op_a.c_str());
  _colortext(NODES, YEL, BLU1, "%-16s: %-6s\n", "BlendingEnable", blendstr_ena.c_str() );
  _colortext(NODES, YEL, GR1, "%-16s: %s\n", "BlendColor", blendstr_color.c_str());
  _colortext(NODES, YEL, BLU1, "%-16s: %s\n", "BlendModeRGB", blendstr_modergb.c_str());
  _colortext(NODES, YEL, GR1, "%-16s: %s\n", "BlendModeAlpha", blendstr_modea.c_str());

  /////////////////////////////////////
  // query depth state
  /////////////////////////////////////

  GLboolean depth_enabled;
  GLint depth_func;
  glGetBooleanv(GL_DEPTH_TEST, &depth_enabled);
  glGetIntegerv(GL_DEPTH_FUNC, &depth_func);
  
  auto depthstr = _glDepthFuncToString(depth_func);
  auto depthstr_ena = depth_enabled ? "ON" : "OFF";

  _colortext(NODES, WHI, BLU1, "%-16s: %-6s Func: %s\n", "DepthTest", depthstr_ena, depthstr.c_str());

  /////////////////////////////////////
  // query stencil state
  /////////////////////////////////////

  GLboolean stencil_enabled;
  GLint stencil_func;
  glGetBooleanv(GL_STENCIL_TEST, &stencil_enabled);
  glGetIntegerv(GL_STENCIL_FUNC, &stencil_func);

  auto stencilstr = _glStencilFuncToString(stencil_func);
  auto stencilstr_ena = stencil_enabled ? "ON" : "OFF";

  _colortext(NODES, YEL, GR1, "%-16s: %-6s Func: %s\n", "StencilTest", stencilstr_ena, stencilstr.c_str() );

  /////////////////////////////////////
  // query cull state
  /////////////////////////////////////

  GLboolean cull_enabled;
  GLint cull_mode;
  glGetBooleanv(GL_CULL_FACE, &cull_enabled);
  glGetIntegerv(GL_CULL_FACE_MODE, &cull_mode);

  auto cullstr = _glCullModeToString(cull_mode);
  auto cullstr_ena = cull_enabled ? "ON" : "OFF";

  _colortext(NODES, WHI, BLU1, "%-16s: %-6s Mode: %s\n", "FaceCulling", cullstr_ena, cullstr.c_str() );

  _node_raster = vbox({
      text("Raster State"),
      separator(),
      vbox(std::move(NODES)),
  });

}

///////////////////////////////////////////////////////////////////////////////
} //namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////
