////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/lev2/gfx/camera/uicam.h>
#include <ork/lev2/gfx/gfxmaterial_test.h>
#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/lev2/gfx/gfxprimitives.h>
#include <ork/lev2/gfx/renderer/compositor.h>
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/pch.h>
#include <ork/reflect/RegisterProperty.h>
#include <ork/rtti/downcast.h>
///////////////////////////////////////////////////////////////////////////////
#include <ork/lev2/gfx/renderer/NodeCompositor/NodeCompositorDeferred.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/NodeCompositorForward.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/NodeCompositorPicking.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/NodeCompositorFx3.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/NodeCompositorScaleBias.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/NodeCompositorScreen.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/NodeCompositorVr.h>
#include <ork/application/application.h>
#include <ork/reflect/DirectObjectMapPropertyType.hpp>
#include <ork/reflect/DirectObjectPropertyType.hpp>

///////////////////////////////////////////////////////////////////////////////
ImplementReflectionX(ork::lev2::CompositingData, "CompositingData");
///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////
void CompositingData::describeX(class_t* c) {
  using namespace ork::reflect;

  RegisterProperty("Enable", &CompositingData::mbEnable);

  RegisterMapProperty("Groups", &CompositingData::_groups);
  annotatePropertyForEditor<CompositingData>("Groups", "editor.factorylistbase", "CompositingGroup");

  RegisterMapProperty("Scenes", &CompositingData::_scenes);
  annotatePropertyForEditor<CompositingData>("Scenes", "editor.factorylistbase", "CompositingScene");

  RegisterProperty("ActiveScene", &CompositingData::_activeScene);
  RegisterProperty("ActiveItem", &CompositingData::_activeItem);

  static const char* EdGrpStr = "grp://Main Enable ActiveScene ActiveItem "
                                "grp://Data Groups Scenes ";
  reflect::annotateClassForEditor<CompositingData>("editor.prop.groups", EdGrpStr);
}

///////////////////////////////////////////////////////////////////////////////

CompositingData::CompositingData()
    : mbEnable(true)
    , mToggle(true) {
}

//////////////////////////////////////////////////////////////////////////////

void CompositingData::presetDefault() {

  auto p1 = new ScaleBiasCompositingNode;

  auto t1 = new NodeCompositingTechnique;
  auto o1 = new ScreenOutputCompositingNode;
  // auto o1 = new VrCompositingNode;
  auto r1 = new deferrednode::DeferredCompositingNode;
  t1->_writeOutputNode(o1);
  t1->_writeRenderNode(r1);
  // t1->_writePostFxNode(p1);

  auto s1 = new CompositingScene;
  auto i1 = new CompositingSceneItem;
  i1->_writeTech(t1);
  s1->items().AddSorted("item1"_pool, i1);
  _activeScene = "scene1"_pool;
  _activeItem  = "item1"_pool;
  _scenes.AddSorted("scene1"_pool, s1);
}

//////////////////////////////////////////////////////////////////////////////

void CompositingData::presetPicking() {

  auto t1 = new NodeCompositingTechnique;
  auto o1 = new ScreenOutputCompositingNode;
  auto r1 = new PickingCompositingNode;
  t1->_writeOutputNode(o1);
  t1->_writeRenderNode(r1);
  // t1->_writePostFxNode(p1);

  auto s1 = new CompositingScene;
  auto i1 = new CompositingSceneItem;
  i1->_writeTech(t1);
  s1->items().AddSorted("item1"_pool, i1);
  _activeScene = "scene1"_pool;
  _activeItem  = "item1"_pool;
  _scenes.AddSorted("scene1"_pool, s1);
}

//////////////////////////////////////////////////////////////////////////////

void CompositingData::presetPBR() {
  auto t1 = new NodeCompositingTechnique;
  auto o1 = new ScreenOutputCompositingNode;
  auto r1 = new deferrednode::DeferredCompositingNodePbr;
  t1->_writeOutputNode(o1);
  t1->_writeRenderNode(r1);
  // t1->_writePostFxNode(p1);
  auto& assetVars = r1->_texAssetVarMap;
  auto envl_asset = asset::AssetManager<TextureAsset>::Create("data://environ/envmaps/tozenv_nebula", assetVars);
  OrkAssert(envl_asset->GetTexture() != nullptr);
  OrkAssert(envl_asset->_varmap.hasKey("postproc"));
  r1->_writeEnvTexture(envl_asset);

  auto s1 = new CompositingScene;
  auto i1 = new CompositingSceneItem;
  i1->_writeTech(t1);
  s1->items().AddSorted("item1"_pool, i1);
  _activeScene = "scene1"_pool;
  _activeItem  = "item1"_pool;
  _scenes.AddSorted("scene1"_pool, s1);
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
///////////////////////////////////////////////////////////////////////////////
