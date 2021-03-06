////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <functional>
#include <unordered_map>
#include <ork/lev2/lev2_types.h>
#include <ork/lev2/gfx/gfxenv_enum.h> // For ETextureDest
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/renderer/rendercontext.h>
#include <ork/lev2/gfx/shadman.h>
#include <ork/lev2/gfx/gfxrasterstate.h>
#include <ork/lev2/gfx/fxstate_instance.h>
#include <ork/kernel/varmap.inl>

namespace ork {
namespace lev2 {

using material_ptr_t      = std::shared_ptr<GfxMaterial>;
using material_constptr_t = std::shared_ptr<const GfxMaterial>;

class Texture;

/////////////////////////////////////////////////////////////////////////

#define MAX_MATERIAL_TEXTURES 4

struct VertexConfig {
  std::string Name;
  std::string Type;
  std::string Source;
  std::string Semantic;
};

///////////////////////////////////////////////////////////////////////////////

struct TextureContext {
  TextureContext(const Texture* ptex = 0, float repU = 1.0f, float repV = 1.0f);

  const Texture* mpTexture;
  float mfRepeatU;
  float mfRepeatV;
};

struct MaterialInstApplicator {
  virtual void ApplyToTarget(Context* pTARG) = 0;
};

struct MaterialInstItem {
  PoolString mObjectName;
  PoolString mChannelName;
  const GfxMaterial* mMaterial;
  MaterialInstApplicator* mApplicator;
  MaterialInstItem()
      : mMaterial(0)
      , mApplicator(0) {
  }
  virtual ~MaterialInstItem() {
  }
  virtual void Set() = 0;
  void SetApplicator(MaterialInstApplicator* papplicator) {
    mApplicator = papplicator;
  }
};

///////////////////////////////////////////////////////////////////////////////

struct MaterialInstItemMatrix : public MaterialInstItem {
  fmtx4 mMatrix;
  MaterialInstItemMatrix() {
  }
  void SetMatrix(const fmtx4& pmat) {
    mMatrix = pmat;
  }
  const fmtx4& GetMatrix() const {
    return mMatrix;
  }

private:
  void Set() override {
  }
};

///////////////////////////////////////////////////////////////////////////////

struct MaterialInstItemMatrixBlock : public MaterialInstItem {
  size_t miNumMatrices;
  const fmtx4* mpMatrices;
  MaterialInstItemMatrixBlock()
      : miNumMatrices(0)
      , mpMatrices(0) {
  }

  void SetNumMatrices(size_t i) {
    miNumMatrices = i;
  }
  void SetMatrixBlock(const fmtx4* pmat) {
    mpMatrices = pmat;
  }

  size_t GetNumMatrices() const {
    return miNumMatrices;
  }
  const fmtx4* GetMatrices() const {
    return mpMatrices;
  }
  void Set() final {
  }
};

///////////////////////////////////////////////////////////////////////////////

struct GfxMaterial : public ork::Object {
  RttiDeclareAbstract(GfxMaterial, ork::Object);
  //////////////////////////////////////////////////////////////////////////////

public:
  f32 mfParticleSize; // todo this does not belong here

  GfxMaterial();
  virtual ~GfxMaterial();

  virtual int GetNumPasses(void) {
    return int(miNumPasses);
  }

  virtual void Update(void) = 0;

  virtual void gpuInit(Context* context) = 0;
  virtual void gpuUpdate(Context* context) {
    if (_doinit) {
      gpuInit(context);
      _doinit = false;
    }
  }
  virtual bool BeginPass(Context* pTARG, int iPass = 0)                                                        = 0;
  virtual void EndPass(Context* pTARG)                                                                         = 0;
  virtual int BeginBlock(Context* pTARG, const RenderContextInstData& MatCtx = RenderContextInstData::Default) = 0;
  virtual void EndBlock(Context* pTARG)                                                                        = 0;

  void SetTexture(ETextureDest edest, const TextureContext& htex);
  const TextureContext& GetTexture(ETextureDest edest) const;
  TextureContext& GetTexture(ETextureDest edest);

  void SetName(const PoolString& nam) {
    mMaterialName = nam;
  }
  const PoolString& GetName(void) const {
    return mMaterialName;
  }

  void SetFogStart(F32 fstart) {
    mfFogStart = float(fstart);
  };
  void SetFogRange(F32 frange) {
    mfFogRange = float(frange);
  };

  virtual void UpdateMVPMatrix(Context* pTARG) {
  }

  virtual void BindMaterialInstItem(MaterialInstItem* pitem) const {
  }
  virtual void UnBindMaterialInstItem(MaterialInstItem* pitem) const {
  }

  const RenderQueueSortingData& GetRenderQueueSortingData() const {
    return mSortingData;
  }
  RenderQueueSortingData& GetRenderQueueSortingData() {
    return mSortingData;
  }

  virtual void SetMaterialProperty(const char* prop, const char* val) {
  }

  void PushDebug(bool bdbg);
  void PopDebug();
  bool IsDebug();

  virtual fxinstance_ptr_t createFxStateInstance(FxStateInstanceConfig& cfg) const;

  //////////////////////////////////////////////////////////////////////////////
  SRasterState swapRasterState(SRasterState rstate);
  //////////////////////////////////////////////////////////////////////////////

  SRasterState _rasterstate;

  int miNumPasses; ///< Number Of Render Passes in this Material (platform specific)
  PoolString mMaterialName;
  TextureContext mTextureMap[ETEXDEST_END];
  float mfFogStart;
  float mfFogRange;
  RenderQueueSortingData mSortingData;
  const RenderContextInstData* mRenderContexInstData;
  std::stack<bool> mDebug;
  bool _doinit = true;

  ork::varmap::VarMap _varmap;
};

///////////////////////////////////////////////////////////////////////////////

typedef orkmap<std::string, material_ptr_t> MaterialMap;
bool LoadMaterialMap(const ork::file::Path& pth, MaterialMap& mmap);

} // namespace lev2

namespace chunkfile {

struct Reader;
class Writer;
class OutputStream;
class InputStream;

struct XgmMaterialWriterContext {
  XgmMaterialWriterContext(Writer& w);
  OutputStream* _outputStream = nullptr;
  lev2::material_constptr_t _material;
  Writer& _writer;
  varmap::VarMap _varmap;
};
struct XgmMaterialReaderContext {
  XgmMaterialReaderContext(Reader& r);
  InputStream* _inputStream = nullptr;
  lev2::material_ptr_t _material;
  Reader& _reader;
  varmap::VarMap _varmap;
};

typedef std::function<lev2::material_ptr_t(XgmMaterialReaderContext& ctx)> materialreader_t;
typedef std::function<void(XgmMaterialWriterContext& ctx)> materialwriter_t;

} // namespace chunkfile

} // namespace ork
