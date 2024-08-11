////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/lev2/gfx/renderer/NodeCompositor/NodeCompositor.h>
#include <ork/lev2/gfx/gfxmaterial_test.h>
#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/lev2/gfx/gfxprimitives.h>
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/pch.h>
#include <ork/reflect/properties/registerX.inl>
#include <ork/rtti/downcast.h>
#include <ork/rtti/RTTI.h>
///////////////////////////////////////////////////////////////////////////////
#include <iostream>
#include <ork/asset/DynamicAssetLoader.h>
#include <ork/reflect/properties/DirectTyped.hpp>
#include <ork/reflect/enum_serializer.inl>
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/gfx/renderer/builtin_frameeffects.h>
#include <ork/profiling.inl>

///////////////////////////////////////////////////////////////////////////////
ImplementReflectionX(ork::lev2::RenderCompositingNode, "RenderCompositingNode");
ImplementReflectionX(ork::lev2::PostCompositingNode, "PostCompositingNode");
ImplementReflectionX(ork::lev2::LambdaPostCompositingNode, "LambdaPostCompositingNode");
ImplementReflectionX(ork::lev2::OutputCompositingNode, "OutputCompositingNode");
ImplementReflectionX(ork::lev2::NodeCompositingTechnique, "NodeCompositingTechnique");
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace lev2 {
///////////////////////////////////////////////////////////////////////////////
void NodeCompositingTechnique::describeX(class_t* c) {
  /*c->accessorProperty("RenderNode", &NodeCompositingTechnique::_readRenderNode, &NodeCompositingTechnique::_writeRenderNode)
      ->annotate<ConstString>("editor.factorylistbase", "RenderCompositingNode");
  c->accessorProperty("PostFxNode", &NodeCompositingTechnique::_readPostFxNode, &NodeCompositingTechnique::_writePostFxNode)
      ->annotate<ConstString>("editor.factorylistbase", "PostCompositingNode");
  c->accessorProperty("OutputNode", &NodeCompositingTechnique::_readOutputNode, &NodeCompositingTechnique::_writeOutputNode)
      ->annotate<ConstString>("editor.factorylistbase", "OutputCompositingNode");*/
}
///////////////////////////////////////////////////////////////////////////////
NodeCompositingTechnique::NodeCompositingTechnique()
    : _renderNode(nullptr)
    , _outputNode(nullptr) {
}
///////////////////////////////////////////////////////////////////////////////
NodeCompositingTechnique::~NodeCompositingTechnique() {
}
///////////////////////////////////////////////////////////////////////////////
void NodeCompositingTechnique::gpuInit(lev2::Context* pTARG, int w, int h) {
  pTARG->debugPushGroup("NodeCompositingTechnique::init");
  if (_renderNode)
    _renderNode->gpuInit(pTARG, w, h);
  for( auto pfxnode : _postEffectNodes ){
    pfxnode->gpuInit(pTARG, w, h);
  }
  if (_outputNode)
    _outputNode->gpuInit(pTARG, w, h);

  pTARG->debugPopGroup();
}
///////////////////////////////////////////////////////////////////////////////
bool NodeCompositingTechnique::assemble(CompositorDrawData& drawdata) {
  EASY_BLOCK("assemble-ctek", profiler::colors::Red);
  bool rval = false;
  drawdata.context()->debugPushGroup("NodeCompositingTechnique::assemble");
  if (_outputNode and _renderNode) {
    rval = true;
    ////////////////////////////////////////////////////////////////////////////
    // if we have a postfx_out, then that is the "final" output
    //  otherwise it is render_out
    ////////////////////////////////////////////////////////////////////////////
    rtgroup_ptr_t render_outg = _renderNode ? _renderNode->GetOutputGroup() : nullptr;
    RtBuffer* render_out      = _renderNode ? _renderNode->GetOutput().get() : nullptr;

    drawdata._properties["render_out"_crcu].set<RtBuffer*>(render_out);
    drawdata._properties["render_outgroup"_crcu].set<rtgroup_ptr_t>(render_outg);
    ////////////////////////////////////////////////////////////////////////////
    _outputNode->beginAssemble(drawdata);
    _renderNode->Render(drawdata);
    _outputNode->endAssemble(drawdata);
    for( auto pfxnode : _postEffectNodes ){
      drawdata._properties["postfx_in"_crcu].set<rtgroup_ptr_t>(render_outg);
      pfxnode->Render(drawdata);
      render_outg = pfxnode->GetOutputGroup();
      render_out      = pfxnode->GetOutput().get();
    }
    drawdata._properties["final_out"_crcu].set<RtBuffer*>(render_out);
    drawdata._properties["final_outgroup"_crcu].set<rtgroup_ptr_t>(render_outg);
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
RenderCompositingNode::RenderCompositingNode()
    : _layers("All") {
}
RenderCompositingNode::~RenderCompositingNode() {
}
void RenderCompositingNode::gpuInit(lev2::Context* pTARG, int w, int h) {
  doGpuInit(pTARG, w, h);
}
void RenderCompositingNode::Render(CompositorDrawData& drawdata) {
  _frameIndex++;
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
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void LambdaPostCompositingNode::describeX(class_t* c) {
}
LambdaPostCompositingNode::LambdaPostCompositingNode() {
}
LambdaPostCompositingNode::~LambdaPostCompositingNode() {
  OrkAssert(false);
}
void LambdaPostCompositingNode::gpuInit(lev2::Context* pTARG, int w, int h) {
  OrkAssert(false);
}
void LambdaPostCompositingNode::Render(CompositorDrawData& drawdata) {
  OrkAssert(false);
}
lev2::rtbuffer_ptr_t LambdaPostCompositingNode::GetOutput() const {
  return nullptr;
}
lev2::rtgroup_ptr_t LambdaPostCompositingNode::GetOutputGroup() const {
  return nullptr;
}
void LambdaPostCompositingNode::doGpuInit(lev2::Context* pTARG, int w, int h) {
  OrkAssert(false);
}
void LambdaPostCompositingNode::DoRender(CompositorDrawData& drawdata) {
  OrkAssert(false);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void OutputCompositingNode::describeX(class_t* c) {
}
OutputCompositingNode::OutputCompositingNode() 
  : _supersample(0) {
}
OutputCompositingNode::~OutputCompositingNode() {
}
void OutputCompositingNode::setSuperSample(int ss) {
  _supersample = ss;
}

///////////////////////////////////////////////////////////////////////////////
}} // namespace ork::lev2
///////////////////////////////////////////////////////////////////////////////
