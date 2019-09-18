////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/dataflow/dataflow.h>
#include "renderer/builtin_frameeffects.h"

namespace ork { namespace lev2 {

class CompositingGroup;
class CompositingSceneItem;
struct CompositorDrawData;
struct CompositingContext;
struct CompositingImpl;
struct CompositingData;
struct CompositingMorphable;

///////////////////////////////////////////////////////////////////////////////

enum EOutputTimeStep {
  EOutputTimeStep_RealTime = 0,
  EOutputTimeStep_15fps,
  EOutputTimeStep_24fps,
  EOutputTimeStep_30fps,
  EOutputTimeStep_48fps,
  EOutputTimeStep_60fps,
  EOutputTimeStep_72fps,
  EOutputTimeStep_96fps,
  EOutputTimeStep_120fps,
  EOutputTimeStep_240fps,
};

enum EOutputRes {
  EOutputRes_640x480 = 0,
  EOutputRes_960x640,
  EOutputRes_1024x1024,
  EOutputRes_1280x720,
  EOutputRes_1600x1200,
  EOutputRes_1920x1080,
};

enum EOutputResMult {
  EOutputResMult_Quarter = 0,
  EOutputResMult_Half,
  EOutputResMult_Full,
  EOutputResMult_Double,
  EOutputResMult_Quadruple,
};

///////////////////////////////////////////////////////////////////////////////

enum ECOMPOSITEBlend {
  BoverAplusC = 0,
  AplusBplusC,
  AlerpBwithC,
  Asolo,
  Bsolo,
  Csolo,
};

///////////////////////////////////////////////////////////////////////////////

class CompositingScene : public ork::Object {
  DeclareConcreteX(CompositingScene, ork::Object);

public:
  CompositingScene();

  const orklut<PoolString, ork::Object*>& GetItems() const { return mItems; }

private:
  orklut<PoolString, ork::Object*> mItems;
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
  virtual void Draw(CompositorDrawData& drawdata, CompositingImpl* pCCI) = 0;
  virtual void CompositeToScreen(ork::lev2::GfxTarget* pT, CompositingImpl* pCCI, CompositingContext& cctx) = 0;
};

///////////////////////////////////////////////////////////////////////////////

class Fx3CompositingTechnique : public CompositingTechnique {
  DeclareConcreteX(Fx3CompositingTechnique, CompositingTechnique);

public:
  Fx3CompositingTechnique();
  ~Fx3CompositingTechnique();

  void CompositeLayerToScreen(lev2::GfxTarget* pT, CompositingContext& cctx, ECOMPOSITEBlend eblend, lev2::RtGroup* psrcgroupA,
                              lev2::RtGroup* psrcgroupB, lev2::RtGroup* psrcgroupC, float levA, float levB, float levC);

  const PoolString& GetGroupA() const { return mGroupA; }
  const PoolString& GetGroupB() const { return mGroupB; }
  const PoolString& GetGroupC() const { return mGroupC; }

  float GetLevelA() const { return mfLevelA; }
  float GetLevelB() const { return mfLevelB; }
  float GetLevelC() const { return mfLevelC; }

  ECOMPOSITEBlend GetBlendMode() const { return meBlendMode; }

  lev2::BuiltinFrameTechniques* mpBuiltinFrameTekA;
  lev2::BuiltinFrameTechniques* mpBuiltinFrameTekB;
  lev2::BuiltinFrameTechniques* mpBuiltinFrameTekC;
  ECOMPOSITEBlend meBlendMode;
  PoolString mGroupA;
  PoolString mGroupB;
  PoolString mGroupC;
  float mfLevelA;
  float mfLevelB;
  float mfLevelC;
  CompositingMaterial mCompositingMaterial;

private:
  void Init(lev2::GfxTarget* pTARG, int w, int h) final;                                                     // virtual
  void Draw(CompositorDrawData& drawdata, CompositingImpl* pCCI) final;                              // virtual
  void CompositeToScreen(ork::lev2::GfxTarget* pT, CompositingImpl* pCCI, CompositingContext& cctx) final; // virtual
};

///////////////////////////////////////////////////////////////////////////////
class CompositingNode : public ork::Object {
  DeclareAbstractX(CompositingNode, ork::Object);

public:
  CompositingNode();
  ~CompositingNode();
  void Init(lev2::GfxTarget* pTARG, int w, int h);
  void Render(CompositorDrawData& drawdata, CompositingImpl* pCCI);
  virtual lev2::RtGroup* GetOutput() const { return nullptr; }

private:
  virtual void DoInit(lev2::GfxTarget* pTARG, int w, int h) = 0;
  virtual void DoRender(CompositorDrawData& drawdata, CompositingImpl* pCCI) = 0;
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
class PassThroughCompositingNode : public CompositingNode {
  DeclareConcreteX(PassThroughCompositingNode, CompositingNode);

public:
  PassThroughCompositingNode();
  ~PassThroughCompositingNode();

private:
  void DoInit(lev2::GfxTarget* pTARG, int w, int h) final;                          // virtual
  void DoRender(CompositorDrawData& drawdata, CompositingImpl* pCCI) final; // virtual

  void _readGroup(ork::rtti::ICastable*& val) const;
  void _writeGroup(ork::rtti::ICastable* const& val);
  lev2::RtGroup* GetOutput() const final;

  CompositingMaterial mCompositingMaterial;
  CompositingGroup* mGroup;
  lev2::BuiltinFrameTechniques* mFTEK;
};
///////////////////////////////////////////////////////////////////////////////
class VrCompositingNode : public CompositingNode {
  DeclareConcreteX(VrCompositingNode, CompositingNode);

public:
  VrCompositingNode();
  ~VrCompositingNode();

private:
  void DoInit(lev2::GfxTarget* pTARG, int w, int h) final;                          // virtual
  void DoRender(CompositorDrawData& drawdata, CompositingImpl* pCCI) final; // virtual

  lev2::RtGroup* GetOutput() const final;

  svar256_t _impl;
};
///////////////////////////////////////////////////////////////////////////////
class SeriesCompositingNode : public CompositingNode {
  DeclareConcreteX(SeriesCompositingNode, CompositingNode);

public:
  SeriesCompositingNode();
  ~SeriesCompositingNode();

private:
  void DoInit(lev2::GfxTarget* pTARG, int w, int h) final;                          // virtual
  void DoRender(CompositorDrawData& drawdata, CompositingImpl* pCCI) final; // virtual

  void GetNode(ork::rtti::ICastable*& val) const;
  void SetNode(ork::rtti::ICastable* const& val);

  lev2::RtGroup* GetOutput() const final;

  CompositingMaterial mCompositingMaterial;
  CompositingNode* mNode;
  lev2::RtGroup* mOutput;
  lev2::BuiltinFrameTechniques* mFTEK;
};
///////////////////////////////////////////////////////////////////////////////
class InsertCompositingNode : public CompositingNode {
  DeclareConcreteX(InsertCompositingNode, CompositingNode);

public:
  InsertCompositingNode();
  ~InsertCompositingNode();

private:
  void DoInit(lev2::GfxTarget* pTARG, int w, int h) final;                          // virtual
  void DoRender(CompositorDrawData& drawdata, CompositingImpl* pCCI) final; // virtual

  void GetNode(ork::rtti::ICastable*& val) const;
  void SetNode(ork::rtti::ICastable* const& val);
  void SetTextureAccessor(ork::rtti::ICastable* const& tex);
  void GetTextureAccessor(ork::rtti::ICastable*& tex) const;

  lev2::RtGroup* GetOutput() const final;

  CompositingMaterial mCompositingMaterial;
  CompositingNode* mNode;
  lev2::RtGroup* mOutput;
  lev2::BuiltinFrameTechniques* mFTEK;
  ork::lev2::TextureAsset* mReturnTexture;
  ork::lev2::TextureAsset* mSendTexture;
  ork::PoolString mDynTexPath;
};
///////////////////////////////////////////////////////////////////////////////
enum EOp2CompositeMode {
  Op2AsumB = 0,
  Op2AmulB,
  Op2AdivB,
  Op2BoverA,
  Op2AoverB,
  Op2Asolo,
  Op2Bsolo,
};
class Op2CompositingNode : public CompositingNode {
  DeclareConcreteX(Op2CompositingNode, CompositingNode);

public:
  Op2CompositingNode();
  ~Op2CompositingNode();

private:
  void DoInit(lev2::GfxTarget* pTARG, int w, int h) override;                          // virtual
  void DoRender(CompositorDrawData& drawdata, CompositingImpl* pCCI) override; // virtual
  void GetNodeA(ork::rtti::ICastable*& val) const;
  void SetNodeA(ork::rtti::ICastable* const& val);
  void GetNodeB(ork::rtti::ICastable*& val) const;
  void SetNodeB(ork::rtti::ICastable* const& val);
  lev2::RtGroup* GetOutput() const override { return mOutput; }

  CompositingNode* mSubA;
  CompositingNode* mSubB;
  CompositingMaterial mCompositingMaterial;
  lev2::RtGroup* mOutput;
  EOp2CompositeMode mMode;
  fvec4 mLevelA;
  fvec4 mLevelB;
  fvec4 mBiasA;
  fvec4 mBiasB;
};
///////////////////////////////////////////////////////////////////////////////
class NodeCompositingTechnique : public CompositingTechnique {
  DeclareConcreteX(NodeCompositingTechnique, CompositingTechnique);

public:
  NodeCompositingTechnique();
  ~NodeCompositingTechnique();

private:
  void Init(lev2::GfxTarget* pTARG, int w, int h) override;                                                     // virtual
  void Draw(CompositorDrawData& drawdata, CompositingImpl* pCCI) override;                              // virtual
  void CompositeToScreen(ork::lev2::GfxTarget* pT, CompositingImpl* pCCI, CompositingContext& cctx) override; // virtual
  //
  void GetRoot(ork::rtti::ICastable*& val) const;
  void SetRoot(ork::rtti::ICastable* const& val);

  ork::ObjectMap mBufferMap;
  CompositingNode* mpRootNode;
  CompositingMaterial mCompositingMaterial;
};
///////////////////////////////////////////////////////////////////////////////

struct CompositingContext {
  int miWidth;
  int miHeight;
  lev2::GfxMaterial3DSolid mUtilMaterial;
  CompositingTechnique* mCTEK;

  CompositingContext();
  ~CompositingContext();
  void Init(lev2::GfxTarget* pTARG);
  void Draw(lev2::GfxTarget* pTARG, CompositorDrawData& drawdata, CompositingImpl* pCCI);
  void CompositeToScreen(ork::lev2::GfxTarget* pT, CompositingImpl* pCCI);
  void Resize(int iW, int iH);
  void SetTechnique(CompositingTechnique* ptek) { mCTEK = ptek; }
};

///////////////////////////////////////////////////////////////////////////

struct CompositingPassData {
  const CompositingGroup* mpGroup;
  lev2::FrameTechniqueBase* mpFrameTek;
  bool mbDrawSource;
  const PoolString* mpCameraName;
  const PoolString* mpLayerName;
  ork::svarp_t _impl;
  ork::fvec4 _clearColor;

  CompositingPassData() : mpGroup(0), mpFrameTek(0), mbDrawSource(true), mpCameraName(0), mpLayerName(0) {
    _impl.Set<void*>(nullptr);
  }
};

///////////////////////////////////////////////////////////////////////////

struct CompositorDrawData {
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

  const orklut<PoolString, ork::Object*>& GetGroups() const { return mCompositingGroups; }
  const orklut<PoolString, ork::Object*>& GetScenes() const { return mScenes; }

  PoolString& GetActiveScene() const { return mActiveScene; }
  PoolString& GetActiveItem() const { return mActiveItem; }

  bool IsEnabled() const { return mbEnable && mToggle; }
  bool IsOutputFramesEnabled() const { return mbOutputFrames; }
  EOutputTimeStep OutputFrameRate() const { return mOutputFrameRate; }

  void Toggle() const { mToggle = !mToggle; }

private:
  orklut<PoolString, ork::Object*> mCompositingGroups;
  orklut<PoolString, ork::Object*> mScenes;
  mutable PoolString mActiveScene;
  mutable PoolString mActiveItem;
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

  void Draw(CompositorDrawData& drawdata);
  void composeToScreen(lev2::GfxTarget* pT);

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

private:
  const CompositingData& _compositingData;


  float mfTimeAccum;
  float mfLastTime;

  CompositingMorphable _morphable;

  std::map<int,prerendercallback_t> _prerendercallbacks;

  int miActiveSceneItem;
  CompositingContext _compcontext;
};

///////////////////////////////////////////////////////////////////////////////

class CompositingGroupEffect : public ork::Object {
  DeclareConcreteX(CompositingGroupEffect, ork::Object);

public:
  ///////////////////////////////////////////////////////
  CompositingGroupEffect();

  EFrameEffect GetFrameEffect() const { return meFrameEffect; }
  float GetEffectAmount() const { return mfEffectAmount; }
  float GetFeedbackAmount() const { return mfFeedbackAmount; }
  float GetFinalRezScale() const { return mfFinalResMult; }
  float GetFxRezScale() const { return mfFxResMult; }
  const char* GetEffectName() const;
  Texture* GetFbUvMap() const;
  bool IsPostFxFeedback() const { return mbPostFxFeedback; }

private:
  void _writeTex(rtti::ICastable* const& tex);
  void _readTex(rtti::ICastable*& tex) const;

  EFrameEffect meFrameEffect;
  TextureAsset* mTexture;
  float mfEffectAmount;
  float mfFeedbackAmount;
  float mfFxResMult;
  float mfFinalResMult;
  bool mbPostFxFeedback;
};

///////////////////////////////////////////////////////////////////////////////

class CompositingGroup : public ork::Object {
  DeclareConcreteX(CompositingGroup, ork::Object);

public:
  CompositingGroup();

  const PoolString& GetCameraName() const { return mCameraName; }
  const PoolString& GetLayers() const { return mLayers; }
  const CompositingGroupEffect& GetEffect() const { return mEffect; }

private:
  PoolString mCameraName;
  PoolString mLayers;
  CompositingGroupEffect mEffect;

  ork::Object* EffectAccessor() { return &mEffect; }
};

///////////////////////////////////////////////////////////////////////////////

class CompositingSceneItem : public ork::Object {
  DeclareConcreteX(CompositingSceneItem, ork::Object);

public:
  CompositingSceneItem();

  CompositingTechnique* GetTechnique() const { return mpTechnique; }

private:
  void GetTech(ork::rtti::ICastable*& val) const;
  void SetTech(ork::rtti::ICastable* const& val);

  CompositingTechnique* mpTechnique;
};

///////////////////////////////////////////////////////////////////////////////
}} // namespace ork::ent
