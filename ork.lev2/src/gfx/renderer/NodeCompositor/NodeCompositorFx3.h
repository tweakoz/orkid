////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include "NodeCompositor.h"

namespace ork::lev2 {

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

  class Fx3CompositingTechnique : public CompositingTechnique {
    DeclareConcreteX(Fx3CompositingTechnique, CompositingTechnique);

  public:
    Fx3CompositingTechnique();
    ~Fx3CompositingTechnique();

    void CompositeLayerToScreen(lev2::GfxTarget* pT, ECOMPOSITEBlend eblend, lev2::RtGroup* psrcgroupA,
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
    void Init(lev2::GfxTarget* pTARG, int w, int h) final;
    bool assemble(CompositorDrawData& drawdata) final;
    void composite(CompositorDrawData& drawdata) final;
  };
  ///////////////////////////////////////////////////////////////////////////////
  class Fx3CompositingNode : public RenderCompositingNode {
    DeclareConcreteX(Fx3CompositingNode, RenderCompositingNode);

  public:
    Fx3CompositingNode();
    ~Fx3CompositingNode();

    void _readGroup(ork::rtti::ICastable*& val) const;
    void _writeGroup(ork::rtti::ICastable* const& val);

    void DoInit(lev2::GfxTarget* pTARG, int w, int h) final;                          // virtual
    void DoRender(CompositorDrawData& drawdata) final; // virtual

    lev2::RtBuffer* GetOutput() const final;

    CompositingMaterial mCompositingMaterial;
    CompositingGroup* mGroup;
    lev2::BuiltinFrameTechniques* mFTEK;
  };

} //namespace ork::lev2 {
