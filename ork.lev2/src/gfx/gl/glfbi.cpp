////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "gl.h"
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/gfxmaterial_ui.h>
#include <ork/lev2/gfx/material_freestyle.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/lev2/ui/viewport.h>
#include <ork/lev2/gfx/pickbuffer.h>
#include <ork/pch.h>

#include <ork/lev2/gfx/dbgfontman.h>
#include <ork/util/logger.h>

#define USE_OIIO

#if defined(USE_OIIO)

#include <OpenImageIO/imageio.h>

OIIO_NAMESPACE_USING
#endif

///////////////////////////////////////////////////////////////////////////////

namespace ork { namespace lev2 {

static logchannel_ptr_t logchan_glfbi = logger()->createChannel("GLFBI", fvec3(0.8, 0.2, 0.5), true);

extern int G_MSAASAMPLES;

GlFrameBufferInterface::GlFrameBufferInterface(ContextGL& target)
    : FrameBufferInterface(target)
    , mTargetGL(target) {
}

GlFrameBufferInterface::~GlFrameBufferInterface() {
}

freestyle_mtl_ptr_t GlFrameBufferInterface::utilshader() {

  if (nullptr == _freestyle_mtl) {
    _freestyle_mtl = std::make_shared<FreestyleMaterial>();
    _freestyle_mtl->gpuInit(&_target, "orkshader://solid");
    _tek_downsample2x2 = _freestyle_mtl->technique("downsample_2x2");
    _tek_blit          = _freestyle_mtl->technique("blit");
    _fxpMVP            = _freestyle_mtl->param("MatMVP");
    _fxpColorMap       = _freestyle_mtl->param("ColorMap");
  }
  return _freestyle_mtl;
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
    float fx = 0.0f; // mTargetGL.FBI()->GetRtGroup()->x;
    float fy = 0.0f; // mTargetGL.FBI()->GetRtGroup()->y;
    float fw = GetRtGroup()->width();
    float fh = GetRtGroup()->height();
    // printf("RTGroup begin x<%f> y<%f> w<%f> h<%f>\n", fx, fy, fw, fh);
    ViewportRect extents(fx, fy, fw, fh);
    // SRect extents( _target.x, _target.y, _target.width(), _target.height() );
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
    ViewportRect extents = _target.mainSurfaceRectAtOrigin();
    // printf( "WINtarg begin x<%d> y<%d> w<%d> h<%d>\n", _target.x, _target.y, _target.width(), _target.height() );
    pushViewport(extents);
    pushScissor(extents);
    // printf("BEGINFRAME<WIN> w<%d> h<%d>\n", extents.miW, extents.miH);
    /////////////////////////////////////////////////

    if (GetAutoClear()) {
      fvec4 rCol = GetClearColor();
      // U32 ClearColorU = _target.fcolor4ToU32(GetClearColor());
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
  _target.RSI()->BindRasterState(defstate, true);
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
    int inumtargets = rtg->GetNumTargets();
    // printf( "ENDFRAME<RtGroup>\n" );
  } else {
    // glFinish();
    // mTargetGL.SwapGLContext(mTargetGL.GetCtxBase());
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

void GlFrameBufferInterface::_initializeContext(DisplayBuffer* pBuf) {
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
  ptexture->_width  = _target.mainSurfaceWidth();
  ptexture->_height = _target.mainSurfaceHeight();

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
  iW = OldStlSchoolClampToRange(iW, 1, 16384);
  iH = OldStlSchoolClampToRange(iH, 1, 16384);

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
  iW = OldStlSchoolClampToRange(iW, 1, 16384);
  iH = OldStlSchoolClampToRange(iH, 1, 16384);

  auto framedata = mTargetGL.topRenderContextFrameData();
  bool stereo    = (framedata and framedata->isStereo());

  // printf("setViewport<%d %d %d %d> stereo<%d>\n", iX, iY, iW, iH, int(stereo));

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
  glClearColor(color.x, color.y, color.z, color.w);

  /*GLuint clearColor[4] = {
    GLuint(color.x * 65535.0f),
    GLuint(color.y * 65535.0f),
    GLuint(color.z * 65535.0f),
    GLuint(color.w * 65535.0f)
  };
  glClearBufferuiv(GL_COLOR, 0, clearColor);*/

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

void GlFrameBufferInterface::clearRectWithColor(
    ViewportRect rect,     //
    const fcolor4& C) { //

  int h   = GetVPH();
  int y1 = rect._y;
  int y2 = y1 + rect._h;
  rect._y = h - y2;
  //printf( "y<%d> h<%d> TH<%d>\n", rect._y, rect._h, h );
  this->pushScissor(rect);
  this->pushViewport(rect);
  glClearColor(C.x,C.y,C.z,C.w);
  glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
  this->popViewport();
  this->popScissor();
}

///////////////////////////////////////////////////////////////////////////////

void GlFrameBufferInterface::capture(const RtBuffer* rtb, const file::Path& pth) {

  if (not rtb->_impl.isSet())
    return;

  auto bufimpl = rtb->_impl.get<GlRtBufferImpl*>();
  auto teximpl = bufimpl->_teximpl.get<gltexobj_ptr_t>();
  auto tex_id  = teximpl->_textureObject;

  int iw = rtb->_width;
  int ih = rtb->_height;

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

  auto outbuf = (uint8_t*)malloc(iw * ih * 4);

  for (int iy = 0; iy < ih; iy++) {
    for (int ix = 0; ix < iw; ix++) {
      int ipix = (ih - 1 - iy) * iw + ix;
      int opix = (iy)*iw + ix;

      int ibyt = ipix * 4;
      int obyt = opix * 4;

      uint8_t c0 = pu8[ibyt + 0];
      uint8_t c1 = pu8[ibyt + 1];
      uint8_t c2 = pu8[ibyt + 2];
      uint8_t c3 = pu8[ibyt + 3];
      // c0 c1 c2

#if defined(DARWIN)
      outbuf[obyt + 0] = c0; // A
      outbuf[obyt + 1] = c1;
      outbuf[obyt + 2] = c2;
      outbuf[obyt + 3] = c3;
#else
      outbuf[obyt + 0] = c2;
      outbuf[obyt + 1] = c1;
      outbuf[obyt + 2] = c0;
      outbuf[obyt + 3] = c3; // A
#endif
    }
  }

#if defined(USE_OIIO)
  auto out = ImageOutput::create(pth.c_str());
  if (!out)
    return;
  ImageSpec spec(iw, ih, 4, TypeDesc::UINT8);
  out->open(pth.c_str(), spec);
  out->write_image(TypeDesc::UINT8, outbuf);
  out->close();

  free((void*)pu8);
  free((void*)outbuf);
#endif
}

///////////////////////////////////////////////////////////////////////////////

bool GlFrameBufferInterface::captureAsFormat(const RtBuffer* rtb, CaptureBuffer* capbuf, EBufferFormat destfmt) {

  if (nullptr == capbuf) {
    OrkAssert(false);
    return false;
  }

  GlFboObject* the_fbo = nullptr;

  int irt = 0;
  int x   = 0;
  int y   = 0;
  int w   = 0;
  int h   = 0;

  GLint readbuffer = 0;
  GL_ERRORCHECK();
  glGetIntegerv(GL_READ_BUFFER, &readbuffer);
  GL_ERRORCHECK();

  if (rtb) {
    auto as_impl = rtb->_rtgroup->_impl.tryAs<glrtgroupimpl_ptr_t>();
    if (as_impl) {
      the_fbo = as_impl.value()->_standard.get();
      irt     = rtb->_slot;
      w       = rtb->_width;
      h       = rtb->_height;
      if (0 == the_fbo->_fbo) {
        OrkAssert(false);
        return false;
      }

      GL_ERRORCHECK();
      glBindFramebuffer(GL_FRAMEBUFFER, the_fbo->_fbo);
      GL_ERRORCHECK();

      switch (rtb->mFormat) {
        case EBufferFormat::RGBA8:
        case EBufferFormat::RGBA16F:
        case EBufferFormat::RGBA32F:
          OrkAssert(irt >= 0 and irt < 8);
          glReadBuffer(GL_COLOR_ATTACHMENT0 + irt);
          GL_ERRORCHECK();
          break;
        case EBufferFormat::Z32:
          glReadBuffer(GL_COLOR_ATTACHMENT0);
          GL_ERRORCHECK();
          break;
        default:
          OrkAssert(false);
          break;
      }
    }
  } else {
    w = mTargetGL.mainSurfaceWidth();
    h = mTargetGL.mainSurfaceHeight();
    GL_ERRORCHECK();
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    GL_ERRORCHECK();
    glReadBuffer(GL_BACK);
    GL_ERRORCHECK();
  }

  // glDepthMask(GL_TRUE); ??? wtf ???

  if (capbuf->_captureW != 0) {
    x = capbuf->_captureX;
    y = capbuf->_captureY;
    w = capbuf->_captureW;
    h = capbuf->_captureH;
  }

  // printf("captureAsFormat w<%d> h<%d>\n", w, h);

  bool fmtmatch = (capbuf->format() == destfmt);
  bool sizmatch = (capbuf->width() == w);
  sizmatch &= (capbuf->height() == h);

  if (not(fmtmatch and sizmatch))
    capbuf->setFormatAndSize(destfmt, w, h);

  GL_ERRORCHECK();
  static size_t yo       = 0;
  constexpr float inv256 = 1.0f / 255.0f;
  switch (destfmt) {
    case EBufferFormat::NV12: {
      size_t rgbasize = w * h * 4;
      if (capbuf->_tempbuffer.size() != rgbasize) {
        capbuf->_tempbuffer.resize(rgbasize);
      }
      glReadPixels(x, y, w, h, GL_RGBA, GL_UNSIGNED_BYTE, capbuf->_tempbuffer.data());
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
          auto yuv      = rgb.YUV();
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
          auto yuv1       = rgb1.YUV();
          auto yuv2       = rgb2.YUV();
          auto yuv3       = rgb3.YUV();
          auto yuv4       = rgb4.YUV();
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
      glReadPixels(x, y, w, h, GL_RGBA, GL_UNSIGNED_BYTE, capbuf->_data);
      break;
    }
    case EBufferFormat::RGB8: {
      //////////////////////////////////////
      // read RGBA
      //////////////////////////////////////
      size_t rgbasize = w * h * 4;
      if (capbuf->_tempbuffer.size() != rgbasize) {
        capbuf->_tempbuffer.resize(rgbasize);
      }
      glReadPixels(x, y, w, h, GL_RGBA, GL_UNSIGNED_BYTE, capbuf->_tempbuffer.data());
      //////////////////////////////////////
      // discard alpha
      //////////////////////////////////////
      auto SRC = (const uint32_t*)capbuf->_tempbuffer.data();
      auto DST = (uint8_t*)capbuf->_data;
      for (size_t ipix = 0; ipix < (w * h); ipix++) {
        int idi    = ipix * 3;
        DST[idi++] = (SRC[ipix] & 0x00ff0000) >> 16;
        DST[idi++] = (SRC[ipix] & 0x0000ff00) >> 8;
        DST[idi++] = (SRC[ipix] & 0x000000ff);
      }
      //////////////////////////////////////
      break;
    }
    case EBufferFormat::RGBA16F:
      glReadPixels(x, y, w, h, GL_RGBA, GL_HALF_FLOAT, capbuf->_data);
      break;
    case EBufferFormat::RGBA32F:
      glReadPixels(x, y, w, h, GL_RGBA, GL_FLOAT, capbuf->_data);
      break;
    case EBufferFormat::R32F:
      glReadPixels(x, y, w, h, GL_RED, GL_FLOAT, capbuf->_data);
      break;
    case EBufferFormat::R32UI:
      glReadPixels(x, y, w, h, GL_RED_INTEGER, GL_UNSIGNED_INT, capbuf->_data);
      break;
    case EBufferFormat::RG32F:
      glReadPixels(x, y, w, h, GL_RG, GL_FLOAT, capbuf->_data);
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

void GlFrameBufferInterface::GetPixel(const fvec4& rAt, PixelFetchContext& pfc) {

  _target.makeCurrentContext();
  fcolor4 Color(0.0f, 0.0f, 0.0f, 0.0f);

  _target.debugPushGroup("GetPixel");

  bool bInBounds = (rAt.x >= 0.0f and rAt.x < 1.0f and rAt.y >= 0.0f and rAt.y < 1.0f);

  logchan_glfbi->log("GetPixel rtg<%p> numbuf<%d>", (void*)pfc._rtgroup.get(), pfc._rtgroup->mNumMrts);

  if (bInBounds) {
    if (pfc._rtgroup) {
      int W  = pfc._rtgroup->width();
      int H  = pfc._rtgroup->height();
      int sx = int((rAt.x) * float(W));
      int sy = int((1.0f - rAt.y) * float(H));

      // printf("GetPixel<%d %d> FboObj<%p>\n", sx, sy, FboObj);
      auto as_impl = pfc._rtgroup->_impl.tryAs<glrtgroupimpl_ptr_t>();
      if (as_impl) {
        auto fboobj = as_impl.value()->_standard;
        GL_ERRORCHECK();
        glBindFramebuffer(GL_FRAMEBUFFER, fboobj->_fbo);
        GL_ERRORCHECK();

        logchan_glfbi->log(
            "GetPixel<%d %d> w<%d> h<%d> FboMaster<%u> rtg<%s>", sx, sy, W, H, fboobj->_fbo, pfc._rtgroup->_name.c_str());

        if (fboobj->_fbo) {

          int MrtMask = pfc.miMrtMask;

          GLint previous_readbuffer = 0;
          GL_ERRORCHECK();
          glGetIntegerv(GL_READ_BUFFER, &previous_readbuffer);
          GL_ERRORCHECK();

          logchan_glfbi->log("previous_readbuffer<%d>", int(previous_readbuffer));

          int pfc_index   = 0;
          size_t pfc_size = pfc._pickvalues.size();
          for (int MrtIndex = 0; MrtIndex < 4; MrtIndex++) {
            int MrtTest = 1 << MrtIndex;

            if (MrtTest & MrtMask) {

              if (MrtIndex < pfc_size) {
                pfc._pickvalues[MrtIndex] = nullptr;
              }

              auto rtbuffer = pfc._rtgroup->GetMrt(MrtIndex);

              OrkAssert(MrtIndex < pfc._rtgroup->GetNumTargets());

              // GL_ERRORCHECK();
              // glDepthMask(GL_TRUE);
              // GL_ERRORCHECK();
              GL_ERRORCHECK();
              glReadBuffer(GL_COLOR_ATTACHMENT0 + MrtIndex);
              GL_ERRORCHECK();

              switch (pfc._usage[MrtIndex]) {
                case PixelFetchContext::EPU_SVARIANT: {
                  switch (rtbuffer->mFormat) {
                    case EBufferFormat::RGBA32F: {
                      fvec4 rgba;
                      glReadPixels(sx, sy, 1, 1, GL_RGBA, GL_FLOAT, (void*)&rgba);
                      if (MrtIndex < pfc_size) {
                        pfc._pickvalues[MrtIndex] = pfc.decodePixel(rgba);
                      }
                      break;
                    }
                    case EBufferFormat::RGBA32UI: {
                      u32vec4 value;
                      glReadPixels(sx, sy, 1, 1, GL_RGBA_INTEGER, GL_UNSIGNED_INT, (void*)&value);
                      if (MrtIndex < pfc_size) {
                        pfc._pickvalues[MrtIndex] = pfc.decodePixel(value);
                      }
                      break;
                    }
                    default:
                      OrkAssert(false);
                      break;
                  }
                  break;
                }
                case PixelFetchContext::EPU_PTR64: {
                  switch (rtbuffer->mFormat) {
                    case EBufferFormat::RGBA16UI: {
                      uint16_t rgba[4] = {0, 0, 0, 0};
                      glReadPixels(sx, sy, 1, 1, GL_RGBA, GL_UNSIGNED_SHORT, (void*)rgba);
                      /////////////////////////////////////////////////////////////////
                      // swizzle so hex appears as xxxxyyyyzzzzwwww
                      /////////////////////////////////////////////////////////////////
                      uint64_t a     = uint64_t(rgba[0]);
                      uint64_t b     = uint64_t(rgba[1]);
                      uint64_t c     = uint64_t(rgba[2]);
                      uint64_t d     = uint64_t(rgba[3]);
                      uint64_t value = (d << 48) | (c << 32) | (b << 16) | a;
                      /////////////////////////////////////////////////////////////////
                      pfc._pickvalues[MrtIndex].set<uint64_t>(value);
                      logchan_glfbi->log(
                          "getpix MrtIndex<%d> rx<%d> ry<%d> rgba(u16)<%u %u %u %u> value<0x%zx>",
                          MrtIndex,
                          sx,
                          sy,
                          rgba[0],
                          rgba[1],
                          rgba[2],
                          rgba[3],
                          value);
                      break;
                    }
                    case EBufferFormat::RGBA32F: {
                      float rgba[4] = {0.0f, 0.0f, 0.0f, 0.0f};
                      glReadPixels(sx, sy, 1, 1, GL_RGBA, GL_FLOAT, (void*)rgba);
                      /////////////////////////////////////////////////////////////////
                      // swizzle so hex appears as xxxxyyyyzzzzwwww
                      /////////////////////////////////////////////////////////////////
                      uint64_t a     = uint64_t(rgba[0]);
                      uint64_t b     = uint64_t(rgba[1]);
                      uint64_t c     = uint64_t(rgba[2]);
                      uint64_t d     = uint64_t(rgba[3]);
                      uint64_t value = (d << 48) | (c << 32) | (b << 16) | a;
                      /////////////////////////////////////////////////////////////////
                      pfc._pickvalues[MrtIndex].set<uint64_t>(value);
                      logchan_glfbi->log(
                          "getpix MrtIndex<%d> rx<%d> ry<%d> rgba(f32)<%g %g %g %g> value<0x%zx>",
                          MrtIndex,
                          sx,
                          sy,
                          rgba[0],
                          rgba[1],
                          rgba[2],
                          rgba[3],
                          value);
                      break;
                    }
                    default:
                      OrkAssert(false);
                      break;
                  }
                  /////////////////////////////////////////////////////////////////
                  break;
                }
                case PixelFetchContext::EPU_FVEC4: {
                  fvec4 rv;
                  glReadPixels(sx, sy, 1, 1, GL_RGBA, GL_FLOAT, (void*)rv.asArray());
                  pfc._pickvalues[MrtIndex].set<fvec4>(rv);
                  // printf("getpix MrtIndex<%d> rx<%d> ry<%d> <%g %g %g %g>\n", MrtIndex, sx, sy, rv.x, rv.y, rv.z, rv.w);
                  break;
                }
                default:
                  OrkAssert(false);
                  break;
              }
              GL_ERRORCHECK();
            }
          }
          GL_ERRORCHECK();
          glBindFramebuffer(GL_FRAMEBUFFER, 0);
          if (previous_readbuffer < 1000)
            glReadBuffer(previous_readbuffer);
          GL_ERRORCHECK();
        } else {
          printf("!!!ERR - GetPix BindFBO<%d>\n", fboobj->_fbo);
        }
      }
    } else {
    }
  } else if (bInBounds) {
    pfc._pickvalues[0] = Color;
  }
  _target.debugPopGroup();
}

///////////////////////////////////////////////////////////////////////////////
}} // namespace ork::lev2
