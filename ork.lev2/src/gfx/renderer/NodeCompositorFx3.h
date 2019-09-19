////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/lev2/gfx/renderer/compositor.h>

namespace ork::lev2 {

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
  class Fx3CompositingNode : public PostCompositingNode {
    DeclareConcreteX(Fx3CompositingNode, PostCompositingNode);

  public:
    Fx3CompositingNode();
    ~Fx3CompositingNode();

    void _readGroup(ork::rtti::ICastable*& val) const;
    void _writeGroup(ork::rtti::ICastable* const& val);

    void DoInit(lev2::GfxTarget* pTARG, int w, int h) final;                          // virtual
    void DoRender(CompositorDrawData& drawdata, CompositingImpl* pCCI) final; // virtual

    lev2::RtGroup* GetOutput() const final;

    CompositingMaterial mCompositingMaterial;
    CompositingGroup* mGroup;
    lev2::BuiltinFrameTechniques* mFTEK;
  };

} //namespace ork::lev2 {
