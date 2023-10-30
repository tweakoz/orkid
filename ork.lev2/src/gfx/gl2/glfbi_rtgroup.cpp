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
  _dsbo      = 0;
  _fbo = 0;
}

///////////////////////////////////////////////////////////////////////////////

  void GlFrameBufferInterface::_pushRtGroup(RtGroup* Base) {
    OrkAssert(false);
  }
  RtGroup* GlFrameBufferInterface::_popRtGroup(bool continue_render) {
    OrkAssert(false);
  }


///////////////////////////////////////////////////////////////////////////////

void GlFrameBufferInterface::_setRtGroup(RtGroup* rtgroup) {

  // printf("FBI<%p> SetRTG<%p>\n", this, rtgroup );

  if (0 == rtgroup) {

    // printf( "SetRtg::disable rt\n" );

    ////////////////////////////////////////////////
    // disable mrt
    //  pop viewport/scissor that was pushed by SetRtGroup( nonzero )
    // on xbox, happens after resolve
    ////////////////////////////////////////////////
    _setAsRenderTarget();
    _active_rtgroup = nullptr;
    return;
  }

  //////////////////////////////////////////////////
  // lazy create mrt's
  //////////////////////////////////////////////////

  int iw = rtgroup->width();
  int ih = rtgroup->height();

  //iw = (iw < 16) ? 16 : iw;
  //ih = (ih < 16) ? 16 : ih;

  GL_ERRORCHECK();

  if( rtgroup->_pseudoRTG ){
    static const SRasterState defstate;
    //_target.RSI()->BindRasterState(defstate, true);
    _active_rtgroup = rtgroup;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    if( rtgroup->_autoclear ){
      rtGroupClear(rtgroup);
    }
    GL_ERRORCHECK();
    return;
  }


  glrtgroupimpl_ptr_t rtg_impl;

  int inumtargets = rtgroup->GetNumTargets();
  int numsamples = msaaEnumToInt(rtgroup->_msaa_samples);
  auto texture_target_2D = (numsamples==1) ? GL_TEXTURE_2D : GL_TEXTURE_2D_MULTISAMPLE;

  if (auto as_impl = rtgroup->_impl.tryAs<glrtgroupimpl_ptr_t>()) {
    rtg_impl = as_impl.value();
  }
  else{

    logchan_rtgroup->log( "create new FBO iw<%d> ih<%d>", iw, ih);

    rtg_impl = std::make_shared<GlRtGroupImpl>();
    rtg_impl->_standard = std::make_shared<GlFboObject>();
    rtg_impl->_depthonly = std::make_shared<GlFboObject>();
    rtgroup->_impl.set<glrtgroupimpl_ptr_t>(rtg_impl);

    glGenFramebuffers(1, &rtg_impl->_standard->_fbo);
    GL_ERRORCHECK();

    if (rtgroup->_needsDepth) {

      // depth renderbuffer

      glGenFramebuffers(1, &rtg_impl->_depthonly->_fbo);
      glGenRenderbuffers(1, &rtg_impl->_standard->_dsbo);
      rtg_impl->_depthonly->_dsbo = rtg_impl->_standard->_dsbo;
      GL_ERRORCHECK();

      // depth texture

      rtgroup->_depthTexture                  = new Texture;
      rtgroup->_depthTexture->_width          = iw;
      rtgroup->_depthTexture->_height         = ih;
      rtgroup->_depthTexture->_msaa_samples = rtgroup->_msaa_samples;

      auto depth_glto = rtgroup->_depthTexture->_impl.makeShared<GLTextureObject>(&mTargetGL.mTxI);

      GL_ERRORCHECK();
    }
    // printf("RtGroup<%p> GenFBO<%d>\n", rtgroup, int(impl->_standard->_fbo));

    //////////////////////////////////////////
    // color buffers
    //////////////////////////////////////////

    for (int it = 0; it < inumtargets; it++) {
      rtbuffer_ptr_t pB = rtgroup->GetMrt(it);
      if (pB->_impl.isA<GlRtBufferImpl*>() == false) {
        auto bufferimpl = new GlRtBufferImpl;
        // printf("RtGroup<%p> RtBuffer<%p> initcolor1\n", rtgroup, pB);
        pB->SetSizeDirty(true);
        //////////////////////////////////////////
        Texture* ptex            = pB->texture();
        ptex->_msaa_samples = rtgroup->_msaa_samples;
        ptex->_debugName         = pB->_debugName;

        auto color_glto = bufferimpl->_teximpl.makeShared<GLTextureObject>(&mTargetGL.mTxI);

        GL_ERRORCHECK();
        glGenTextures(1, (GLuint*)&color_glto->mObject);
        glBindTexture(texture_target_2D, color_glto->mObject);


        if (pB->_debugName.length()) {
          mTargetGL.debugLabel(GL_TEXTURE, color_glto->mObject, pB->_debugName);
        }
        glBindTexture(texture_target_2D, 0);
        GL_ERRORCHECK();
           //////////////////////////////////////////
        ptex->_width          = iw;
        ptex->_height         = ih;
        ptex->_impl = bufferimpl->_teximpl;

        ptex->_varmap.makeValueForKey<GLuint>("gltexobj") = color_glto->mObject;

        mTargetGL.TXI()->ApplySamplingMode(ptex);
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

    logchan_rtgroup->log( "resize FBO iw<%d> ih<%d>", iw, ih);

    //////////////////////////////////////////
    // initialize depth renderbuffer
    if (rtgroup->_needsDepth) {
      GL_ERRORCHECK();
      glBindRenderbuffer(GL_RENDERBUFFER, rtg_impl->_standard->_dsbo);
      GL_ERRORCHECK();
      if(numsamples==1){
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT32, iw, ih);
      }
      else{
        glRenderbufferStorageMultisample(GL_RENDERBUFFER, numsamples, GL_DEPTH_COMPONENT32, iw, ih);
      }
      GL_ERRORCHECK();

      if(rtg_impl->_standard->_depthTexture!=0){
        glDeleteTextures( 1, &rtg_impl->_standard->_depthTexture );
        rtg_impl->_standard->_depthTexture = 0;
      }

      glGenTextures(1, &rtg_impl->_standard->_depthTexture);
      glBindTexture(texture_target_2D, rtg_impl->_standard->_depthTexture);

      rtg_impl->_depthonly->_depthTexture = rtg_impl->_standard->_depthTexture;

      GL_ERRORCHECK();
      std::string DepthTexName("RtgDepth");
      mTargetGL.debugLabel(GL_TEXTURE, rtg_impl->_standard->_depthTexture, DepthTexName);
      if(numsamples==1){
        glTexImage2D(texture_target_2D, 0, GL_DEPTH_COMPONENT32, iw, ih, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
        glTexParameteri(texture_target_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(texture_target_2D, GL_TEXTURE_MAX_LEVEL, 0);
      }
      else{
        glTexImage2DMultisample(texture_target_2D, // target
                                numsamples, // numsamples
                                GL_DEPTH_COMPONENT32, // internal format
                                iw, // w
                                ih,  // h
                                GL_TRUE ); // fixed sample locations
        //glTexParameteri(texture_target_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        //glTexParameteri(texture_target_2D, GL_TEXTURE_MAX_LEVEL, 0);
      }
      // glTexParameteri(texture_target_2D, GL_DEPTH_TEXTURE_MODE, GL_INTENSITY);
      // glTexParameteri(texture_target_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_R_TO_TEXTURE);
      // glTexParameteri(texture_target_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
      GL_ERRORCHECK();
    } else {
      glBindRenderbuffer(GL_RENDERBUFFER, 0);
    }
    glBindTexture(texture_target_2D, 0);
    GL_ERRORCHECK();
    glBindFramebuffer(GL_FRAMEBUFFER, rtg_impl->_standard->_fbo);
    GL_ERRORCHECK();
    if (rtgroup->_needsDepth) {
      // printf("RtGroup<%p> initdepth3\n", rtgroup);
      glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rtg_impl->_standard->_dsbo);
      GL_ERRORCHECK();
      glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, texture_target_2D, rtg_impl->_standard->_depthTexture, 0);
      GL_ERRORCHECK();
      rtgroup->_depthTexture->_varmap.makeValueForKey<GLuint>("gltexobj") = rtg_impl->_standard->_depthTexture;
      mTargetGL.TXI()->ApplySamplingMode(rtgroup->_depthTexture);
      rtgroup->_depthTexture->_isDepthTexture = true;
      auto depthtexobj                     = rtgroup->_depthTexture->_impl.get<gltexobj_ptr_t>();
      depthtexobj->mObject                 = rtg_impl->_standard->_depthTexture;
    }
    //////
    for (int it = 0; it < inumtargets; it++) {
      rtbuffer_ptr_t rtbuffer = rtgroup->GetMrt(it);
      auto bufferimpl    = rtbuffer->_impl.get<GlRtBufferImpl*>();

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

        auto glto = tex->_impl.get<gltexobj_ptr_t>();
        GLuint texobj   = glto->mObject;

        if (tex->_formatSupportsFiltering) {
          tex->mTexSampleMode.PresetTrilinearWrap();
        } else {
          tex->mTexSampleMode.PresetPointAndClamp();
        }

        mTargetGL.debugPushGroup("init-rt-tex",fvec4(1,1,1,1));

        glBindTexture(texture_target_2D, texobj);
        GL_ERRORCHECK();
        void* initialdata = calloc(1, iw * ih * 16);
        if(numsamples==1){
          glTexImage2D(texture_target_2D, 0, glinternalformat, iw, ih, 0, glformat, gltype, initialdata);
        }
        else{
          glTexImage2DMultisample(texture_target_2D, numsamples, glinternalformat, iw, ih, GL_TRUE);
        }
        free(initialdata);
        GL_ERRORCHECK();

        switch (rtbuffer->_mipgen) {
          case RtBuffer::EMG_AUTOCOMPUTE:
          case RtBuffer::EMG_USER: {
            glGenerateMipmap(texture_target_2D);
            int nummips         = std::ceil(log_base(2, std::max(iw, ih))) + 1;
            glto->_maxmip = nummips - 2;
            // printf("SetRtg::gentex<%d> w<%d> h<%d> nummips<%d>\n", int(glto->mObject), iw, ih, nummips);
            break;
          }
          default:
            break;
        }
        // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_LEVEL, 0);
        // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, nummips - 1);
        GL_ERRORCHECK();
        mTargetGL.TXI()->ApplySamplingMode(tex);
        GL_ERRORCHECK();
      } // if (bufferimpl->_init or rtbuffer->mSizeDirty) {

      //////////////////////////////////////////
      //////////////////////////////////////////
      // attach texture to framebuffercolor buffer

      auto color_glto = bufferimpl->_teximpl.get<gltexobj_ptr_t>();

      // printf("RtGroup<%p> RtBuffer<%p> attachcoloridx<%d> fbo<%d>\n", rtgroup, rtbuffer, it, int(glto->mObject));
      glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + it, texture_target_2D, color_glto->mObject, 0);
      GL_ERRORCHECK();

      if (bufferimpl->_init or rtbuffer->mSizeDirty) {
        bufferimpl->_init = false;
        mTargetGL.debugPopGroup();
      }
      rtbuffer->SetSizeDirty(false);
    }
    rtgroup->SetSizeDirty(false);
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    switch (status) {
      case GL_FRAMEBUFFER_COMPLETE:
        break;
      case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
        deco::printf(fvec3::Red(), "GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT\n");
        OrkAssert(false);
        break;
      case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
        deco::printf(fvec3::Red(), "GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT\n");
        OrkAssert(false);
        break;
      case GL_FRAMEBUFFER_UNSUPPORTED:
        deco::printf(fvec3::Red(), "GL_FRAMEBUFFER_UNSUPPORTED\n");
        OrkAssert(false);
        break;
      default:
        deco::printf(fvec3::Red(), "GL_FRAMEBUFFER incomplete (?)\n");
        OrkAssert(false);
        break;
    }
  } // if (rtgroup->IsSizeDirty()) {
  GL_ERRORCHECK();

  //////////////////////////////////////////////////
  // enable mrts
  //////////////////////////////////////////////////

  GLenum buffers[] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3};

  if(rtgroup->_depthOnly){
    glBindFramebuffer(GL_FRAMEBUFFER, rtg_impl->_depthonly->_fbo);
    glBindRenderbuffer(GL_RENDERBUFFER, rtg_impl->_depthonly->_dsbo);
    glDrawBuffers(0, buffers);
  }
  else{
    // printf( "SetRtg::BindFBO<%d> numattachments<%d>\n", int(rtg_impl->_standard->_fbo), inumtargets );

    glBindFramebuffer(GL_FRAMEBUFFER, rtg_impl->_standard->_fbo);
    if (rtgroup->_needsDepth) {
      glBindRenderbuffer(GL_RENDERBUFFER, rtg_impl->_standard->_dsbo);
    }
    glDrawBuffers(inumtargets, buffers);
    GL_ERRORCHECK();
  }



  //////////////////////////////////////////

  GL_ERRORCHECK();

  //static const SRasterState defstate;
  //_target.RSI()->BindRasterState(defstate, true);

  _active_rtgroup = rtgroup;

}

///////////////////////////////////////////////////////////////////////////////

void GlFrameBufferInterface::rtGroupClear(RtGroup* rtg) {
  // glClearColor( 1.0f,1.0f,0.0f,1.0f );
  GL_ERRORCHECK();
  GLuint BufferBits = 0;
  if (rtg->_needsDepth) {
    BufferBits |= GL_DEPTH_BUFFER_BIT;
    glClearDepth(1.0f);
    glDepthRange(0.0, 1.0f);
  }
  if (rtg->GetNumTargets()) {
    BufferBits |= GL_COLOR_BUFFER_BIT;
    const auto& C = rtg->_clearColor;
    glClearColor(C.x, C.y, C.z, C.w);
  }
  glClear(BufferBits);
  GL_ERRORCHECK();
}

///////////////////////////////////////////////////////////////////////////////

void GlFrameBufferInterface::rtGroupMipGen(RtGroup* rtg) {
  auto as_impl     = rtg->_impl.tryAs<glrtgroupimpl_ptr_t>();
  if( as_impl ){
    int inumtargets = rtg->GetNumTargets();
    for (int it = 0; it < inumtargets; it++) {
      auto b = rtg->GetMrt(it);
      if (b) {
        auto bufferimpl = b->_impl.get<GlRtBufferImpl*>();
        auto glto = bufferimpl->_teximpl.get<gltexobj_ptr_t>();
        GLuint tex_obj    = glto->mObject;
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
  dst->Resize(src->width(),src->height());
  PushRtGroup(dst.get());
  auto src_rtb = src->GetMrt(0);
  auto dst_rtb = dst->GetMrt(0);
  auto src_groupimpl = src->_impl.get<glrtgroupimpl_ptr_t>();
  auto dst_groupimpl = dst->_impl.get<glrtgroupimpl_ptr_t>();

  //auto src_bufferimpl = src_rtb->_impl.get<GlRtBufferImpl*>();
  //auto dst_bufferimpl = dst_rtb->_impl.get<GlRtBufferImpl*>();
  //GLuint src_texture    = src_glto->mObject;
  int numsamples = msaaEnumToInt(src->_msaa_samples);
  OrkAssert(numsamples!=1);
  GL_ERRORCHECK();
  glBindFramebuffer(GL_READ_FRAMEBUFFER, src_groupimpl->_standard->_fbo);
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, dst_groupimpl->_standard->_fbo);
  glBlitFramebuffer(0, 0, src->width(), src->height(), //
                    0, 0, dst->width(), dst->height(), //
                    GL_COLOR_BUFFER_BIT, GL_LINEAR); 
  GL_ERRORCHECK();
  PopRtGroup();
}

////////////////////////////////////////////////////////////////////////////////

void GlFrameBufferInterface::blit(rtgroup_ptr_t src, rtgroup_ptr_t dst) {

  int w = dst->width();
  int h = dst->height();

  auto framedata = mTargetGL.topRenderContextFrameData();

  //dst->Resize(w,h);
  PushRtGroup(dst.get());
  auto src_rtb = src->GetMrt(0);
  auto dst_rtb = dst->GetMrt(0);
  auto src_groupimpl = src->_impl.get<glrtgroupimpl_ptr_t>();
  auto dst_groupimpl = dst->_impl.get<glrtgroupimpl_ptr_t>();
  auto this_buf     = this->GetThisBuffer();

  auto shader = utilshader();

  shader->begin(_tek_blit,*framedata);
  shader->_rasterstate->setBlendingMacro(BlendingMacro::OFF);
  shader->bindParamCTex(_fxpColorMap, src->GetMrt(0)->_texture.get());
  shader->bindParamMatrix(_fxpMVP, fmtx4::Identity());
  ViewportRect extents(0, 0, w, h);
  this->pushViewport(extents);
  this->pushScissor(extents);
  this_buf->Render2dQuadEML(fvec4(-1, -1, 2, 2), fvec4(0, 0, 1, 1), fvec4(0, 0, 1, 1));
  this->popViewport();
  this->popScissor();
  shader->end(*framedata);

  PopRtGroup();
}


////////////////////////////////////////////////////////////////////////////////

void GlFrameBufferInterface::downsample2x2(rtgroup_ptr_t src, rtgroup_ptr_t dst) {

  auto DWI = _target.DWI();

  int w = src->width();
  int h = src->height();
  int wd2 = w/2;
  int hd2 = h/2;

  auto framedata = mTargetGL.topRenderContextFrameData();

  dst->Resize(wd2,hd2);
  PushRtGroup(dst.get());
  auto src_rtb = src->GetMrt(0);
  auto dst_rtb = dst->GetMrt(0);
  auto src_groupimpl = src->_impl.get<glrtgroupimpl_ptr_t>();
  auto dst_groupimpl = dst->_impl.get<glrtgroupimpl_ptr_t>();
  auto this_buf     = this->GetThisBuffer();

  auto shader = utilshader();

  shader->begin(_tek_downsample2x2,*framedata);
  shader->_rasterstate->setBlendingMacro(BlendingMacro::OFF);
  shader->bindParamCTex(_fxpColorMap, src->GetMrt(0)->_texture.get());
  shader->bindParamMatrix(_fxpMVP, fmtx4::Identity());
  ViewportRect extents(0, 0, wd2, hd2);
  this->pushViewport(extents);
  this->pushScissor(extents);
  DWI->quad2DEMLCCL(fvec4(-1, -1, 2, 2), fvec4(0, 0, 1, 1), fvec4(0, 0, 1, 1));
  this->popViewport();
  this->popScissor();
  shader->end(*framedata);

  PopRtGroup();
}

////////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
