#include "gl.h"

namespace ork::lev2 {
//////////////////////////////////////////////////////////////////////////////

static std::string _glFormatToName(GLenum format) {
  switch (format) {
    case GL_DEPTH_COMPONENT:
      return "GL_DEPTH_COMPONENT";
    case GL_DEPTH_STENCIL:
      return "GL_DEPTH_STENCIL";
    case GL_RGBA8:
      return "GL_RGBA8";
    case GL_RGBA32F:
      return "GL_RGBA32F";
    case GL_DEPTH_COMPONENT32F:
      return "GL_DEPTH_COMPONENT32F";
    case 0x81a7:
      return "DEPTH_COMPONENT32_ARB";
    default:
      return "Unknown";
  }
}

///////////////////////////////////////////////////////////////////////////////

static void _handleTextureAttachment(GLint textureID, RtGroup* rtg) {
  if (textureID == 0)
    return; // Early exit if no texture is bound

  GLint width, height, format;
  std::string formatName, targetDesc;

  if (rtg->_cubeMap) {
    glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);
    glGetTexLevelParameteriv(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, GL_TEXTURE_WIDTH, &width);
    glGetTexLevelParameteriv(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, GL_TEXTURE_HEIGHT, &height);
    glGetTexLevelParameteriv(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, GL_TEXTURE_INTERNAL_FORMAT, &format);
    targetDesc = "Cubemap";
  } else { // Assume Texture2D if not cubemap
    glBindTexture(GL_TEXTURE_2D, textureID);
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &width);
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &height);
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_INTERNAL_FORMAT, &format);
    targetDesc = "2D Texture";
  }
  formatName = _glFormatToName(format);
  GlFrameBufferInterface::_logchan_rtgroup->log_continue(
      "%s, TEXID<%d> (%dx%d, format: 0x%x:%s)", targetDesc.c_str(), textureID, width, height, format, formatName.c_str());
}

void _handleRenderbufferAttachment(GLint renderbufferID, RtGroup* rtg) {
  if (renderbufferID == 0)
    return; // Early exit if no renderbuffer is bound

  // Query size and format
  GLint width, height, format;
  glBindRenderbuffer(GL_RENDERBUFFER, renderbufferID);
  glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH, &width);
  glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_HEIGHT, &height);
  glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_INTERNAL_FORMAT, &format);
  std::string formatName = _glFormatToName(format); // Assuming _glFormatToName converts GLenum format to a readable string

  GlFrameBufferInterface::_logchan_rtgroup->log_continue(
      "Renderbuffer, RBOID<%d> (%dx%d, format: 0x%x:%s)", renderbufferID, width, height, format, formatName.c_str());
}

///////////////////////////////////////////////////////////////////////////////

static void _dumpFBOstructure(GLuint fboID, std::string name, RtGroup* rtg) {
  GLint previousFBO;
  glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previousFBO); // Save the currently bound FBO

  glBindFramebuffer(GL_FRAMEBUFFER, fboID); // Bind the target FBO to query its structure

  GlFrameBufferInterface::_logchan_rtgroup->log("FBO Structure name<%s> FBOID<%u>", name.c_str(), fboID);

  // Query depth attachment
  GLint depthAttachment = 0;
  glGetFramebufferAttachmentParameteriv(
      GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &depthAttachment);

  if (depthAttachment) {
    GlFrameBufferInterface::_logchan_rtgroup->log_begin("  Depth Attachment: ");
    if (glIsTexture(depthAttachment)) {
      // Detect if the attached texture is a cubemap
      glBindTexture(GL_TEXTURE_2D, depthAttachment); // Temporarily bind to get the target type

      GLint width, height, format;
      std::string formatName, targetDesc;
      if (rtg->_cubeMap) {
        glBindTexture(GL_TEXTURE_CUBE_MAP, depthAttachment);
        glGetTexLevelParameteriv(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, GL_TEXTURE_WIDTH, &width);
        glGetTexLevelParameteriv(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, GL_TEXTURE_HEIGHT, &height);
        glGetTexLevelParameteriv(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, GL_TEXTURE_INTERNAL_FORMAT, &format);
        targetDesc = "Cubemap";
      } else {
        glBindTexture(GL_TEXTURE_2D, depthAttachment);
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &width);
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &height);
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_INTERNAL_FORMAT, &format);
        targetDesc = "Texture";
      }
      formatName = _glFormatToName(format);
      GlFrameBufferInterface::_logchan_rtgroup->log_continue(
          "%s, TEXID<%d> (%dx%d, format: 0x%x:%s)", targetDesc.c_str(), depthAttachment, width, height, format, formatName.c_str());
    } else if (glIsRenderbuffer(depthAttachment)) {
      // query size and format for renderbuffers as usual
      GLint width, height, format;
      glBindRenderbuffer(GL_RENDERBUFFER, depthAttachment);
      glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH, &width);
      glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_HEIGHT, &height);
      glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_INTERNAL_FORMAT, &format);
      std::string formatName = _glFormatToName(format);
      GlFrameBufferInterface::_logchan_rtgroup->log_continue(
          "Renderbuffer, RBOID<%d> (%dx%d, format: 0x%x:%s)", depthAttachment, width, height, format, formatName.c_str());
    }
    GlFrameBufferInterface::_logchan_rtgroup->log_continue("\n");
  }

  // Repeat similar checks and handling for color attachments
  GLint maxColorAttachments = 0;
  glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS, &maxColorAttachments);

  for (GLint i = 0; i < maxColorAttachments; ++i) {
    GLint attachmentType = 0, textureID = 0;
    glGetFramebufferAttachmentParameteriv(
        GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &attachmentType);

    if (attachmentType == GL_NONE)
      continue; // Skip unattached points

      GlFrameBufferInterface::_logchan_rtgroup->log_begin("  Color Attachment idx<%d> : ", i);

    if (attachmentType == GL_TEXTURE) {
      glGetFramebufferAttachmentParameteriv(
          GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &textureID);

      // Handle cubemap and 2D textures appropriately
      _handleTextureAttachment(textureID, rtg); // Implement this function similar to depth attachment handling
    } else if (attachmentType == GL_RENDERBUFFER) {
      // Handle renderbuffer attachment
      //_handleRenderbufferAttachment(renderbufferID,rtg); // Similar handling to above
    }

    GlFrameBufferInterface::_logchan_rtgroup->log_continue("\n");
  }

  glBindFramebuffer(GL_FRAMEBUFFER, previousFBO); // Restore the previously bound FBO
}

// You need to define _handleTextureAttachment and _handleRenderbufferAttachment based on the above examples

///////////////////////////////////////////////////////////////////////////////

static bool _checkFboComplete(GLuint fboID, std::string name, RtGroup* rtg) {
  bool rval              = false;
  GLuint cache_prior_fbo = 0;
  glGetIntegerv(GL_FRAMEBUFFER_BINDING, (GLint*)&cache_prior_fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, fboID);
  GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
  switch (status) {
    case GL_FRAMEBUFFER_COMPLETE:
    deco::printf(fvec3::Green(), "GL_FRAMEBUFFER_COMPLETE!\n");
    rval = true;
      break;
    case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
      deco::printf(fvec3::Red(), "GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT\n");
      break;
    case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
      deco::printf(fvec3::Red(), "GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT\n");
      break;
    case GL_FRAMEBUFFER_UNSUPPORTED:
      deco::printf(fvec3::Red(), "GL_FRAMEBUFFER_UNSUPPORTED\n");
      break;
    default:
      deco::printf(fvec3::Red(), "GL_FRAMEBUFFER incomplete (?) status: %08x\n", status);
      break;
  }
  glBindFramebuffer(GL_FRAMEBUFFER, cache_prior_fbo);

  if (not rval) {
    _dumpFBOstructure(fboID, name, rtg);
    OrkAssert(false);
  }
  return rval;
}

////////////////////////////////////////////////////////////////////////////////

void GlFrameBufferInterface::validateRtGroup(RtGroup* rtg) {
  auto as_impl = rtg->_impl.tryAs<glrtgroupimpl_ptr_t>();
  if (as_impl) {
    auto rtg_impl = as_impl.value();
    if (rtg_impl->_standard) {
      _checkFboComplete(rtg_impl->_standard->_fbo, rtg->_name + ".std", rtg);
    }
    if (rtg_impl->_depthonly) {
      _checkFboComplete(rtg_impl->_depthonly->_fbo, rtg->_name + ".donly", rtg);
    }
  }
}

} //namespace ork::lev2 {
