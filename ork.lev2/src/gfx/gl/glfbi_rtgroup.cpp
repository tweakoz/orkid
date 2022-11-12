////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include "gl.h"
#include <ork/kernel/string/deco.inl>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/gfxmaterial_ui.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/lev2/ui/viewport.h>
#include <ork/pch.h>

#include <ork/lev2/gfx/dbgfontman.h>

namespace ork::lev2 {

///////////////////////////////////////////////////////////////////////////////

GlFboObject::GlFboObject() {
  _dsbo      = 0;
  _fbo = 0;
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

  iw = (iw < 16) ? 16 : iw;
  ih = (ih < 16) ? 16 : ih;

  GL_ERRORCHECK();

  glrtgroupimpl_ptr_t impl;

  int inumtargets = rtgroup->GetNumTargets();

  if (auto as_impl = rtgroup->_impl.tryAs<glrtgroupimpl_ptr_t>()) {
    impl = as_impl.value();
  }
  else{

    impl = std::make_shared<GlRtGroupImpl>();
    impl->_standard = std::make_shared<GlFboObject>();
    impl->_depthonly = std::make_shared<GlFboObject>();
    rtgroup->_impl.set<glrtgroupimpl_ptr_t>(impl);

    glGenFramebuffers(1, &impl->_standard->_fbo);
    GL_ERRORCHECK();

    if (rtgroup->_needsDepth) {

      // depth renderbuffer

      glGenFramebuffers(1, &impl->_depthonly->_fbo);
      glGenRenderbuffers(1, &impl->_standard->_dsbo);
      impl->_depthonly->_dsbo = impl->_standard->_dsbo;
      GL_ERRORCHECK();

      // depth texture

      rtgroup->_depthTexture                  = new Texture;
      rtgroup->_depthTexture->_width          = iw;
      rtgroup->_depthTexture->_height         = ih;
      GLTextureObject* depthtexobj         = new GLTextureObject;
      rtgroup->_depthTexture->_internalHandle = (void*)depthtexobj;
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
        ptex->_debugName         = pB->_debugName;
        GLTextureObject* ptexOBJ = new GLTextureObject;
        bufferimpl->_teximpl     = ptexOBJ;

        GL_ERRORCHECK();
        glGenTextures(1, (GLuint*)&bufferimpl->_texture);
        glBindTexture(GL_TEXTURE_2D, bufferimpl->_texture);
        if (pB->_debugName.length()) {
          mTargetGL.debugLabel(GL_TEXTURE, bufferimpl->_texture, pB->_debugName);
        }
        glBindTexture(GL_TEXTURE_2D, 0);
        GL_ERRORCHECK();
        ptexOBJ->mObject = bufferimpl->_texture;
        //////////////////////////////////////////
        ptex->_width          = iw;
        ptex->_height         = ih;
        ptex->_internalHandle = (void*)ptexOBJ;

        ptex->_varmap.makeValueForKey<GLuint>("gltexobj") = bufferimpl->_texture;

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

  OrkAssert(impl);

  if (rtgroup->IsSizeDirty()) {
    //////////////////////////////////////////
    // initialize depth renderbuffer
    if (rtgroup->_needsDepth) {
      // printf("RtGroup<%p> initdepth2\n", rtgroup);
      GL_ERRORCHECK();
      glBindRenderbuffer(GL_RENDERBUFFER, impl->_standard->_dsbo);
      GL_ERRORCHECK();
      glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT32, iw, ih);
      GL_ERRORCHECK();
      glGenTextures(1, &impl->_standard->_depthTexture);
      glBindTexture(GL_TEXTURE_2D, impl->_standard->_depthTexture);

      impl->_depthonly->_depthTexture = impl->_standard->_depthTexture;

      GL_ERRORCHECK();
      std::string DepthTexName("RtgDepth");
      mTargetGL.debugLabel(GL_TEXTURE, impl->_standard->_depthTexture, DepthTexName);
      glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32, iw, ih, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
      // glTexParameteri(GL_TEXTURE_2D, GL_DEPTH_TEXTURE_MODE, GL_INTENSITY);
      // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_R_TO_TEXTURE);
      // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
      GL_ERRORCHECK();
    } else {
      glBindRenderbuffer(GL_RENDERBUFFER, 0);
    }
    glBindTexture(GL_TEXTURE_2D, 0);
    GL_ERRORCHECK();
    glBindFramebuffer(GL_FRAMEBUFFER, impl->_standard->_fbo);
    GL_ERRORCHECK();
    if (rtgroup->_needsDepth) {
      // printf("RtGroup<%p> initdepth3\n", rtgroup);
      glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, impl->_standard->_dsbo);
      GL_ERRORCHECK();
      glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, impl->_standard->_depthTexture, 0);
      GL_ERRORCHECK();
      rtgroup->_depthTexture->_varmap.makeValueForKey<GLuint>("gltexobj") = impl->_standard->_depthTexture;
      mTargetGL.TXI()->ApplySamplingMode(rtgroup->_depthTexture);
      rtgroup->_depthTexture->_isDepthTexture = true;
      auto depthtexobj                     = (GLTextureObject*)rtgroup->_depthTexture->_internalHandle;
      depthtexobj->mObject                 = impl->_standard->_depthTexture;
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

        auto orkteximpl = (GLTextureObject*)tex->GetTexIH();
        GLuint texobj   = bufferimpl->_texture;

        if (tex->_formatSupportsFiltering) {
          tex->mTexSampleMode.PresetTrilinearWrap();
        } else {
          tex->mTexSampleMode.PresetPointAndClamp();
        }

        mTargetGL.debugPushGroup("init-rt-tex");

        glBindTexture(GL_TEXTURE_2D, texobj);
        GL_ERRORCHECK();
        void* initialdata = calloc(1, iw * ih * 16);
        glTexImage2D(GL_TEXTURE_2D, 0, glinternalformat, iw, ih, 0, glformat, gltype, initialdata);
        free(initialdata);

        switch (rtbuffer->_mipgen) {
          case RtBuffer::EMG_AUTOCOMPUTE:
          case RtBuffer::EMG_USER: {
            glGenerateMipmap(GL_TEXTURE_2D);
            int nummips         = std::ceil(log_base(2, std::max(iw, ih))) + 1;
            orkteximpl->_maxmip = nummips - 2;
            // printf("SetRtg::gentex<%d> w<%d> h<%d> nummips<%d>\n", int(bufferimpl->_texture), iw, ih, nummips);
            break;
          }
          default:
            break;
        }
        // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_LEVEL, 0);
        // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, nummips - 1);
        mTargetGL.TXI()->ApplySamplingMode(tex);
        GL_ERRORCHECK();
      } // if (bufferimpl->_init or rtbuffer->mSizeDirty) {

      //////////////////////////////////////////
      //////////////////////////////////////////
      // attach texture to framebuffercolor buffer

      // printf("RtGroup<%p> RtBuffer<%p> attachcoloridx<%d> fbo<%d>\n", rtgroup, rtbuffer, it, int(bufferimpl->_texture));

      glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + it, GL_TEXTURE_2D, bufferimpl->_texture, 0);
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
    glBindFramebuffer(GL_FRAMEBUFFER, impl->_depthonly->_fbo);
    glBindRenderbuffer(GL_RENDERBUFFER, impl->_depthonly->_dsbo);
    glDrawBuffers(0, buffers);
  }
  else{
    // printf( "SetRtg::BindFBO<%d> numattachments<%d>\n", int(impl->_standard->_fbo), inumtargets );

    glBindFramebuffer(GL_FRAMEBUFFER, impl->_standard->_fbo);
    if (rtgroup->_needsDepth) {
      glBindRenderbuffer(GL_RENDERBUFFER, impl->_standard->_dsbo);
    }
    glDrawBuffers(inumtargets, buffers);
    GL_ERRORCHECK();
  }



  //////////////////////////////////////////

  GL_ERRORCHECK();

  static const SRasterState defstate;
  _target.RSI()->BindRasterState(defstate, true);

  _currentRtGroup = rtgroup;

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
        auto tex_obj    = bufferimpl->_texture;
        if (b->_mipgen == RtBuffer::EMG_AUTOCOMPUTE) {
          glBindTexture(GL_TEXTURE_2D, tex_obj);
          glGenerateMipmap(GL_TEXTURE_2D);
          b->texture()->TexSamplingMode().PresetPointAndClamp();
          mTargetGL.TXI()->ApplySamplingMode(b->texture());
        }
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
