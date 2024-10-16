////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "gl.h"
#include <ork/kernel/string/deco.inl>
#include <ork/math/misc_math.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/gfxmaterial_ui.h>
#include <ork/lev2/gfx/material_freestyle.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/lev2/ui/viewport.h>
#include <ork/pch.h>

#include <ork/lev2/gfx/dbgfontman.h>
#include <ork/util/logger.h>

namespace ork::lev2 {
static logchannel_ptr_t logchan_rtgroup = logger()->createChannel("GLRTG", fvec3(0.8, 0.2, 0.5), true);

///////////////////////////////////////////////////////////////////////////////

GlFboObject::GlFboObject() {
  _fbo = 0;
}

///////////////////////////////////////////////////////////////////////////////

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

void _handleTextureAttachment(GLint textureID, RtGroup* rtg) {
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
  logchan_rtgroup->log_continue(
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

  logchan_rtgroup->log_continue(
      "Renderbuffer, RBOID<%d> (%dx%d, format: 0x%x:%s)", renderbufferID, width, height, format, formatName.c_str());
}

///////////////////////////////////////////////////////////////////////////////

static void _dumpFBOstructure(GLuint fboID, std::string name, RtGroup* rtg) {
  GLint previousFBO;
  glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previousFBO); // Save the currently bound FBO

  glBindFramebuffer(GL_FRAMEBUFFER, fboID); // Bind the target FBO to query its structure

  logchan_rtgroup->log("FBO Structure name<%s> FBOID<%u>", name.c_str(), fboID);

  // Query depth attachment
  GLint depthAttachment = 0;
  glGetFramebufferAttachmentParameteriv(
      GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &depthAttachment);

  if (depthAttachment) {
    logchan_rtgroup->log_begin("  Depth Attachment: ");
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
      logchan_rtgroup->log_continue(
          "%s, TEXID<%d> (%dx%d, format: 0x%x:%s)", targetDesc.c_str(), depthAttachment, width, height, format, formatName.c_str());
    } else if (glIsRenderbuffer(depthAttachment)) {
      // query size and format for renderbuffers as usual
      GLint width, height, format;
      glBindRenderbuffer(GL_RENDERBUFFER, depthAttachment);
      glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH, &width);
      glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_HEIGHT, &height);
      glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_INTERNAL_FORMAT, &format);
      std::string formatName = _glFormatToName(format);
      logchan_rtgroup->log_continue(
          "Renderbuffer, RBOID<%d> (%dx%d, format: 0x%x:%s)", depthAttachment, width, height, format, formatName.c_str());
    }
    logchan_rtgroup->log_continue("\n");
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

    logchan_rtgroup->log_begin("  Color Attachment idx<%d> : ", i);

    if (attachmentType == GL_TEXTURE) {
      glGetFramebufferAttachmentParameteriv(
          GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &textureID);

      // Handle cubemap and 2D textures appropriately
      _handleTextureAttachment(textureID, rtg); // Implement this function similar to depth attachment handling
    } else if (attachmentType == GL_RENDERBUFFER) {
      // Handle renderbuffer attachment
      //_handleRenderbufferAttachment(renderbufferID,rtg); // Similar handling to above
    }

    logchan_rtgroup->log_continue("\n");
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

///////////////////////////////////////////////////////////////////////////////

static void _validateRtGroup(RtGroup* rtg) {
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

///////////////////////////////////////////////////////////////////////////////

void GlFrameBufferInterface::SetRtGroup(RtGroup* rtgroup) {

  // printf("FBI<%p> SetRTG<%p>\n", this, rtgroup );

  if (0 == rtgroup) {

    // printf( "SetRtg::disable rt\n" );

    ////////////////////////////////////////////////
    // disable mrt
    //  pop viewport/scissor that was pushed by SetRtGroup( nonzero )
    // on xbox, happens after resolve
    ////////////////////////////////////////////////
    _setAsRenderTarget();
    _currentRtGroup = nullptr;
    return;
  }

  //////////////////////////////////////////////////
  // lazy create mrt's
  //////////////////////////////////////////////////

  int iw = rtgroup->width();
  int ih = rtgroup->height();

  if (iw < 1)
    iw = 1;
  if (ih < 1)
    ih = 1;

  // iw = (iw < 16) ? 16 : iw;
  // ih = (ih < 16) ? 16 : ih;

  GL_ERRORCHECK();

  if (rtgroup->_pseudoRTG) {
    static const SRasterState defstate;
    _target.RSI()->BindRasterState(defstate, true);
    _currentRtGroup = rtgroup;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    if (rtgroup->_autoclear) {
      rtGroupClear(rtgroup);
    }
    GL_ERRORCHECK();
    return;
  }

  glrtgroupimpl_ptr_t rtg_impl;

  int inumtargets     = rtgroup->GetNumTargets();
  int numsamples      = msaaEnumToInt(rtgroup->_msaa_samples);
  auto texture_target = (numsamples == 1) ? GL_TEXTURE_2D : GL_TEXTURE_2D_MULTISAMPLE;
  if (rtgroup->_cubeMap) {
    texture_target = GL_TEXTURE_CUBE_MAP;
  }
  if (auto as_impl = rtgroup->_impl.tryAs<glrtgroupimpl_ptr_t>()) {
    rtg_impl = as_impl.value();
  } else {

    rtg_impl             = std::make_shared<GlRtGroupImpl>();
    rtg_impl->_standard  = std::make_shared<GlFboObject>();
    rtg_impl->_depthonly = std::make_shared<GlFboObject>();
    rtgroup->_impl.set<glrtgroupimpl_ptr_t>(rtg_impl);

    glGenFramebuffers(1, &rtg_impl->_standard->_fbo);
    GL_ERRORCHECK();

    logchan_rtgroup->log("create new FBO iw<%d> ih<%d> std FBOID<%d>", iw, ih, int(rtg_impl->_standard->_fbo));

    //////////////////////////////////////////
    // depth only FBO
    //////////////////////////////////////////

    glGenFramebuffers(1, &rtg_impl->_depthonly->_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, rtg_impl->_depthonly->_fbo);

    GL_ERRORCHECK();

    logchan_rtgroup->log("create new FBO iw<%d> ih<%d> donly FBOID<%d>", iw, ih, int(rtg_impl->_depthonly->_fbo));

    //////////////////////////////////////////
    // depth texture
    //////////////////////////////////////////

    rtgroup->_depthBuffer = std::make_shared<RtBuffer>(rtgroup, -1, EBufferFormat::Z32, iw, ih);
    GL_ERRORCHECK();

    auto dtex           = rtgroup->_depthBuffer->_texture;
    dtex->_width        = iw;
    dtex->_height       = ih;
    dtex->_msaa_samples = rtgroup->_msaa_samples;
    dtex->_texFormat    = EBufferFormat::Z32;
    dtex->_debugName    = "RtgDepth";
    auto depth_glto     = dtex->_impl.makeShared<GLTextureObject>(&mTargetGL.mTxI);

    GL_ERRORCHECK();

    // printf("RtGroup<%p> GenFBO<%d>\n", rtgroup, int(impl->_standard->_fbo));

    //////////////////////////////////////////
    // color buffers
    //////////////////////////////////////////

    glBindFramebuffer(GL_FRAMEBUFFER, rtg_impl->_standard->_fbo);
    logchan_rtgroup->log("bind std FBOID<%d>", int(rtg_impl->_standard->_fbo));

    for (int it = 0; it < inumtargets; it++) {
      rtbuffer_ptr_t pB = rtgroup->GetMrt(it);
      if (pB->_impl.isA<GlRtBufferImpl*>() == false) {
        auto bufferimpl = new GlRtBufferImpl;
        // printf("RtGroup<%p> RtBuffer<%p> initcolor1\n", rtgroup, pB);
        pB->SetSizeDirty(true);
        //////////////////////////////////////////
        Texture* ptex       = pB->texture();
        ptex->_msaa_samples = rtgroup->_msaa_samples;
        ptex->_debugName    = pB->_debugName;

        auto color_glto = bufferimpl->_teximpl.makeShared<GLTextureObject>(&mTargetGL.mTxI);

        GL_ERRORCHECK();
        glGenTextures(1, (GLuint*)&color_glto->_textureObject);
        glBindTexture(texture_target, color_glto->_textureObject);

        if (pB->_debugName.length()) {
          mTargetGL.debugLabel(GL_TEXTURE, color_glto->_textureObject, pB->_debugName);
        }
        glBindTexture(texture_target, 0);
        GL_ERRORCHECK();
        //////////////////////////////////////////
        ptex->_width  = iw;
        ptex->_height = ih;
        ptex->_impl   = bufferimpl->_teximpl;

        ptex->_vars->makeValueForKey<GLuint>("gltexobj") = color_glto->_textureObject;

        if (not rtgroup->_cubeMap) {
          mTargetGL.TXI()->ApplySamplingMode(ptex);
        }
        //////////////////////////////////////////
        pB->_impl.set<GlRtBufferImpl*>(bufferimpl);
      }
    }

    rtgroup->SetSizeDirty(true);
  }
  GL_ERRORCHECK();

  //////////////////////////////////////////
  // regen impl
  //////////////////////////////////////////

  OrkAssert(rtg_impl);

  if (rtgroup->IsSizeDirty()) {

    glBindFramebuffer(GL_FRAMEBUFFER, rtg_impl->_standard->_fbo);
    logchan_rtgroup->log("resize FBOID<%d> iw<%d> ih<%d> ", int(rtg_impl->_standard->_fbo), iw, ih);

    //////////////////////////////////////////
    // resize depth texture
    //////////////////////////////////////////

    GL_ERRORCHECK();

    if (rtg_impl->_standard->_depthTexObject != 0) {
      glDeleteTextures(1, &rtg_impl->_standard->_depthTexObject);
      rtg_impl->_standard->_depthTexObject = 0;
    }

    glGenTextures(1, &rtg_impl->_standard->_depthTexObject);
    glBindTexture(texture_target, rtg_impl->_standard->_depthTexObject);

    rtg_impl->_depthonly->_depthTexObject = rtg_impl->_standard->_depthTexObject;

    auto dtex2           = rtgroup->_depthBuffer->_texture;
    dtex2->_width        = iw;
    dtex2->_height       = ih;
    dtex2->_msaa_samples = rtgroup->_msaa_samples;
    dtex2->_texFormat    = EBufferFormat::Z32;
    dtex2->_debugName    = "RtgDepth";
    auto depth_glto      = dtex2->_impl.getShared<GLTextureObject>();

    depth_glto->_textureObject = rtg_impl->_standard->_depthTexObject;
    depth_glto->mTarget        = texture_target;

    GL_ERRORCHECK();
    std::string DepthTexName("RtgDepth");
    mTargetGL.debugLabel(GL_TEXTURE, rtg_impl->_standard->_depthTexObject, DepthTexName);
    if (numsamples == 1) {
      if (rtgroup->_cubeMap) {
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, GL_DEPTH_COMPONENT32, iw, ih, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 0, GL_DEPTH_COMPONENT32, iw, ih, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, 0, GL_DEPTH_COMPONENT32, iw, ih, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, 0, GL_DEPTH_COMPONENT32, iw, ih, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, 0, GL_DEPTH_COMPONENT32, iw, ih, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, 0, GL_DEPTH_COMPONENT32, iw, ih, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
      } else {
        glTexImage2D(texture_target, 0, GL_DEPTH_COMPONENT32, iw, ih, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
        glTexParameteri(texture_target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(texture_target, GL_TEXTURE_MAX_LEVEL, 0);
      }
    } else {
      glTexImage2DMultisample(
          texture_target,       // target
          numsamples,           // numsamples
          GL_DEPTH_COMPONENT32, // internal format
          iw,                   // w
          ih,                   // h
          GL_TRUE);             // fixed sample locations
    }
    GL_ERRORCHECK();

    glBindTexture(texture_target, 0);
    GL_ERRORCHECK();
    glBindFramebuffer(GL_FRAMEBUFFER, rtg_impl->_standard->_fbo);
    GL_ERRORCHECK();

    // printf("RtGroup<%p> initdepth3\n", rtgroup);
    if (rtgroup->_cubeMap) {
      GLenum faces[6] = {
          GL_TEXTURE_CUBE_MAP_POSITIVE_X,
          GL_TEXTURE_CUBE_MAP_NEGATIVE_X,
          GL_TEXTURE_CUBE_MAP_POSITIVE_Y,
          GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
          GL_TEXTURE_CUBE_MAP_POSITIVE_Z,
          GL_TEXTURE_CUBE_MAP_NEGATIVE_Z};

      for (int i = 0; i < 6; ++i) {
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, faces[i], rtg_impl->_standard->_depthTexObject, 0);
      }
    } else {
      glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, texture_target, rtg_impl->_standard->_depthTexObject, 0);
    }

    GL_ERRORCHECK();
    auto depth_texture                                        = rtgroup->_depthBuffer->_texture;
    depth_texture->_vars->makeValueForKey<GLuint>("gltexobj") = rtg_impl->_standard->_depthTexObject;
    mTargetGL.TXI()->ApplySamplingMode(depth_texture.get());
    depth_texture->_isDepthTexture = true;
    auto depthtexobj               = depth_texture->_impl.get<gltexobj_ptr_t>();
    depthtexobj->_textureObject    = rtg_impl->_standard->_depthTexObject;

    //////////////////////////////////////////
    // attach depthtexture to depth only FBO
    //////////////////////////////////////////

    glBindFramebuffer(GL_FRAMEBUFFER, rtg_impl->_depthonly->_fbo);
    if (rtgroup->_cubeMap) {
      GLenum faces[6] = {
          GL_TEXTURE_CUBE_MAP_POSITIVE_X,
          GL_TEXTURE_CUBE_MAP_NEGATIVE_X,
          GL_TEXTURE_CUBE_MAP_POSITIVE_Y,
          GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
          GL_TEXTURE_CUBE_MAP_POSITIVE_Z,
          GL_TEXTURE_CUBE_MAP_NEGATIVE_Z};

      for (int i = 0; i < 6; ++i) {
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, faces[i], rtg_impl->_standard->_depthTexObject, 0);
      }
    } else {
      glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, texture_target, rtg_impl->_standard->_depthTexObject, 0);
    }

    //////////////////////////////////////////
    // resize color buffers
    //////////////////////////////////////////

    glBindFramebuffer(GL_FRAMEBUFFER, rtg_impl->_standard->_fbo);

    //////
    for (int it = 0; it < inumtargets; it++) {
      rtbuffer_ptr_t rtbuffer = rtgroup->GetMrt(it);
      auto bufferimpl         = rtbuffer->_impl.get<GlRtBufferImpl*>();

      auto tex = rtbuffer->texture();
      if (bufferimpl->_init or rtbuffer->mSizeDirty) {
        // printf("RtGroup<%p> RtBuffer<%p> initcolor2\n", rtgroup, rtbuffer);
        // D3DFORMAT efmt = D3DFMT_A8R8G8B8;
        GLuint glinternalformat = 0;
        GLuint glformat         = GL_RGBA;
        GLenum gltype           = 0;
        switch (rtbuffer->format()) {
          case EBufferFormat::R32F:
            glformat         = GL_RED;
            glinternalformat = GL_R32F;
            gltype           = GL_FLOAT;
            break;
          case EBufferFormat::R32UI:
            glformat                      = GL_RED_INTEGER;
            glinternalformat              = GL_R32UI;
            gltype                        = GL_UNSIGNED_INT;
            tex->_formatSupportsFiltering = false;
            break;
          case EBufferFormat::RG16F:
            glformat         = GL_RG;
            glinternalformat = GL_RG16F;
            gltype           = GL_HALF_FLOAT;
            break;
          case EBufferFormat::RG32F:
            glformat         = GL_RG;
            glinternalformat = GL_RG32F;
            gltype           = GL_FLOAT;
            break;
          case EBufferFormat::RGBA8:
            glinternalformat = GL_RGBA8;
            gltype           = GL_UNSIGNED_BYTE;
            break;
          case EBufferFormat::RGBA16F:
            glinternalformat = GL_RGBA16F;
            gltype           = GL_HALF_FLOAT;
            break;
          case EBufferFormat::RGBA16UI:
            glformat                      = GL_RGBA_INTEGER;
            glinternalformat              = GL_RGBA16UI;
            gltype                        = GL_UNSIGNED_SHORT;
            tex->_formatSupportsFiltering = false;
            break;
          case EBufferFormat::RGBA32F:
            glinternalformat = GL_RGBA32F;
            gltype           = GL_FLOAT;
            break;
          case EBufferFormat::RGB10A2:
            glinternalformat = GL_RGB10_A2;
            gltype           = GL_UNSIGNED_INT_10_10_10_2;
            break;
          case EBufferFormat::RGB32UI:
            glformat                      = GL_RGB_INTEGER;
            glinternalformat              = GL_RGB32UI;
            gltype                        = GL_UNSIGNED_INT;
            tex->_formatSupportsFiltering = false;
            break;
          case EBufferFormat::RGBA32UI:
            glformat                      = GL_RGBA_INTEGER;
            glinternalformat              = GL_RGBA32UI;
            gltype                        = GL_UNSIGNED_INT;
            tex->_formatSupportsFiltering = false;
            break;
          default:
            OrkAssert(false);
            break;
            // case EBufferFormat::RGBA32F: glinternalformat = GL_RGBA32; break;
        }

        //////////////////////////////////////////
        // initialize texture
        //////////////////////////////////////////

        auto glto     = tex->_impl.get<gltexobj_ptr_t>();
        GLuint texobj = glto->_textureObject;

        if (tex->_formatSupportsFiltering) {
          tex->mTexSampleMode.PresetTrilinearWrap();
        } else {
          tex->mTexSampleMode.PresetPointAndClamp();
        }

        mTargetGL.debugPushGroup("init-rt-tex");

        glBindTexture(texture_target, texobj);
        GL_ERRORCHECK();
        void* initialdata = calloc(1, iw * ih * 16);
        if (numsamples == 1) {
          if (rtgroup->_cubeMap) {
            for (int i = 0; i < 6; ++i) {
              glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, glinternalformat, iw, ih, 0, glformat, gltype, initialdata);
            }
          } else {
            glTexImage2D(texture_target, 0, glinternalformat, iw, ih, 0, glformat, gltype, initialdata);
          }
        } else {
          glTexImage2DMultisample(texture_target, numsamples, glinternalformat, iw, ih, GL_TRUE);
        }
        free(initialdata);
        GL_ERRORCHECK();

        switch (rtbuffer->_mipgen) {
          case RtBuffer::EMG_AUTOCOMPUTE:
          case RtBuffer::EMG_USER: {
            glGenerateMipmap(texture_target);
            int nummips   = std::ceil(log_base(2, std::max(iw, ih))) + 1;
            glto->_maxmip = nummips - 2;
            // printf("SetRtg::gentex<%d> w<%d> h<%d> nummips<%d>\n", int(glto->_textureObject), iw, ih, nummips);
            break;
          }
          default:
            break;
        }
        // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_LEVEL, 0);
        // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, nummips - 1);
        GL_ERRORCHECK();
        if (not rtgroup->_cubeMap) {
          mTargetGL.TXI()->ApplySamplingMode(tex);
        }
        GL_ERRORCHECK();
      } // if (bufferimpl->_init or rtbuffer->mSizeDirty) {

      //////////////////////////////////////////
      // attach texture to framebuffercolor buffer
      //////////////////////////////////////////

      auto color_glto = bufferimpl->_teximpl.get<gltexobj_ptr_t>();
      color_glto->mTarget = texture_target;
      // printf("RtGroup<%p> RtBuffer<%p> attachcoloridx<%d> fbo<%d>\n", rtgroup, rtbuffer, it, int(glto->_textureObject));

      if (rtgroup->_cubeMap) {
        GLenum faces[6] = {
            GL_TEXTURE_CUBE_MAP_POSITIVE_X,
            GL_TEXTURE_CUBE_MAP_NEGATIVE_X,
            GL_TEXTURE_CUBE_MAP_POSITIVE_Y,
            GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
            GL_TEXTURE_CUBE_MAP_POSITIVE_Z,
            GL_TEXTURE_CUBE_MAP_NEGATIVE_Z};

        for (int i = 0; i < 6; ++i) {
          glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + it, faces[i], color_glto->_textureObject, 0);
        }
      } else {
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + it, texture_target, color_glto->_textureObject, 0);
      }

      GL_ERRORCHECK();

      if (bufferimpl->_init or rtbuffer->mSizeDirty) {
        bufferimpl->_init = false;
        mTargetGL.debugPopGroup();
      }
      rtbuffer->SetSizeDirty(false);

    } //     for (int it = 0; it < inumtargets; it++) {

    //////////////////////////////////////////

    rtgroup->SetSizeDirty(false);

    _validateRtGroup(rtgroup);
    //_dumpFBOstructure(rtg_impl->_standard->_fbo, "UPDIMPL::" + rtgroup->_name + ".STD",rtgroup);
    //_dumpFBOstructure(rtg_impl->_depthonly->_fbo, "UPDIMPL::" + rtgroup->_name + ".DONLY",rtgroup);

  } // if (rtgroup->IsSizeDirty()) {
  GL_ERRORCHECK();

  //////////////////////////////////////////////////
  // enable mrts
  //////////////////////////////////////////////////

  GLenum buffers[] = {
      GL_COLOR_ATTACHMENT0,
      GL_COLOR_ATTACHMENT1,
      GL_COLOR_ATTACHMENT2,
      GL_COLOR_ATTACHMENT3,
      GL_COLOR_ATTACHMENT4,
      GL_COLOR_ATTACHMENT5,
      GL_COLOR_ATTACHMENT6,
      GL_COLOR_ATTACHMENT7};

  if (rtgroup->_depthOnly) {
    //_dumpFBOstructure(rtg_impl->_depthonly->_fbo, "UPDIMPL::"+rtgroup->_name + ".DONLY");
    // printf( "SetRtg::BindFBO<%d> depthONLY!!\n", int(rtg_impl->_depthonly->_fbo) );
    glBindFramebuffer(GL_FRAMEBUFFER, rtg_impl->_depthonly->_fbo);
    glBindTexture(texture_target, rtg_impl->_depthonly->_depthTexObject);
    //color_glto->mTarget = texture_target;
    glDrawBuffers(0, nullptr);
  } else {

    if (rtgroup->_cubeMap) {
      //
      auto bufferimpl = rtgroup->mMrt[0]->_impl.get<GlRtBufferImpl*>();
      auto color_glto = bufferimpl->_teximpl.getShared<GLTextureObject>();
      color_glto->mTarget = GL_TEXTURE_CUBE_MAP;

      /////////////////////////////////////////////////////////////
      // bind rtgroup->_cubeRenderFace to 
      // framebuffer color and depth attachment 0
      /////////////////////////////////////////////////////////////

      glBindFramebuffer(GL_FRAMEBUFFER, rtg_impl->_standard->_fbo);
      glFramebufferTexture2D( GL_FRAMEBUFFER, 
                              GL_COLOR_ATTACHMENT0, // attachment point
                              GL_TEXTURE_CUBE_MAP_POSITIVE_X+rtgroup->_cubeRenderFace, // face
                              color_glto->_textureObject, 
                              0); // mip

      glFramebufferTexture2D( GL_FRAMEBUFFER, 
                              GL_DEPTH_ATTACHMENT, 
                              GL_TEXTURE_CUBE_MAP_POSITIVE_X+rtgroup->_cubeRenderFace, 
                              rtg_impl->_standard->_depthTexObject, 
                              0);

    } else {
      glBindFramebuffer(GL_FRAMEBUFFER, rtg_impl->_standard->_fbo);
      glBindTexture(texture_target, rtg_impl->_standard->_depthTexObject);
    }

    glDrawBuffers(inumtargets, buffers);
    GL_ERRORCHECK();
  }


  //////////////////////////////////////////

  GL_ERRORCHECK();

  static const SRasterState defstate;
  _target.RSI()->BindRasterState(defstate, true);

  _currentRtGroup = rtgroup;

  //_validateRtGroup(_currentRtGroup);
}

///////////////////////////////////////////////////////////////////////////////

void GlFrameBufferInterface::rtGroupClear(RtGroup* rtg) {
  // glClearColor( 1.0f,1.0f,0.0f,1.0f );
  GL_ERRORCHECK();
  GLuint BufferBits = rtg->_clearMaskDepth ? GL_DEPTH_BUFFER_BIT : 0;
  if(rtg->_clearMaskDepth){
    glClearDepth(1.0f);
  }
  // printf( "clear<%p> depthONLY<%d>\n", rtg, int(rtg->_depthOnly) );
  if(rtg->_clearMaskColor and rtg->GetNumTargets() ) {
    BufferBits |= GL_COLOR_BUFFER_BIT;
    const auto& C = rtg->_clearColor;
    glClearColor(C.x, C.y, C.z, C.w);
  }
  glClear(BufferBits);
  glDepthRange(0.0, 1.0f);
  GL_ERRORCHECK();
}

///////////////////////////////////////////////////////////////////////////////

void GlFrameBufferInterface::rtGroupMipGen(RtGroup* rtg) {
  auto as_impl = rtg->_impl.tryAs<glrtgroupimpl_ptr_t>();
  if (as_impl) {
    int inumtargets = rtg->GetNumTargets();
    for (int it = 0; it < inumtargets; it++) {
      auto b = rtg->GetMrt(it);
      if (b) {
        auto bufferimpl = b->_impl.get<GlRtBufferImpl*>();
        auto glto       = bufferimpl->_teximpl.get<gltexobj_ptr_t>();
        GLuint tex_obj  = glto->_textureObject;
        if (b->_mipgen == RtBuffer::EMG_AUTOCOMPUTE) {
          GL_ERRORCHECK();
          glBindTexture(GL_TEXTURE_2D, tex_obj);
          glGenerateMipmap(GL_TEXTURE_2D);
          b->texture()->TexSamplingMode().PresetPointAndClamp();
          mTargetGL.TXI()->ApplySamplingMode(b->texture());
          GL_ERRORCHECK();
        }
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////

void GlFrameBufferInterface::msaaBlit(rtgroup_ptr_t src, rtgroup_ptr_t dst) {
  dst->Resize(src->width(), src->height());
  PushRtGroup(dst.get());
  auto src_rtb       = src->GetMrt(0);
  auto dst_rtb       = dst->GetMrt(0);
  auto src_groupimpl = src->_impl.get<glrtgroupimpl_ptr_t>();
  auto dst_groupimpl = dst->_impl.get<glrtgroupimpl_ptr_t>();

  // auto src_bufferimpl = src_rtb->_impl.get<GlRtBufferImpl*>();
  // auto dst_bufferimpl = dst_rtb->_impl.get<GlRtBufferImpl*>();
  // GLuint src_texture    = src_glto->_textureObject;
  int numsamples = msaaEnumToInt(src->_msaa_samples);
  OrkAssert(numsamples != 1);
  GL_ERRORCHECK();
  glBindFramebuffer(GL_READ_FRAMEBUFFER, src_groupimpl->_standard->_fbo);
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, dst_groupimpl->_standard->_fbo);
  glBlitFramebuffer(
      0,
      0,
      src->width(),
      src->height(), //
      0,
      0,
      dst->width(),
      dst->height(), //
      GL_COLOR_BUFFER_BIT,
      GL_LINEAR);
  GL_ERRORCHECK();
  PopRtGroup();
}

////////////////////////////////////////////////////////////////////////////////

void GlFrameBufferInterface::blit(rtgroup_ptr_t src, rtgroup_ptr_t dst) {

  int w = dst->width();
  int h = dst->height();

  auto framedata = mTargetGL.topRenderContextFrameData();

  // dst->Resize(w,h);
  PushRtGroup(dst.get());
  auto src_rtb       = src->GetMrt(0);
  auto dst_rtb       = dst->GetMrt(0);
  auto src_groupimpl = src->_impl.get<glrtgroupimpl_ptr_t>();
  auto dst_groupimpl = dst->_impl.get<glrtgroupimpl_ptr_t>();
  auto this_buf      = this->GetThisBuffer();

  auto shader = utilshader();

  shader->begin(_tek_blit, framedata);
  shader->_rasterstate.SetBlending(Blending::OFF);
  shader->bindParamCTex(_fxpColorMap, src->GetMrt(0)->_texture.get());
  shader->bindParamMatrix(_fxpMVP, fmtx4::Identity());
  ViewportRect extents(0, 0, w, h);
  this->pushViewport(extents);
  this->pushScissor(extents);
  this_buf->Render2dQuadEML(fvec4(-1, -1, 2, 2), fvec4(0, 0, 1, 1), fvec4(0, 0, 1, 1));
  this->popViewport();
  this->popScissor();
  shader->end(framedata);

  PopRtGroup();
}

////////////////////////////////////////////////////////////////////////////////

void GlFrameBufferInterface::cloneDepthBuffer(rtgroup_ptr_t src_rtg, rtgroup_ptr_t dst_rtg) {
  // Extract width and height from source RTG
  int width = src_rtg->miW;
  int height = src_rtg->miH;

  // Determine if source is using MSAA
  int num_samples = msaaEnumToInt(src_rtg->_msaa_samples);
  bool isMSAA = num_samples > 1;

  // Destination RTG implementation pointer and texture object
  glrtgroupimpl_ptr_t dst_rtg_impl;
  gltexobj_ptr_t dst_glto;

  // Try to get the existing implementation or create a new one
  if (auto try_dest = dst_rtg->_impl.tryAs<glrtgroupimpl_ptr_t>()) {
    dst_rtg_impl = try_dest.value();
    dst_glto = dst_rtg->_depthBuffer->_texture->_impl.get<gltexobj_ptr_t>();
  } else {
    dst_rtg_impl = dst_rtg->_impl.makeShared<GlRtGroupImpl>();
    dst_rtg->_msaa_samples = MsaaSamples(num_samples);  // Copy MSAA settings from source
    dst_rtg->_name = "RtgDepthCopy";
    dst_rtg->_depthOnly = true;

    // Create new depth buffer and texture as per MSAA settings
    dst_rtg->_depthBuffer = dst_rtg->createRenderTarget(EBufferFormat::Z32);
    auto texture = std::make_shared<Texture>();
    texture->_texFormat = EBufferFormat::Z32;
    texture->_debugName = "RtgDepthCopy";
    dst_glto = texture->_impl.makeShared<GLTextureObject>(&mTargetGL.mTxI);
    dst_rtg->_depthBuffer->_texture = texture;
  }

  // Resize if necessary
  bool need_resize = (dst_rtg->miW != width) || (dst_rtg->miH != height);
  if (need_resize) {
    dst_rtg->miW = width;
    dst_rtg->miH = height;
    auto dest_rtb = dst_rtg->_depthBuffer;

    dest_rtb->_width = width;
    dest_rtb->_height = height;
    dst_rtg_impl->_depthonly = std::make_shared<GlFboObject>();

    auto dest_rtbo = new GlRtBufferImpl;
    dest_rtbo->_teximpl = dst_glto;

    dest_rtb->_impl.set<GlRtBufferImpl*>(dest_rtbo);
    auto dest_tex = dest_rtb->texture();

    auto src_glto = src_rtg->_depthBuffer->_texture->_impl.get<gltexobj_ptr_t>();

    // Handle texture creation based on MSAA
    glGenTextures(1, &dst_glto->_textureObject);
    glBindTexture(isMSAA ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D, dst_glto->_textureObject);
    if (!isMSAA) {
      glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32, width, height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
    } else {
      glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, num_samples, GL_DEPTH_COMPONENT32, width, height, GL_TRUE);
    }
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Setup framebuffer
    GLuint fboCopy;
    glGenFramebuffers(1, &fboCopy);
    glBindFramebuffer(GL_FRAMEBUFFER, fboCopy);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, isMSAA ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D, dst_glto->_textureObject, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    dst_rtg_impl->_depthonly->_fbo = fboCopy;
  }

  // Blit from source to destination
  if (auto try_src_rtg_impl = src_rtg->_impl.tryAs<glrtgroupimpl_ptr_t>()) {
    auto src_rtg_impl = try_src_rtg_impl.value();
    bool is_complete = _checkFboComplete(src_rtg_impl->_depthonly->_fbo, "clone", src_rtg.get());

    // Setup and perform the blit operation
    GLint scissor[4];
    glGetIntegerv(GL_SCISSOR_BOX, scissor);
    glScissor(0, 0, width, height);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, src_rtg_impl->_depthonly->_fbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, dst_rtg_impl->_depthonly->_fbo);
    glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_DEPTH_BUFFER_BIT, GL_NEAREST);
    glScissor(scissor[0], scissor[1], scissor[2], scissor[3]);
    GL_ERRORCHECK();
  }
}

////////////////////////////////////////////////////////////////////////////////

void GlFrameBufferInterface::validateRtGroup(rtgroup_ptr_t rtg) {
  _validateRtGroup(rtg.get());
}

////////////////////////////////////////////////////////////////////////////////

void GlFrameBufferInterface::downsample2x2(rtgroup_ptr_t src, rtgroup_ptr_t dst) {

  auto DWI = _target.DWI();

  int w   = src->width();
  int h   = src->height();
  int wd2 = w / 2;
  int hd2 = h / 2;

  auto framedata = mTargetGL.topRenderContextFrameData();

  dst->Resize(wd2, hd2);
  PushRtGroup(dst.get());
  auto src_rtb       = src->GetMrt(0);
  auto dst_rtb       = dst->GetMrt(0);
  auto src_groupimpl = src->_impl.get<glrtgroupimpl_ptr_t>();
  auto dst_groupimpl = dst->_impl.get<glrtgroupimpl_ptr_t>();
  auto this_buf      = this->GetThisBuffer();

  auto shader = utilshader();

  shader->begin(_tek_downsample2x2, framedata);
  shader->_rasterstate.SetBlending(Blending::OFF);
  shader->bindParamCTex(_fxpColorMap, src->GetMrt(0)->_texture.get());
  shader->bindParamMatrix(_fxpMVP, fmtx4::Identity());
  ViewportRect extents(0, 0, wd2, hd2);
  this->pushViewport(extents);
  this->pushScissor(extents);
  DWI->quad2DEMLCCL(fvec4(-1, -1, 2, 2), fvec4(0, 0, 1, 1), fvec4(0, 0, 1, 1));
  this->popViewport();
  this->popScissor();
  shader->end(framedata);

  PopRtGroup();
}

////////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
