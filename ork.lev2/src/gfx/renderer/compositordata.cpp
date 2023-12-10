////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
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
}

///////////////////////////////////////////////////////////////////////////////

CompositingData::CompositingData() {
}

//////////////////////////////////////////////////////////////////////////////

void CompositingData::presetDefault() {
  presetDeferredPBR();
}

//////////////////////////////////////////////////////////////////////////////

RenderPresetContext CompositingData::presetUnlit(rtgroup_ptr_t outputgrp) {


  auto t1 = std::make_shared<NodeCompositingTechnique>();
  auto r1 = t1->createRenderNode<compositor::UnlitNode>();
  compositoroutnode_ptr_t o1;
  if(outputgrp)
    o1 = t1->createOutputNode<RtGroupOutputCompositingNode>();
  else
    o1 = t1->createOutputNode<ScreenOutputCompositingNode>();

  auto s1 = std::make_shared<CompositingScene>();
  auto i1 = std::make_shared<CompositingSceneItem>();
  i1->_technique = t1;
  s1->_items["item1"]=i1;
  _activeScene = "scene1";
  _activeItem  = "item1";
  _scenes["scene1"]=s1;

  RenderPresetContext rval;
  rval._nodetek    = t1;
  rval._outputnode = o1;
  rval._rendernode = r1;

  return rval;
}

//////////////////////////////////////////////////////////////////////////////

compositorimpl_ptr_t CompositingData::createImpl() const {
  return std::make_shared<CompositingImpl>(*this);
}

//////////////////////////////////////////////////////////////////////////////

RenderPresetContext CompositingData::presetDeferredPBR(rtgroup_ptr_t outputgrp) {
  RenderPresetContext rval;
  auto t1 = std::make_shared<NodeCompositingTechnique>();
  auto r1 = t1->createRenderNode<pbr::deferrednode::DeferredCompositingNodePbr>();

  compositoroutnode_ptr_t selected_output_node = nullptr;
  if(outputgrp){
    selected_output_node = t1->createOutputNode<RtGroupOutputCompositingNode>(outputgrp);
  }else{
    auto screennode = t1->createOutputNode<ScreenOutputCompositingNode>();
    screennode->setSuperSample(_ginitdata->_ssaa_samples);
    selected_output_node = screennode;
  }

  auto pbr_common = r1->_pbrcommon;

  auto load_req = pbr_common->requestSkyboxTexture("src://envmaps/tozenv_nebula");

  auto s1 = std::make_shared<CompositingScene>();
  auto i1 = std::make_shared<CompositingSceneItem>();
  i1->_technique = t1;
  s1->_items["item1"]=i1;
  _activeScene = "scene1";
  _activeItem  = "item1";
  _scenes["scene1"]=s1;

  rval._nodetek    = t1;
  rval._outputnode = selected_output_node;
  rval._rendernode = r1;

  return rval;
}

//////////////////////////////////////////////////////////////////////////////

RenderPresetContext CompositingData::presetPBRVR() {
  RenderPresetContext rval;
  auto t1 = std::make_shared<NodeCompositingTechnique>();
  auto o1 = t1->createOutputNode<VrCompositingNode>();
  auto r1 = t1->createRenderNode<pbr::deferrednode::DeferredCompositingNodePbr>();

  auto pbr_common = r1->_pbrcommon;

  auto load_req = pbr_common->requestSkyboxTexture("src://envmaps/tozenv_nebula");

  auto s1 = std::make_shared<CompositingScene>();
  auto i1 = std::make_shared<CompositingSceneItem>();
  i1->_technique = t1;
  s1->_items["item1"]=i1;
  _activeScene = "scene1";
  _activeItem  = "item1";
  _scenes["scene1"]=s1;

  rval._nodetek    = t1;
  rval._outputnode = o1;
  rval._rendernode = r1;

  return rval;
}

//////////////////////////////////////////////////////////////////////////////

void CompositingData::presetPicking() {

  auto t1 = std::make_shared<NodeCompositingTechnique>();
  auto o1 = t1->createOutputNode<RtGroupOutputCompositingNode>();
  auto r1 = t1->createRenderNode<PickingCompositingNode>();

  auto s1 = std::make_shared<CompositingScene>();
  auto i1 = std::make_shared<CompositingSceneItem>();
  i1->_technique = t1;
  s1->_items["item1"]=i1;
  _activeScene = "scene1";
  _activeItem  = "item1";
  _scenes["scene1"]=s1;
}
void CompositingData::presetPickingDebug() {

  auto t1 = std::make_shared<NodeCompositingTechnique>();
  auto o1 = t1->createOutputNode<ScreenOutputCompositingNode>();
  auto r1 = t1->createRenderNode<PickingCompositingNode>();

  auto s1 = std::make_shared<CompositingScene>();
  auto i1 = std::make_shared<CompositingSceneItem>();
  i1->_technique = t1;
  s1->_items["item1"]=i1;
  _activeScene = "scene1";
  _activeItem  = "item1";
  _scenes["scene1"]=s1;
}

//////////////////////////////////////////////////////////////////////////////

RenderPresetContext CompositingData::presetForwardPBR(rtgroup_ptr_t outputgrp) {
  RenderPresetContext rval;
  auto t1 = std::make_shared<NodeCompositingTechnique>();
  compositoroutnode_ptr_t selected_output_node = nullptr;
  auto r1 = t1->createRenderNode<pbr::ForwardNode>();
  if(outputgrp){
    selected_output_node = t1->createOutputNode<RtGroupOutputCompositingNode>(outputgrp);
  }else{
    auto screennode = t1->createOutputNode<ScreenOutputCompositingNode>();
    screennode->setSuperSample(_ginitdata->_ssaa_samples);
    selected_output_node = screennode;
  }

  auto load_req = r1->_pbrcommon->requestSkyboxTexture("src://envmaps/tozenv_nebula");

  auto s1 = std::make_shared<CompositingScene>();
  auto i1 = std::make_shared<CompositingSceneItem>();
  i1->_technique = t1;
  s1->_items["item1"]=i1;
  _activeScene = "scene1";
  _activeItem  = "item1";
  _scenes["scene1"]=s1;

  rval._nodetek    = t1;
  rval._outputnode = selected_output_node;
  rval._rendernode = r1;

  return rval;
}

//////////////////////////////////////////////////////////////////////////////

RenderPresetContext CompositingData::presetForwardPBRVR() {
  RenderPresetContext rval;
  auto t1 = std::make_shared<NodeCompositingTechnique>();
  auto o1 = t1->createOutputNode<VrCompositingNode>();
  auto r1 = t1->createRenderNode<pbr::ForwardNode>();

  auto load_req = r1->_pbrcommon->requestSkyboxTexture("src://envmaps/tozenv_nebula");

  auto s1 = std::make_shared<CompositingScene>();
  auto i1 = std::make_shared<CompositingSceneItem>();
  i1->_technique = t1;
  s1->_items["item1"]=i1;
  _activeScene = "scene1";
  _activeItem  = "item1";
  _scenes["scene1"]=s1;

  rval._nodetek    = t1;
  rval._outputnode = o1;
  rval._rendernode = r1;

  return rval;
}

//////////////////////////////////////////////////////////////////////////////

compositingscene_constptr_t CompositingData::findScene(const std::string& named) const {
  compositingscene_constptr_t rval = nullptr;
  auto it                      = _scenes.find(named);
  if (it != _scenes.end()) {
    rval = it->second;
  }
  return rval;
}

//////////////////////////////////////////////////////////////////////////////

compositingsceneitem_constptr_t CompositingScene::findItem(const std::string& named) const {
  compositingsceneitem_constptr_t rval = nullptr;
  auto it                          = _items.find(named);
  if (it != _items.end()) {
    rval = it->second;
  }
  return rval;
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
///////////////////////////////////////////////////////////////////////////////
