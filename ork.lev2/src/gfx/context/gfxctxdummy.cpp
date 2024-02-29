////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/file/file.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/gfxctxdummy.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/lev2/gfx/shadman.h>
#include <ork/lev2/lev2_asset.h>
#include <ork/asset/Asset.inl>

/////////////////////////////////////////////////////////////////////////
bool LoadIL(const ork::AssetPath& pth, ork::lev2::Texture* ptex);
/////////////////////////////////////////////////////////////////////////

ImplementReflectionX(ork::lev2::ContextDummy, "ContextDummy");

namespace ork { namespace lev2 {

void ContextDummy::describeX(class_t* clazz) {
  clazz->annotateTyped<context_factory_t>("context_factory", [](){
    return std::make_shared<ContextDummy>();
  });
}
/////////////////////////////////////////////////////////////////////////

namespace dummy{
  void touchClasses(){
    ContextDummy::GetClassStatic();
  }
context_ptr_t createLoaderContext() {
  auto clazz = dynamic_cast<const object::ObjectClass*>(ContextDummy::GetClassStatic());
  GfxEnv::setContextClass(clazz);

  auto loader = std::make_shared<FxShaderLoader>();
  FxShader::RegisterLoaders("shaders/dummy/", "fxml");
  auto shadctx = FileEnv::contextForUriProto("orkshader://");
  auto democtx = FileEnv::contextForUriProto("demo://");
  loader->addLocation(shadctx, ".fxml"); // for glsl targets
  if( democtx ){
    loader->addLocation(democtx, ".fxml"); // for glsl targets
  }
  asset::registerLoader<FxShaderAsset>(loader);



  auto ctx = std::make_shared<ContextDummy>();
 // FxShader::RegisterLoaders("shaders/dummy/", "fxml");
  return ctx;
}
};

/*DuRasterStateInterface::DuRasterStateInterface(Context& target)
    : RasterStateInterface(target) {
}*/

/////////////////////////////////////////////////////////////////////////

DummyDrawingInterface::DummyDrawingInterface(ContextDummy& ctx)
    : DrawingInterface(ctx) {
}

bool DummyFxInterface::LoadFxShader(const AssetPath& pth, FxShader* pfxshader) {
  AssetPath assetname = pth;
  assetname.setExtension("fxml");
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
    //, mRsI(*this)
    , mGbI(*this)
    , mFbI(*this)
    , mDWI(*this)
    , mTxI(this) {

  static bool binit = true;

  if (true == binit) {
    binit = false;
  }
}

void ContextDummy::initializeWindowContext(Window* pWin, CTXBASE* pctxbase) {
}

void ContextDummy::initializeOffscreenContext(DisplayBuffer* pBuf) {
}

void ContextDummy::initializeLoaderContext() {
}

void ContextDummy::_doResizeMainSurface(int iw, int ih) {
  miW = iw;
  miH = ih;
}

///////////////////////////////////////////////////////////////////////////////

using vbo_t = void*;
using ibo_t = void*;

///////////////////////////////////////////////////////////////////////////////

DuGeometryBufferInterface::DuGeometryBufferInterface(ContextDummy& ctx)
    : GeometryBufferInterface(ctx)
    , _ducontext(ctx) {
}

///////////////////////////////////////////////////////////////////////////////

void* DuGeometryBufferInterface::LockIB(IndexBufferBase& idx_buf, int ibase, int icount) {
  ibo_t du_buffer = nullptr;
  if (auto try_du_buf = idx_buf._impl.tryAs<ibo_t>()) {
    du_buffer = try_du_buf.value();
  } else {
    du_buffer = (ibo_t) std::malloc(idx_buf.GetNumIndices() * idx_buf.GetIndexSize());
    idx_buf._impl.set<ibo_t>(du_buffer);
  }
  char* pch = (char*)du_buffer;
  return (void*)(pch + ibase);
}
void DuGeometryBufferInterface::UnLockIB(IndexBufferBase& idx_buf) {
}

///////////////////////////////////////////////////////////////////////////////

const void* DuGeometryBufferInterface::LockIB(const IndexBufferBase& idx_buf, int ibase, int icount) {
  ibo_t du_buffer = nullptr;
  if (auto try_du_buf = idx_buf._impl.tryAs<ibo_t>()) {
    du_buffer = try_du_buf.value();
  } else {
    du_buffer = (ibo_t) std::malloc(idx_buf.GetNumIndices() * idx_buf.GetIndexSize());
    auto& as_mutable = const_cast<IndexBufferBase&>(idx_buf);
    as_mutable._impl.set<ibo_t>(du_buffer);
  }
  char* pch = (char*)du_buffer;
  return (void*)(pch + ibase);
}
void DuGeometryBufferInterface::UnLockIB(const IndexBufferBase& idx_buf) {
}

void DuGeometryBufferInterface::ReleaseIB(IndexBufferBase& idx_buf) {
  ibo_t du_buffer = idx_buf._impl.get<ibo_t>();
  std::free(du_buffer);
  idx_buf._impl.clear();
}

///////////////////////////////////////////////////////////////////////////////

void* DuGeometryBufferInterface::LockVB(VertexBufferBase& VBuf, int ibase, int icount) {
  OrkAssert(false == VBuf.IsLocked());
  int iVBlen = VBuf.GetVtxSize() * VBuf.GetMax();
  if (not VBuf._impl.isSet()) {
    void* pdata = std::malloc(iVBlen);
    // orkprintf( "DuGeometryBufferInterface::LockVB() malloc_vblen<%d>\n", iVBlen );
    VBuf._impl.set<void*>(pdata);
  }
  VBuf.Lock();
  // VBuf.Reset();
  return VBuf._impl.get<void*>();
}

const void* DuGeometryBufferInterface::LockVB(const VertexBufferBase& VBuf, int ibase, int icount) {
  OrkAssert(false == VBuf.IsLocked());
  int iVBlen = VBuf.GetVtxSize() * VBuf.GetMax();
  VBuf.Lock();
  const void* pdata = VBuf._impl.get<void*>();
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
  std::free((void*)VBuf._impl.get<void*>());
  VBuf._impl = nullptr;
}

bool ContextDummy::SetDisplayMode(DisplayMode* mode) {
  return false;
}

void DuGeometryBufferInterface::DrawIndexedPrimitiveEML(
    const VertexBufferBase& VBuf,
    const IndexBufferBase& IdxBuf,
    PrimitiveType eType,
    int ivbase, int ivcount) {
}
void DuGeometryBufferInterface::DrawPrimitiveEML(const VertexBufferBase& VBuf, PrimitiveType eType, int ivbase, int ivcount) {
}

void DuGeometryBufferInterface::DrawPrimitiveEML(
    const FxShaderStorageBuffer* SSBO, //
    PrimitiveType eType,
    int ivbase,
    int ivcount) {
}

void DuGeometryBufferInterface::DrawInstancedIndexedPrimitiveEML(
    const VertexBufferBase& VBuf,
    const IndexBufferBase& IdxBuf,
    PrimitiveType eType,
    size_t instance_count) {
}

}} // namespace ork::lev2
