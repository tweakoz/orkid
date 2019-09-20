////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include "NodeCompositor.h"
#include <ork/lev2/gfx/gfxmaterial_fx.h>
#include <ork/lev2/gfx/gfxmaterial_test.h>
#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/lev2/gfx/gfxprimitives.h>
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/pch.h>
#include <ork/reflect/RegisterProperty.h>
#include <ork/rtti/downcast.h>
#include <ork/rtti/RTTI.h>
///////////////////////////////////////////////////////////////////////////////
#include <iostream>
#include <ork/asset/DynamicAssetLoader.h>
#include <ork/reflect/DirectObjectPropertyType.hpp>
#include <ork/reflect/enum_serializer.inl>
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/gfx/renderer/builtin_frameeffects.h>

BEGIN_ENUM_SERIALIZER(ork::lev2, EOp2CompositeMode)
DECLARE_ENUM(Op2AsumB)
DECLARE_ENUM(Op2AmulB)
DECLARE_ENUM(Op2AdivB)
DECLARE_ENUM(Op2BoverA)
DECLARE_ENUM(Op2Asolo)
DECLARE_ENUM(Op2Bsolo)
END_ENUM_SERIALIZER()

///////////////////////////////////////////////////////////////////////////////
ImplementReflectionX(ork::lev2::RenderCompositingNode, "RenderCompositingNode");
ImplementReflectionX(ork::lev2::PostCompositingNode, "PostCompositingNode");
ImplementReflectionX(ork::lev2::OutputCompositingNode, "OutputCompositingNode");
ImplementReflectionX(ork::lev2::NodeCompositingTechnique, "NodeCompositingTechnique");
ImplementReflectionX(ork::lev2::SeriesCompositingNode, "SeriesCompositingNode");
ImplementReflectionX(ork::lev2::Op2CompositingNode, "Op2CompositingNode");
ImplementReflectionX(ork::lev2::ChainCompositingNode, "ChainCompositingNode");
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace lev2 {
///////////////////////////////////////////////////////////////////////////////
void ChainCompositingNode::describeX(class_t*c) {}
///////////////////////////////////////////////////////////////////////////////
void NodeCompositingTechnique::describeX(class_t*c) {
  c->accessorProperty("RenderNode", &NodeCompositingTechnique::_readRenderNode, &NodeCompositingTechnique::_writeRenderNode)
   ->annotate<ConstString>("editor.factorylistbase", "RenderCompositingNode");
  c->accessorProperty("PostFxNode", &NodeCompositingTechnique::_readPostFxNode, &NodeCompositingTechnique::_writePostFxNode)
   ->annotate<ConstString>("editor.factorylistbase", "PostCompositingNode");
  c->accessorProperty("OutputNode", &NodeCompositingTechnique::_readOutputNode, &NodeCompositingTechnique::_writeOutputNode)
   ->annotate<ConstString>("editor.factorylistbase", "OutputCompositingNode");
}
///////////////////////////////////////////////////////////////////////////////
NodeCompositingTechnique::NodeCompositingTechnique()
  : _renderNode(nullptr)
  , _postfxNode(nullptr)
  , _outputNode(nullptr) {}
///////////////////////////////////////////////////////////////////////////////
NodeCompositingTechnique::~NodeCompositingTechnique() {
  if (_renderNode)
    delete _renderNode;
  if (_postfxNode)
    delete _postfxNode;
  if (_outputNode)
    delete _outputNode;
}
///////////////////////////////////////////////////////////////////////////////
void NodeCompositingTechnique::_readRenderNode(ork::rtti::ICastable*& val) const {
  auto nonconst = const_cast<RenderCompositingNode*>(_renderNode);
  val = nonconst;
}
///////////////////////////////////////////////////////////////////////////////
void NodeCompositingTechnique::_writeRenderNode(ork::rtti::ICastable* const& val) {
  ork::rtti::ICastable* ptr = val;
  _renderNode = ((ptr == nullptr) ? nullptr : rtti::safe_downcast<RenderCompositingNode*>(ptr));
}
///////////////////////////////////////////////////////////////////////////////
void NodeCompositingTechnique::_readPostFxNode(ork::rtti::ICastable*& val) const {
  auto nonconst = const_cast<PostCompositingNode*>(_postfxNode);
  val = nonconst;
}
///////////////////////////////////////////////////////////////////////////////
void NodeCompositingTechnique::_writePostFxNode(ork::rtti::ICastable* const& val) {
  ork::rtti::ICastable* ptr = val;
  _postfxNode = ((ptr == nullptr) ? nullptr : rtti::safe_downcast<PostCompositingNode*>(ptr));
}
///////////////////////////////////////////////////////////////////////////////
void NodeCompositingTechnique::_readOutputNode(ork::rtti::ICastable*& val) const {
  auto nonconst = const_cast<OutputCompositingNode*>(_outputNode);
  val = nonconst;
}
///////////////////////////////////////////////////////////////////////////////
void NodeCompositingTechnique::_writeOutputNode(ork::rtti::ICastable* const& val) {
  ork::rtti::ICastable* ptr = val;
  _outputNode = ((ptr == nullptr) ? nullptr : rtti::safe_downcast<OutputCompositingNode*>(ptr));
}
///////////////////////////////////////////////////////////////////////////////
void NodeCompositingTechnique::Init(lev2::GfxTarget* pTARG, int w, int h) {
  if (_renderNode)
    _renderNode->Init(pTARG, w, h);
  if (_postfxNode)
    _postfxNode->Init(pTARG, w, h);
  if (_outputNode)
    _outputNode->gpuInit(pTARG, w, h);

  mCompositingMaterial.Init(pTARG);
}
///////////////////////////////////////////////////////////////////////////////
bool NodeCompositingTechnique::assemble(CompositorDrawData& drawdata, CompositingImpl* cimpl) {
  bool rval = false;
  if (_outputNode and _renderNode) {
    _outputNode->beginFrame(drawdata, cimpl);
    rval = true;
    _renderNode->Render(drawdata, cimpl);
    if (_postfxNode)
      _postfxNode->Render(drawdata, cimpl);
  }
  return rval;
}
///////////////////////////////////////////////////////////////////////////////
void NodeCompositingTechnique::composite(CompositorDrawData& drawdata, CompositingImpl* cimpl) {
  if (_outputNode and _renderNode) {
    auto render = _renderNode->GetOutput();
    if (render) {
      _outputNode->endFrame(drawdata, cimpl,render);
    }
  }
}
///////////////////////////////////////////////////////////////////////////////
CompositingBuffer::CompositingBuffer() {}
///////////////////////////////////////////////////////////////////////////////
CompositingBuffer::~CompositingBuffer() {}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void RenderCompositingNode::describeX(class_t*c) {}
RenderCompositingNode::RenderCompositingNode() {}
RenderCompositingNode::~RenderCompositingNode() {}
void RenderCompositingNode::Init(lev2::GfxTarget* pTARG, int w, int h) { DoInit(pTARG, w, h); }
void RenderCompositingNode::Render(CompositorDrawData& drawdata, CompositingImpl* pCCI) { DoRender(drawdata, pCCI); }

void PostCompositingNode::describeX(class_t*c) {}
PostCompositingNode::PostCompositingNode() {}
PostCompositingNode::~PostCompositingNode() {}
void PostCompositingNode::Init(lev2::GfxTarget* pTARG, int w, int h) { DoInit(pTARG, w, h); }
void PostCompositingNode::Render(CompositorDrawData& drawdata, CompositingImpl* pCCI) { DoRender(drawdata, pCCI); }

void OutputCompositingNode::describeX(class_t*c) {}
OutputCompositingNode::OutputCompositingNode() {}
OutputCompositingNode::~OutputCompositingNode() {}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void SeriesCompositingNode::describeX(class_t*c) {
  ork::reflect::RegisterProperty("Node", &SeriesCompositingNode::GetNode, &SeriesCompositingNode::SetNode);

  auto anno = [&](const char* p, const char* k, const char* v) {
    ork::reflect::AnnotatePropertyForEditor<SeriesCompositingNode>(p, k, v);
  };

  // anno( "Mode", "editor.class", "ged.factory.enum" );
  anno("Node", "editor.factorylistbase", "CompositingNode");
}
///////////////////////////////////////////////////////////////////////////////
SeriesCompositingNode::SeriesCompositingNode() : mFTEK(nullptr), mNode(nullptr), mOutput(nullptr) {}
SeriesCompositingNode::~SeriesCompositingNode() {
  if (mFTEK)
    delete mFTEK;
  if (mOutput)
    delete mOutput;
}
void SeriesCompositingNode::GetNode(ork::rtti::ICastable*& val) const { val = const_cast<PostCompositingNode*>(mNode); }
void SeriesCompositingNode::SetNode(ork::rtti::ICastable* const& val) {
  ork::rtti::ICastable* ptr = val;
  mNode = ((ptr == 0) ? 0 : rtti::safe_downcast<PostCompositingNode*>(ptr));
}
void SeriesCompositingNode::DoInit(lev2::GfxTarget* pTARG, int iW, int iH) // virtual
{
  if (nullptr == mOutput) {
    mCompositingMaterial.Init(pTARG);

    mOutput = new lev2::RtGroup(pTARG, iW, iH);
    mOutput->SetMrt(0, new lev2::RtBuffer(mOutput, lev2::ETGTTYPE_MRT0, lev2::EBUFFMT_RGBA64, iW, iH));

    mFTEK = new lev2::BuiltinFrameTechniques(iW, iH);
    mFTEK->Init(pTARG);

    if (mNode)
      mNode->Init(pTARG, iW, iH);
  }
}
void SeriesCompositingNode::DoRender(CompositorDrawData& drawdata, CompositingImpl* pCCI) // virtual
{
  // const ent::CompositingGroup* pCG = mGroup;
  lev2::FrameRenderer& the_renderer = drawdata.mFrameRenderer;
  lev2::RenderContextFrameData& framedata = the_renderer.framedata();
  orkstack<CompositingPassData>& cgSTACK = drawdata.mCompositingGroupStack;
  auto target = framedata.GetTarget();
  auto fbi = target->FBI();
  auto gbi = target->GBI();
  int iw = target->GetW();
  int ih = target->GetH();

  if (mNode)
    mNode->Render(drawdata, pCCI);

  SRect vprect(0, 0, iw - 1, ih - 1);
  SRect quadrect(0, ih - 1, iw - 1, 0);
  if (mOutput && mNode) {
    fbi->SetAutoClear(false);
    fbi->PushRtGroup(mOutput);
    gbi->BeginFrame();

    lev2::Texture* ptex = mNode->GetOutput()->GetMrt(0)->GetTexture();

    mCompositingMaterial.SetTextureA(ptex);
    mCompositingMaterial.SetTechnique("Asolo");

    fbi->GetThisBuffer()->RenderMatOrthoQuad(vprect, quadrect, &mCompositingMaterial, 0.0f, 0.0f, 1.0f, 1.0f, 0, fvec4::White());

    gbi->EndFrame();
    fbi->PopRtGroup();
  }
}
lev2::RtGroup* SeriesCompositingNode::GetOutput() const {
  lev2::RtGroup* pRT = mFTEK ? mFTEK->GetFinalRenderTarget() : nullptr;
  return pRT;
}


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void Op2CompositingNode::describeX(class_t*c) {

  c->memberProperty("Mode", &Op2CompositingNode::mMode)
   ->annotate<ConstString>("editor.class", "ged.factory.enum");
  c->accessorProperty("NodeA", &Op2CompositingNode::GetNodeA, &Op2CompositingNode::SetNodeA)
   ->annotate<ConstString>("editor.factorylistbase", "PostCompositingNode");
  c->accessorProperty("NodeB", &Op2CompositingNode::GetNodeB, &Op2CompositingNode::SetNodeB)
   ->annotate<ConstString>("editor.factorylistbase", "PostCompositingNode");

  c->memberProperty("LevelA", &Op2CompositingNode::mLevelA);
  c->memberProperty("LevelB", &Op2CompositingNode::mLevelB);
  c->memberProperty("BiasA", &Op2CompositingNode::mBiasA);
  c->memberProperty("BiasB", &Op2CompositingNode::mBiasB);


}
void Op2CompositingNode::GetNodeA(ork::rtti::ICastable*& val) const {
  auto nonconst = const_cast<PostCompositingNode*>(mSubA);
  val = nonconst;
}
void Op2CompositingNode::SetNodeA(ork::rtti::ICastable* const& val) {
  ork::rtti::ICastable* ptr = val;
  mSubA = ((ptr == 0) ? 0 : rtti::safe_downcast<PostCompositingNode*>(ptr));
}
void Op2CompositingNode::GetNodeB(ork::rtti::ICastable*& val) const {
  auto nonconst = const_cast<PostCompositingNode*>(mSubB);
  val = nonconst;
}
void Op2CompositingNode::SetNodeB(ork::rtti::ICastable* const& val) {
  ork::rtti::ICastable* ptr = val;
  mSubB = ((ptr == 0) ? 0 : rtti::safe_downcast<PostCompositingNode*>(ptr));
}
///////////////////////////////////////////////////////////////////////////////
Op2CompositingNode::Op2CompositingNode()
    : mSubA(nullptr), mSubB(nullptr), mOutput(nullptr), mMode(Op2AsumB), mLevelA(1.0f, 1.0f, 1.0f, 1.0f),
      mLevelB(1.0f, 1.0f, 1.0f, 1.0f), mBiasA(0.0f, 0.0f, 0.0f, 0.0f), mBiasB(0.0f, 0.0f, 0.0f, 0.0f) {}
///////////////////////////////////////////////////////////////////////////////
Op2CompositingNode::~Op2CompositingNode() {
  if (mSubA)
    delete mSubA;
  if (mSubB)
    delete mSubB;
  if (mOutput)
    delete mOutput;
}
///////////////////////////////////////////////////////////////////////////////
void Op2CompositingNode::DoInit(lev2::GfxTarget* pTARG, int iW, int iH) // virtual
{
  if (mSubA)
    mSubA->Init(pTARG, iW, iH);
  if (mSubB)
    mSubB->Init(pTARG, iW, iH);

  if (nullptr == mOutput) {
    mCompositingMaterial.Init(pTARG);

    mOutput = new lev2::RtGroup(pTARG, iW, iH);
    mOutput->SetMrt(0, new lev2::RtBuffer(mOutput, lev2::ETGTTYPE_MRT0, lev2::EBUFFMT_RGBA64, iW, iH));
  }
}
///////////////////////////////////////////////////////////////////////////////
void Op2CompositingNode::DoRender(CompositorDrawData& drawdata, CompositingImpl* pCCI) // virtual
{
  auto& the_renderer = drawdata.mFrameRenderer;
  auto& framedata = the_renderer.framedata();
  auto& cgSTACK = drawdata.mCompositingGroupStack;
  auto target = framedata.GetTarget();
  auto fbi = target->FBI();
  auto gbi = target->GBI();
  int iw = target->GetW();
  int ih = target->GetH();

  if (mSubA) {
    mSubA->Render(drawdata, pCCI);
  }
  if (mSubB) {
    mSubB->Render(drawdata, pCCI);
  }

  fbi->SetAutoClear(false);
  fbi->PushRtGroup(mOutput);
  gbi->BeginFrame();

  SRect vprect(0, 0, iw - 1, ih - 1);
  SRect quadrect(0, ih - 1, iw - 1, 0);
  if (mOutput && mSubA && mSubB) {
    lev2::Texture* ptexa = mSubA->GetOutput()->GetMrt(0)->GetTexture();
    lev2::Texture* ptexb = mSubB->GetOutput()->GetMrt(0)->GetTexture();
    mCompositingMaterial.SetTextureA(ptexa);
    mCompositingMaterial.SetTextureB(ptexb);
    mCompositingMaterial.SetBiasA(mBiasA);
    mCompositingMaterial.SetBiasB(mBiasB);
    mCompositingMaterial.SetLevelA(mLevelA);
    mCompositingMaterial.SetLevelB(mLevelB);
    mCompositingMaterial.SetLevelC(fvec4(0.0f, 0.0f, 0.0f, 0.0f));

    switch (mMode) {
      case Op2AsumB:
        mCompositingMaterial.SetTechnique("AplusBplusC");
        break;
      case Op2AmulB:
        mCompositingMaterial.SetTechnique("Op2AmulB");
        break;
      case Op2AdivB:
        mCompositingMaterial.SetTechnique("Op2AdivB");
        break;
      case Op2AoverB:
        mCompositingMaterial.SetTechnique("AoverBplusC");
        break;
      case Op2BoverA:
        mCompositingMaterial.SetTechnique("BoverAplusC");
        break;
      case Op2Asolo:
        mCompositingMaterial.SetTechnique("Asolo");
        break;
      case Op2Bsolo:
        mCompositingMaterial.SetTechnique("Bsolo");
        break;
    }

    fbi->GetThisBuffer()->RenderMatOrthoQuad(vprect, quadrect, &mCompositingMaterial, 0.0f, 0.0f, 1.0f, 1.0f, 0, fvec4::White());
  }

  gbi->EndFrame();
  fbi->PopRtGroup();
}
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
}} // namespace ork::lev2
///////////////////////////////////////////////////////////////////////////////
