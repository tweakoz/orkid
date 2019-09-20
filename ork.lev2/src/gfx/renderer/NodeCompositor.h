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
#include <ork/lev2/gfx/renderer/compositor.h>
#include <ork/lev2/gfx/renderer/builtin_frameeffects.h>

namespace ork::lev2 {

  ///////////////////////////////////////////////////////////////////////////////
  class OutputCompositingNode : public ork::Object {
    DeclareAbstractX(OutputCompositingNode, ork::Object);

  public:
    typedef std::function<void()> innerl_t;
    OutputCompositingNode();
    ~OutputCompositingNode();
    void Init(lev2::GfxTarget* pTARG, int w, int h);
    void produce(CompositorDrawData& drawdata, CompositingImpl* pCCI,innerl_t lambda);

  private:
    virtual void DoInit(lev2::GfxTarget* pTARG, int w, int h) = 0;
    virtual void _produce(CompositorDrawData& drawdata,
                         CompositingImpl* pCCI,
                         innerl_t lambda) = 0;
  };
  ///////////////////////////////////////////////////////////////////////////////
  class RenderCompositingNode : public ork::Object {
    DeclareAbstractX(RenderCompositingNode, ork::Object);

  public:
    RenderCompositingNode();
    ~RenderCompositingNode();
    void Init(lev2::GfxTarget* pTARG, int w, int h);
    void Render(CompositorDrawData& drawdata, CompositingImpl* pCCI);
    virtual lev2::RtGroup* GetOutput() const { return nullptr; }

  private:
    virtual void DoInit(lev2::GfxTarget* pTARG, int w, int h) = 0;
    virtual void DoRender(CompositorDrawData& drawdata, CompositingImpl* pCCI) = 0;
  };
  ///////////////////////////////////////////////////////////////////////////////
  class PostCompositingNode : public ork::Object {
    DeclareAbstractX(PostCompositingNode, ork::Object);

  public:
    PostCompositingNode();
    ~PostCompositingNode();
    void Init(lev2::GfxTarget* pTARG, int w, int h);
    void Render(CompositorDrawData& drawdata, CompositingImpl* pCCI);
    virtual lev2::RtGroup* GetOutput() const { return nullptr; }

  private:
    virtual void DoInit(lev2::GfxTarget* pTARG, int w, int h) = 0;
    virtual void DoRender(CompositorDrawData& drawdata, CompositingImpl* pCCI) = 0;
  };
  ///////////////////////////////////////////////////////////////////////////////
  class ChainCompositingNode : public PostCompositingNode {
    DeclareAbstractX(ChainCompositingNode, PostCompositingNode);
  };
  ///////////////////////////////////////////////////////////////////////////////
  class SeriesCompositingNode : public PostCompositingNode {
    DeclareConcreteX(SeriesCompositingNode, PostCompositingNode);

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
    PostCompositingNode* mNode;
    lev2::RtGroup* mOutput;
    lev2::BuiltinFrameTechniques* mFTEK;
  };
  ///////////////////////////////////////////////////////////////////////////////
  class Op2CompositingNode : public PostCompositingNode {
    DeclareConcreteX(Op2CompositingNode, PostCompositingNode);

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

    PostCompositingNode* mSubA;
    PostCompositingNode* mSubB;
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

    void _readRenderNode(ork::rtti::ICastable*& val) const;
    void _writeRenderNode(ork::rtti::ICastable* const& val);
    void _readPostFxNode(ork::rtti::ICastable*& val) const;
    void _writePostFxNode(ork::rtti::ICastable* const& val);
    void _readOutputNode(ork::rtti::ICastable*& val) const;
    void _writeOutputNode(ork::rtti::ICastable* const& val);

  private:
    void Init(lev2::GfxTarget* pTARG, int w, int h) override;                                                     // virtual
    void assemble(CompositorDrawData& drawdata, CompositingImpl* pCCI) override;                              // virtual
    void composite(ork::lev2::GfxTarget* pT, CompositingImpl* pCCI, CompositingContext& cctx) override; // virtual
    //

    ork::ObjectMap mBufferMap;
    RenderCompositingNode* _renderNode;
    PostCompositingNode* _postfxNode;
    OutputCompositingNode* _outputNode;
    CompositingMaterial mCompositingMaterial;
  };
} // namespace ork::lev2 {
