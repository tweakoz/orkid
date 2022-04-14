////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/file/file.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/gfxctxdummy.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/lev2/gfx/shadman.h>

/////////////////////////////////////////////////////////////////////////
bool LoadIL(const ork::AssetPath& pth, ork::lev2::Texture* ptex);
/////////////////////////////////////////////////////////////////////////

ImplementReflectionX(ork::lev2::ContextDummy, "ContextDummy");

namespace ork { namespace lev2 {

void ContextDummy::describeX(class_t* clazz) {
}
/////////////////////////////////////////////////////////////////////////

void DummyContextInit() {
  auto clazz = dynamic_cast<const object::ObjectClass*>(ContextDummy::GetClassStatic());
  GfxEnv::setContextClass(clazz);
}

DuRasterStateInterface::DuRasterStateInterface(Context& target)
    : RasterStateInterface(target) {
}

/////////////////////////////////////////////////////////////////////////

DummyDrawingInterface::DummyDrawingInterface(ContextDummy& ctx)
    : DrawingInterface(ctx) {
}

bool DummyFxInterface::LoadFxShader(const AssetPath& pth, FxShader* pfxshader) {
  AssetPath assetname = pth;
  assetname.SetExtension("fxml");
  FxShader* shader = new FxShader;
  printf("DUMMYFX::LOADED<%s>\n", pth.c_str());
  // bool bOK = LoadFxShader( shader );
  // OrkAssert(bOK);
  return true;
}

///////////////////////////////////////////////////////////////////////////////

fmtx4 DuMatrixStackInterface::Ortho(float left, float right, float top, float bottom, float fnear, float ffar) {
  fmtx4 mat;
  mat.ortho(left, right, top, bottom, fnear, ffar);
  return mat;
}

///////////////////////////////////////////////////////////////////////////////

DuFrameBufferInterface::DuFrameBufferInterface(Context& target)
    : FrameBufferInterface(target) {
}

DuFrameBufferInterface::~DuFrameBufferInterface() {
}

///////////////////////////////////////////////////////////////////////////////

ContextDummy::~ContextDummy() {
}

///////////////////////////////////////////////////////////////////////////////

ContextDummy::ContextDummy()
    : Context()
    , mMtxI(*this)
    , mRsI(*this)
    , mGbI(*this)
    , mFbI(*this)
    , mDWI(*this) {
  DummyContextInit();
  static bool binit = true;

  if (true == binit) {
    binit = false;
    // FxShader::RegisterLoaders("shaders/dummy/", "fxml");
  }
}

void ContextDummy::initializeWindowContext(Window* pWin, CTXBASE* pctxbase) {
}

void ContextDummy::initializeOffscreenContext(OffscreenBuffer* pBuf) {
}

void ContextDummy::initializeLoaderContext() {
}

void ContextDummy::_doResizeMainSurface(int iw, int ih) {
  miW = iw;
  miH = ih;
}

///////////////////////////////////////////////////////////////////////////////

DuGeometryBufferInterface::DuGeometryBufferInterface(ContextDummy& ctx)
    : GeometryBufferInterface(ctx)
    , _ducontext(ctx) {
}

void* DuGeometryBufferInterface::LockIB(IndexBufferBase& IdxBuf, int ibase, int icount) {
  if (0 == IdxBuf.GetHandle()) {
    IdxBuf.SetHandle((void*)std::malloc(IdxBuf.GetNumIndices() * IdxBuf.GetIndexSize()));
  }
  char* pch = (char*)IdxBuf.GetHandle();
  return (void*)(pch + ibase);
}
void DuGeometryBufferInterface::UnLockIB(IndexBufferBase& IdxBuf) {
}

const void* DuGeometryBufferInterface::LockIB(const IndexBufferBase& IdxBuf, int ibase, int icount) {
  if (0 == IdxBuf.GetHandle()) {
    IdxBuf.SetHandle((void*)std::malloc(IdxBuf.GetNumIndices() * IdxBuf.GetIndexSize()));
  }
  const char* pch = (const char*)IdxBuf.GetHandle();
  return (const void*)(pch + ibase);
}
void DuGeometryBufferInterface::UnLockIB(const IndexBufferBase& IdxBuf) {
}

void DuGeometryBufferInterface::ReleaseIB(IndexBufferBase& IdxBuf) {
  std::free(IdxBuf.GetHandle());
  IdxBuf.SetHandle(0);
}

void* DuGeometryBufferInterface::LockVB(VertexBufferBase& VBuf, int ibase, int icount) {
  OrkAssert(false == VBuf.IsLocked());
  int iVBlen = VBuf.GetVtxSize() * VBuf.GetMax();
  if (0 == VBuf.GetHandle()) {
    void* pdata = std::malloc(iVBlen);
    // orkprintf( "DuGeometryBufferInterface::LockVB() malloc_vblen<%d>\n", iVBlen );
    VBuf.SetHandle(pdata);
  }
  VBuf.Lock();
  // VBuf.Reset();
  return VBuf.GetHandle();
}

const void* DuGeometryBufferInterface::LockVB(const VertexBufferBase& VBuf, int ibase, int icount) {
  OrkAssert(false == VBuf.IsLocked());
  int iVBlen = VBuf.GetVtxSize() * VBuf.GetMax();
  VBuf.Lock();
  const void* pdata = VBuf.GetHandle();
  OrkAssert(pdata != 0);
  return pdata;
}

void DuGeometryBufferInterface::UnLockVB(VertexBufferBase& VBuf) {
  OrkAssert(VBuf.IsLocked());
  VBuf.Unlock();
}
void DuGeometryBufferInterface::UnLockVB(const VertexBufferBase& VBuf) {
  OrkAssert(VBuf.IsLocked());
  VBuf.Unlock();
}
void DuGeometryBufferInterface::ReleaseVB(VertexBufferBase& VBuf) {
  std::free((void*)VBuf.GetHandle());
}

bool ContextDummy::SetDisplayMode(DisplayMode* mode) {
  return false;
}

void DuGeometryBufferInterface::DrawIndexedPrimitiveEML(
    const VertexBufferBase& VBuf,
    const IndexBufferBase& IdxBuf,
    PrimitiveType eType,
    int ivbase,
    int ivcount) {
}
void DuGeometryBufferInterface::DrawPrimitiveEML(const VertexBufferBase& VBuf, PrimitiveType eType, int ivbase, int ivcount) {
}

void DuGeometryBufferInterface::DrawInstancedIndexedPrimitiveEML(
    const VertexBufferBase& VBuf,
    const IndexBufferBase& IdxBuf,
    PrimitiveType eType,
    size_t instance_count) {
}

bool DuTextureInterface::LoadTexture(const AssetPath& fname, texture_ptr_t ptex) {
  ///////////////////////////////////////////////
  AssetPath Filename = fname;
  bool bHasExt       = Filename.HasExtension();
  if (false == bHasExt) {
    Filename.SetExtension("dds");
  }
  ///////////////////////////////////////////////
  File TextureFile(Filename, ork::EFM_READ);
  if (false == TextureFile.IsOpen()) {
    return false;
  }
  return true;
}

}} // namespace ork::lev2
