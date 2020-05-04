////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/lev2/gfx/renderer/NodeCompositor/NodeCompositor.h>
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
#include <ork/profiling.inl>

///////////////////////////////////////////////////////////////////////////////
ImplementReflectionX(ork::lev2::RenderCompositingNode, "RenderCompositingNode");
ImplementReflectionX(ork::lev2::PostCompositingNode, "PostCompositingNode");
ImplementReflectionX(ork::lev2::OutputCompositingNode, "OutputCompositingNode");
ImplementReflectionX(ork::lev2::NodeCompositingTechnique, "NodeCompositingTechnique");
ImplementReflectionX(ork::lev2::ChainCompositingNode, "ChainCompositingNode");
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace lev2 {
///////////////////////////////////////////////////////////////////////////////
void ChainCompositingNode::describeX(class_t* c) {
}
///////////////////////////////////////////////////////////////////////////////
void NodeCompositingTechnique::describeX(class_t* c) {
  c->accessorProperty("RenderNode", &NodeCompositingTechnique::_readRenderNode, &NodeCompositingTechnique::_writeRenderNode)
      ->annotate<ConstString>("editor.factorylistbase", "RenderCompositingNode");
  c->accessorProperty("PostFxNode", &NodeCompositingTechnique::_readPostFxNode, &NodeCompositingTechnique::_writePostFxNode)
      ->annotate<ConstString>("editor.factorylistbase", "PostCompositingNode");
  c->accessorProperty("OutputNode", &NodeCompositingTechnique::_readOutputNode, &NodeCompositingTechnique::_writeOutputNode)
      ->annotate<ConstString>("editor.factorylistbase", "OutputCompositingNode");
}
///////////////////////////////////////////////////////////////////////////////
NodeCompositingTechnique::NodeCompositingTechnique()
    : _renderNode(nullptr)
    , _postfxNode(nullptr)
    , _outputNode(nullptr) {
}
///////////////////////////////////////////////////////////////////////////////
NodeCompositingTechnique::~NodeCompositingTechnique() {
  if (_renderNode)
    delete _renderNode;
  if (_postfxNode)
    delete _postfxNode;
  if (_outputNode)
    delete _outputNode;
}
///////////////////////////////////////////////////////////////////////////////
void NodeCompositingTechnique::_readRenderNode(ork::rtti::ICastable*& val) const {
  auto nonconst = const_cast<RenderCompositingNode*>(_renderNode);
  val           = nonconst;
}
///////////////////////////////////////////////////////////////////////////////
void NodeCompositingTechnique::_writeRenderNode(ork::rtti::ICastable* const& val) {
  ork::rtti::ICastable* ptr = val;
  _renderNode               = ((ptr == nullptr) ? nullptr : rtti::safe_downcast<RenderCompositingNode*>(ptr));
}
///////////////////////////////////////////////////////////////////////////////
void NodeCompositingTechnique::_readPostFxNode(ork::rtti::ICastable*& val) const {
  auto nonconst = const_cast<PostCompositingNode*>(_postfxNode);
  val           = nonconst;
}
///////////////////////////////////////////////////////////////////////////////
void NodeCompositingTechnique::_writePostFxNode(ork::rtti::ICastable* const& val) {
  ork::rtti::ICastable* ptr = val;
  _postfxNode               = ((ptr == nullptr) ? nullptr : rtti::safe_downcast<PostCompositingNode*>(ptr));
}
///////////////////////////////////////////////////////////////////////////////
void NodeCompositingTechnique::_readOutputNode(ork::rtti::ICastable*& val) const {
  auto nonconst = const_cast<OutputCompositingNode*>(_outputNode);
  val           = nonconst;
}
///////////////////////////////////////////////////////////////////////////////
void NodeCompositingTechnique::_writeOutputNode(ork::rtti::ICastable* const& val) {
  ork::rtti::ICastable* ptr = val;
  _outputNode               = ((ptr == nullptr) ? nullptr : rtti::safe_downcast<OutputCompositingNode*>(ptr));
}
///////////////////////////////////////////////////////////////////////////////
void NodeCompositingTechnique::gpuInit(lev2::Context* pTARG, int w, int h) {
  pTARG->debugPushGroup("NodeCompositingTechnique::init");
  if (_renderNode)
    _renderNode->gpuInit(pTARG, w, h);
  if (_postfxNode)
    _postfxNode->gpuInit(pTARG, w, h);
  if (_outputNode)
    _outputNode->gpuInit(pTARG, w, h);

  pTARG->debugPopGroup();
}
///////////////////////////////////////////////////////////////////////////////
bool NodeCompositingTechnique::assemble(CompositorDrawData& drawdata) {
  EASY_BLOCK("assemble-ctek");
  bool rval = false;
  drawdata.context()->debugPushGroup("NodeCompositingTechnique::assemble");
  if (_outputNode and _renderNode) {
    rval = true;
    ////////////////////////////////////////////////////////////////////////////
    // if we have a postfx_out, then that is the "final" output
    //  otherwise it is render_out
    ////////////////////////////////////////////////////////////////////////////
    RtGroup* render_outg = _renderNode ? _renderNode->GetOutputGroup() : nullptr;
    RtBuffer* render_out = _renderNode ? _renderNode->GetOutput() : nullptr;
    RtBuffer* postfx_out = _postfxNode ? _postfxNode->GetOutput() : nullptr;
    RtBuffer* final_out  = postfx_out ? postfx_out : render_out;
    drawdata._properties["render_out"_crcu].Set<RtBuffer*>(render_out);
    drawdata._properties["render_outgroup"_crcu].Set<RtGroup*>(render_outg);
    // todo - techinically only the 'root' postfx node should get input
    //  from the render out... we need to isolate the root node somehow..
    drawdata._properties["postfx_out"_crcu].Set<RtBuffer*>(postfx_out);
    drawdata._properties["final_out"_crcu].Set<RtBuffer*>(final_out);
    ////////////////////////////////////////////////////////////////////////////
    _outputNode->beginAssemble(drawdata);
    _renderNode->Render(drawdata);
    if (_postfxNode)
      _postfxNode->Render(drawdata);
    _outputNode->endAssemble(drawdata);
  }
  drawdata.context()->debugPopGroup();
  return rval;
}
///////////////////////////////////////////////////////////////////////////////
void NodeCompositingTechnique::composite(CompositorDrawData& drawdata) {
  if (_outputNode and _renderNode) {
    auto render = _renderNode->GetOutput();
    if (render) { // todo get post...
      _outputNode->composite(drawdata);
    }
  }
}
///////////////////////////////////////////////////////////////////////////////
CompositingBuffer::CompositingBuffer() {
}
///////////////////////////////////////////////////////////////////////////////
CompositingBuffer::~CompositingBuffer() {
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void RenderCompositingNode::describeX(class_t* c) {
}
RenderCompositingNode::RenderCompositingNode() {
}
RenderCompositingNode::~RenderCompositingNode() {
}
void RenderCompositingNode::gpuInit(lev2::Context* pTARG, int w, int h) {
  doGpuInit(pTARG, w, h);
}
void RenderCompositingNode::Render(CompositorDrawData& drawdata) {
  drawdata.context()->debugPushGroup("RenderCompositingNode::Render");
  DoRender(drawdata);
  drawdata.context()->debugPopGroup();
}

void PostCompositingNode::describeX(class_t* c) {
}
PostCompositingNode::PostCompositingNode() {
}
PostCompositingNode::~PostCompositingNode() {
}
void PostCompositingNode::gpuInit(lev2::Context* pTARG, int w, int h) {
  doGpuInit(pTARG, w, h);
}
void PostCompositingNode::Render(CompositorDrawData& drawdata) {
  drawdata.context()->debugPushGroup("PostCompositingNode::Render");
  DoRender(drawdata);
  drawdata.context()->debugPopGroup();
}

void OutputCompositingNode::describeX(class_t* c) {
}
OutputCompositingNode::OutputCompositingNode() {
}
OutputCompositingNode::~OutputCompositingNode() {
}
///////////////////////////////////////////////////////////////////////////////
}} // namespace ork::lev2
///////////////////////////////////////////////////////////////////////////////
