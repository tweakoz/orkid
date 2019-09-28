////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include "NodeCompositor.h"
#include <ork/lev2/gfx/proctex/proctex.h>

namespace ork::lev2 {

struct PtxCompositingNode : public PostCompositingNode {
  DeclareConcreteX(PtxCompositingNode, PostCompositingNode);

public:
  PtxCompositingNode();
  ~PtxCompositingNode() final;

  void DoInit(lev2::GfxTarget* pTARG, int w, int h) final;
  void DoRender(CompositorDrawData& drawdata) final;
  proctex::ProcTex& getTemplate() const { return _template; }
  int bufferDim() const { return _bufferDim; }

  void SetTextureAccessor(ork::rtti::ICastable* const& tex);
  void GetTextureAccessor(ork::rtti::ICastable*& tex) const;
  ork::Object* _accessTemplate() { return &_template; }

  lev2::RtGroup* GetOutput() const final;

  ork::lev2::TextureAsset* mReturnTexture = nullptr;
  ork::lev2::TextureAsset* mSendTexture   = nullptr;
  ork::PoolString mDynTexPath;
  mutable proctex::ProcTex _template;
  int _bufferDim = 256;

  svar256_t _impl;
};

} // namespace ork::lev2
