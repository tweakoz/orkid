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

namespace ork::lev2 {

///////////////////////////////////////////////////////////////////////////////

GlFboObject::GlFboObject() {
  _fbo = 0;
}

///////////////////////////////////////////////////////////////////////////////

void GlFrameBufferInterface::_pushRtGroup(RtGroup* Base) { // final
  RtGroup* prev = mRtGroupStack.top();
  __setRtGroup(Base);
}

void GlFrameBufferInterface::_popRtGroup(bool continue_render) { // final
  RtGroup* prev = mRtGroupStack.top();
  __setRtGroup(prev);
}

///////////////////////////////////////////////////////////////////////////////

void GlFrameBufferInterface::bindRtGroup(RtGroup* rtg) {
  __setRtGroup(rtg);
}

///////////////////////////////////////////////////////////////////////////////

void GlFrameBufferInterface::__setRtGroup(RtGroup* rtgroup) {

  //printf("__setRtGroup<%p:%s> prev<%p>\n", rtgroup, rtgroup ? rtgroup->_name.c_str() : "mainsurf", _active_rtgroup);
  GL_ERRORCHECK();

  _active_rtgroup = rtgroup;

  //////////////////////////////////////////////
  // no rtgroup just means main surface
  //////////////////////////////////////////////

  if (nullptr == rtgroup) {
    _bindMainSurface();
    return;
  }

  //////////////////////////////////////////////
  // initial creation ?
  //////////////////////////////////////////////

  glrtgroupimpl_ptr_t rtg_impl;

  if (auto as_impl = rtgroup->_impl.tryAs<glrtgroupimpl_ptr_t>()) {
    rtg_impl = as_impl.value();
  } else {
    switch(rtgroup->_usage) {
      case "user"_crcu: // user defined rtgroup
        _regenRtgImplFromScratch(rtgroup);
        break;
      case "swapchain"_crcu: // swapchain
      case "popup"_crcu: // popup
        rtg_impl = _buildRtgImplForMainSurface(rtgroup);
        break;
      case "arrayslice"_crcu: // popup
        rtg_impl = _buildRtgImplFromTextureArraySlice(rtgroup);
        break;
      default:
        OrkAssert(false); // unknown usage
        break;
    }
  }

  //////////////////////////////////////////
  // bind it 
  //////////////////////////////////////////

  GL_ERRORCHECK();
  rtg_impl->_bindop();
  GL_ERRORCHECK();

  //////////////////////////////////////////
  // autoclear, if enabled
  //////////////////////////////////////////

  if (rtgroup->_autoclear) {
    rtGroupClear(rtgroup);
  }

  //////////////////////////////////////////
  // apply default raster state
  //////////////////////////////////////////

  _target.FXI()->applyRasterState(_defaultRasterState);

  //////////////////////////////////////////

  GL_ERRORCHECK();
}

///////////////////////////////////////////////////////////////////////////////

glrtgroupimpl_ptr_t GlFrameBufferInterface::_buildRtgImplForMainSurface(RtGroup* rtgroup) {
  
  glrtgroupimpl_ptr_t impl = rtgroup->_impl.makeShared<GlRtGroupImpl>();
  impl->_bindop = [this]() {
    _bindMainSurface();
  };
  return impl;
}

///////////////////////////////////////////////////////////////////////////////

void GlFrameBufferInterface::rtGroupClear(RtGroup* rtg) {

  if (auto as_impl = rtg->_impl.tryAs<glrtgroupimpl_ptr_t>()) {
    as_impl.value()->_bindop();
  }
  
  // glClearColor( 1.0f,1.0f,0.0f,1.0f );
  //printf("clearing rtgroup<%p:%s> color<%d> depth<%d>\n", rtg, rtg->_name.c_str(), int(rtg->_clearMaskColor), int(rtg->_clearMaskDepth));
  GL_ERRORCHECK();
  GLuint BufferBits = rtg->_clearMaskDepth ? GL_DEPTH_BUFFER_BIT : 0;
  if (rtg->_clearMaskDepth) {
    glClearDepth(1.0f);
  }
  // printf( "clear<%p> depthONLY<%d>\n", rtg, int(rtg->_depthOnly) );
  if (rtg->_clearMaskColor and rtg->numImageBuffers()) {
    BufferBits |= GL_COLOR_BUFFER_BIT;
    const auto& C = rtg->_clearColor;
    glClearColor(C.x, C.y, C.z, C.w);
  }
  glClear(BufferBits);
  glDepthRange(0.0, 1.0f);
  GL_ERRORCHECK();
}

////////////////////////////////////////////////////////////////////////////////

void GlFrameBufferInterface::cloneDepthBuffer(rtgroup_ptr_t src_rtg, rtgroup_ptr_t dst_rtg) {
  // Extract width and height from source RTG
  int width  = src_rtg->miW;
  int height = src_rtg->miH;

  // Determine if source is using MSAA
  int num_samples = msaaEnumToInt(src_rtg->_msaa_samples);
  bool isMSAA     = num_samples > 1;

  // Destination RTG implementation pointer and texture object
  glrtgroupimpl_ptr_t dst_rtg_impl;
  gltexobj_ptr_t dst_glto;

  // Try to get the existing implementation or create a new one
  if (auto try_dest = dst_rtg->_impl.tryAs<glrtgroupimpl_ptr_t>()) {
    dst_rtg_impl = try_dest.value();
    dst_glto     = dst_rtg->_depthBuffer->_texture->_impl.get<gltexobj_ptr_t>();
  } else {
    dst_rtg_impl           = dst_rtg->_impl.makeShared<GlRtGroupImpl>();
    dst_rtg->_msaa_samples = MsaaSamples(num_samples); // Copy MSAA settings from source
    dst_rtg->_name         = "RtgDepthCopy";
    dst_rtg->_depthOnly    = true;

    // Create new depth buffer and texture as per MSAA settings
    dst_rtg->_depthBuffer           = dst_rtg->createRenderTarget(EBufferFormat::Z32F);
    dst_rtg->_depthBuffer->_usage = "depth"_crcu;
    auto texture                    = std::make_shared<Texture>();
    texture->_texFormat             = EBufferFormat::Z32F;
    texture->_debugName             = "RtgDepthCopy";
    dst_glto                        = texture->_impl.makeShared<GLTextureObject>(&mTargetGL.mTxI);
    dst_rtg->_depthBuffer->_texture = texture;
  }

  // Resize if necessary
  bool need_resize = (dst_rtg->miW != width) || (dst_rtg->miH != height);
  if (need_resize) {
    dst_rtg->miW  = width;
    dst_rtg->miH  = height;
    auto dest_rtb = dst_rtg->_depthBuffer;

    dest_rtb->_width         = width;
    dest_rtb->_height        = height;
    dst_rtg_impl->_depthonly = std::make_shared<GlFboObject>();

    auto dest_rtbo      = new GlRtBufferImpl;
    dest_rtbo->_teximpl = dst_glto;

    dest_rtb->_impl.set<GlRtBufferImpl*>(dest_rtbo);
    auto dest_tex = dest_rtb->texture();

    auto src_glto = src_rtg->_depthBuffer->_texture->_impl.get<gltexobj_ptr_t>();

    // Handle texture creation based on MSAA
    glGenTextures(1, &dst_glto->_textureObject);

    GLenum target = isMSAA ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D;
    glBindTexture(target, dst_glto->_textureObject);
    if (!isMSAA) {
      glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32, width, height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
    } else {
      glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, num_samples, GL_DEPTH_COMPONENT32, width, height, GL_TRUE);
    }
    glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Setup framebuffer
    GLuint fboCopy;
    glGenFramebuffers(1, &fboCopy);
    glBindFramebuffer(GL_FRAMEBUFFER, fboCopy);
    glFramebufferTexture2D(
        GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, isMSAA ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D, dst_glto->_textureObject, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    dst_rtg_impl->_depthonly->_fbo = fboCopy;
  }

  // Blit from source to destination
  if (auto try_src_rtg_impl = src_rtg->_impl.tryAs<glrtgroupimpl_ptr_t>()) {
    auto src_rtg_impl = try_src_rtg_impl.value();
    //bool is_complete  = _checkFboComplete(src_rtg_impl->_depthonly->_fbo, "clone", src_rtg.get());

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
} // namespace ork::lev2
