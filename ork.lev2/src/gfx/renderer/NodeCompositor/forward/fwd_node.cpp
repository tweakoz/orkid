////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "fwdnode_impl.h"

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
  _pbrcommon->onGpuInit(pTARG);
  _impl.get<std::shared_ptr<ForwardPbrNodeImpl>>()->init(pTARG, iW, iH);
}
///////////////////////////////////////////////////////////////////////////////
void ForwardNode::DoRender(CompositorDrawData& drawdata) {
  auto impl = _impl.get<std::shared_ptr<ForwardPbrNodeImpl>>();
  impl->_render_top(drawdata);
}
///////////////////////////////////////////////////////////////////////////////
const std::vector<std::string>& ForwardNode::renderedLayerRoles() const {
  static const std::vector<std::string> s_base = {
      "depth_prepass",
      "std_forward",
      // std_transparent: ordered between std_forward (opaques) and the
      // overlay layers — blended content (particles, transmissive lobes)
      // composites OVER opaque geometry here.
      "std_transparent",
      "std_editor",
      "probe",
      "depth_probe",
      // hud_overlay: screen-space UI (text, debug HUD) — rendered last;
      // skipped during probe cubemap captures (cubemapCPD overrides with
      // assignLayers(probe->renderLayer())).
      "hud_overlay",
  };
  // + one "aux_<name>" role per configured aux channel (E2B item D) — the
  // role makes the CPD enqueue that layer's drawables into the draw queue
  // AND makes Scene::initWithParams pre-create the layer.
  if (_rolesCache.size() != s_base.size() + _auxChannels.size()) {
    _rolesCache = s_base;
    for (const auto& chan : _auxChannels)
      _rolesCache.push_back("aux_" + chan);
  }
  return _rolesCache;
}
///////////////////////////////////////////////////////////////////////////////
rtgroup_ptr_t ForwardNode::GetOutputGroup() const {
  auto fwd_impl   = _impl.get<std::shared_ptr<ForwardPbrNodeImpl>>();
  auto rtg_output = fwd_impl->_rtgs_primary->fetch(_bufferKey);
  /*if (fwd_impl->_rtgs_resolve_msaa) {
    rtg_output = fwd_impl->_rtgs_resolve_msaa->fetch(_bufferKey);
  }*/
  return rtg_output;
}
///////////////////////////////////////////////////////////////////////////////
rtbuffer_ptr_t ForwardNode::GetOutput() const {
  return GetOutputGroup()->buffer(0);
}
///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::pbr
