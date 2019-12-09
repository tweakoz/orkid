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
#include <ork/lev2/gfx/renderer/irendertarget.h>

ImplementReflectionX(ork::lev2::SeriesCompositingNode, "SeriesCompositingNode");

namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////
void SeriesCompositingNode::describeX(class_t*c) {
  ork::reflect::RegisterProperty("Node", &SeriesCompositingNode::GetNode, &SeriesCompositingNode::SetNode);

  auto anno = [&](const char* p, const char* k, const char* v) {
    ork::reflect::annotatePropertyForEditor<SeriesCompositingNode>(p, k, v);
  };

  // anno( "Mode", "editor.class", "ged.factory.enum" );
  anno("Node", "editor.factorylistbase", "PostCompositingNode");
}
///////////////////////////////////////////////////////////////////////////////
SeriesCompositingNode::SeriesCompositingNode() : mFTEK(nullptr), mNode(nullptr), mOutput(nullptr) {}
///////////////////////////////////////////////////////////////////////////////
SeriesCompositingNode::~SeriesCompositingNode() {
  if (mFTEK)
    delete mFTEK;
  if (mOutput)
    delete mOutput;
}
///////////////////////////////////////////////////////////////////////////////
void SeriesCompositingNode::GetNode(ork::rtti::ICastable*& val) const { val = const_cast<PostCompositingNode*>(mNode); }
///////////////////////////////////////////////////////////////////////////////
void SeriesCompositingNode::SetNode(ork::rtti::ICastable* const& val) {
  ork::rtti::ICastable* ptr = val;
  mNode = ((ptr == 0) ? 0 : rtti::safe_downcast<PostCompositingNode*>(ptr));
}
///////////////////////////////////////////////////////////////////////////////
void SeriesCompositingNode::DoInit(lev2::GfxTarget* pTARG, int iW, int iH)
{
  if (nullptr == mOutput) {
    mCompositingMaterial.Init(pTARG);

    _rtg = new lev2::RtGroup(pTARG, iW, iH);
    mOutput = new lev2::RtBuffer(_rtg, lev2::ETGTTYPE_MRT0, lev2::EBUFFMT_RGBA16F, iW, iH);
    mOutput->_debugName = FormatString("SeriesCompositingNode::output");
    _rtg->SetMrt(0,mOutput);

    mFTEK = new lev2::BuiltinFrameTechniques(iW, iH);
    mFTEK->Init(pTARG);

    if (mNode)
      mNode->Init(pTARG, iW, iH);
  }
}
///////////////////////////////////////////////////////////////////////////////
void SeriesCompositingNode::DoRender(CompositorDrawData& drawdata)
{
  lev2::FrameRenderer& the_renderer = drawdata.mFrameRenderer;
  lev2::RenderContextFrameData& framedata = the_renderer.framedata();
  auto target = framedata.GetTarget();
  auto fbi = target->FBI();
  auto gbi = target->GBI();
  int iw = target->GetW();
  int ih = target->GetH();

  if (mNode)
    mNode->Render(drawdata);

  SRect vprect(0, 0, iw - 1, ih - 1);
  SRect quadrect(0, ih - 1, iw - 1, 0);
  if (mOutput && mNode) {
    fbi->SetAutoClear(false);
    fbi->PushRtGroup(_rtg);
    gbi->BeginFrame();

    lev2::Texture* ptex = mNode->GetOutput()->GetTexture();

    mCompositingMaterial.SetTextureA(ptex);
    mCompositingMaterial.SetTechnique("Asolo");

    fbi->GetThisBuffer()->RenderMatOrthoQuad(vprect, quadrect, &mCompositingMaterial, 0.0f, 0.0f, 1.0f, 1.0f, 0, fvec4::White());

    gbi->EndFrame();
    fbi->PopRtGroup();
  }
}
///////////////////////////////////////////////////////////////////////////////
lev2::RtBuffer* SeriesCompositingNode::GetOutput() const {
  lev2::RtBuffer* pRT = mFTEK ? mFTEK->GetFinalRenderTarget()->GetMrt(0) : nullptr;
  return pRT;
}
///////////////////////////////////////////////////////////////////////////////
} //namespace ork::lev2 {
