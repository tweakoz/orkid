////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include "NodeCompositor.h"
#include <ork/lev2/gfx/gfxmaterial_fx.h>
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
void ChainCompositingNode::describeX(class_t*c) {}
///////////////////////////////////////////////////////////////////////////////
void NodeCompositingTechnique::describeX(class_t*c) {
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
  , _outputNode(nullptr) {}
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
  val = nonconst;
}
///////////////////////////////////////////////////////////////////////////////
void NodeCompositingTechnique::_writeRenderNode(ork::rtti::ICastable* const& val) {
  ork::rtti::ICastable* ptr = val;
  _renderNode = ((ptr == nullptr) ? nullptr : rtti::safe_downcast<RenderCompositingNode*>(ptr));
}
///////////////////////////////////////////////////////////////////////////////
void NodeCompositingTechnique::_readPostFxNode(ork::rtti::ICastable*& val) const {
  auto nonconst = const_cast<PostCompositingNode*>(_postfxNode);
  val = nonconst;
}
///////////////////////////////////////////////////////////////////////////////
void NodeCompositingTechnique::_writePostFxNode(ork::rtti::ICastable* const& val) {
  ork::rtti::ICastable* ptr = val;
  _postfxNode = ((ptr == nullptr) ? nullptr : rtti::safe_downcast<PostCompositingNode*>(ptr));
}
///////////////////////////////////////////////////////////////////////////////
void NodeCompositingTechnique::_readOutputNode(ork::rtti::ICastable*& val) const {
  auto nonconst = const_cast<OutputCompositingNode*>(_outputNode);
  val = nonconst;
}
///////////////////////////////////////////////////////////////////////////////
void NodeCompositingTechnique::_writeOutputNode(ork::rtti::ICastable* const& val) {
  ork::rtti::ICastable* ptr = val;
  _outputNode = ((ptr == nullptr) ? nullptr : rtti::safe_downcast<OutputCompositingNode*>(ptr));
}
///////////////////////////////////////////////////////////////////////////////
void NodeCompositingTechnique::Init(lev2::GfxTarget* pTARG, int w, int h) {
  if (_renderNode)
    _renderNode->Init(pTARG, w, h);
  if (_postfxNode)
    _postfxNode->Init(pTARG, w, h);
  if (_outputNode)
    _outputNode->gpuInit(pTARG, w, h);

  mCompositingMaterial.Init(pTARG);
}
///////////////////////////////////////////////////////////////////////////////
bool NodeCompositingTechnique::assemble(CompositorDrawData& drawdata, CompositingImpl* cimpl) {
  bool rval = false;
  if (_outputNode and _renderNode) {
    _outputNode->beginFrame(drawdata, cimpl);
    rval = true;
    _renderNode->Render(drawdata, cimpl);
    if (_postfxNode)
      _postfxNode->Render(drawdata, cimpl);
  }
  return rval;
}
///////////////////////////////////////////////////////////////////////////////
void NodeCompositingTechnique::composite(CompositorDrawData& drawdata, CompositingImpl* cimpl) {
  if (_outputNode and _renderNode) {
    auto render = _renderNode->GetOutput();
    if (render) {
      _outputNode->endFrame(drawdata, cimpl,render);
    }
  }
}
///////////////////////////////////////////////////////////////////////////////
CompositingBuffer::CompositingBuffer() {}
///////////////////////////////////////////////////////////////////////////////
CompositingBuffer::~CompositingBuffer() {}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void RenderCompositingNode::describeX(class_t*c) {}
RenderCompositingNode::RenderCompositingNode() {}
RenderCompositingNode::~RenderCompositingNode() {}
void RenderCompositingNode::Init(lev2::GfxTarget* pTARG, int w, int h) { DoInit(pTARG, w, h); }
void RenderCompositingNode::Render(CompositorDrawData& drawdata, CompositingImpl* pCCI) { DoRender(drawdata, pCCI); }

void PostCompositingNode::describeX(class_t*c) {}
PostCompositingNode::PostCompositingNode() {}
PostCompositingNode::~PostCompositingNode() {}
void PostCompositingNode::Init(lev2::GfxTarget* pTARG, int w, int h) { DoInit(pTARG, w, h); }
void PostCompositingNode::Render(CompositorDrawData& drawdata, CompositingImpl* pCCI) { DoRender(drawdata, pCCI); }

void OutputCompositingNode::describeX(class_t*c) {}
OutputCompositingNode::OutputCompositingNode() {}
OutputCompositingNode::~OutputCompositingNode() {}
///////////////////////////////////////////////////////////////////////////////
}} // namespace ork::lev2
///////////////////////////////////////////////////////////////////////////////
