////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/lev2/gfx/renderer/NodeCompositor/NodeCompositor.h>
#include <ork/lev2/gfx/gfxmaterial_test.h>
#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/lev2/gfx/gfxprimitives.h>
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/pch.h>
#include <ork/reflect/properties/registerX.inl>
#include <ork/rtti/downcast.h>
#include <ork/rtti/RTTI.h>
///////////////////////////////////////////////////////////////////////////////
#include <iostream>
#include <ork/asset/DynamicAssetLoader.h>
#include <ork/reflect/properties/DirectTyped.hpp>
#include <ork/reflect/enum_serializer.inl>
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/gfx/renderer/builtin_frameeffects.h>

ImplementReflectionX(ork::lev2::Op2CompositingNode, "Op2CompositingNode");

namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////
BeginEnumRegistration(Op2CompositeMode);
RegisterEnum(Op2CompositeMode, Op2AsumB);
RegisterEnum(Op2CompositeMode, Op2AmulB);
RegisterEnum(Op2CompositeMode, Op2AdivB);
RegisterEnum(Op2CompositeMode, Op2BoverA);
RegisterEnum(Op2CompositeMode, Op2Asolo);
RegisterEnum(Op2CompositeMode, Op2Bsolo);
EndEnumRegistration();
///////////////////////////////////////////////////////////////////////////////
void Op2CompositingNode::describeX(class_t* c) {
  /*
    c->directProperty("Mode", &Op2CompositingNode::mMode)->annotate<ConstString>("editor.class", "ged.factory.enum");
    c->accessorProperty("NodeA", &Op2CompositingNode::GetNodeA, &Op2CompositingNode::SetNodeA)
        ->annotate<ConstString>("editor.factorylistbase", "PostCompositingNode");
    c->accessorProperty("NodeB", &Op2CompositingNode::GetNodeB, &Op2CompositingNode::SetNodeB)
        ->annotate<ConstString>("editor.factorylistbase", "PostCompositingNode");

    c->directProperty("LevelA", &Op2CompositingNode::mLevelA);
    c->directProperty("LevelB", &Op2CompositingNode::mLevelB);
    c->directProperty("BiasA", &Op2CompositingNode::mBiasA);
    c->directProperty("BiasB", &Op2CompositingNode::mBiasB);
    */
}
///////////////////////////////////////////////////////////////////////////////
void Op2CompositingNode::GetNodeA(ork::rtti::ICastable*& val) const {
  auto nonconst = const_cast<PostCompositingNode*>(mSubA);
  val           = nonconst;
}
///////////////////////////////////////////////////////////////////////////////
void Op2CompositingNode::SetNodeA(ork::rtti::ICastable* const& val) {
  ork::rtti::ICastable* ptr = val;
  mSubA                     = ((ptr == 0) ? 0 : rtti::safe_downcast<PostCompositingNode*>(ptr));
}
///////////////////////////////////////////////////////////////////////////////
void Op2CompositingNode::GetNodeB(ork::rtti::ICastable*& val) const {
  auto nonconst = const_cast<PostCompositingNode*>(mSubB);
  val           = nonconst;
}
///////////////////////////////////////////////////////////////////////////////
void Op2CompositingNode::SetNodeB(ork::rtti::ICastable* const& val) {
  ork::rtti::ICastable* ptr = val;
  mSubB                     = ((ptr == 0) ? 0 : rtti::safe_downcast<PostCompositingNode*>(ptr));
}
///////////////////////////////////////////////////////////////////////////////
Op2CompositingNode::Op2CompositingNode()
    : mSubA(nullptr)
    , mSubB(nullptr)
    , mMode(Op2CompositeMode::Op2AsumB)
    , mLevelA(1.0f, 1.0f, 1.0f, 1.0f)
    , mLevelB(1.0f, 1.0f, 1.0f, 1.0f)
    , mBiasA(0.0f, 0.0f, 0.0f, 0.0f)
    , mBiasB(0.0f, 0.0f, 0.0f, 0.0f) {
}
///////////////////////////////////////////////////////////////////////////////
Op2CompositingNode::~Op2CompositingNode() {
  if (mSubA)
    delete mSubA;
  if (mSubB)
    delete mSubB;
}
///////////////////////////////////////////////////////////////////////////////
void Op2CompositingNode::doGpuInit(lev2::Context* pTARG, int iW, int iH) // virtual
{
  if (mSubA)
    mSubA->gpuInit(pTARG, iW, iH);
  if (mSubB)
    mSubB->gpuInit(pTARG, iW, iH);

  if (nullptr == _output) {
    mCompositingMaterial.gpuInit(pTARG);

    _rtg                = std::make_shared<lev2::RtGroup>(pTARG, iW, iH);
    _output             = _rtg->createRenderTarget(lev2::EBufferFormat::RGBA16F);
    _output->_debugName = FormatString("Op2CompositingNode::output");
  }
}
///////////////////////////////////////////////////////////////////////////////
void Op2CompositingNode::DoRender(CompositorDrawData& drawdata) // virtual
{
  auto& the_renderer = drawdata.mFrameRenderer;
  auto& framedata    = the_renderer.framedata();
  auto target        = framedata.GetTarget();
  auto fbi           = target->FBI();
  auto gbi           = target->GBI();
  int iw             = target->mainSurfaceWidth();
  int ih             = target->mainSurfaceHeight();

  if (mSubA) {
    mSubA->Render(drawdata);
  }
  if (mSubB) {
    mSubB->Render(drawdata);
  }

  fbi->SetAutoClear(false);
  fbi->PushRtGroup(_rtg.get());
  gbi->BeginFrame();

  SRect vprect(0, 0, iw - 1, ih - 1);
  SRect quadrect(0, ih - 1, iw - 1, 0);
  if (_output && mSubA && mSubB) {
    lev2::Texture* ptexa = mSubA->GetOutput()->texture();
    lev2::Texture* ptexb = mSubB->GetOutput()->texture();
    mCompositingMaterial.SetTextureA(ptexa);
    mCompositingMaterial.SetTextureB(ptexb);
    mCompositingMaterial.SetBiasA(mBiasA);
    mCompositingMaterial.SetBiasB(mBiasB);
    mCompositingMaterial.SetLevelA(mLevelA);
    mCompositingMaterial.SetLevelB(mLevelB);
    mCompositingMaterial.SetLevelC(fvec4(0.0f, 0.0f, 0.0f, 0.0f));

    switch (mMode) {
      case Op2CompositeMode::Op2AsumB:
        mCompositingMaterial.SetTechnique("AplusBplusC");
        break;
      case Op2CompositeMode::Op2AmulB:
        mCompositingMaterial.SetTechnique("Op2AmulB");
        break;
      case Op2CompositeMode::Op2AdivB:
        mCompositingMaterial.SetTechnique("Op2AdivB");
        break;
      case Op2CompositeMode::Op2AoverB:
        mCompositingMaterial.SetTechnique("AoverBplusC");
        break;
      case Op2CompositeMode::Op2BoverA:
        mCompositingMaterial.SetTechnique("BoverAplusC");
        break;
      case Op2CompositeMode::Op2Asolo:
        mCompositingMaterial.SetTechnique("Asolo");
        break;
      case Op2CompositeMode::Op2Bsolo:
        mCompositingMaterial.SetTechnique("Bsolo");
        break;
    }

    fbi->GetThisBuffer()->RenderMatOrthoQuad(vprect, quadrect, &mCompositingMaterial, 0.0f, 0.0f, 1.0f, 1.0f, 0, fvec4::White());
  }

  gbi->EndFrame();
  fbi->PopRtGroup();
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
