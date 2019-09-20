////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include "NodeCompositorFx3.h"
#include <ork/lev2/gfx/renderer/builtin_frameeffects.h>
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/reflect/DirectObjectPropertyType.hpp>
#include <ork/reflect/enum_serializer.inl>
#include <ork/lev2/lev2_asset.h>

BEGIN_ENUM_SERIALIZER(ork::lev2, ECOMPOSITEBlend)
DECLARE_ENUM(BoverAplusC)
DECLARE_ENUM(AplusBplusC)
DECLARE_ENUM(AlerpBwithC)
DECLARE_ENUM(Asolo)
DECLARE_ENUM(Bsolo)
DECLARE_ENUM(Csolo)
END_ENUM_SERIALIZER()

///////////////////////////////////////////////////////////////////////////////
ImplementReflectionX(ork::lev2::Fx3CompositingTechnique, "Fx3CompositingTechnique");
ImplementReflectionX(ork::lev2::Fx3CompositingNode, "Fx3CompositingNode");
ImplementReflectionX(ork::lev2::CompositingGroup, "CompositingGroup");
ImplementReflectionX(ork::lev2::CompositingGroupEffect, "CompositingGroupEffect");
///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////
void Fx3CompositingTechnique::describeX(class_t* c) {
  c->memberProperty("Mode", &Fx3CompositingTechnique::meBlendMode)
   ->annotate<ConstString>("editor.class", "ged.factory.enum");
  c->floatProperty("LevelA", float_range{-10,10}, &Fx3CompositingTechnique::mfLevelA);
  c->floatProperty("LevelB", float_range{-10,10}, &Fx3CompositingTechnique::mfLevelB);
  c->floatProperty("LevelC", float_range{-10,10}, &Fx3CompositingTechnique::mfLevelC);
  c->memberProperty("GroupA", &Fx3CompositingTechnique::mGroupA);
  c->memberProperty("GroupB", &Fx3CompositingTechnique::mGroupB);
  c->memberProperty("GroupC", &Fx3CompositingTechnique::mGroupC);
}
///////////////////////////////////////////////////////////////////////////////
Fx3CompositingTechnique::Fx3CompositingTechnique()
    : mpBuiltinFrameTekA(0)
    , mpBuiltinFrameTekB(0)
    , mpBuiltinFrameTekC(0)
    , mfLevelA(1.0f)
    , mfLevelB(1.0f)
    , mfLevelC(1.0f)
    , meBlendMode(AplusBplusC) {}
///////////////////////////////////////////////////////////////////////////////
Fx3CompositingTechnique::~Fx3CompositingTechnique() {
  if (mpBuiltinFrameTekA)
    delete mpBuiltinFrameTekA;
  if (mpBuiltinFrameTekB)
    delete mpBuiltinFrameTekB;
  if (mpBuiltinFrameTekC)
    delete mpBuiltinFrameTekC;
}
///////////////////////////////////////////////////////////////////////////////
void Fx3CompositingTechnique::Init(ork::lev2::GfxTarget* pTARG, int iW, int iH) {
  if (nullptr == mpBuiltinFrameTekA) {
    mCompositingMaterial.Init(pTARG);

    mpBuiltinFrameTekA = new lev2::BuiltinFrameTechniques(iW, iH);
    mpBuiltinFrameTekA->Init(pTARG);
    mpBuiltinFrameTekB = new lev2::BuiltinFrameTechniques(iW, iH);
    mpBuiltinFrameTekB->Init(pTARG);
    mpBuiltinFrameTekC = new lev2::BuiltinFrameTechniques(iW, iH);
    mpBuiltinFrameTekC->Init(pTARG);
  }
}
///////////////////////////////////////////////////////////////////////////////
bool Fx3CompositingTechnique::assemble(CompositorDrawData& drawdata) {

  const lev2::CompositingGroup* pCGA = drawdata._cimpl->compositingGroup(mGroupA);
  const lev2::CompositingGroup* pCGB = drawdata._cimpl->compositingGroup(mGroupB);
  const lev2::CompositingGroup* pCGC = drawdata._cimpl->compositingGroup(mGroupC);

  struct yo {
    static void rend_lyr_2_comp_group(CompositorDrawData& drawdata,
                                      const lev2::CompositingGroup* pCG,
                                      lev2::BuiltinFrameTechniques* pFT,
                                      const char* layername) {
      lev2::FrameRenderer& the_renderer       = drawdata.mFrameRenderer;
      lev2::RenderContextFrameData& framedata = the_renderer.framedata();
      orkstack<CompositingPassData>& cgSTACK  = drawdata.mCompositingGroupStack;
      auto node = pFT->createPassData(pCG);
      the_renderer.framedata().setLayerName(layername);
      cgSTACK.push(node);
      pFT->Render(the_renderer);
      cgSTACK.pop();
    }
  };

  /////////////////////////////////
  if (mpBuiltinFrameTekA) // render layerA
    yo::rend_lyr_2_comp_group(drawdata, pCGA, mpBuiltinFrameTekA, "A");
  if (mpBuiltinFrameTekB) // render layerB
    yo::rend_lyr_2_comp_group(drawdata, pCGB, mpBuiltinFrameTekB, "B");
  if (mpBuiltinFrameTekC) // render layerC
    yo::rend_lyr_2_comp_group(drawdata, pCGC, mpBuiltinFrameTekC, "C");
  return true;
}
///////////////////////////////////////////////////////////////////////////////
void Fx3CompositingTechnique::CompositeLayerToScreen(lev2::GfxTarget* pT,
                                                     ECOMPOSITEBlend eblend,
                                                     lev2::RtGroup* psrcgroupA,
                                                     lev2::RtGroup* psrcgroupB,
                                                     lev2::RtGroup* psrcgroupC,
                                                     float levA,
                                                     float levB,
                                                     float levC) {

  static const float kMAXW = 1.0f;
  static const float kMAXH = 1.0f;
  auto fbi                 = pT->FBI();
  auto this_buf            = fbi->GetThisBuffer();
  int itw                  = pT->GetW();
  int ith                  = pT->GetH();
  SRect vprect(0, 0, itw, ith - 1);
  SRect quadrect(0, ith - 1, itw - 1, 0);
  auto out_buf = this_buf;

  if (psrcgroupA) {
    lev2::Texture* ptexA = (psrcgroupA != 0) ? psrcgroupA->GetMrt(0)->GetTexture() : 0;
    lev2::Texture* ptexB = (psrcgroupB != 0) ? psrcgroupB->GetMrt(0)->GetTexture() : 0;
    lev2::Texture* ptexC = (psrcgroupC != 0) ? psrcgroupC->GetMrt(0)->GetTexture() : 0;
    mCompositingMaterial.SetTextureA(ptexA);
    mCompositingMaterial.SetTextureB(ptexB);
    mCompositingMaterial.SetTextureC(ptexC);

    mCompositingMaterial.SetLevelA(fvec4(levA, levA, levA, levA));
    mCompositingMaterial.SetLevelB(fvec4(levB, levB, levB, levB));
    mCompositingMaterial.SetLevelC(fvec4(levC, levC, levC, levC));

    switch (eblend) {
      case BoverAplusC:
        mCompositingMaterial.SetTechnique("BoverAplusC");
        break;
      case AplusBplusC:
        mCompositingMaterial.SetTechnique("AplusBplusC");
        break;
      case AlerpBwithC:
        mCompositingMaterial.SetTechnique("AlerpBwithC");
        break;
      case Asolo:
        mCompositingMaterial.SetTechnique("Asolo");
        break;
      case Bsolo:
        mCompositingMaterial.SetTechnique("Bsolo");
        break;
      case Csolo:
        mCompositingMaterial.SetTechnique("Csolo");
        break;
      default:
        mCompositingMaterial.SetTechnique("AplusBplusC");
        break;
    }

    out_buf->RenderMatOrthoQuad(vprect, quadrect, &mCompositingMaterial, 0.0f, 0.0f, kMAXW, kMAXH, 0, fvec4::White());
  }
}
///////////////////////////////////////////////////////////////////////////////
void Fx3CompositingTechnique::composite(CompositorDrawData& drawdata) {
  auto pT = drawdata.target();
  /////////////////////////////////////////////////////////////////////
  int iCSitem     = 0;
  float levAA     = 0.5f;
  float levBB     = 0.5f;
  float levCC     = 0.5f;
  float levA      = 0.5f;
  float levB      = 0.5f;
  float levC      = 0.5f;
  float levMaster = 1.0f;
  // ECOMPOSITEBlend eblend = AplusBplusC;
  /////////////////////////////////////////////////////////////////////
  const lev2::CompositingSceneItem* pCSI = 0;
  if (drawdata._cimpl) {
    pCSI = drawdata._cimpl->compositingItem(0, iCSitem);
  }
  /////////////////////////////////////////////////////////////////////
  if (pCSI) {
    levA = mfLevelA * levMaster * levAA;
    levB = mfLevelB * levMaster * levBB;
    levC = mfLevelC * levMaster * levCC;
  }
  /////////////////////////////////////////////////////////////////////
  lev2::BuiltinFrameTechniques* pFTEKA = mpBuiltinFrameTekA;
  lev2::BuiltinFrameTechniques* pFTEKB = mpBuiltinFrameTekB;
  lev2::BuiltinFrameTechniques* pFTEKC = mpBuiltinFrameTekC;
  lev2::RtGroup* pRTA                  = pFTEKA ? pFTEKA->GetFinalRenderTarget() : 0;
  lev2::RtGroup* pRTB                  = pFTEKB ? pFTEKB->GetFinalRenderTarget() : 0;
  lev2::RtGroup* pRTC                  = pFTEKC ? pFTEKC->GetFinalRenderTarget() : 0;
  if (pRTA || pRTB || pRTC) {
    CompositeLayerToScreen(pT, meBlendMode, pRTA, pRTB, pRTC, levA, levB, levC);
  }
  /////////////////////////////////////////////////////////////////////
}
///////////////////////////////////////////////////////////////////////////////
void Fx3CompositingNode::describeX(class_t*c) {
  c->accessorProperty("Group", &Fx3CompositingNode::_readGroup, &Fx3CompositingNode::_writeGroup)
   ->annotate<ConstString>("editor.factorylistbase", "CompositingGroup");
}
///////////////////////////////////////////////////////////////////////////////
Fx3CompositingNode::Fx3CompositingNode() : mFTEK(nullptr), mGroup(nullptr) {}
///////////////////////////////////////////////////////////////////////////////
Fx3CompositingNode::~Fx3CompositingNode() {
  if (mFTEK)
    delete mFTEK;
}
///////////////////////////////////////////////////////////////////////////////
void Fx3CompositingNode::_readGroup(ork::rtti::ICastable*& val) const {
  CompositingGroup* nonconst = const_cast<CompositingGroup*>(mGroup);
  val = nonconst;
}
///////////////////////////////////////////////////////////////////////////////
void Fx3CompositingNode::_writeGroup(ork::rtti::ICastable* const& val) {
  ork::rtti::ICastable* ptr = val;
  mGroup = ((ptr == 0) ? 0 : rtti::safe_downcast<CompositingGroup*>(ptr));
}
///////////////////////////////////////////////////////////////////////////////
void Fx3CompositingNode::DoInit(lev2::GfxTarget* pTARG, int iW, int iH) // virtual
{
  if (nullptr == mFTEK) {
    mCompositingMaterial.Init(pTARG);

    mFTEK = new lev2::BuiltinFrameTechniques(iW, iH);
    mFTEK->Init(pTARG);
  }
}
///////////////////////////////////////////////////////////////////////////////
void Fx3CompositingNode::DoRender(CompositorDrawData& drawdata) // virtual
{
  const CompositingGroup* pCG = mGroup;
  lev2::FrameRenderer& the_renderer = drawdata.mFrameRenderer;
  lev2::RenderContextFrameData& framedata = the_renderer.framedata();
  orkstack<CompositingPassData>& cgSTACK = drawdata.mCompositingGroupStack;

  CompositingPassData node;
  node.mbDrawSource = (pCG != nullptr);

  if (mFTEK) {
    framedata.setLayerName("All");
    auto node = mFTEK->createPassData(pCG);
    cgSTACK.push(node);
    mFTEK->Render(the_renderer);
    cgSTACK.pop();
  }
}

lev2::RtGroup* Fx3CompositingNode::GetOutput() const {
  lev2::RtGroup* pRT = mFTEK ? mFTEK->GetFinalRenderTarget() : nullptr;
  return pRT;
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void CompositingGroupEffect::describeX(class_t* c) {
  c->memberProperty("Type", &CompositingGroupEffect::_effectID)->annotate<ConstString>("editor.class", "ged.factory.enum");

  c->accessorProperty("FbUvTexture", &CompositingGroupEffect::_readTex, &CompositingGroupEffect::_writeTex)
      ->annotate<ConstString>("editor.class", "ged.factory.assetlist")
      ->annotate<ConstString>("editor.assettype", "lev2tex")
      ->annotate<ConstString>("editor.assetclass", "lev2tex");

  c->floatProperty("FinalRezScale", float_range{0.025, 1.0}, &CompositingGroupEffect::_finalResolution);
  c->floatProperty("FxRezScale", float_range{0.025, 1.0}, &CompositingGroupEffect::_fxResolution);

  c->memberProperty("Amount", &CompositingGroupEffect::_effectAmount);
  c->memberProperty("FeedbackAmount", &CompositingGroupEffect::_feedbackLevel);
  c->memberProperty("PostFxFeedback", &CompositingGroupEffect::_postFxFeedback);
}

///////////////////////////////////////////////////////////////////////////////

CompositingGroupEffect::CompositingGroupEffect()
    : _effectID(lev2::EFRAMEFX_NONE)
    , _effectAmount(0.0f)
    , _feedbackLevel(0.0f)
    , _finalResolution(0.5f)
    , _fxResolution(0.5f)
    , _texture(nullptr)
    , _postFxFeedback(false) {}

///////////////////////////////////////////////////////////////////////////////

void CompositingGroupEffect::_writeTex(rtti::ICastable* const& tex) { _texture = tex ? rtti::autocast(tex) : nullptr; }

///////////////////////////////////////////////////////////////////////////////

void CompositingGroupEffect::_readTex(rtti::ICastable*& tex) const { tex = _texture; }

Texture* CompositingGroupEffect::GetFbUvMap() const { return (_texture == 0) ? 0 : _texture->GetTexture(); }

///////////////////////////////////////////////////////////////////////////////

const char* CompositingGroupEffect::GetEffectName() const {
  static const char* None      = "none";
  static const char* Std       = "standard";
  static const char* Comic     = "comic";
  static const char* Glow      = "glow";
  static const char* Ghostly   = "ghostly";
  static const char* AfterLife = "afterlife";

  const char* EffectName = None;
  switch (_effectID) {
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
  ork::reflect::RegisterProperty("Camera", &CompositingGroup::_cameraName);
  ork::reflect::RegisterProperty("Layers", &CompositingGroup::_layers);
  ork::reflect::RegisterProperty("Effect", &CompositingGroup::_accessEffect);
}

///////////////////////////////////////////////////////////////////////////////

CompositingGroup::CompositingGroup() {}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
///////////////////////////////////////////////////////////////////////////////
