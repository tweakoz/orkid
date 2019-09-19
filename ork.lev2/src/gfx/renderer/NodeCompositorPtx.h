////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include "NodeCompositor.h"

namespace ork::lev2 {

  class PtxCompositingNode : public PostCompositingNode {
    DeclareConcreteX(PtxCompositingNode, PostCompositingNode);

  public:
    PtxCompositingNode();
    ~PtxCompositingNode();

  private:
    void DoInit(lev2::GfxTarget* pTARG, int w, int h) final;                          // virtual
    void DoRender(CompositorDrawData& drawdata, CompositingImpl* pCCI) final; // virtual

    void GetNode(ork::rtti::ICastable*& val) const;
    void SetNode(ork::rtti::ICastable* const& val);
    void SetTextureAccessor(ork::rtti::ICastable* const& tex);
    void GetTextureAccessor(ork::rtti::ICastable*& tex) const;

    lev2::RtGroup* GetOutput() const final;

    CompositingMaterial mCompositingMaterial;
    PostCompositingNode* mNode;
    lev2::RtGroup* mOutput;
    lev2::BuiltinFrameTechniques* mFTEK;
    ork::lev2::TextureAsset* mReturnTexture;
    ork::lev2::TextureAsset* mSendTexture;
    ork::PoolString mDynTexPath;
  };

} //namespace ork::lev2 {
