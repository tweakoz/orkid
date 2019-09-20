////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/dataflow/dataflow.h>
#include <ork/lev2/gfx/renderer/renderer.h>
#include <ork/lev2/gfx/renderer/rendercontext.h>
#include <ork/lev2/gfx/gfxmaterial_test.h>
#include <ork/lev2/gfx/renderer/frametek.h>

namespace ork::lev2 {

class CompositingGroup;
class CompositingSceneItem;
struct CompositorDrawData;
struct CompositingContext;
struct CompositingImpl;
struct CompositingData;
struct CompositingMorphable;
class DrawableBuffer;
class LightManager;

///////////////////////////////////////////////////////////////////////////////

class CompositingScene : public ork::Object {
  DeclareConcreteX(CompositingScene, ork::Object);

public:
  CompositingScene();

  const orklut<PoolString, ork::Object*>& items() const { return _items; }
  orklut<PoolString, ork::Object*>& items() { return _items; }

private:
  orklut<PoolString, ork::Object*> _items;
};

///////////////////////////////////////////////////////////////////////////////

struct CompositingMorphable : public dataflow::morphable {
  void WriteMorphTarget(dataflow::MorphKey name, float flerpval); // virtual
  void RecallMorphTarget(dataflow::MorphKey name);                // virtual
  void Morph1D(const dataflow::morph_event* pevent);              // virtual
};

///////////////////////////////////////////////////////////////////////////////

class CompositingMaterial : public lev2::GfxMaterial {
public:
  CompositingMaterial();
  ~CompositingMaterial();
  /////////////////////////////////////////////////
  virtual void Update(void) {}
  virtual void Init(lev2::GfxTarget* pTarg);
  virtual bool BeginPass(lev2::GfxTarget* pTARG, int iPass = 0);
  virtual void EndPass(lev2::GfxTarget* pTARG);
  virtual int BeginBlock(lev2::GfxTarget* pTARG, const lev2::RenderContextInstData& MatCtx);
  virtual void EndBlock(lev2::GfxTarget* pTARG);
  /////////////////////////////////////////////////
  void SetTextureA(lev2::Texture* ptex) { mCurrentTextureA = ptex; }
  void SetTextureB(lev2::Texture* ptex) { mCurrentTextureB = ptex; }
  void SetTextureC(lev2::Texture* ptex) { mCurrentTextureC = ptex; }
  void SetLevelA(const fvec4& la) { mLevelA = la; }
  void SetLevelB(const fvec4& lb) { mLevelB = lb; }
  void SetLevelC(const fvec4& lc) { mLevelC = lc; }
  void SetBiasA(const fvec4& ba) { mBiasA = ba; }
  void SetBiasB(const fvec4& bb) { mBiasB = bb; }
  void SetBiasC(const fvec4& bc) { mBiasC = bc; }
  void SetTechnique(const std::string& tek);
  /////////////////////////////////////////////////
  lev2::Texture* mCurrentTextureA;
  lev2::Texture* mCurrentTextureB;
  lev2::Texture* mCurrentTextureC;
  fvec4 mLevelA;
  fvec4 mLevelB;
  fvec4 mLevelC;
  fvec4 mBiasA;
  fvec4 mBiasB;
  fvec4 mBiasC;

  const lev2::FxShaderTechnique* hTekOp2AmulB;
  const lev2::FxShaderTechnique* hTekOp2AdivB;

  const lev2::FxShaderTechnique* hTekBoverAplusC;
  const lev2::FxShaderTechnique* hTekAplusBplusC;
  const lev2::FxShaderTechnique* hTekAlerpBwithC;
  const lev2::FxShaderTechnique* hTekAsolo;
  const lev2::FxShaderTechnique* hTekBsolo;
  const lev2::FxShaderTechnique* hTekCsolo;

  const lev2::FxShaderTechnique* hTekCurrent;

  const lev2::FxShaderParam* hMapA;
  const lev2::FxShaderParam* hMapB;
  const lev2::FxShaderParam* hLevelA;
  const lev2::FxShaderParam* hLevelB;
  const lev2::FxShaderParam* hLevelC;
  const lev2::FxShaderParam* hBiasA;
  const lev2::FxShaderParam* hBiasB;
  const lev2::FxShaderParam* hBiasC;
  const lev2::FxShaderParam* hMapC;
  const lev2::FxShaderParam* hMatMVP;
  lev2::FxShader* hModFX;
};

///////////////////////////////////////////////////////////////////////////////

class CompositingTechnique : public ork::Object {
  RttiDeclareAbstract(CompositingTechnique, ork::Object);

public:
  virtual void Init(lev2::GfxTarget* pTARG, int w, int h) = 0;
  virtual bool assemble(CompositorDrawData& drawdata) = 0;
  virtual void composite(CompositorDrawData& drawdata) = 0;
};

///////////////////////////////////////////////////////////////////////////////
class CompositingBuffer : public ork::Object {
  int miWidth;
  int miHeight;
  ork::lev2::EBufferFormat meBufferFormat;

  CompositingBuffer();
  ~CompositingBuffer();
};

///////////////////////////////////////////////////////////////////////////////

struct CompositingContext {
  int miWidth;
  int miHeight;
  lev2::GfxMaterial3DSolid mUtilMaterial;
  CompositingTechnique* _compositingTechnique = nullptr;

  CompositingContext();
  ~CompositingContext();
  void Init(lev2::GfxTarget* pTARG);
  bool assemble(CompositorDrawData& drawdata);
  void composite(CompositorDrawData& drawdata);
  void Resize(int iW, int iH);
};

///////////////////////////////////////////////////////////////////////////

struct CompositingPassData {
  const CompositingGroup* mpGroup = nullptr;
  lev2::FrameTechniqueBase* mpFrameTek = nullptr;
  bool mbDrawSource = true;
  const PoolString* mpCameraName = nullptr;
  const PoolString* mpLayerName = nullptr;
  ork::svarp_t _impl;
  ork::fvec4 _clearColor;
  CompositingPassData() {
    _impl.Set<void*>(nullptr);
  }
  static CompositingPassData FromRCFD(const RenderContextFrameData& RCFD);
  std::vector<PoolString> getLayerNames() const;
  const CameraData* getCamera(lev2::RenderContextFrameData& FrameData, int icamindex, int icullcamindex);
  void updateCompositingSize(int w, int h);
  void renderPass(lev2::RenderContextFrameData& RCFD,void_lambda_t CALLBACK);
};

typedef std::stack<lev2::CompositingPassData> compositingpassdatastack_t;

///////////////////////////////////////////////////////////////////////////

struct CompositorDrawData {
  GfxTarget* target() const;
  const CompositingImpl* _cimpl = nullptr;
  std::map<uint64_t,svar16_t> _properties;
  lev2::FrameRenderer& mFrameRenderer;
  orkstack<CompositingPassData> mCompositingGroupStack;

  CompositorDrawData(lev2::FrameRenderer& renderer) : mFrameRenderer(renderer) {}
};

///////////////////////////////////////////////////////////////////////////////

class CompositingData : public ork::Object {
  DeclareConcreteX(CompositingData, ork::Object);

public:
  ///////////////////////////////////////////////////////
  CompositingData();
  ///////////////////////////////////////////////////////

  void defaultSetup();

  const orklut<PoolString, ork::Object*>& GetGroups() const { return _groups; }
  const orklut<PoolString, ork::Object*>& GetScenes() const { return _scenes; }

  PoolString& GetActiveScene() const { return _activeScene; }
  PoolString& GetActiveItem() const { return _activeItem; }

  bool IsEnabled() const { return mbEnable && mToggle; }
  bool IsOutputFramesEnabled() const { return mbOutputFrames; }
  EOutputTimeStep OutputFrameRate() const { return mOutputFrameRate; }

  void Toggle() const { mToggle = !mToggle; }

  orklut<PoolString, ork::Object*> _groups;
  orklut<PoolString, ork::Object*> _scenes;
  mutable PoolString _activeScene;
  mutable PoolString _activeItem;
  mutable bool mToggle;
  bool mbEnable;
  bool mbOutputFrames;
  EOutputRes mOutputBaseResolution;
  EOutputResMult mOutputResMult;
  EOutputTimeStep mOutputFrameRate;

  CompositingImpl* createImpl() const;
};

///////////////////////////////////////////////////////////////////////////

class CompositingImpl {
public:

  typedef std::function<void(lev2::GfxTarget* targ)> prerendercallback_t;

  CompositingImpl(const CompositingData& data);
  ~CompositingImpl();

  bool assemble(lev2::CompositorDrawData& drawdata);
  void composite(lev2::CompositorDrawData& drawdata);

  const CompositingData& compositingData() const { return _compositingData; }

  bool IsEnabled() const;

  EOutputTimeStep currentFrameRateEnum() const;
  float currentFrameRate() const;

  const CompositingContext& compositingContext() const;
  CompositingContext& compositingContext();

  const CompositingSceneItem* compositingItem(int isceneidx, int itemidx) const;

  const CompositingGroup* compositingGroup(const PoolString& grpname) const;

  void update(float dt);

  inline void setPrerenderCallback(int key,prerendercallback_t cb){
    _prerendercallbacks[key]=cb;
  }
  void bindLighting(LightManager* lmgr) { _lightmgr=lmgr; }

private:
  const CompositingData& _compositingData;


  float mfTimeAccum;
  float mfLastTime;
  LightManager* _lightmgr = nullptr;

  CompositingMorphable _morphable;

  std::map<int,prerendercallback_t> _prerendercallbacks;

  int miActiveSceneItem;
  CompositingContext _compcontext;
};

///////////////////////////////////////////////////////////////////////////////

class CompositingSceneItem : public ork::Object {
  DeclareConcreteX(CompositingSceneItem, ork::Object);

public:
  CompositingSceneItem();

  CompositingTechnique* GetTechnique() const { return mpTechnique; }
  void _readTech(ork::rtti::ICastable*& val) const;
  void _writeTech(ork::rtti::ICastable* const& val);


  CompositingTechnique* mpTechnique;
};

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2 {
