////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/application/application.h>
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

void CompositingTechnique::Describe() {
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void CompositingScene::describeX(class_t* c) {
  ork::reflect::RegisterMapProperty("Items", &CompositingScene::_items);
  ork::reflect::annotatePropertyForEditor<CompositingScene>("Items", "editor.factorylistbase", "CompositingSceneItem");
}

///////////////////////////////////////////////////////////////////////////////

CompositingScene::CompositingScene() {
}

///////////////////////////////////////////////////////////////////////////////

void CompositingSceneItem::describeX(class_t* c) {

  ork::reflect::RegisterProperty("Technique", &CompositingSceneItem::_readTech, &CompositingSceneItem::_writeTech);
  ork::reflect::annotatePropertyForEditor<CompositingSceneItem>("Technique", "editor.factorylistbase", "CompositingTechnique");
}

///////////////////////////////////////////////////////////////////////////////

CompositingSceneItem::CompositingSceneItem()
    : mpTechnique(nullptr) {
}

///////////////////////////////////////////////////////////////////////////////

void CompositingMorphable::WriteMorphTarget(dataflow::MorphKey name, float flerpval) {
}

///////////////////////////////////////////////////////////////////////////////

void CompositingMorphable::RecallMorphTarget(dataflow::MorphKey name) {
}

///////////////////////////////////////////////////////////////////////////////

void CompositingMorphable::Morph1D(const dataflow::morph_event* pme) {
}

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
///////////////////////////////////////////////////////////////////////////////

void PickingCompositorTechnique::Init(lev2::Context* pTARG, int w, int h) {
}
bool PickingCompositorTechnique::assemble(CompositorDrawData& drawdata) {
  OrkAssert(false);
  return true;
}
void PickingCompositorTechnique::composite(CompositorDrawData& drawdata) {
  OrkAssert(false);
}

}} // namespace ork::lev2
