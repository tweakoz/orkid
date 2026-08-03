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

struct DuIndexBufferImpl {
  int miNumIndices = 0;
  void* mpIndices  = nullptr;
  bool _locked    = false;
  ~DuIndexBufferImpl() {
    if (mpIndices)
      std::free(mpIndices);
  }
};
struct DuVertexBufferImpl {
  ~DuVertexBufferImpl() {
    if (_pmemory)
      std::free(_pmemory);
  }

  void* _pmemory = nullptr;
};

/////////////////////////////////////////////////////////////////////////
bool LoadIL(const ork::AssetPath& pth, ork::lev2::Texture* ptex);
/////////////////////////////////////////////////////////////////////////

ImplementReflectionX(ork::lev2::ContextDummy, "ContextDummy");

namespace ork { namespace lev2 {

void ContextDummy::describeX(class_t* clazz) {
  clazz->annotateTyped<context_factory_t>("context_factory", []() { return std::make_shared<ContextDummy>(); });
}

/////////////////////////////////////////////////////////////////////////

namespace dummy {
  void touchClasses() {
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
    if (democtx) {
      loader->addLocation(democtx, ".fxml"); // for glsl targets
    }
    asset::registerLoader<FxShaderAsset>(loader);

    auto ctx = std::make_shared<ContextDummy>();
    // FxShader::RegisterLoaders("shaders/dummy/", "fxml");
    return ctx;
  }
}; // namespace dummy

/////////////////////////////////////////////////////////////////////////

DummyDrawingInterface::DummyDrawingInterface(ContextDummy& ctx)
    : DrawingInterface(ctx) {
}

bool DummyFxInterface::LoadFxShader(const AssetPath& pth, FxShader* pfxshader) {
  OrkAssert(false);
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
    , mGbI(*this)
    , mTxI(*this) 
    , mFbI(*this)
    , mDWI(*this) {

  static bool binit = true;

  if (true == binit) {
    binit = false;
    // FxShader::RegisterLoaders("shaders/dummy/", "fxml");
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

DuGeometryBufferInterface::DuGeometryBufferInterface(ContextDummy& ctx)
    : GeometryBufferInterface(ctx)
    , _ducontext(ctx) {
}

void* DuGeometryBufferInterface::LockIB(IndexBufferBase& IdxBuf, int ibase, int icount) {
  if (not IdxBuf._impl.isSet()) {
    auto impl          = IdxBuf._impl.makeShared<DuIndexBufferImpl>();
    impl->miNumIndices = IdxBuf.GetNumIndices();
    impl->mpIndices    = std::malloc(IdxBuf.GetNumIndices() * IdxBuf.indexSize());
  }
  auto impl      = IdxBuf._impl.getShared<DuIndexBufferImpl>();
  char* pch      = (char*)impl->mpIndices;
  impl->_locked = true;
  return (void*)(pch + ibase);
}
void DuGeometryBufferInterface::UnLockIB(IndexBufferBase& IdxBuf) {
  auto impl      = IdxBuf._impl.getShared<DuIndexBufferImpl>();
  impl->_locked = false;
}

const void* DuGeometryBufferInterface::LockIB(const IndexBufferBase& IdxBuf, int ibase, int icount) {
  if (not IdxBuf._impl.isSet()) {
    auto impl          = IdxBuf._impl.makeShared<DuIndexBufferImpl>();
    impl->miNumIndices = IdxBuf.GetNumIndices();
    impl->mpIndices    = std::malloc(IdxBuf.GetNumIndices() * IdxBuf.indexSize());
  }
  auto impl      = IdxBuf._impl.getShared<DuIndexBufferImpl>();
  char* pch      = (char*)impl->mpIndices;
  impl->_locked = true;
  return (void*)(pch + ibase);
}
void DuGeometryBufferInterface::UnLockIB(const IndexBufferBase& IdxBuf) {
  auto impl      = IdxBuf._impl.getShared<DuIndexBufferImpl>();
  impl->_locked = false;
}

void DuGeometryBufferInterface::ReleaseIB(IndexBufferBase& IdxBuf) {
}

void* DuGeometryBufferInterface::LockVB(VertexBufferBase& VBuf, int ibase, int icount) {
  OrkAssert(false == VBuf.IsLocked());
  int iVBlen = VBuf.GetVtxSize() * VBuf.GetMax();
  if (not VBuf._impl.isSet()) {
    auto impl      = VBuf._impl.makeShared<DuVertexBufferImpl>();
    impl->_pmemory = std::malloc(iVBlen);
  }
  VBuf.Lock();
  auto impl = VBuf._impl.getShared<DuVertexBufferImpl>();
  return impl->_pmemory;
}

const void* DuGeometryBufferInterface::LockVB(const VertexBufferBase& VBuf, int ibase, int icount) {
  OrkAssert(false == VBuf.IsLocked());
  int iVBlen = VBuf.GetVtxSize() * VBuf.GetMax();
  VBuf.Lock();
  auto impl = VBuf._impl.getShared<DuVertexBufferImpl>();
  return impl->_pmemory;
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
  VBuf._impl = nullptr;
}

bool ContextDummy::SetDisplayMode(DisplayMode* mode) {
  return false;
}

void DuGeometryBufferInterface::DrawIndexedPrimitiveEML(
    const VertexBufferBase& VBuf,
    const IndexBufferBase& IdxBuf,
    PrimitiveType eType) {
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

void DuGeometryBufferInterface::DrawInstancedIndexedPrimitiveEML(
    const VertexBufferBase& VBuf,
    const IndexBufferBase& IdxBuf,
    PrimitiveType eType,
    size_t instance_count,
    size_t first_instance) {
}

void DuGeometryBufferInterface::DrawInstancedIndexedPrimitiveIndirectEML(
    const VertexBufferBase& VBuf,
    const IndexBufferBase& IdxBuf,
    PrimitiveType eType,
    const FxShaderStorageBuffer* indirect_args,
    size_t args_offset) {
}

void DuGeometryBufferInterface::DrawIndirectEML(
    PrimitiveType eType,
    const FxShaderStorageBuffer* indirect_args,
    size_t args_offset) {
}

void DuGeometryBufferInterface::DrawIndexedIndirectEML(
    const FxShaderStorageBuffer* index_buffer,
    PrimitiveType eType,
    const FxShaderStorageBuffer* indirect_args,
    size_t args_offset,
    int index_size) {
}

void DuGeometryBufferInterface::DrawMeshTasksEML(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) {
}

void DuGeometryBufferInterface::DrawMeshTasksIndirectEML(const FxShaderStorageBuffer* indirect_args, size_t args_offset) {
}

DuTextureInterface::DuTextureInterface(Context& ctx)
    : TextureInterface(&ctx) {
}

}} // namespace ork::lev2
