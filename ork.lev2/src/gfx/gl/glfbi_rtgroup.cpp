////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include "gl.h"
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/gfxmaterial_ui.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/lev2/ui/viewport.h>
#include <ork/pch.h>

#include <ork/lev2/gfx/dbgfontman.h>

namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////

void GlFrameBufferInterface::SetRtGroup(RtGroup* Base) {
  mTargetGL.debugPushGroup("GlFrameBufferInterface::SetRtGroup");

  mTargetGL.MakeCurrentContext();

  if (0 == Base) {
    if (mCurrentRtGroup) {
      auto FboObj = (GlFboObject*)mCurrentRtGroup->GetInternalHandle();
      int inumtargets     = mCurrentRtGroup->GetNumTargets();

      for (int it = 0; it < inumtargets; it++) {
        auto b = mCurrentRtGroup->GetMrt(it);

        if (FboObj && b && b->mComputeMips) {
          auto tex_obj = FboObj->mTEX[it];

          // printf( "GENMIPS texobj<%p>\n", (void*) tex_obj );

          // glBindTexture( GL_TEXTURE_2D, tex_obj );
          // glGenerateMipmap( GL_TEXTURE_2D );
        }
      }
    }

    // printf( "SetRtg::disable rt\n" );

    ////////////////////////////////////////////////
    // disable mrt
    //  pop viewport/scissor that was pushed by SetRtGroup( nonzero )
    // on xbox, happens after resolve
    ////////////////////////////////////////////////
    _setAsRenderTarget();
    mCurrentRtGroup = 0;
    mTargetGL.debugPopGroup();
    return;
  }

  //////////////////////////////////////////////////
  // lazy create mrt's
  //////////////////////////////////////////////////

  int iw = Base->GetW();
  int ih = Base->GetH();

  iw = (iw < 16) ? 16 : iw;
  ih = (ih < 16) ? 16 : ih;

  auto FboObj = (GlFboObject*)Base->GetInternalHandle();

  int inumtargets = Base->GetNumTargets();

  // D3DMULTISAMPLE_TYPE sampletype;
  switch (Base->GetSamples()) {
    case 2:
      // sampletype = D3DMULTISAMPLE_2_SAMPLES;
      break;
    case 4:
      // sampletype = D3DMULTISAMPLE_4_SAMPLES;
      break;
    default:
      // sampletype = D3DMULTISAMPLE_NONE;
      break;
  }

  GL_ERRORCHECK();

  if (0 == FboObj) {
    mTargetGL.debugPushGroup("GlFrameBufferInterface::SetRtGroup::newFbo");
    FboObj = new GlFboObject;

    Base->SetInternalHandle(FboObj);

    GL_ERRORCHECK();
    glGenRenderbuffers(1, &FboObj->mDSBO);
    GL_ERRORCHECK();
    glGenFramebuffers(1, &FboObj->mFBOMaster);
    GL_ERRORCHECK();
    // printf( "GenFBO<%d>\n", int(FboObj->mFBOMaster) );

    //////////////////////////////////////////

    for (int it = 0; it < inumtargets; it++) {
      RtBuffer* pB = Base->GetMrt(it);
      pB->SetSizeDirty(true);
      //////////////////////////////////////////
      Texture* ptex            = new Texture;
      GLTextureObject* ptexOBJ = new GLTextureObject;
      GL_ERRORCHECK();
      glGenTextures(1, (GLuint*)&FboObj->mTEX[it]);
      glBindTexture(GL_TEXTURE_2D,FboObj->mTEX[it]);
      if( pB->_debugName.length() ){
        glLabelObjectEXT(GL_TEXTURE,FboObj->mTEX[it], pB->_debugName.length(), pB->_debugName.c_str() );
      }
      glBindTexture(GL_TEXTURE_2D,0);
      GL_ERRORCHECK();
      ptexOBJ->mObject = FboObj->mTEX[it];
      //////////////////////////////////////////
      ptex->_width          = iw;
      ptex->_height         = ih;
      ptex->_internalHandle = (void*)ptexOBJ;

      ptex->setProperty<GLuint>("gltexobj", FboObj->mTEX[it]);

      mTargetGL.TXI()->ApplySamplingMode(ptex);
      pB->SetTexture(ptex);
      //////////////////////////////////////////
      ork::lev2::GfxMaterialUITextured* pmtl = new ork::lev2::GfxMaterialUITextured(&mTarget);
      pmtl->SetTexture(ETEXDEST_DIFFUSE, ptex);
      pB->SetMaterial(pmtl);
      //////////////////////////////////////////
    }
    Base->_depthTexture = new Texture;
    Base->_depthTexture->_width          = iw;
    Base->_depthTexture->_height         = ih;
    GLTextureObject* depthtexobj = new GLTextureObject;
    Base->_depthTexture->_internalHandle = (void*) depthtexobj;
    GL_ERRORCHECK();




    Base->SetSizeDirty(true);
    mTargetGL.debugPopGroup();
  }
  GL_ERRORCHECK();

  if (Base->IsSizeDirty()) {
    mTargetGL.debugPushGroup("GlFrameBufferInterface::SetRtGroup::SizeDirty");
    //////////////////////////////////////////
    // initialize depth renderbuffer
    GL_ERRORCHECK();
    glBindRenderbuffer(GL_RENDERBUFFER, FboObj->mDSBO);
    GL_ERRORCHECK();
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT32, iw, ih);
    GL_ERRORCHECK();
    glGenTextures(1,&FboObj->_depthTexture);
    glBindTexture(GL_TEXTURE_2D, FboObj->_depthTexture);
    GL_ERRORCHECK();
    std::string DepthTexName("RtgDepth");
    glLabelObjectEXT(GL_TEXTURE,FboObj->_depthTexture, DepthTexName.length(), DepthTexName.c_str() );
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32, iw, ih, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
    //glTexParameteri(GL_TEXTURE_2D, GL_DEPTH_TEXTURE_MODE, GL_INTENSITY);
    //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_R_TO_TEXTURE);
    //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
    GL_ERRORCHECK();
    glBindTexture(GL_TEXTURE_2D, 0);
    GL_ERRORCHECK();
    glBindFramebuffer(GL_FRAMEBUFFER, FboObj->mFBOMaster);
    GL_ERRORCHECK();
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, FboObj->mDSBO);
    GL_ERRORCHECK();
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, FboObj->_depthTexture, 0);
    GL_ERRORCHECK();
    Base->_depthTexture->setProperty<GLuint>("gltexobj", FboObj->_depthTexture);
    mTargetGL.TXI()->ApplySamplingMode(Base->_depthTexture);
    Base->_depthTexture->_isDepthTexture = true;
    auto depthtexobj = (GLTextureObject*) Base->_depthTexture->_internalHandle;
    depthtexobj->mObject =FboObj->_depthTexture;
    //////
    for (int it = 0; it < inumtargets; it++) {
      RtBuffer* pB = Base->GetMrt(it);
      // D3DFORMAT efmt = D3DFMT_A8R8G8B8;
      GLuint glinternalformat = 0;
      GLuint glformat         = GL_RGBA;
      GLenum gltype           = 0;

      switch (pB->GetBufferFormat()) {
        case EBUFFMT_R32F:
          glformat = GL_RED;
          glinternalformat = GL_R32F;
          gltype           = GL_FLOAT;
          break;
        case EBUFFMT_RG16F:
          glformat = GL_RG;
          glinternalformat = GL_RG16F;
          gltype           = GL_HALF_FLOAT;
          break;
        case EBUFFMT_RGBA8:
          glinternalformat = GL_RGBA8;
          gltype           = GL_UNSIGNED_BYTE;
          break;
        case EBUFFMT_RGBA16F:
          glinternalformat = GL_RGBA16F;
          gltype           = GL_HALF_FLOAT;
          break;
        case EBUFFMT_RGBA32F:
          glinternalformat = GL_RGBA32F;
          gltype           = GL_FLOAT;
          break;
        case EBUFFMT_RGB10A2:
          glinternalformat = GL_RGB10_A2;
          gltype           = GL_UNSIGNED_INT_10_10_10_2;
          break;
        case EBUFFMT_RGB32UI:
          glformat = GL_RGB_INTEGER;
          glinternalformat = GL_RGB32UI;
          gltype           = GL_UNSIGNED_INT;
          break;
        default:
          OrkAssert(false);
          break;
          // case EBUFFMT_RGBA32F: glinternalformat = GL_RGBA32; break;
      }
      OrkAssert(glinternalformat != 0);

      //////////////////////////////////////////
      // initialize texture
      //////////////////////////////////////////

      glBindTexture(GL_TEXTURE_2D, FboObj->mTEX[it]);
      GL_ERRORCHECK();
      glTexImage2D(GL_TEXTURE_2D, 0, glinternalformat, iw, ih, 0, glformat, gltype, NULL);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
      GL_ERRORCHECK();

      // printf( "SetRtg::gentex<%d> w<%d> h<%d>\n", int(FboObj->mTEX[it]), iw,ih );

      //////////////////////////////////////////
      //////////////////////////////////////////
      // attach texture to framebuffercolor buffer

      glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + it, GL_TEXTURE_2D, FboObj->mTEX[it], 0);
      GL_ERRORCHECK();

      pB->SetSizeDirty(false);
      mTargetGL.debugPopGroup();
    }
    Base->SetSizeDirty(false);
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);

    OrkAssert(status == GL_FRAMEBUFFER_COMPLETE);
  }
  GL_ERRORCHECK();

  //////////////////////////////////////////////////
  // enable mrts
  //////////////////////////////////////////////////

  GLenum buffers[] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3};

  // printf( "SetRtg::BindFBO<%d> numattachments<%d>\n", int(FboObj->mFBOMaster), inumtargets );

  glBindFramebuffer(GL_FRAMEBUFFER, FboObj->mFBOMaster);
  glBindRenderbuffer(GL_RENDERBUFFER, FboObj->mDSBO);
  glDrawBuffers(inumtargets, buffers);
  GL_ERRORCHECK();

  //////////////////////////////////////////

  static const SRasterState defstate;
  mTarget.RSI()->BindRasterState(defstate, true);

  mCurrentRtGroup = Base;

  if (GetAutoClear()) {
    glClearColor(mcClearColor.GetX(), mcClearColor.GetY(), mcClearColor.GetZ(), mcClearColor.GetW());
    // glClearColor( 1.0f,1.0f,0.0f,1.0f );
    GL_ERRORCHECK();
    glClearDepth(1.0f);
    glDepthRange(0.0, 1.0f);
    GL_ERRORCHECK();
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    GL_ERRORCHECK();
  }
  mTargetGL.debugPopGroup();
}

////////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
