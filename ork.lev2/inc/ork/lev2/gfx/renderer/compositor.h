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

namespace ork { namespace lev2 {

class CompositingGroup;
class CompositingSceneItem;
struct CompositorDrawData;
struct CompositingContext;
struct CompositingImpl;
struct CompositingData;
struct CompositingMorphable;
class BuiltinFrameTechniques;
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
class ChainCompositingNode : public CompositingNode {
  DeclareAbstractX(ChainCompositingNode, CompositingNode);
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

  void _readGroup(ork::rtti::ICastable*& val) const;
  void _writeGroup(ork::rtti::ICastable* const& val);

  void DoInit(lev2::GfxTarget* pTARG, int w, int h) final;                          // virtual
  void DoRender(CompositorDrawData& drawdata, CompositingImpl* pCCI) final; // virtual

  lev2::RtGroup* GetOutput() const final;

  CompositingMaterial mCompositingMaterial;
  CompositingGroup* mGroup;
  lev2::BuiltinFrameTechniques* mFTEK;
};
///////////////////////////////////////////////////////////////////////////////
class VrCompositingNode : public ChainCompositingNode {
  DeclareConcreteX(VrCompositingNode, ChainCompositingNode);

public:
  VrCompositingNode();
  ~VrCompositingNode();
  CompositingNode* _nextNode = nullptr;

private:
  void DoInit(lev2::GfxTarget* pTARG, int w, int h) final;                          // virtual
  void DoRender(CompositorDrawData& drawdata, CompositingImpl* pCCI) final; // virtual
  void _readNext(ork::rtti::ICastable*& val) const;
  void _writeNext(ork::rtti::ICastable* const& val);

  lev2::RtGroup* GetOutput() const final;

  svar256_t _impl;

};
///////////////////////////////////////////////////////////////////////////////
class IdentityCompositingNode : public CompositingNode {
  DeclareConcreteX(IdentityCompositingNode, CompositingNode);

public:
  IdentityCompositingNode();
  ~IdentityCompositingNode();

  PoolString _layername;

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

  void _readFrameNode(ork::rtti::ICastable*& val) const;
  void _writeFrameNode(ork::rtti::ICastable* const& val);
  void _readPostFxNode(ork::rtti::ICastable*& val) const;
  void _writePostFxNode(ork::rtti::ICastable* const& val);
  void _readOutputNode(ork::rtti::ICastable*& val) const;
  void _writeOutputNode(ork::rtti::ICastable* const& val);

private:
  void Init(lev2::GfxTarget* pTARG, int w, int h) override;                                                     // virtual
  void Draw(CompositorDrawData& drawdata, CompositingImpl* pCCI) override;                              // virtual
  void CompositeToScreen(ork::lev2::GfxTarget* pT, CompositingImpl* pCCI, CompositingContext& cctx) override; // virtual
  //

  ork::ObjectMap mBufferMap;
  CompositingNode* _frameNode;
  CompositingNode* _postfxNode;
  ChainCompositingNode* _outputNode;
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

typedef orkstack<lev2::CompositingPassData> compositingpassdatastack_t;

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

  void renderContent(lev2::FrameRenderer& framerenderer);
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

class CompositingGroupEffect : public ork::Object {
  DeclareConcreteX(CompositingGroupEffect, ork::Object);

public:
  ///////////////////////////////////////////////////////
  CompositingGroupEffect();

  EFrameEffect GetFrameEffect() const { return _effectID; }
  float GetEffectAmount() const { return _effectAmount; }
  float GetFeedbackAmount() const { return _feedbackLevel; }
  float GetFinalRezScale() const { return _finalResolution; }
  float GetFxRezScale() const { return _fxResolution; }
  const char* GetEffectName() const;
  Texture* GetFbUvMap() const;
  bool IsPostFxFeedback() const { return _postFxFeedback; }

  EFrameEffect _effectID;
  TextureAsset* _texture;
  float _feedbackLevel;
  float _fxResolution;
  float _finalResolution;
  bool _postFxFeedback;
  float _effectAmount;

  void _writeTex(rtti::ICastable* const& tex);
private:
  void _readTex(rtti::ICastable*& tex) const;

};

///////////////////////////////////////////////////////////////////////////////

struct CompositingGroup : public ork::Object {
  DeclareConcreteX(CompositingGroup, ork::Object);

public:
  CompositingGroup();

  PoolString _cameraName;
  PoolString _layers;
  CompositingGroupEffect _effect;

  ork::Object* _accessEffect() { return &_effect; }
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
}} // namespace ork::ent
