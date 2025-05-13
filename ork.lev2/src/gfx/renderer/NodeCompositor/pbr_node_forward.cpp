////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "pbr_node_forward_impl.h"

ImplementReflectionX(ork::lev2::pbr::ForwardNode, "PbrForwardNode");

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::pbr {
///////////////////////////////////////////////////////////////////////////////
void ForwardNode::describeX(class_t* c) {
}
///////////////////////////////////////////////////////////////////////////////
ForwardNode::ForwardNode(pbr::commonstuff_ptr_t pbrc) {
  _impl           = std::make_shared<ForwardPbrNodeImpl>(this);
  _renderingmodel = RenderingModel("FORWARD_PBR"_crcu);
  _pbrcommon      = pbrc;
  if (_pbrcommon == nullptr) {
    _pbrcommon        = std::make_shared<pbr::CommonStuff>();
    _pbrcommon->_name = "fromForwardNode";
  }
}
///////////////////////////////////////////////////////////////////////////////
ForwardNode::~ForwardNode() {
}
///////////////////////////////////////////////////////////////////////////////
void ForwardNode::doGpuInit(lev2::Context* pTARG, int iW, int iH) {
  _impl.get<std::shared_ptr<ForwardPbrNodeImpl>>()->init(pTARG, iW, iH);
}
///////////////////////////////////////////////////////////////////////////////
void ForwardNode::DoRender(CompositorDrawData& drawdata) {
  auto impl = _impl.get<std::shared_ptr<ForwardPbrNodeImpl>>();
  impl->_render(this, drawdata);
}
///////////////////////////////////////////////////////////////////////////////
rtgroup_ptr_t ForwardNode::GetOutputGroup() const {
  auto fwd_impl   = _impl.get<std::shared_ptr<ForwardPbrNodeImpl>>();
  auto rtg_output = fwd_impl->_rtgs_main->fetch(_bufferKey);
  if (fwd_impl->_rtgs_resolve_msaa) {
    rtg_output = fwd_impl->_rtgs_resolve_msaa->fetch(_bufferKey);
  }
  return rtg_output;
}
///////////////////////////////////////////////////////////////////////////////
rtbuffer_ptr_t ForwardNode::GetOutput() const {
  return GetOutputGroup()->GetMrt(0);
}
///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::pbr
