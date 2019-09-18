////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/lev2/gfx/compositor.h>
#include <ork/lev2/gfx/gfxmaterial_fx.h>
#include <ork/lev2/gfx/gfxmaterial_test.h>
#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/lev2/gfx/gfxprimitives.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/lev2/lev2_asset.h>
#include <ork/pch.h>
#include <ork/reflect/RegisterProperty.h>
#include <ork/rtti/downcast.h>
///////////////////////////////////////////////////////////////////////////////
#include <ork/reflect/DirectObjectPropertyType.hpp>
#include <ork/reflect/enum_serializer.h>
///////////////////////////////////////////////////////////////////////////////
ImplementReflectionX(ork::lev2::CompositingGroup, "CompositingGroup");
ImplementReflectionX(ork::lev2::CompositingScene, "CompositingScene");
ImplementReflectionX(ork::lev2::CompositingSceneItem, "CompositingSceneItem");
ImplementReflectionX(ork::lev2::CompositingGroupEffect, "CompositingGroupEffect");
INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::CompositingTechnique, "CompositingTechnique");
///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace lev2 {
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void CompositingTechnique::Describe() {}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
CompositingContext::CompositingContext()
    : miWidth(0)
    , miHeight(0)
    , mCTEK(nullptr) {}

///////////////////////////////////////////////////////////////////////////////

CompositingContext::~CompositingContext() {}

///////////////////////////////////////////////////////////////////////////////

void CompositingContext::Init(lev2::GfxTarget* pTARG) {
  if ((miWidth != pTARG->GetW()) || (miHeight != pTARG->GetH())) {
    miWidth  = pTARG->GetW();
    miHeight = pTARG->GetH();
  }
  mUtilMaterial.Init(pTARG);
}

///////////////////////////////////////////////////////////////////////////////

void CompositingContext::Resize(int iW, int iH) {
  miWidth  = iW;
  miHeight = iH;
}

///////////////////////////////////////////////////////////////////////////////

void CompositingContext::Draw(lev2::GfxTarget* pTARG, CompositorDrawData& drawdata, CompositingImpl* pCCI) {
  Init(pTARG); // fixme lazy init
  if (mCTEK) {
    mCTEK->Init(pTARG, miWidth, miHeight);
    mCTEK->Draw(drawdata, pCCI);
  }
}

///////////////////////////////////////////////////////////////////////////////

void CompositingContext::CompositeToScreen(ork::lev2::GfxTarget* pT, CompositingImpl* pCCI) {
  Init(pT);
  if (mCTEK)
    mCTEK->CompositeToScreen(pT, pCCI, *this);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void CompositingScene::describeX(class_t* c) {
  ork::reflect::RegisterMapProperty("Items", &CompositingScene::mItems);
  ork::reflect::AnnotatePropertyForEditor<CompositingScene>("Items", "editor.factorylistbase", "CompositingSceneItem");
}

///////////////////////////////////////////////////////////////////////////////

CompositingScene::CompositingScene() {}

///////////////////////////////////////////////////////////////////////////////

void CompositingSceneItem::describeX(class_t* c) {

  ork::reflect::RegisterProperty("Technique", &CompositingSceneItem::GetTech, &CompositingSceneItem::SetTech);
  ork::reflect::AnnotatePropertyForEditor<CompositingSceneItem>("Technique", "editor.factorylistbase", "CompositingTechnique");
}

///////////////////////////////////////////////////////////////////////////////

CompositingSceneItem::CompositingSceneItem()
    : mpTechnique(nullptr) {}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void CompositingGroupEffect::describeX(class_t* c) {
  c->memberProperty("Type", &CompositingGroupEffect::meFrameEffect)->annotate<ConstString>("editor.class", "ged.factory.enum");

  c->accessorProperty("FbUvTexture", &CompositingGroupEffect::_readTex, &CompositingGroupEffect::_writeTex)
      ->annotate<ConstString>("editor.class", "ged.factory.assetlist")
      ->annotate<ConstString>("editor.assettype", "lev2tex")
      ->annotate<ConstString>("editor.assetclass", "lev2tex");

  c->floatProperty("FinalRezScale", float_range{0.025, 1.0}, &CompositingGroupEffect::mfFinalResMult);
  c->floatProperty("FxRezScale", float_range{0.025, 1.0}, &CompositingGroupEffect::mfFxResMult);

  c->memberProperty("Amount", &CompositingGroupEffect::mfEffectAmount);
  c->memberProperty("FeedbackAmount", &CompositingGroupEffect::mfFeedbackAmount);
  c->memberProperty("PostFxFeedback", &CompositingGroupEffect::mbPostFxFeedback);
}

///////////////////////////////////////////////////////////////////////////////

CompositingGroupEffect::CompositingGroupEffect()
    : meFrameEffect(lev2::EFRAMEFX_NONE)
    , mfEffectAmount(0.0f)
    , mfFeedbackAmount(0.0f)
    , mfFinalResMult(0.5f)
    , mfFxResMult(0.5f)
    , mTexture(0)
    , mbPostFxFeedback(false) {}

///////////////////////////////////////////////////////////////////////////////

void CompositingGroupEffect::_writeTex(rtti::ICastable* const& tex) { mTexture = tex ? rtti::autocast(tex) : nullptr; }

///////////////////////////////////////////////////////////////////////////////

void CompositingGroupEffect::_readTex(rtti::ICastable*& tex) const { tex = mTexture; }

Texture* CompositingGroupEffect::GetFbUvMap() const { return (mTexture == 0) ? 0 : mTexture->GetTexture(); }

///////////////////////////////////////////////////////////////////////////////

const char* CompositingGroupEffect::GetEffectName() const {
  static const char* None      = "none";
  static const char* Std       = "standard";
  static const char* Comic     = "comic";
  static const char* Glow      = "glow";
  static const char* Ghostly   = "ghostly";
  static const char* AfterLife = "afterlife";

  const char* EffectName = None;
  switch (meFrameEffect) {
    case lev2::EFRAMEFX_NONE:
      EffectName = None;
      break;
    case lev2::EFRAMEFX_STANDARD:
      EffectName = Std;
      break;
    case lev2::EFRAMEFX_COMIC:
      EffectName = Comic;
      break;
    case lev2::EFRAMEFX_GLOW:
      EffectName = Glow;
      break;
    case lev2::EFRAMEFX_GHOSTLY:
      EffectName = Ghostly;
      break;
    case lev2::EFRAMEFX_AFTERLIFE:
      EffectName = AfterLife;
      break;
    default:
      break;
  }
  return EffectName;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void CompositingGroup::describeX(class_t* c) {
  ork::reflect::RegisterProperty("Camera", &CompositingGroup::mCameraName);
  ork::reflect::RegisterProperty("Layers", &CompositingGroup::mLayers);
  ork::reflect::RegisterProperty("Effect", &CompositingGroup::EffectAccessor);
}

///////////////////////////////////////////////////////////////////////////////

CompositingGroup::CompositingGroup() {}

///////////////////////////////////////////////////////////////////////////////

void CompositingMorphable::WriteMorphTarget(dataflow::MorphKey name, float flerpval) {}

///////////////////////////////////////////////////////////////////////////////

void CompositingMorphable::RecallMorphTarget(dataflow::MorphKey name) {}

///////////////////////////////////////////////////////////////////////////////

void CompositingMorphable::Morph1D(const dataflow::morph_event* pme) {}

///////////////////////////////////////////////////////////////////////////////
void CompositingSceneItem::GetTech(ork::rtti::ICastable*& val) const {
  CompositingTechnique* nonconst = const_cast<CompositingTechnique*>(mpTechnique);
  val                            = nonconst;
}
///////////////////////////////////////////////////////////////////////////////
void CompositingSceneItem::SetTech(ork::rtti::ICastable* const& val) {
  ork::rtti::ICastable* ptr = val;
  mpTechnique               = ((ptr == 0) ? 0 : rtti::safe_downcast<CompositingTechnique*>(ptr));
}
}} // namespace ork::lev2
