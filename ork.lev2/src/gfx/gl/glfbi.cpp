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

#define USE_OIIO

#if defined(USE_OIIO)

#include <OpenImageIO/imageio.h>

OIIO_NAMESPACE_USING
#endif

///////////////////////////////////////////////////////////////////////////////

namespace ork { namespace lev2 {

GlFrameBufferInterface::GlFrameBufferInterface(GfxTargetGL& target)
    : FrameBufferInterface(target)
    , mTargetGL(target) {}

GlFrameBufferInterface::~GlFrameBufferInterface() {}

///////////////////////////////////////////////////////////////////////////////

void GlFrameBufferInterface::_setAsRenderTarget(void) {
  mTargetGL.makeCurrentContext();
  // mTargetGL.debugPushGroup("GlFrameBufferInterface::_setAsRenderTarget");
  GL_ERRORCHECK();
  GL_ERRORCHECK();
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  GL_ERRORCHECK();
  glBindRenderbuffer(GL_RENDERBUFFER, 0);
  GL_ERRORCHECK();
  GL_ERRORCHECK();
  // mTargetGL.debugPopGroup();
}

///////////////////////////////////////////////////////////////////////////////

void GlFrameBufferInterface::DoBeginFrame(void) {

  mTargetGL.makeCurrentContext();

  // mTargetGL.debugPushGroup("GlFrameBufferInterface::DoBeginFrameA");
  // glFinish();
  GL_ERRORCHECK();

  if (mTargetGL.FBI()->GetRtGroup()) {
    glDepthRange(0.0, 1.0f);
    float fx = 0.0f; // mTargetGL.FBI()->GetRtGroup()->GetX();
    float fy = 0.0f; // mTargetGL.FBI()->GetRtGroup()->GetY();
    float fw = mTargetGL.FBI()->GetRtGroup()->GetW();
    float fh = mTargetGL.FBI()->GetRtGroup()->GetH();
    // printf( "RTGroup begin x<%f> y<%f> w<%f> h<%f>\n", fx, fy, fw, fh );
    SRect extents(fx, fy, fw, fh);
    // SRect extents( mTarget.GetX(), mTarget.GetY(), mTarget.GetW(), mTarget.GetH() );
    PushViewport(extents);
    PushScissor(extents);
    // printf( "BEGINFRAME<RtGroup>\n" );
    // mTargetGL.debugPopGroup();
  }
  ////////////////////////////////
  else if (IsOffscreenTarget())
  ////////////////////////////////
  {
    glDepthRange(0.0, 1.0f);
    SRect extents(mTarget.GetX(), mTarget.GetY(), mTarget.GetW(), mTarget.GetH());
    // printf( "OST begin x<%d> y<%d> w<%d> h<%d>\n", mTarget.GetX(), mTarget.GetY(), mTarget.GetW(), mTarget.GetH() );
    PushViewport(extents);
    PushScissor(extents);
    // mTargetGL.debugPopGroup();
  }
  /////////////////////////////////////////////////
  else // window (On Screen Target)
  /////////////////////////////////////////////////
  {
    GL_ERRORCHECK();
    _setAsRenderTarget();
    GL_ERRORCHECK();

    glDepthRange(0.0, 1.0f);
    SRect extents(mTarget.GetX(), mTarget.GetY(), mTarget.GetW(), mTarget.GetH());
    // printf( "WINtarg begin x<%d> y<%d> w<%d> h<%d>\n", mTarget.GetX(), mTarget.GetY(), mTarget.GetW(), mTarget.GetH() );
    PushViewport(extents);
    PushScissor(extents);
    // printf( "BEGINFRAME<WIN> w<%d> h<%d>\n", mTarget.GetW(), mTarget.GetH() );
    /////////////////////////////////////////////////

    if (GetAutoClear()) {
      fvec4 rCol = GetClearColor();
      // U32 ClearColorU = mTarget.fcolor4ToU32(GetClearColor());
      if (IsPickState())
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
      else
        glClearColor(rCol.GetX(), rCol.GetY(), rCol.GetZ(), 1.0f);

      // printf( "GlFrameBufferInterface::ClearViewport()\n" );
      GL_ERRORCHECK();
      glClearDepth(1.0f);
      GL_ERRORCHECK();
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
      GL_ERRORCHECK();
    }
    // mTargetGL.debugPopGroup();
  }

  /////////////////////////////////////////////////
  // Set Initial Rendering States
  GL_ERRORCHECK();

  // mTargetGL.debugPushGroup("GlFrameBufferInterface::DoBeginFrameB");

  const SRasterState defstate;
  mTarget.RSI()->BindRasterState(defstate, true);
  // mTargetGL.debugPopGroup();

  GL_ERRORCHECK();
}

///////////////////////////////////////////////////////////////////////////////

void GlFrameBufferInterface::DoEndFrame(void) {
  // mTargetGL.debugPushGroup("GlFrameBufferInterface::DoEndFrame");
  GL_ERRORCHECK();

  ///////////////////////////////////////////
  // release all resources for this frame
  ///////////////////////////////////////////

  // glFinish();

  ////////////////////////////////
  auto rtg = mTargetGL.FBI()->GetRtGroup();

  if (rtg) {
    GlFboObject* FboObj = (GlFboObject*)rtg->GetInternalHandle();
    int inumtargets     = rtg->GetNumTargets();

    for (int it = 0; it < inumtargets; it++) {
      auto b = rtg->GetMrt(it);

      if (b && b->mComputeMips) {
        auto tex_obj = FboObj->mTEX[it];

        printf("GENMIPS texobj<%d>\n", tex_obj);

        // glBindTexture( GL_TEXTURE_2D, tex_obj );
        // glGenerateMipmap( GL_TEXTURE_2D );
      }
    }

    // printf( "ENDFRAME<RtGroup>\n" );
  } else if (IsOffscreenTarget()) {
    // printf( "ENDFRAME<OST>\n" );
    GfxBuffer* pbuf            = GetThisBuffer();
    pbuf->GetTexture()->_dirty = false;
    pbuf->SetDirty(false);
    // mTargetGL.EndContextFBO();
  } else {
    glFinish();
    mTargetGL.SwapGLContext(mTargetGL.GetCtxBase());
  }
  ////////////////////////////////
  PopViewport();
  PopScissor();
  GL_ERRORCHECK();
  ////////////////////////////////
  glBindTexture(GL_TEXTURE_2D, 0);
  // mTargetGL.debugPopGroup();
}

///////////////////////////////////////////////////////////////////////////////

void GlFrameBufferInterface::_initializeContext(GfxBuffer* pBuf) {
  ///////////////////////////////////////////
  // create texture surface

  // D3DFORMAT efmt = D3DFMT_A8R8G8B8;
  int ibytesperpix = 0;

  bool Zonly = false;

  switch (pBuf->format()) {
    case EBUFFMT_RGBA8:
      // efmt = D3DFMT_A8R8G8B8;
      ibytesperpix = 4;
      break;
    case EBUFFMT_RGBA16F:
      // efmt = D3DFMT_A16B16G16R16F;
      ibytesperpix = 8;
      break;
    case EBUFFMT_RGBA32F:
      // efmt = D3DFMT_A32B32G32R32F;
      ibytesperpix = 16;
      break;
    case EBUFFMT_Z16:
      // efmt = D3DFMT_R16F;
      ibytesperpix = 2;
      Zonly        = true;
      break;
    case EBUFFMT_Z32:
      // efmt = D3DFMT_R32F;
      ibytesperpix = 2;
      Zonly        = true;
      break;
    default:
      OrkAssert(false);
      break;
  }

  ///////////////////////////////////////////
  // create orknum texture and link it

  Texture* ptexture = new Texture();
  ptexture->_width  = mTarget.GetW();
  ptexture->_height = mTarget.GetH();

  SetBufferTexture(ptexture);

  ///////////////////////////////////////////
  // create material

  pBuf->SetTexture(ptexture);

  ///////////////////////////////////////////
}

///////////////////////////////////////////////////////////////////////////////

void GlFrameBufferInterface::PushScissor(const SRect& rScissorRect) {
  SRect OldRect;
  OldRect.miX  = miCurScissorX;
  OldRect.miY  = miCurScissorY;
  OldRect.miX2 = miCurScissorX + miCurScissorW;
  OldRect.miY2 = miCurScissorY + miCurScissorH;
  OldRect.miW  = miCurScissorW;
  OldRect.miH  = miCurScissorH;

  maScissorStack[miScissorStackIndex] = OldRect;

  SetScissor(rScissorRect.miX, rScissorRect.miY, rScissorRect.miW, rScissorRect.miH);
  GL_ERRORCHECK();

  miScissorStackIndex++;
}

///////////////////////////////////////////////////////////////////////////////

SRect& GlFrameBufferInterface::PopScissor(void) {
  miScissorStackIndex--;

  SRect& OldRect = maScissorStack[miScissorStackIndex];
  int W          = OldRect.miX2 - OldRect.miX;
  int H          = OldRect.miY2 - OldRect.miY;

  SetScissor(OldRect.miX, OldRect.miY, W, H);
  GL_ERRORCHECK();

  return OldRect;
}

///////////////////////////////////////////////////////////////////////////////

void GlFrameBufferInterface::SetScissor(int iX, int iY, int iW, int iH) {
  iX = OldStlSchoolClampToRange(iX, 0, 8192);
  iY = OldStlSchoolClampToRange(iY, 0, 8192);
  iW = OldStlSchoolClampToRange(iW, 24, 8192);
  iH = OldStlSchoolClampToRange(iH, 24, 8192);

  // printf( "SetScissor<%d %d %d %d>\n", iX, iY, iW, iH );
  GL_ERRORCHECK();

  glScissor(iX, iY, iW, iH);

  GL_ERRORCHECK();
  glEnable(GL_SCISSOR_TEST);

  GL_ERRORCHECK();

  miCurScissorX = iX;
  miCurScissorY = iY;
  miCurScissorW = iW;
  miCurScissorH = iH;
}

///////////////////////////////////////////////////////////////////////////////

void GlFrameBufferInterface::SetViewport(int iX, int iY, int iW, int iH) {
  iX = OldStlSchoolClampToRange(iX, 0, 8192);
  iY = OldStlSchoolClampToRange(iY, 0, 8192);
  iW = OldStlSchoolClampToRange(iW, 32, 8192);
  iH = OldStlSchoolClampToRange(iH, 32, 8192);

  miCurVPX = iX;
  miCurVPY = iY;
  miCurVPW = iW;
  miCurVPH = iH;
  // printf( "SetViewport<%d %d %d %d>\n", iX, iY, iW, iH );

  auto framedata = mTargetGL.topRenderContextFrameData();
  bool stereo    = false;
  if (framedata) {
    const auto& CPD = framedata->topCPD();
    stereo          = CPD.isStereoOnePass();
  }

  GL_ERRORCHECK();

  if (stereo) {
    int wd2 = iW / 2;
    glViewportIndexedf(0, 0, 0, wd2, iH);
    glViewportIndexedf(1, wd2, 0, wd2, iH);
  } else {
    glViewport(iX, iY, iW, iH);
  }

  GL_ERRORCHECK();
}

///////////////////////////////////////////////////////////////////////////////

void GlFrameBufferInterface::Clear(const fcolor4& color, float fdepth) {
  if (IsPickState())
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
  else
    glClearColor(color.GetX(), color.GetY(), color.GetZ(), color.GetW());

  // printf( "GlFrameBufferInterface::ClearViewport()\n" );
  GL_ERRORCHECK();
  glClearDepth(fdepth);
  GL_ERRORCHECK();
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  GL_ERRORCHECK();
}

///////////////////////////////////////////////////////////////////////////////

void GlFrameBufferInterface::Capture(const RtGroup& rtg, int irt, const file::Path& pth) {
  auto FboObj = (GlFboObject*)rtg.GetInternalHandle();

  if ((nullptr == FboObj) || (irt >= rtg.GetNumTargets()))
    return;

  auto tex_id = FboObj->mTEX[irt];

  int iw        = rtg.GetW();
  int ih        = rtg.GetH();
  RtBuffer* rtb = rtg.GetMrt(irt);

  printf("pth<%s> BUFW<%d> BUF<%d>\n", pth.c_str(), iw, ih);

  uint8_t* pu8 = (uint8_t*)malloc(iw * ih * 4);

  GL_ERRORCHECK();
  // glFlush();
  GL_ERRORCHECK();
  glBindTexture(GL_TEXTURE_2D, tex_id);
  GL_ERRORCHECK();
  // glReadPixels( 0, 0, iw, ih, GL_RGBA, GL_UNSIGNED_BYTE, (void*) pu8 );
#if defined(DARWIN)
  glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, (void*)pu8);
#else
  glGetTexImage(GL_TEXTURE_2D, 0, GL_BGRA, GL_UNSIGNED_BYTE, (void*)pu8);
#endif

  GL_ERRORCHECK();
  glBindTexture(GL_TEXTURE_2D, 0);
  // glBindFramebuffer(GL_FRAMEBUFFER, 0 );
  GL_ERRORCHECK();

  for (int ipix = 0; ipix < (iw * ih); ipix++) {
    int ibyt   = ipix * 4;
    uint8_t c0 = pu8[ibyt + 0];
    uint8_t c1 = pu8[ibyt + 1];
    uint8_t c2 = pu8[ibyt + 2];
    uint8_t c3 = pu8[ibyt + 3];
    // c0 c1 c2

#if defined(DARWIN)
    pu8[ibyt + 0] = c0; // A
    pu8[ibyt + 1] = c1;
    pu8[ibyt + 2] = c2;
    pu8[ibyt + 3] = c3;
#else
    pu8[ibyt + 0] = c2;
    pu8[ibyt + 1] = c1;
    pu8[ibyt + 2] = c0;
    pu8[ibyt + 3] = c3; // A
#endif
  }

#if defined(USE_OIIO)
  auto out = ImageOutput::create(pth.c_str());
  if (!out)
    return;
  ImageSpec spec(iw, ih, 4, TypeDesc::UINT8);
  out->open(pth.c_str(), spec);
  out->write_image(TypeDesc::UINT8, pu8);
  out->close();

  free((void*)pu8);
#endif
}

///////////////////////////////////////////////////////////////////////////////

bool GlFrameBufferInterface::capture(const RtGroup& rtg, int irt, CaptureBuffer* capbuf) {

  GlFboObject* FboObj = (GlFboObject*)rtg.GetInternalHandle();

  if (nullptr == FboObj)
    return false;
  if (nullptr == capbuf)
    return false;
  if (0 == FboObj->mFBOMaster)
    return false;

  GL_ERRORCHECK();
  glBindFramebuffer(GL_FRAMEBUFFER, FboObj->mFBOMaster);
  GL_ERRORCHECK();

  OrkAssert(irt < rtg.GetNumTargets());
  auto rtb        = rtg.mMrt[irt];
  auto rtb_format = rtb->format();

  GLint readbuffer = 0;
  GL_ERRORCHECK();
  glGetIntegerv(GL_READ_BUFFER, &readbuffer);
  GL_ERRORCHECK();

  // glDepthMask(GL_TRUE); ??? wtf ???
  GL_ERRORCHECK();
  glReadBuffer(GL_COLOR_ATTACHMENT0 + irt);
  GL_ERRORCHECK();

  bool fmtmatch = (capbuf->format() == rtb_format);
  bool sizmatch = (capbuf->width() == rtb->miW);
  sizmatch &= (capbuf->height() == rtb->miH);
  int w = rtb->miW;
  int h = rtb->miH;

  if (not(fmtmatch and sizmatch))
    capbuf->setFormatAndSize(rtb_format, w, h);

  //glPixelStore()

  GL_ERRORCHECK();
  switch (rtb_format) {
    case EBUFFMT_RGBA8:
      glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, capbuf->_data);
      break;
    case EBUFFMT_RGBA16F:
      glReadPixels(0, 0, w, h, GL_RGBA, GL_HALF_FLOAT, capbuf->_data);
      break;
    case EBUFFMT_RGBA32F:
      glReadPixels(0, 0, w, h, GL_RGBA, GL_FLOAT, capbuf->_data);
      break;
    case EBUFFMT_R32F:
      glReadPixels(0, 0, w, h, GL_RED, GL_FLOAT, capbuf->_data);
      break;
    case EBUFFMT_RG32F:
      glReadPixels(0, 0, w, h, GL_RG, GL_FLOAT, capbuf->_data);
      break;
    default:
      assert(false);
      break;
  }
  GL_ERRORCHECK();

  glBindFramebuffer(GL_FRAMEBUFFER, 0);
//  glReadBuffer( readbuffer ); // restore read buffer
  GL_ERRORCHECK();
  return true;
}

///////////////////////////////////////////////////////////////////////////////

void GlFrameBufferInterface::GetPixel(const fvec4& rAt, PixelFetchContext& ctx) {
  fcolor4 Color(0.0f, 0.0f, 0.0f, 0.0f);

  int sx = int((rAt.GetX()) * float(mTarget.GetW()));
  int sy = int((1.0f - rAt.GetY()) * float(mTarget.GetH()));

  bool bInBounds = ((sx < mTarget.GetW()) && (sy < mTarget.GetH()) && (sx > 0) && (sy > 0));

  if (IsOffscreenTarget() && bInBounds) {
    if (ctx.mRtGroup) {
      GlFboObject* FboObj = (GlFboObject*)ctx.mRtGroup->GetInternalHandle();

      if (FboObj) {
        GL_ERRORCHECK();
        glBindFramebuffer(GL_FRAMEBUFFER, FboObj->mFBOMaster);
        GL_ERRORCHECK();

        if (FboObj->mFBOMaster) {

          int MrtMask = ctx.miMrtMask;

          GLint readbuffer = 0;
          GL_ERRORCHECK();
          glGetIntegerv(GL_READ_BUFFER, &readbuffer);
          GL_ERRORCHECK();

          // printf( "readbuf<%d>\n", int(readbuffer) );

          for (int MrtIndex = 0; MrtIndex < 4; MrtIndex++) {
            int MrtTest = 1 << MrtIndex;

            ctx.mPickColors[MrtIndex] = fcolor4(0.0f, 0.0f, 0.0f, 0.0f);

            if (MrtTest & MrtMask) {

              OrkAssert(MrtIndex < ctx.mRtGroup->GetNumTargets());

              GL_ERRORCHECK();
              glDepthMask(GL_TRUE);
              GL_ERRORCHECK();
              GL_ERRORCHECK();
              glReadBuffer(GL_COLOR_ATTACHMENT0 + MrtIndex);
              GL_ERRORCHECK();

              float rgba[4] = {0.0f, 0.0f, 0.0f, 0.0f};

              glReadPixels(sx, sy, 1, 1, GL_RGBA, GL_FLOAT, (void*)rgba);
              GL_ERRORCHECK();
              fvec4 rv                  = fvec4(rgba[0], rgba[1], rgba[2], rgba[3]);
              ctx.mPickColors[MrtIndex] = rv;

              printf("getpix MrtIndex<%d> rx<%d> ry<%d> <%g %g %g %g>\n", MrtIndex, sx, sy, rv.x, rv.y, rv.z, rv.w);
            }
          }
          GL_ERRORCHECK();
          glBindFramebuffer(GL_FRAMEBUFFER, 0);
          // glReadBuffer( readbuffer );
          GL_ERRORCHECK();
        } else {
          printf("!!!ERR - GetPix BindFBO<%d>\n", FboObj->mFBOMaster);
        }
      }
    } else {
    }
  } else if (bInBounds) {
    ctx.mPickColors[0] = Color;
  }
}

///////////////////////////////////////////////////////////////////////////////

GlFboObject::GlFboObject() {
  for (int i = 0; i < kmaxrt; i++) {
    mTEX[i] = 0;
  }
  mDSBO      = 0;
  mFBOMaster = 0;
}

///////////////////////////////////////////////////////////////////////////////

}} // namespace ork::lev2
