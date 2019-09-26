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
ImplementReflectionX(ork::lev2::CompositingScene, "CompositingScene");
ImplementReflectionX(ork::lev2::CompositingSceneItem, "CompositingSceneItem");
INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::CompositingTechnique, "CompositingTechnique");
///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace lev2 {
///////////////////////////////////////////////////////////////////////////////
CompositingPassData CompositingPassData::FromRCFD(const RenderContextFrameData& RCFD) {
    lev2::rendervar_t passdata = RCFD.getUserProperty("nodes"_crc);
    auto cstack                = passdata.Get<compositingpassdatastack_t*>();
    OrkAssert(cstack != nullptr);
    return cstack->top();
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
void CompositingPassData::updateCompositingSize(int w, int h) {
  if( mpFrameTek )
    if( auto ftek = dynamic_cast<FrameTechniqueBase*>(mpFrameTek) )
        ftek->update(*this,w,h);
}
///////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void CompositingTechnique::Describe() {}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
CompositingContext::CompositingContext()
    : miWidth(0)
    , miHeight(0)
    , _compositingTechnique(nullptr) {}

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

bool CompositingContext::assemble(CompositorDrawData& drawdata) {
  bool rval = false;
  Init(drawdata.target()); // fixme lazy init
  if (_compositingTechnique) {
    _compositingTechnique->Init(drawdata.target(), miWidth, miHeight);
    rval = _compositingTechnique->assemble(drawdata);
  }
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

GfxTarget* CompositorDrawData::target() const {
  return mFrameRenderer.framedata().GetTarget();
}

///////////////////////////////////////////////////////////////////////////////

void CompositingContext::composite(CompositorDrawData& drawdata) {
  Init(drawdata.target());
  if (_compositingTechnique)
    _compositingTechnique->composite(drawdata);
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
