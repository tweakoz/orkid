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
  GLint attached_obj_type = GL_NONE;
  std::string attachment_type;
  for( int a=0; a<8; a++ ){
    glGetFramebufferAttachmentParameteriv(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0+a, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &attached_obj_type);
    attachment_type = GLenumToString(attached_obj_type);
    _colortext(NODES, WHI, BLK, "  CLR attached<%d> is %s", a, attachment_type.c_str());
  }

  // get depth attachment
  glGetFramebufferAttachmentParameteriv(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &attached_obj_type);
  attachment_type = GLenumToString(attached_obj_type);
  switch(attached_obj_type){
    case GL_TEXTURE: {
      // get texture id
      GLint tex_id = 0;
      glGetFramebufferAttachmentParameteriv(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &tex_id);
      // get texture format (rgba, depth, etc)
      GLint tex_format = GL_NONE;
      glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_INTERNAL_FORMAT, &tex_format);
      std::string tex_format_str = GLenumToString(tex_format);
      _colortext(NODES, WHI, BLK, "  DEPTH attached is texture fmt<%s> id<%d>\n", tex_format_str.c_str(), tex_id);
      break;
    }
    default: {
      _colortext(NODES, WHI, BLK, "  DEPTH attached %s\n", attachment_type.c_str());
      break;
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
