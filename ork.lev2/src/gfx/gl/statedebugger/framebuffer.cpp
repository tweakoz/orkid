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

void _FtxGlDebugger::_validateCurrentFramebuffer() {

  using namespace ftxui;

  GLint currentFBO = 0;
  glGetIntegerv(GL_FRAMEBUFFER_BINDING, &currentFBO);
  GLint status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
  node_vect_t NODES;
  bool complete = (status == GL_FRAMEBUFFER_COMPLETE);

  // get framebuffer statistics (w,h,attachments, formats)
  GLint fbo_w = 0;
  GLint fbo_h = 0;
  GLint fbo_d = 0;
  GLenum format = GL_NONE;
  GLint numattachments = 0;
  // get w 


  _colortext(NODES, WHI, BLK, "currentFBO<%d> status<%x> complete<%d>\n", currentFBO, status, int(complete));
  _node_framebuffer = vbox({
      text("Framebuffer State"),
      separator(),
      vbox(std::move(NODES)),
  });
}

///////////////////////////////////////////////////////////////////////////////
} //namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////
