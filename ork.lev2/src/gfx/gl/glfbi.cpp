////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
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
extern int G_MSAASAMPLES;

GlFrameBufferInterface::GlFrameBufferInterface(ContextGL& target)
    : FrameBufferInterface(target)
    , mTargetGL(target) {
}

GlFrameBufferInterface::~GlFrameBufferInterface() {
}

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

void GlFrameBufferInterface::_doBeginFrame(void) {

  mTargetGL.makeCurrentContext();

  // mTargetGL.debugPushGroup("GlFrameBufferInterface::_doBeginFrameA");
  // glFinish();
  GL_ERRORCHECK();

  RtGroup* rtg = mTargetGL.FBI()->GetRtGroup();

  if (mTargetGL._defaultRTG and (rtg != nullptr)) {
    SetRtGroup(mTargetGL._defaultRTG);
    rtGroupClear(mTargetGL._defaultRTG);
    rtg = mTargetGL._defaultRTG;
  }

  if (rtg) {
    glDepthRange(0.0, 1.0f);
    float fx = 0.0f; // mTargetGL.FBI()->GetRtGroup()->GetX();
    float fy = 0.0f; // mTargetGL.FBI()->GetRtGroup()->GetY();
    float fw = GetRtGroup()->GetW();
    float fh = GetRtGroup()->GetH();
    // printf("RTGroup begin x<%f> y<%f> w<%f> h<%f>\n", fx, fy, fw, fh);
    ViewportRect extents(fx, fy, fw, fh);
    // SRect extents( mTarget.GetX(), mTarget.GetY(), mTarget.GetW(), mTarget.GetH() );
    pushViewport(extents);
    pushScissor(extents);
    // printf("BEGINFRAME<RtGroup>\n");
    // mTargetGL.debugPopGroup();
    if (rtg->_autoclear) {
      rtGroupClear(rtg);
    }
  }
  /////////////////////////////////////////////////
  else // (Main Target)
  /////////////////////////////////////////////////
  {
    GL_ERRORCHECK();
    _setAsRenderTarget();
    GL_ERRORCHECK();

    if (G_MSAASAMPLES > 1)
      glEnable(GL_MULTISAMPLE);
    else
      glDisable(GL_MULTISAMPLE);

    glDepthRange(0.0, 1.0f);
    ViewportRect extents = mTarget.mainSurfaceRectAtOrigin();
    // printf( "WINtarg begin x<%d> y<%d> w<%d> h<%d>\n", mTarget.GetX(), mTarget.GetY(), mTarget.GetW(), mTarget.GetH() );
    pushViewport(extents);
    pushScissor(extents);
    // printf("BEGINFRAME<WIN> w<%d> h<%d>\n", extents.miW, extents.miH);
    /////////////////////////////////////////////////

    if (GetAutoClear()) {
      fvec4 rCol = GetClearColor();
      // U32 ClearColorU = mTarget.fcolor4ToU32(GetClearColor());
      if (isPickState())
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
      else
        glClearColor(rCol.x, rCol.y, rCol.z, rCol.w);

      // printf("GlFrameBufferInterface::ClearViewport()\n");
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

  // mTargetGL.debugPushGroup("GlFrameBufferInterface::_doBeginFrameB");

  const SRasterState defstate;
  mTarget.RSI()->BindRasterState(defstate, true);
  // mTargetGL.debugPopGroup();

  GL_ERRORCHECK();
}

///////////////////////////////////////////////////////////////////////////////

void GlFrameBufferInterface::_doEndFrame(void) {
  // mTargetGL.debugPushGroup("GlFrameBufferInterface::_doEndFrame");
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
    // printf( "ENDFRAME<RtGroup>\n" );
  } else {
    glFinish();
    mTargetGL.SwapGLContext(mTargetGL.GetCtxBase());
  }
  ////////////////////////////////
  popViewport();
  popScissor();
  GL_ERRORCHECK();
  ////////////////////////////////
  glBindTexture(GL_TEXTURE_2D, 0);
  // mTargetGL.debugPopGroup();
}

///////////////////////////////////////////////////////////////////////////////

void GlFrameBufferInterface::_initializeContext(OffscreenBuffer* pBuf) {
  ///////////////////////////////////////////
  // create texture surface

  // D3DFORMAT efmt = D3DFMT_A8R8G8B8;
  int ibytesperpix = 0;

  bool Zonly = false;

  switch (pBuf->format()) {
    case EBufferFormat::RGBA8:
      // efmt = D3DFMT_A8R8G8B8;
      ibytesperpix = 4;
      break;
    case EBufferFormat::RGBA16F:
      // efmt = D3DFMT_A16B16G16R16F;
      ibytesperpix = 8;
      break;
    case EBufferFormat::RGBA32F:
      // efmt = D3DFMT_A32B32G32R32F;
      ibytesperpix = 16;
      break;
    case EBufferFormat::Z16:
      // efmt = D3DFMT_R16F;
      ibytesperpix = 2;
      Zonly        = true;
      break;
    case EBufferFormat::Z32:
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
  ptexture->_width  = mTarget.mainSurfaceWidth();
  ptexture->_height = mTarget.mainSurfaceHeight();

  SetBufferTexture(ptexture);

  ///////////////////////////////////////////
  // create material

  pBuf->SetTexture(ptexture);

  ///////////////////////////////////////////
}

///////////////////////////////////////////////////////////////////////////////

void GlFrameBufferInterface::_setScissor(int iX, int iY, int iW, int iH) {
  iX = OldStlSchoolClampToRange(iX, 0, 16384);
  iY = OldStlSchoolClampToRange(iY, 0, 16384);
  iW = OldStlSchoolClampToRange(iW, 24, 16384);
  iH = OldStlSchoolClampToRange(iH, 24, 16384);

  // printf("setScissor<%d %d %d %d>\n", iX, iY, iW, iH);
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

void GlFrameBufferInterface::_setViewport(int iX, int iY, int iW, int iH) {
  iX = OldStlSchoolClampToRange(iX, 0, 16384);
  iY = OldStlSchoolClampToRange(iY, 0, 16384);
  iW = OldStlSchoolClampToRange(iW, 32, 16384);
  iH = OldStlSchoolClampToRange(iH, 32, 16384);

  // printf("setViewport<%d %d %d %d>\n", iX, iY, iW, iH);

  auto framedata = mTargetGL.topRenderContextFrameData();
  bool stereo    = (framedata and framedata->isStereo());

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
  if (isPickState())
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
  else
    glClearColor(color.GetX(), color.GetY(), color.GetZ(), color.GetW());

  // printf("GlFrameBufferInterface::ClearViewport() color<%g %g %g %g>\n", color.x, color.y, color.z, color.w);
  GL_ERRORCHECK();
  glClearDepth(fdepth);
  GL_ERRORCHECK();
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  GL_ERRORCHECK();
}

///////////////////////////////////////////////////////////////////////////////

void GlFrameBufferInterface::clearDepth(float fdepth) {
  glDepthRange(0.0, 1.0f);
  glClearDepth(fdepth);
  GL_ERRORCHECK();
  glClear(GL_DEPTH_BUFFER_BIT);
  GL_ERRORCHECK();
}

///////////////////////////////////////////////////////////////////////////////

void GlFrameBufferInterface::Capture(const RtGroup& rtg, int irt, const file::Path& pth) {
  auto FboObj = (GlFboObject*)rtg.GetInternalHandle();

  if ((nullptr == FboObj) || (irt >= rtg.GetNumTargets()))
    return;

  auto buffer = rtg.GetMrt(irt);
  if (buffer->_impl.IsSet() == false)
    return;

  auto tex_id = buffer->_impl.Get<GlRtBufferImpl*>()->_texture;

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

bool GlFrameBufferInterface::captureAsFormat(const RtGroup& rtg, int irt, CaptureBuffer* capbuf, EBufferFormat destfmt) {

  if (nullptr == capbuf) {
    OrkAssert(false);
    return false;
  }

  OrkAssert(irt < rtg.GetNumTargets());
  auto rtb    = rtg.mMrt[irt];
  auto FboObj = (GlFboObject*)rtg.GetInternalHandle();

  /*if (0 == FboObj) {
    FboObj = (GlFboObject*)mTargetGL._defaultRTG->GetInternalHandle();
    OrkAssert(irt < mTargetGL._defaultRTG->GetNumTargets());
    rtb = mTargetGL._defaultRTG->mMrt[irt];
  }*/

  GLint readbuffer = 0;
  GL_ERRORCHECK();
  glGetIntegerv(GL_READ_BUFFER, &readbuffer);
  GL_ERRORCHECK();

  if (FboObj) {

    if (0 == FboObj->mFBOMaster) {
      OrkAssert(false);
      return false;
    }

    GL_ERRORCHECK();
    glBindFramebuffer(GL_FRAMEBUFFER, FboObj->mFBOMaster);
    GL_ERRORCHECK();
    glReadBuffer(GL_COLOR_ATTACHMENT0 + irt);
    GL_ERRORCHECK();

  } else {
    OrkAssert(false);
    GL_ERRORCHECK();
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    GL_ERRORCHECK();
    glReadBuffer(GL_BACK);
    GL_ERRORCHECK();
  }

  // glDepthMask(GL_TRUE); ??? wtf ???

  bool fmtmatch = (capbuf->format() == destfmt);
  bool sizmatch = (capbuf->width() == rtb->miW);
  sizmatch &= (capbuf->height() == rtb->miH);
  int w = rtb->miW;
  int h = rtb->miH;

  // printf("captureAsFormat w<%d> h<%d>\n", w, h);

  if (not(fmtmatch and sizmatch))
    capbuf->setFormatAndSize(destfmt, w, h);

  // glPixelStore()

  GL_ERRORCHECK();
  static size_t yo       = 0;
  constexpr float inv256 = 1.0f / 255.0f;
  switch (destfmt) {
    case EBufferFormat::NV12: {
      size_t rgbasize = w * h * 4;
      if (capbuf->_tempbuffer.size() != rgbasize) {
        capbuf->_tempbuffer.resize(rgbasize);
      }
      glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, capbuf->_tempbuffer.data());
      GL_ERRORCHECK();
      // todo convert RGBA8 to NV12 (on GPU)
      auto outptr      = (uint8_t*)capbuf->_data;
      size_t numpixels = w * h;
      fvec3 avgcol;
      for (size_t yin = 0; yin < h; yin++) {
        int yout = (h - 1) - yin;
        for (size_t x = 0; x < w; x++) {
          int i_in       = (yin * w) + x;
          int i_out      = (yout * w) + x;
          size_t srcbase = i_in * 4;
          int R          = capbuf->_tempbuffer[srcbase + 0];
          int G          = capbuf->_tempbuffer[srcbase + 1];
          int B          = capbuf->_tempbuffer[srcbase + 2];
          // printf("RGB<%d %d %d>\n", R, G, B);
          auto rgb = fvec3(R, G, B) * inv256;
          avgcol += rgb;
          auto yuv      = rgb.getYUV();
          outptr[i_out] = uint8_t(yuv.x * 255.0f);
        }
      }
      avgcol *= 1.0f / float(numpixels);
      for (size_t yin = 0; yin < h / 2; yin++) {
        int yout = ((h / 2) - 1) - yin;
        for (size_t x = 0; x < w / 2; x++) {
          size_t ybase    = yin * 2;
          size_t xbase    = x * 2;
          size_t srcbase1 = (((ybase + 0) * w) + (xbase + 0)) * 4;
          size_t srcbase2 = (((ybase + 0) * w) + (xbase + 1)) * 4;
          size_t srcbase3 = (((ybase + 1) * w) + (xbase + 0)) * 4;
          size_t srcbase4 = (((ybase + 1) * w) + (xbase + 1)) * 4;
          int R1          = capbuf->_tempbuffer[srcbase1 + 0];
          int G1          = capbuf->_tempbuffer[srcbase1 + 1];
          int B1          = capbuf->_tempbuffer[srcbase1 + 2];
          int R2          = capbuf->_tempbuffer[srcbase2 + 0];
          int G2          = capbuf->_tempbuffer[srcbase2 + 1];
          int B2          = capbuf->_tempbuffer[srcbase2 + 2];
          int R3          = capbuf->_tempbuffer[srcbase3 + 0];
          int G3          = capbuf->_tempbuffer[srcbase3 + 1];
          int B3          = capbuf->_tempbuffer[srcbase3 + 2];
          int R4          = capbuf->_tempbuffer[srcbase4 + 0];
          int G4          = capbuf->_tempbuffer[srcbase4 + 1];
          int B4          = capbuf->_tempbuffer[srcbase4 + 2];
          auto rgb1       = fvec3(R1, G1, B1) * inv256;
          auto rgb2       = fvec3(R2, G2, B2) * inv256;
          auto rgb3       = fvec3(R3, G3, B3) * inv256;
          auto rgb4       = fvec3(R4, G4, B4) * inv256;
          auto yuv1       = rgb1.getYUV();
          auto yuv2       = rgb2.getYUV();
          auto yuv3       = rgb3.getYUV();
          auto yuv4       = rgb4.getYUV();
          auto yuv        = (yuv1 + yuv2 + yuv3 + yuv4) * 0.125;
          yuv += fvec3(0.5, 0.5, 0.5);
          int u                            = int(yuv.y * 255.0f);
          int v                            = int(yuv.z * 255.0f);
          int outindex                     = (yout * (w / 2) + x) * 2;
          outptr[numpixels + outindex + 0] = u;
          outptr[numpixels + outindex + 1] = v;
        }
      }
      break;
    }
    case EBufferFormat::RGBA8: {
      glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, capbuf->_data);
      break;
    }
    case EBufferFormat::RGBA16F:
      glReadPixels(0, 0, w, h, GL_RGBA, GL_HALF_FLOAT, capbuf->_data);
      break;
    case EBufferFormat::RGBA32F:
      glReadPixels(0, 0, w, h, GL_RGBA, GL_FLOAT, capbuf->_data);
      break;
    case EBufferFormat::R32F:
      glReadPixels(0, 0, w, h, GL_RED, GL_FLOAT, capbuf->_data);
      break;
    case EBufferFormat::R32UI:
      glReadPixels(0, 0, w, h, GL_RED_INTEGER, GL_UNSIGNED_INT, capbuf->_data);
      break;
    case EBufferFormat::RG32F:
      glReadPixels(0, 0, w, h, GL_RG, GL_FLOAT, capbuf->_data);
      break;
    default:
      OrkAssert(false);
      break;
  }
  GL_ERRORCHECK();

  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  //  glReadBuffer( readbuffer ); // restore read buffer
  GL_ERRORCHECK();
  return true;
}

///////////////////////////////////////////////////////////////////////////////

bool GlFrameBufferInterface::capture(const RtGroup& rtg, int irt, CaptureBuffer* capbuf) {
  OrkAssert(irt < rtg.GetNumTargets());
  auto rtb        = rtg.mMrt[irt];
  auto rtb_format = rtb->format();
  return captureAsFormat(rtg, irt, capbuf, rtb_format);
}

///////////////////////////////////////////////////////////////////////////////

void GlFrameBufferInterface::GetPixel(const fvec4& rAt, PixelFetchContext& ctx) {

  mTarget.makeCurrentContext();
  fcolor4 Color(0.0f, 0.0f, 0.0f, 0.0f);

  mTarget.debugPushGroup("GetPixel");

  bool bInBounds = (rAt.x >= 0.0f and rAt.x < 1.0f and rAt.y >= 0.0f and rAt.y < 1.0f);

  // printf("GetPixel rtg<%p> numbuf<%d>\n", ctx.mRtGroup, ctx.mRtGroup->mNumMrts );

  if (bInBounds) {
    if (ctx.mRtGroup) {
      int W  = ctx.mRtGroup->GetW();
      int H  = ctx.mRtGroup->GetH();
      int sx = int((rAt.GetX()) * float(W));
      int sy = int((1.0f - rAt.GetY()) * float(H));

      GlFboObject* FboObj = (GlFboObject*)ctx.mRtGroup->GetInternalHandle();

      // printf("GetPixel<%d %d> FboObj<%p>\n", sx, sy, FboObj);

      if (FboObj) {
        GL_ERRORCHECK();
        glBindFramebuffer(GL_FRAMEBUFFER, FboObj->mFBOMaster);
        GL_ERRORCHECK();

        // printf("GetPixel<%d %d> FboMaster<%u>\n", sx, sy, FboObj->mFBOMaster);

        if (FboObj->mFBOMaster) {

          int MrtMask = ctx.miMrtMask;

          GLint readbuffer = 0;
          GL_ERRORCHECK();
          glGetIntegerv(GL_READ_BUFFER, &readbuffer);
          GL_ERRORCHECK();

          // printf( "readbuf<%d>\n", int(readbuffer) );

          for (int MrtIndex = 0; MrtIndex < 4; MrtIndex++) {
            int MrtTest = 1 << MrtIndex;

            ctx._pickvalues[MrtIndex] = fcolor4(0.0f, 0.0f, 0.0f, 0.0f);

            if (MrtTest & MrtMask) {

              auto rtbuffer = ctx.mRtGroup->GetMrt(MrtIndex);

              OrkAssert(MrtIndex < ctx.mRtGroup->GetNumTargets());

              GL_ERRORCHECK();
              glDepthMask(GL_TRUE);
              GL_ERRORCHECK();
              GL_ERRORCHECK();
              glReadBuffer(GL_COLOR_ATTACHMENT0 + MrtIndex);
              GL_ERRORCHECK();

              float rgba[4] = {0.0f, 0.0f, 0.0f, 0.0f};
              switch (ctx.mUsage[MrtIndex]) {
                case PixelFetchContext::EPU_PTR64: {
                  uint64_t value = 0xffffffffffffffff;
                  OrkAssert(rtbuffer->mFormat == EBufferFormat::RGBA16UI);
                  glReadPixels(sx, sy, 1, 1, GL_RGBA_INTEGER, GL_UNSIGNED_SHORT, (void*)&value);
                  /////////////////////////////////////////////////////////////////
                  // swizzle so hex appears as xxxxyyyyzzzzwwww
                  /////////////////////////////////////////////////////////////////
                  // uint64_t a, b, c, d;
                  // a             = (value >> 48) & 0xffff;
                  // b             = (value >> 32) & 0xffff;
                  // c             = (value >> 16) & 0xffff;
                  // d             = (value >> 0) & 0xffff;
                  // uint64_t rval = (d << 48) | (c << 32) | (b << 16) | a;
                  /////////////////////////////////////////////////////////////////
                  ctx._pickvalues[MrtIndex].Set<uint64_t>(value);
                  /////////////////////////////////////////////////////////////////
                  break;
                }
                case PixelFetchContext::EPU_FLOAT: {
                  fvec4 rv;
                  glReadPixels(sx, sy, 1, 1, GL_RGBA, GL_FLOAT, (void*)rv.GetArray());
                  ctx._pickvalues[MrtIndex].Set<fvec4>(rv);
                  break;
                }
                default:
                  OrkAssert(false);
                  break;
              }
              GL_ERRORCHECK();

              // printf("getpix MrtIndex<%d> rx<%d> ry<%d> <%g %g %g %g>\n", MrtIndex, sx, sy, rv.x, rv.y, rv.z, rv.w);
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
    ctx._pickvalues[0] = Color;
  }
  mTarget.debugPopGroup();
}

///////////////////////////////////////////////////////////////////////////////

GlFboObject::GlFboObject() {
  mDSBO      = 0;
  mFBOMaster = 0;
}

///////////////////////////////////////////////////////////////////////////////
}} // namespace ork::lev2
