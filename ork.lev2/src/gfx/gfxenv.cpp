////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#include <ork/kernel/opq.h>
#include <ork/kernel/prop.h>
#include <ork/kernel/prop.hpp>
#include <ork/lev2/gfx/gfxctxdummy.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/gfxmaterial_test.h>
#include <ork/lev2/gfx/material_pbr.inl>
#include <ork/lev2/gfx/gfxmaterial_ui.h>
#include <ork/lev2/gfx/gfxprimitives.h>
#include <ork/lev2/gfx/pickbuffer.h>
#include <ork/lev2/gfx/renderer/renderable.h>
#include <ork/lev2/gfx/shadman.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/lev2/gfx/dbgfontman.h>
#include <ork/lev2/ui/ui.h>
#include <ork/reflect/enum_serializer.inl>

#include <ork/reflect/properties/register.h>

namespace ork::lev2 {
int G_MSAASAMPLES = 4;
}
INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::IManipInterface, "IManipInterface");

///////////////////////////////////////////////////////////////////////////////

using namespace ork::lev2; // too many things to add ork::lev2:: in front of in this file...

BeginEnumRegistration(Blending);
RegisterEnum(Blending, OFF);
RegisterEnum(Blending, PREMA);
RegisterEnum(Blending, ALPHA);
RegisterEnum(Blending, DSTALPHA);
RegisterEnum(Blending, ADDITIVE);
RegisterEnum(Blending, ALPHA_ADDITIVE);
RegisterEnum(Blending, SUBTRACTIVE);
RegisterEnum(Blending, ALPHA_SUBTRACTIVE);
RegisterEnum(Blending, MODULATE);
EndEnumRegistration();

BeginEnumRegistration(PrimitiveType);
RegisterEnum(PrimitiveType, NONE);
RegisterEnum(PrimitiveType, POINTS);
RegisterEnum(PrimitiveType, LINES);
RegisterEnum(PrimitiveType, LINESTRIP);
RegisterEnum(PrimitiveType, LINELOOP);
RegisterEnum(PrimitiveType, TRIANGLES);
RegisterEnum(PrimitiveType, QUADS);
RegisterEnum(PrimitiveType, TRIANGLESTRIP);
RegisterEnum(PrimitiveType, TRIANGLEFAN);
RegisterEnum(PrimitiveType, QUADSTRIP);
RegisterEnum(PrimitiveType, MULTI);
RegisterEnum(PrimitiveType, END);
EndEnumRegistration();

///////////////////////////////////////////////////////////////////////////////

ECullTest GlobalCullTest = ECULLTEST_PASS_FRONT;

////////////////////////////////////////////////////////////////////////////////

void IManipInterface::Describe() {
}

/////////////////////////////////////////////////////////////////////////
SRasterState::SRasterState() {
  mPointSize = 1;
  setScissorTest(ESCISSORTEST_OFF);
  SetAlphaTest(EALPHATEST_OFF, 0);
  SetBlending(Blending::OFF);
  SetDepthTest(EDEPTHTEST_LEQUALS);
  SetShadeModel(ESHADEMODEL_SMOOTH);
  SetCullTest(ECULLTEST_PASS_FRONT);
  SetZWriteMask(true);
  SetRGBAWriteMask(true, true);
  SetStencilMode(ESTENCILTEST_OFF, ESTENCILOP_KEEP, ESTENCILOP_KEEP, 0, 0);
  SetSortID(0);
  SetTransparent(false);
}

/////////////////////////////////////////////////////////////////////////

const ork::object::ObjectClass* GfxEnv::gpTargetClass = nullptr;

void GfxEnv::SetRuntimeEnvironmentVariable(const std::string& key, const std::string& val) {
  mRuntimeEnvironment[key] = val;
}
const std::string& GfxEnv::GetRuntimeEnvironmentVariable(const std::string& key) const {
  static const std::string EmptyString("");
  orkmap<std::string, std::string>::const_iterator it = mRuntimeEnvironment.find(key);
  return (it == mRuntimeEnvironment.end()) ? EmptyString : it->second;
}

DynamicVertexBuffer<SVtxV12C4T16>& GfxEnv::GetSharedDynamicVB() {
  return GetRef().mVtxBufSharedVect;
}

DynamicVertexBuffer<SVtxV12N12B12T8C4>& GfxEnv::GetSharedDynamicVB2() {
  return GetRef().mVtxBufSharedVect2;
}

DynamicVertexBuffer<SVtxV16T16C16>& GfxEnv::GetSharedDynamicV16T16C16() {
  return GetRef()._vtxBufSharedV16T16C16;
}

GfxEnv::GfxEnv()
    : NoRttiSingleton<GfxEnv>()
    , mpMainWindow(nullptr)
    , mGfxEnvMutex("GfxEnvGlobalMutex")
    , gLoaderTarget(nullptr)
    , mVtxBufSharedVect(256 << 10, 0, PrimitiveType::TRIANGLES)    // SVtxV12C4T16==32bytes
    , mVtxBufSharedVect2(256 << 10, 0, PrimitiveType::TRIANGLES)   // SvtxV12N12B12T8C4==48bytes
    , _vtxBufSharedV16T16C16(1 << 20, 0, PrimitiveType::TRIANGLES) // SvtxV12N12B12T8C4==48bytes
{
  mVtxBufSharedVect.SetRingLock(true);
  mVtxBufSharedVect2.SetRingLock(true);
  _vtxBufSharedV16T16C16.SetRingLock(true);
  ContextCreationParams params;
  params.miNumSharedVerts = 8 << 10;

  PushCreationParams(params);
  Texture::RegisterLoaders();
}

/////////////////////////////////////////////////////////////////////////

void GfxEnv::atomicOp(recursive_mutex::atomicop_t op) {
  GetRef().mGfxEnvMutex.atomicOp(op);
}

/////////////////////////////////////////////////////////////////////////

void GfxEnv::RegisterWinContext(Window* pWin) {
  // orkprintf("GfxEnv::RegisterWinContext\n");
  // gfxenvlateinit();
}

bool GfxEnv::initialized() {
  return GetRef()._initialized;
}

void GfxEnv::SetLoaderTarget(Context* target) {
  gLoaderTarget = target;

  auto gfxenvlateinit = [=]() {
    auto ctx = GfxEnv::loadingContext();
    ctx->makeCurrentContext();

#if !defined(__APPLE__)
    // ctx->beginFrame();
#endif
    ctx->debugPushGroup("GfxEnv.Lateinit");

    ork::lev2::GfxPrimitives::Init(ctx);
    ctx->debugPopGroup();
#if !defined(__APPLE__)
    // ctx->endFrame();
#endif
    _initialized = true;
  };
  opq::mainSerialQueue()->enqueue(gfxenvlateinit);
}

/////////////////////////////////////////////////////////////////////////

CaptureBuffer::CaptureBuffer()
    : _data(0)
    , meFormat(EBufferFormat::NONE)
    , _buffersize(0) {
}
CaptureBuffer::~CaptureBuffer() {
  if (_data) {
    free(_data);
  }
}
int CaptureBuffer::GetStride() const {
  int istride = 0;
  switch (meFormat) {
    case EBufferFormat::NV12:
      istride = -1;
      break;
    case EBufferFormat::RGBA8:
      istride = 4;
      break;
    case EBufferFormat::RGBA16F:
    case EBufferFormat::RGBA16UI:
      istride = 8;
      break;
    case EBufferFormat::RGBA32F:
      istride = 16;
      break;
    case EBufferFormat::R32F:
    case EBufferFormat::R32UI:
      istride = 4;
      break;
    default:
      OrkAssert(false);
      break;
  }
  return istride;
}
int CaptureBuffer::CalcDataIndex(int ix, int iy) const {
  return ix + (iy * miW);
}
void CaptureBuffer::SetWidth(int iw) {
  miW = iw;
}
void CaptureBuffer::SetHeight(int ih) {
  miH = ih;
}
int CaptureBuffer::width() const {
  return miW;
}
int CaptureBuffer::height() const {
  return miH;
}
EBufferFormat CaptureBuffer::format() const {
  return meFormat;
}
void CaptureBuffer::CopyData(const void* pfrom, int isize) {
  int icapsize = GetStride() * miW * miH;
  OrkAssert(isize == icapsize);
  memcpy(_data, pfrom, isize);
}

void CaptureBuffer::setFormatAndSize(EBufferFormat fmt, int w, int h) {
  if (_data != nullptr)
    free(_data);

  int bytesperpix = 0;
  switch (fmt) {

    case EBufferFormat::RGBA8:
    case EBufferFormat::R32F:
    case EBufferFormat::R32UI:
      _buffersize = 4 * w * h;
      break;
    case EBufferFormat::RGBA16F:
    case EBufferFormat::RGBA16UI:
    case EBufferFormat::RG32F:
      _buffersize = 8 * w * h;
      break;
    case EBufferFormat::RGBA32F:
      _buffersize = 16 * w * h;
      break;
    case EBufferFormat::NV12: {
      size_t ysize  = w * h;
      size_t uvsize = ysize >> 1;
      _buffersize   = ysize + uvsize;
      break;
    }
    default:
      assert(false);
      break;
  }
  _data    = malloc(_buffersize);
  meFormat = fmt;
  miW      = w;
  miH      = h;
}
