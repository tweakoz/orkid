////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
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
#include <ork/util/Context.hpp>
#include <ork/kernel/memcpy.inl>
#include <ork/lev2/gfx/gfxvtxbuf.inl>

#include <ork/reflect/properties/register.h>

///////////////////////////////////////////////////////////////////////////////

template class ork::util::ContextTLS<ork::lev2::ThreadGfxContext>;

ork::lev2::Context* ork::lev2::contextForCurrentThread(){
    auto thrctx = ork::lev2::ThreadGfxContext::context();
  return thrctx ? thrctx->_context : nullptr;
}

///////////////////////////////////////////////////////////////////////////////

namespace ork::lev2 {

bool GfxEnv::_bc7Disabled = false;

bool GfxEnv::supportsBC7(){
  return not _bc7Disabled;
}
void GfxEnv::disableBC7(){
  _bc7Disabled = true;
}

int msaaEnumToInt( const MsaaSamples& samples ){
  int convsamples = 0;
  switch(samples){
    case MsaaSamples::MSAA_1X:
      convsamples = 1;
      break;
    case MsaaSamples::MSAA_4X:
      convsamples = 4;
      break;
    case MsaaSamples::MSAA_8X:
      convsamples = 8;
      break;
    case MsaaSamples::MSAA_9X:
      convsamples = 9;
      break;
    case MsaaSamples::MSAA_16X:
      convsamples = 16;
      break;
    case MsaaSamples::MSAA_25X:
      convsamples = 25;
      break;
    default:
      OrkAssert(false);
      break;
  }
  return convsamples;
}

MsaaSamples intToMsaaEnum( int samples ){
  MsaaSamples rval = MsaaSamples::MSAA_1X;
  switch(samples){
    case 0:
    case 1:
      rval = MsaaSamples::MSAA_1X;
      break;
    case 2:
      rval = MsaaSamples::MSAA_4X;
      break;
    case 3:
      rval = MsaaSamples::MSAA_9X;
      break;
    case 4:
      rval = MsaaSamples::MSAA_16X;
      break;
    case 5:
      rval = MsaaSamples::MSAA_25X;
      break;
    default:
      printf( "invalid msaa samples<%d>\n", samples );
      OrkAssert(false);
      break;
  }
  return rval;
}

std::string EBufferFormatToName(EBufferFormat fmt){
  std::string rval;
  switch(fmt){
    case EBufferFormat::NONE:
      rval = "NONE";
      break;
    case EBufferFormat::R8:
      rval = "R8";
      break;
    case EBufferFormat::R16:
      rval = "R16";
      break;
    case EBufferFormat::R32F:
      rval = "R32F";
      break;
    case EBufferFormat::R32UI:
      rval = "R32UI";
      break;
    case EBufferFormat::BGR5A1:
      rval = "BGR5A1";
      break;
    case EBufferFormat::BGR8:
      rval = "BGR8";
      break;
    case EBufferFormat::RGB8:
      rval = "RGB8";
      break;
    case EBufferFormat::BGRA8:
      rval = "BGRA8";
      break;
    case EBufferFormat::RGBA8:
      rval = "RGBA8";
      break;
    case EBufferFormat::RGB16:
      rval = "RGB16";
      break;
    case EBufferFormat::RGBA16F:
      rval = "RGBA16F";
      break;
    case EBufferFormat::RGBA16UI:
      rval = "RGBA16UI";
      break;
    case EBufferFormat::RGBA32F:
      rval = "RGBA32F";
      break;
    case EBufferFormat::RGB32UI:
      rval = "RGB32UI";
      break;
    case EBufferFormat::RGBA32UI:
      rval = "RGBA32UI";
      break;
    case EBufferFormat::NV12:
      rval = "NV12";
      break;
    case EBufferFormat::YUV420P:
      rval = "YUV420P";
      break;
    case EBufferFormat::Z32:
      rval = "Z32";
      break;
    case EBufferFormat::RGBA_BPTC_UNORM:
      rval = "RGBA_BPTC_UNORM";
      break;
    case EBufferFormat::SRGB_ALPHA_BPTC_UNORM:
      rval = "SRGB_ALPHA_BPTC_UNORM";
      break;
    case EBufferFormat::RGBA_ASTC_4X4:
      rval = "RGBA_ASTC_4X4";
      break;
    case EBufferFormat::SRGB_ASTC_4X4:
      rval = "SRGB_ASTC_4X4";
      break;
    case EBufferFormat::S3TC_DXT1:
      rval = "S3TC_DXT1";
      break;
    case EBufferFormat::S3TC_DXT3:
      rval = "S3TC_DXT3";
      break;
    case EBufferFormat::S3TC_DXT5:
      rval = "S3TC_DXT5";
      break;
    default:
      printf( "invalid buffer format<%0zx>\n", size_t(fmt) );
      OrkAssert(false);
      break;
  }
  return rval;
}


extern std::atomic<int> __FIND_IT;
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

ECullTest GlobalCullTest = ECullTest::PASS_FRONT;

////////////////////////////////////////////////////////////////////////////////

void IManipInterface::Describe() {
}

/////////////////////////////////////////////////////////////////////////
SRasterState::SRasterState() {
  mPointSize = 1;
  setScissorTest(ESCISSORTEST_OFF);
  SetAlphaTest(EALPHATEST_OFF, 0);
  SetBlending(Blending::OFF);
  SetDepthTest(EDepthTest::LEQUALS);
  SetShadeModel(ESHADEMODEL_SMOOTH);
  SetCullTest(ECullTest::PASS_FRONT);
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
    , mVtxBufSharedVect(16 << 20, 0)    // SVtxV12C4T16==32bytes
    , mVtxBufSharedVect2(256 << 10, 0)   // SvtxV12N12B12T8C4==48bytes
    , _vtxBufSharedV16T16C16(1 << 20, 0) // SvtxV12N12B12T8C4==48bytes
    , mGfxEnvMutex("GfxEnvGlobalMutex")
{
  _lockCounter.store(0);

  mVtxBufSharedVect.SetRingLock(true);
  mVtxBufSharedVect2.SetRingLock(true);
  _vtxBufSharedV16T16C16.SetRingLock(true);
  ContextCreationParams params;
  params.miNumSharedVerts = 8 << 10;

  PushCreationParams(params);
  Texture::RegisterLoaders();
}

/////////////////////////////////////////////////////////////////////////

uint64_t GfxEnv::createLock(){
  uint64_t l = GetRef()._lockCounter.fetch_add(1);
  GetRef()._waitlockdata.atomicOp([l](WaitLockData& unlocked){
    unlocked._locks.insert(l);
  });
  return l;
}

/////////////////////////////////////////////////////////////////////////

void GfxEnv::releaseLock(uint64_t lock){
  locknotifset_t notifs;
  GetRef()._waitlockdata.atomicOp([lock,&notifs](WaitLockData& unlocked){
    auto it = unlocked._locks.find(lock);
    OrkAssert(it!=unlocked._locks.end());
    unlocked._locks.erase(it);
    if(unlocked._locks.size()==0){
      notifs = unlocked._notifs;
      unlocked._notifs.clear();
    }
  });
  for(auto n : notifs) n();
}

/////////////////////////////////////////////////////////////////////////

GfxEnv::lockset_t GfxEnv::dumpLocks(){
  GfxEnv::lockset_t rval;
  GetRef()._waitlockdata.atomicOp([&rval](WaitLockData& unlocked){
    rval = unlocked._locks;
  });
  return rval;
}

/////////////////////////////////////////////////////////////////////////

void GfxEnv::onLocksDone(void_lambda_t l){
  bool execute_now = false;
  GetRef()._waitlockdata.atomicOp([l,&execute_now](WaitLockData& unlocked){
    if(unlocked._locks.size()==0){
      execute_now = true;
    }
    else{
      unlocked._notifs.push_back(l);
    }
  });
  if(execute_now)
    l();
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

void GfxEnv::initializeWithContext(context_ptr_t target){

  auto op = [target](){

    if( not GetRef()._initialized  ){
      target->makeCurrentContext();
      /////////////////////////////////////
      #if !defined(__APPLE__)
      //target->beginFrame();
      #endif
      /////////////////////////////////////
      target->debugPushGroup("GfxEnv.Lateinit");
      ork::lev2::GfxPrimitives::Init(target.get());
      __FIND_IT.store(0);
      target->debugPopGroup();
      /////////////////////////////////////
      #if !defined(__APPLE__)
      //target->endFrame();
      #endif
      /////////////////////////////////////
      GfxEnv::GetRef()._initialized = true;
    }
  };

  opq::mainSerialQueue()->enqueue(op);
}

/////////////////////////////////////////////////////////////////////////

CaptureBuffer::CaptureBuffer()
    : meFormat(EBufferFormat::NONE)
    , _data(0)
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
    case EBufferFormat::RGB8:
      istride = 3;
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
  memcpy_fast(_data, pfrom, isize);
}

void CaptureBuffer::setFormatAndSize(EBufferFormat fmt, int w, int h) {
  if (_data != nullptr)
    free(_data);

  int bytesperpix = 0;
  switch (fmt) {

    case EBufferFormat::RGB8:
      _buffersize = 3 * w * h;
      break;
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
