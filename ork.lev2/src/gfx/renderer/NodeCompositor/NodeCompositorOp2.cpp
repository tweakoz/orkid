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

ImplementReflectionX(ork::lev2::Op2CompositingNode, "Op2CompositingNode");

namespace ork::lev2 {
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
///////////////////////////////////////////////////////////////////////////////
void Op2CompositingNode::GetNodeA(ork::rtti::ICastable*& val) const {
  auto nonconst = const_cast<PostCompositingNode*>(mSubA);
  val = nonconst;
}
///////////////////////////////////////////////////////////////////////////////
void Op2CompositingNode::SetNodeA(ork::rtti::ICastable* const& val) {
  ork::rtti::ICastable* ptr = val;
  mSubA = ((ptr == 0) ? 0 : rtti::safe_downcast<PostCompositingNode*>(ptr));
}
///////////////////////////////////////////////////////////////////////////////
void Op2CompositingNode::GetNodeB(ork::rtti::ICastable*& val) const {
  auto nonconst = const_cast<PostCompositingNode*>(mSubB);
  val = nonconst;
}
///////////////////////////////////////////////////////////////////////////////
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

    _rtg = new lev2::RtGroup(pTARG, iW, iH);
    mOutput = new lev2::RtBuffer(_rtg, lev2::ETGTTYPE_MRT0, lev2::EBUFFMT_RGBA64, iW, iH);
    mOutput->_debugName = FormatString("Op2CompositingNode::output");
    _rtg->SetMrt(0,mOutput);
  }
}
///////////////////////////////////////////////////////////////////////////////
void Op2CompositingNode::DoRender(CompositorDrawData& drawdata) // virtual
{
  auto& the_renderer = drawdata.mFrameRenderer;
  auto& framedata = the_renderer.framedata();
  auto target = framedata.GetTarget();
  auto fbi = target->FBI();
  auto gbi = target->GBI();
  int iw = target->GetW();
  int ih = target->GetH();

  if (mSubA) {
    mSubA->Render(drawdata);
  }
  if (mSubB) {
    mSubB->Render(drawdata);
  }

  fbi->SetAutoClear(false);
  fbi->PushRtGroup(_rtg);
  gbi->BeginFrame();

  SRect vprect(0, 0, iw - 1, ih - 1);
  SRect quadrect(0, ih - 1, iw - 1, 0);
  if (mOutput && mSubA && mSubB) {
    lev2::Texture* ptexa = mSubA->GetOutput()->GetTexture();
    lev2::Texture* ptexb = mSubB->GetOutput()->GetTexture();
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
} //namespace ork::lev2 {
