////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/application/application.h>
#include <ork/lev2/gfx/gfxmaterial_fx.h>
#include <ork/lev2/gfx/gfxmaterial_test.h>
#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/lev2/gfx/gfxprimitives.h>
#include <ork/lev2/gfx/renderer/compositor.h>
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/gfx/renderer/builtin_frameeffects.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/lev2/lev2_asset.h>
#include <ork/pch.h>
#include <ork/reflect/RegisterProperty.h>
#include <ork/rtti/downcast.h>
///////////////////////////////////////////////////////////////////////////////
#include <ork/reflect/DirectObjectMapPropertyType.hpp>
#include <ork/reflect/DirectObjectPropertyType.hpp>
#include <ork/reflect/enum_serializer.inl>
///////////////////////////////////////////////////////////////////////////////
ImplementReflectionX(ork::lev2::CompositingGroup, "CompositingGroup");
ImplementReflectionX(ork::lev2::CompositingScene, "CompositingScene");
ImplementReflectionX(ork::lev2::CompositingSceneItem, "CompositingSceneItem");
ImplementReflectionX(ork::lev2::CompositingGroupEffect, "CompositingGroupEffect");
INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::CompositingTechnique, "CompositingTechnique");
///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace lev2 {
///////////////////////////////////////////////////////////////////////////////
const CameraData* CompositingPassData::getCamera(lev2::RenderContextFrameData& framedata, int icamindex, int icullcamindex) {
  auto DB      = framedata.GetDB();
  auto gfxtarg = framedata.GetTarget();
  CameraData TempCamData, TempCullCamData;
  const CameraData* pcamdata     = DB->GetCameraData(icamindex);
  const CameraData* pcullcamdata = DB->GetCameraData(icullcamindex);
  if (nullptr == pcamdata)
    return nullptr;
  /////////////////////////////////////////
  // Culling camera ? (for debug)
  /////////////////////////////////////////
  if (pcullcamdata) {
    TempCullCamData = *pcullcamdata;
    TempCullCamData.BindGfxTarget(gfxtarg);
    TempCullCamData.CalcCameraData(framedata.GetCameraCalcCtx());
    TempCamData.SetVisibilityCamDat(&TempCullCamData);
  }
  /////////////////////////////////////////
  // try named CameraData from NODE
  /////////////////////////////////////////
  if (mpCameraName) {
    const CameraData* pcamdataNAMED = DB->GetCameraData(*mpCameraName);
    if (pcamdataNAMED)
      pcamdata = pcamdataNAMED;
  }
  /////////////////////////////////////////
  // try direct CameraData from NODE
  /////////////////////////////////////////
  if (auto from_node = _impl.TryAs<const CameraData*>()) {
    pcamdata = from_node.value();
    // printf( "from node\n");
  }
  /////////////////////////////////////////
  return pcamdata;
}

///////////////////////////////////////////////////////////////////////////////

std::vector<PoolString> CompositingPassData::getLayerNames() const {
  std::vector<PoolString> LayerNames;
  if (mpLayerName) {
    const char* layername = mpLayerName->c_str();
    if (layername) {
      char temp_buf[256];
      strncpy(&temp_buf[0], layername, sizeof(temp_buf));
      char* tok = strtok(&temp_buf[0], ",");
      while (tok != 0) {
        LayerNames.push_back(AddPooledString(tok));
        tok = strtok(0, ",");
      }
    }
  } else {
    LayerNames.push_back(AddPooledLiteral("All"));
  }
  return LayerNames;
}

///////////////////////////////////////////////////////////////////////////////

CompositingPassData CompositingPassData::FromRCFD(const RenderContextFrameData& RCFD) {
  lev2::rendervar_t passdata = RCFD.getUserProperty("nodes"_crc);
  auto cstack                = passdata.Get<compositingpassdatastack_t*>();
  OrkAssert(cstack != nullptr);
  return cstack->top();
}
///////////////////////////////////////////////////////////////////////////////
void CompositingPassData::updateCompositingSize(int w, int h) {
  if( auto ftek = dynamic_cast<BuiltinFrameTechniques*>(mpFrameTek) )
      ftek->update(*this,w,h);
}
///////////////////////////////////////////////////////////////////////////////
void CompositingPassData::renderPass(lev2::RenderContextFrameData& RCFD,void_lambda_t CALLBACK) {
  lev2::GfxTarget* pTARG = RCFD.GetTarget();
  lev2::IRenderTarget* pIT = RCFD.GetRenderTarget();
  auto FBI = pTARG->FBI();
  ///////////////////////////////////////////////////////////////////////////
  RCFD.GetCameraCalcCtx().mfAspectRatio = float(pTARG->GetW()) / float(pTARG->GetH());
  ///////////////////////////////////////////////////////////////////////////
  SRect VPRect(0, 0, pIT->GetW(), pIT->GetH());
  FBI->PushViewport(VPRect);
  FBI->PushScissor(VPRect);
      RCFD.addStandardLayers();
      CALLBACK();
  FBI->PopScissor();
  FBI->PopViewport();
}
///////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void CompositingTechnique::Describe() {}
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
  ork::reflect::RegisterMapProperty("Items", &CompositingScene::_items);
  ork::reflect::AnnotatePropertyForEditor<CompositingScene>("Items", "editor.factorylistbase", "CompositingSceneItem");
}

///////////////////////////////////////////////////////////////////////////////

CompositingScene::CompositingScene() {}

///////////////////////////////////////////////////////////////////////////////

void CompositingSceneItem::describeX(class_t* c) {

  ork::reflect::RegisterProperty("Technique", &CompositingSceneItem::_readTech, &CompositingSceneItem::_writeTech);
  ork::reflect::AnnotatePropertyForEditor<CompositingSceneItem>("Technique", "editor.factorylistbase", "CompositingTechnique");
}

///////////////////////////////////////////////////////////////////////////////

CompositingSceneItem::CompositingSceneItem()
    : mpTechnique(nullptr) {}

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

void CompositingMorphable::WriteMorphTarget(dataflow::MorphKey name, float flerpval) {}

///////////////////////////////////////////////////////////////////////////////

void CompositingMorphable::RecallMorphTarget(dataflow::MorphKey name) {}

///////////////////////////////////////////////////////////////////////////////

void CompositingMorphable::Morph1D(const dataflow::morph_event* pme) {}

///////////////////////////////////////////////////////////////////////////////
void CompositingSceneItem::_readTech(ork::rtti::ICastable*& val) const {
  CompositingTechnique* nonconst = const_cast<CompositingTechnique*>(mpTechnique);
  val                            = nonconst;
}
///////////////////////////////////////////////////////////////////////////////
void CompositingSceneItem::_writeTech(ork::rtti::ICastable* const& val) {
  ork::rtti::ICastable* ptr = val;
  mpTechnique               = ((ptr == 0) ? 0 : rtti::safe_downcast<CompositingTechnique*>(ptr));
}
}} // namespace ork::lev2
