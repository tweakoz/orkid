////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
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
#include <ork/reflect/properties/register.h>
#include <ork/rtti/downcast.h>
#include <ork/application/application.h>
#include <ork/reflect/properties/DirectTypedMap.hpp>
#include <ork/reflect/properties/DirectTyped.hpp>
///////////////////////////////////////////////////////////////////////////////
#include <ork/lev2/gfx/renderer/NodeCompositor/NodeCompositorPicking.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/NodeCompositorScaleBias.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/NodeCompositorScreen.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/NodeCompositorVr.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/OutputNodeRtGroup.h>
///////////////////////////////////////////////////////////////////////////////
#include <ork/lev2/gfx/renderer/NodeCompositor/pbr_node_deferred.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/pbr_node_forward.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/unlit_node.h>
///////////////////////////////////////////////////////////////////////////////
ImplementReflectionX(ork::lev2::CompositingData, "CompositingData");
///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2{
extern appinitdata_ptr_t _ginitdata;
} // namespace ork::lev2{
///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////
void CompositingData::describeX(class_t* c) {
/*  using namespace ork::reflect;

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
*/}

///////////////////////////////////////////////////////////////////////////////

CompositingData::CompositingData() {
}

//////////////////////////////////////////////////////////////////////////////

void CompositingData::presetDefault() {
  presetDeferredPBR();
}

//////////////////////////////////////////////////////////////////////////////

RenderPresetContext CompositingData::presetUnlit(rtgroup_ptr_t outputgrp) {

  OutputCompositingNode* selected_output_node = nullptr;
  if(outputgrp){
    selected_output_node = new RtGroupOutputCompositingNode(outputgrp);
  }else{
    selected_output_node = new ScreenOutputCompositingNode;
  }


  auto t1 = new NodeCompositingTechnique;
  auto r1 = new compositor::UnlitNode;
  t1->_writeOutputNode(selected_output_node);
  t1->_writeRenderNode(r1);
  // t1->_writePostFxNode(p1);

  auto s1 = new CompositingScene;
  auto i1 = new CompositingSceneItem;
  i1->_writeTech(t1);
  s1->items().AddSorted("item1"_pool, i1);
  _activeScene = "scene1"_pool;
  _activeItem  = "item1"_pool;
  _scenes.AddSorted("scene1"_pool, s1);

  RenderPresetContext rval;
  rval._nodetek    = t1;
  rval._outputnode = selected_output_node;
  rval._rendernode = r1;

  return rval;
}

//////////////////////////////////////////////////////////////////////////////

void CompositingData::presetPicking() {

  auto t1 = new NodeCompositingTechnique;
  auto o1 = new RtGroupOutputCompositingNode;
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

compositorimpl_ptr_t CompositingData::createImpl() const {
  auto impl = std::make_shared<CompositingImpl>(*this);

  return impl;
}

//////////////////////////////////////////////////////////////////////////////

RenderPresetContext CompositingData::presetForwardPBR(rtgroup_ptr_t outputgrp) {
  RenderPresetContext rval;
  auto t1 = new NodeCompositingTechnique;
  OutputCompositingNode* selected_output_node = nullptr;
  auto r1 = new pbr::ForwardNode;
  if(outputgrp){
    selected_output_node = new RtGroupOutputCompositingNode(outputgrp);
  }else{
    auto screennode = new ScreenOutputCompositingNode;
    screennode->setSuperSample(_ginitdata->_ssaa_samples);
    selected_output_node = screennode;
  }

  t1->_writeOutputNode(selected_output_node);
  t1->_writeRenderNode(r1);

  // t1->_writePostFxNode(p1);

  auto load_req = r1->_pbrcommon->createSkyboxTextureLoadRequest("src://envmaps/tozenv_nebula");
  auto envl_asset = asset::AssetManager<TextureAsset>::load(load_req);
  // todo inject postload ops
  OrkAssert(envl_asset->GetTexture() != nullptr);
  OrkAssert(envl_asset->_varmap.hasKey("postproc"));
  r1->_pbrcommon->_writeEnvTexture(envl_asset);

  auto s1 = new CompositingScene;
  auto i1 = new CompositingSceneItem;
  i1->_writeTech(t1);
  s1->items().AddSorted("item1"_pool, i1);
  _activeScene = "scene1"_pool;
  _activeItem  = "item1"_pool;
  _scenes.AddSorted("scene1"_pool, s1);

  rval._nodetek    = t1;
  rval._outputnode = selected_output_node;
  rval._rendernode = r1;

  return rval;}

//////////////////////////////////////////////////////////////////////////////

RenderPresetContext CompositingData::presetDeferredPBR(rtgroup_ptr_t outputgrp) {
  RenderPresetContext rval;
  auto t1 = new NodeCompositingTechnique;
  OutputCompositingNode* selected_output_node = nullptr;
  auto r1 = new pbr::deferrednode::DeferredCompositingNodePbr;
  if(outputgrp){
    selected_output_node = new RtGroupOutputCompositingNode(outputgrp);
  }else{
    auto screennode = new ScreenOutputCompositingNode;
    screennode->setSuperSample(_ginitdata->_ssaa_samples);
    selected_output_node = screennode;
  }

  t1->_writeOutputNode(selected_output_node);
  t1->_writeRenderNode(r1);

  auto pbr_common = r1->_pbrcommon;

  // t1->_writePostFxNode(p1);

  auto load_req = pbr_common->createSkyboxTextureLoadRequest("src://envmaps/tozenv_nebula");

  auto envl_asset = asset::AssetManager<TextureAsset>::load(load_req);
  // todo inject postload ops
  OrkAssert(envl_asset->GetTexture() != nullptr);
  OrkAssert(envl_asset->_varmap.hasKey("postproc"));
  pbr_common->_writeEnvTexture(envl_asset);

  auto s1 = new CompositingScene;
  auto i1 = new CompositingSceneItem;
  i1->_writeTech(t1);
  s1->items().AddSorted("item1"_pool, i1);
  _activeScene = "scene1"_pool;
  _activeItem  = "item1"_pool;
  _scenes.AddSorted("scene1"_pool, s1);

  rval._nodetek    = t1;
  rval._outputnode = selected_output_node;
  rval._rendernode = r1;

  return rval;
}

//////////////////////////////////////////////////////////////////////////////

RenderPresetContext CompositingData::presetPBRVR() {
  RenderPresetContext rval;
  auto t1 = new NodeCompositingTechnique;
  auto o1 = new VrCompositingNode;
  auto r1 = new pbr::deferrednode::DeferredCompositingNodePbr;
  t1->_writeOutputNode(o1);
  t1->_writeRenderNode(r1);
  // t1->_writePostFxNode(p1);

  auto pbr_common = r1->_pbrcommon;

  auto load_req = pbr_common->createSkyboxTextureLoadRequest("src://envmaps/tozenv_nebula");

  auto assetVars  = pbr_common->_texAssetVarMap;
  auto envl_asset = asset::AssetManager<TextureAsset>::load(load_req);
  // todo inject postload ops
  OrkAssert(envl_asset->GetTexture() != nullptr);
  OrkAssert(envl_asset->_varmap.hasKey("postproc"));
  pbr_common->_writeEnvTexture(envl_asset);

  auto s1 = new CompositingScene;
  auto i1 = new CompositingSceneItem;
  i1->_writeTech(t1);
  s1->items().AddSorted("item1"_pool, i1);
  _activeScene = "scene1"_pool;
  _activeItem  = "item1"_pool;
  _scenes.AddSorted("scene1"_pool, s1);

  rval._nodetek    = t1;
  rval._outputnode = o1;
  rval._rendernode = r1;

  return rval;
}

//////////////////////////////////////////////////////////////////////////////

const CompositingScene* CompositingData::findScene(const PoolString& named) const {
  const CompositingScene* rval = nullptr;
  auto it                      = _scenes.find(named);
  if (it != _scenes.end()) {
    rval = dynamic_cast<const CompositingScene*>(it->second);
  }
  return rval;
}

//////////////////////////////////////////////////////////////////////////////

const CompositingSceneItem* CompositingScene::findItem(const PoolString& named) const {
  const CompositingSceneItem* rval = nullptr;
  auto it                          = _items.find(named);
  if (it != _items.end()) {
    rval = dynamic_cast<const CompositingSceneItem*>(it->second);
  }
  return rval;
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
///////////////////////////////////////////////////////////////////////////////
