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

  _colortext(NODES, WHI, BLK, "currentFBO<%d> status<%x> complete<%d>\n", currentFBO, status, int(complete));

  // get number of attachments
  for( int a=0; a<8; a++ ){
    GLint attached_obj_type = GL_NONE;
    glGetFramebufferAttachmentParameteriv(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0+a, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &attached_obj_type);
    switch(attached_obj_type){
      case GL_NONE: {
        _colortext(NODES, WHI, BLK, "  attached<%d> is_none", a);
        break;
      }
      case GL_FRAMEBUFFER_DEFAULT: {
        _colortext(NODES, WHI, BLK, "  attached<%d> is_framebuffer_default", a);
        break;
      }
      case GL_TEXTURE: {
        _colortext(NODES, WHI, BLK, "  attached<%d> is_texture", a);
        break;
      }
      case GL_RENDERBUFFER: {
        _colortext(NODES, WHI, BLK, "  attached<%d> is_renderbuffer\n", a);
        break;
      }
    }
  }

  _node_framebuffer = vbox({
      text("Framebuffer State"),
      separator(),
      vbox(std::move(NODES)),
  });
}

///////////////////////////////////////////////////////////////////////////////
} //namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////
