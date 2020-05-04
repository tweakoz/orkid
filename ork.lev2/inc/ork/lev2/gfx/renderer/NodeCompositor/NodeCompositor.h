////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/dataflow/dataflow.h>
#include <ork/lev2/gfx/renderer/renderer.h>
#include <ork/lev2/gfx/renderer/rendercontext.h>
#include <ork/lev2/gfx/gfxmaterial_test.h>
#include <ork/lev2/gfx/renderer/frametek.h>
#include <ork/lev2/gfx/renderer/compositor.h>
#include <ork/lev2/gfx/renderer/compositormaterial.h>
#include <ork/lev2/gfx/renderer/builtin_frameeffects.h>

namespace ork::lev2 {

///////////////////////////////////////////////////////////////////////////////
class OutputCompositingNode : public ork::Object {
  DeclareAbstractX(OutputCompositingNode, ork::Object);

public:
  OutputCompositingNode();
  ~OutputCompositingNode();
  virtual void gpuInit(lev2::Context* pTARG, int w, int h) {
  }
  virtual void beginAssemble(CompositorDrawData& drawdata) {
  }
  virtual void endAssemble(CompositorDrawData& drawdata) {
  }
  virtual void composite(CompositorDrawData& drawdata) {
  }
};
using outputcompositingnode_ptr_t      = std::shared_ptr<OutputCompositingNode>;
using outputcompositingnode_constptr_t = std::shared_ptr<const OutputCompositingNode>;

///////////////////////////////////////////////////////////////////////////////
class RenderCompositingNode : public ork::Object {
  DeclareAbstractX(RenderCompositingNode, ork::Object);

public:
  RenderCompositingNode();
  ~RenderCompositingNode();
  void gpuInit(lev2::Context* pTARG, int w, int h);
  void Render(CompositorDrawData& drawdata);
  virtual lev2::RtBuffer* GetOutput() const {
    return nullptr;
  }
  virtual lev2::RtGroup* GetOutputGroup() const {
    return nullptr;
  }

private:
  virtual void doGpuInit(lev2::Context* pTARG, int w, int h) = 0;
  virtual void DoRender(CompositorDrawData& drawdata)        = 0;
};
///////////////////////////////////////////////////////////////////////////////
class PostCompositingNode : public ork::Object {
  DeclareAbstractX(PostCompositingNode, ork::Object);

public:
  PostCompositingNode();
  ~PostCompositingNode();
  void gpuInit(lev2::Context* pTARG, int w, int h);
  void Render(CompositorDrawData& drawdata);
  virtual lev2::RtBuffer* GetOutput() const {
    return nullptr;
  }

private:
  virtual void doGpuInit(lev2::Context* pTARG, int w, int h) = 0;
  virtual void DoRender(CompositorDrawData& drawdata)        = 0;
};
///////////////////////////////////////////////////////////////////////////////
class ChainCompositingNode : public PostCompositingNode {
  DeclareAbstractX(ChainCompositingNode, PostCompositingNode);
};
///////////////////////////////////////////////////////////////////////////////
class Op2CompositingNode : public PostCompositingNode {
  DeclareConcreteX(Op2CompositingNode, PostCompositingNode);

public:
  Op2CompositingNode();
  ~Op2CompositingNode();

private:
  void doGpuInit(lev2::Context* pTARG, int w, int h) override; // virtual
  void DoRender(CompositorDrawData& drawdata) override;        // virtual
  void GetNodeA(ork::rtti::ICastable*& val) const;
  void SetNodeA(ork::rtti::ICastable* const& val);
  void GetNodeB(ork::rtti::ICastable*& val) const;
  void SetNodeB(ork::rtti::ICastable* const& val);
  lev2::RtBuffer* GetOutput() const override {
    return mOutput;
  }

  PostCompositingNode* mSubA = nullptr;
  PostCompositingNode* mSubB = nullptr;
  CompositingMaterial mCompositingMaterial;
  lev2::RtBuffer* mOutput = nullptr;
  lev2::RtGroup* _rtg     = nullptr;
  EOp2CompositeMode mMode;
  fvec4 mLevelA;
  fvec4 mLevelB;
  fvec4 mBiasA;
  fvec4 mBiasB;
};
///////////////////////////////////////////////////////////////////////////////
class NodeCompositingTechnique final : public CompositingTechnique {
  DeclareConcreteX(NodeCompositingTechnique, CompositingTechnique);

public:
  NodeCompositingTechnique();
  ~NodeCompositingTechnique();

  template <typename T> T* createRenderNode() {
    auto rval = new T;
    _writeRenderNode(rval);
    return rval;
  }
  template <typename T> T* createPostFxNode() {
    auto rval = new T;
    _writePostFxNode(rval);
    return rval;
  }
  template <typename T> T* createOutputNode() {
    auto rval = new T;
    _writeOutputNode(rval);
    return rval;
  }

  void _readRenderNode(ork::rtti::ICastable*& val) const;
  void _writeRenderNode(ork::rtti::ICastable* const& val);
  void _readPostFxNode(ork::rtti::ICastable*& val) const;
  void _writePostFxNode(ork::rtti::ICastable* const& val);
  void _readOutputNode(ork::rtti::ICastable*& val) const;
  void _writeOutputNode(ork::rtti::ICastable* const& val);

  void gpuInit(lev2::Context* context, int w, int h) override;
  bool assemble(CompositorDrawData& drawdata) override;
  void composite(CompositorDrawData& drawdata) override;
  //

  template <typename T> T* tryRenderNodeAs() {
    return dynamic_cast<T*>(_renderNode);
  }
  template <typename T> T* tryOutputNodeAs() {
    return dynamic_cast<T*>(_outputNode);
  }

  ork::ObjectMap mBufferMap;
  RenderCompositingNode* _renderNode;
  PostCompositingNode* _postfxNode;
  OutputCompositingNode* _outputNode;
  // CompositingMaterial mCompositingMaterial;
};
} // namespace ork::lev2
