////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include "NodeCompositor.h"
#include <ork/lev2/gfx/proctex/proctex.h>

///////////////////////////////////////////////////////////////////////////////
/// PtxCompositingNode : PostCompositingNode utilizing the procedural texturing
///  dataflow graph for post processing.
///////////////////////////////////////////////////////////////////////////////

namespace ork::lev2 {

struct PtxCompositingNode final : public PostCompositingNode {
  DeclareConcreteX(PtxCompositingNode, PostCompositingNode);

public:
  PtxCompositingNode();
  ~PtxCompositingNode();

  void doGpuInit(lev2::Context* pTARG, int w, int h) final;
  void DoRender(CompositorDrawData& drawdata) final;
  //proctex::ProcTex& getTemplate() const {
    //return _template;
  //}
  int bufferDim() const {
    return _bufferDim;
  }

  void SetTextureAccessor(ork::rtti::ICastable* const& tex);
  void GetTextureAccessor(ork::rtti::ICastable*& tex) const;
  //ork::Object* _accessTemplate() {
    //return &_template;
  //}

  lev2::rtbuffer_ptr_t GetOutput() const final;

  ork::lev2::TextureAsset* mReturnTexture = nullptr;
  ork::lev2::TextureAsset* mSendTexture   = nullptr;
  ork::PoolString mDynTexPath;
  //mutable proctex::ProcTex _template;
  int _bufferDim = 256;

  svar256_t _impl;
};

} // namespace ork::lev2
