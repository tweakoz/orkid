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
#include <ork/lev2/gfx/renderer/compositormaterial.h>
#include <ork/lev2/gfx/renderer/builtin_frameeffects.h>

namespace ork::lev2 {

  ///////////////////////////////////////////////////////////////////////////////
  class OutputCompositingNode : public ork::Object {
    DeclareAbstractX(OutputCompositingNode, ork::Object);

  public:
    OutputCompositingNode();
    ~OutputCompositingNode();
    virtual void gpuInit(lev2::Context* pTARG, int w, int h) {}
    virtual void beginAssemble(CompositorDrawData& drawdata) {}
    virtual void endAssemble(CompositorDrawData& drawdata) {}
    virtual void composite(CompositorDrawData& drawdata) {}
  };
  ///////////////////////////////////////////////////////////////////////////////
  class RenderCompositingNode : public ork::Object {
    DeclareAbstractX(RenderCompositingNode, ork::Object);

  public:
    RenderCompositingNode();
    ~RenderCompositingNode();
    void Init(lev2::Context* pTARG, int w, int h);
    void Render(CompositorDrawData& drawdata);
    virtual lev2::RtBuffer* GetOutput() const { return nullptr; }

  private:
    virtual void DoInit(lev2::Context* pTARG, int w, int h) = 0;
    virtual void DoRender(CompositorDrawData& drawdata) = 0;
  };
  ///////////////////////////////////////////////////////////////////////////////
  class PostCompositingNode : public ork::Object {
    DeclareAbstractX(PostCompositingNode, ork::Object);

  public:
    PostCompositingNode();
    ~PostCompositingNode();
    void Init(lev2::Context* pTARG, int w, int h);
    void Render(CompositorDrawData& drawdata);
    virtual lev2::RtBuffer* GetOutput() const { return nullptr; }

  private:
    virtual void DoInit(lev2::Context* pTARG, int w, int h) = 0;
    virtual void DoRender(CompositorDrawData& drawdata) = 0;
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
    void DoInit(lev2::Context* pTARG, int w, int h) final;                          // virtual
    void DoRender(CompositorDrawData& drawdata) final; // virtual

    void GetNode(ork::rtti::ICastable*& val) const;
    void SetNode(ork::rtti::ICastable* const& val);

    lev2::RtBuffer* GetOutput() const final;

    CompositingMaterial mCompositingMaterial;
    PostCompositingNode* mNode = nullptr;
    lev2::RtBuffer* mOutput = nullptr;
    lev2::RtGroup* _rtg = nullptr;
    lev2::BuiltinFrameTechniques* mFTEK;
  };
  ///////////////////////////////////////////////////////////////////////////////
  class Op2CompositingNode : public PostCompositingNode {
    DeclareConcreteX(Op2CompositingNode, PostCompositingNode);

  public:
    Op2CompositingNode();
    ~Op2CompositingNode();

  private:
    void DoInit(lev2::Context* pTARG, int w, int h) override;                          // virtual
    void DoRender(CompositorDrawData& drawdata) override; // virtual
    void GetNodeA(ork::rtti::ICastable*& val) const;
    void SetNodeA(ork::rtti::ICastable* const& val);
    void GetNodeB(ork::rtti::ICastable*& val) const;
    void SetNodeB(ork::rtti::ICastable* const& val);
    lev2::RtBuffer* GetOutput() const override { return mOutput; }

    PostCompositingNode* mSubA = nullptr;
    PostCompositingNode* mSubB = nullptr;
    CompositingMaterial mCompositingMaterial;
    lev2::RtBuffer* mOutput = nullptr;
    lev2::RtGroup* _rtg = nullptr;
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
    void Init(lev2::Context* pTARG, int w, int h) override;
    bool assemble(CompositorDrawData& drawdata) override;
    void composite(CompositorDrawData& drawdata) override;
    //

    ork::ObjectMap mBufferMap;
    RenderCompositingNode* _renderNode;
    PostCompositingNode* _postfxNode;
    OutputCompositingNode* _outputNode;
    CompositingMaterial mCompositingMaterial;
  };
} // namespace ork::lev2 {
