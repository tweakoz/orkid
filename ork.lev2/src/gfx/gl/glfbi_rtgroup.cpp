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
    Base->SetSizeDirty(true);
  }
  GL_ERRORCHECK();

  if (Base->IsSizeDirty()) {
    //////////////////////////////////////////
    // initialize depth renderbuffer
    GL_ERRORCHECK();
    glBindRenderbuffer(GL_RENDERBUFFER, FboObj->mDSBO);
    GL_ERRORCHECK();
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, iw, ih);
    GL_ERRORCHECK();

    // glRenderbufferStorageMultisampleEXT( GL_RENDERBUFFER_EXT, 1, GL_DEPTH_COMPONENT24, iw, ih );

    //////
    // attach it to the FBO
    //////
    glBindFramebuffer(GL_FRAMEBUFFER, FboObj->mFBOMaster);
    GL_ERRORCHECK();
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, FboObj->mDSBO);
    GL_ERRORCHECK();

    //////
    for (int it = 0; it < inumtargets; it++) {
      RtBuffer* pB = Base->GetMrt(it);
      // D3DFORMAT efmt = D3DFMT_A8R8G8B8;
      GLuint glinternalformat = 0;
      GLuint glformat         = GL_RGBA;
      GLenum gltype           = 0;

      switch (pB->GetBufferFormat()) {
        case EBUFFMT_RGBA32:
          glinternalformat = GL_RGBA8;
          gltype           = GL_UNSIGNED_BYTE;
          break;
        case EBUFFMT_RGBA64:
          glinternalformat = GL_RGBA16F;
          gltype           = GL_HALF_FLOAT;
          break;
        case EBUFFMT_RGBA128:
          glinternalformat = GL_RGBA32F;
          gltype           = GL_FLOAT;
          break;
        default:
          OrkAssert(false);
          break;
          // case EBUFFMT_RGBA128: glinternalformat = GL_RGBA32; break;
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
}

////////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
