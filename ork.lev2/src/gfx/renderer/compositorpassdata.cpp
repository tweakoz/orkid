////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
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
#include <ork/reflect/properties/register.h>
#include <ork/rtti/downcast.h>
///////////////////////////////////////////////////////////////////////////////
#include <ork/reflect/properties/DirectTypedMap.hpp>
#include <ork/reflect/properties/DirectTyped.hpp>
#include <ork/reflect/enum_serializer.inl>
///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////
CompositingPassData CompositingPassData::FromRCFD(const RenderContextFrameData& RCFD) {
  lev2::rendervar_t passdata = RCFD.getUserProperty("nodes"_crc);
  auto cstack                = passdata.get<compositingpassdatastack_t*>();
  OrkAssert(cstack != nullptr);
  return cstack->top();
}

///////////////////////////////////////////////////////////////////////////////

CompositingPassData CompositingPassData::clone() const {
  CompositingPassData cpd;
  cpd = *this;
  return cpd;
}

///////////////////////////////////////////////////////////////////////////////

const ViewportRect& CompositingPassData::GetDstRect() const {
  return mDstRect;
}
const ViewportRect& CompositingPassData::bufferRect() const {
  return mMrtRect;
}
void CompositingPassData::SetDstRect(const ViewportRect& rect) {
  mDstRect = rect;
}
void CompositingPassData::SetMrtRect(const ViewportRect& rect) {
  mMrtRect = rect;
}

///////////////////////////////////////////////////////////////////////////////

void CompositingPassData::defaultSetup(CompositorDrawData& drawdata) {
  this->AddLayer("All");
  this->mbDrawSource = true;
  this->_camera_name  = "";
  this->_clearColor  = fvec4(0, 0, 0, 0);
  int w              = drawdata._properties["OutputWidth"_crcu].get<int>();
  int h              = drawdata._properties["OutputHeight"_crcu].get<int>();
  ViewportRect tgt_rect(0, 0, w, h);
  this->SetDstRect(tgt_rect);
  bool stereo_1pass = drawdata._properties["SinglePassStereo"_crcu].get<bool>();
  this->setSinglePassStereo(stereo_1pass);
  this->_mono_cam_matrices       = nullptr;
  this->_stereo_cam_matrices = nullptr;
  if (auto try_scm = drawdata._properties["StereoMatrices"_crcu].tryAs<const StereoCameraMatrices*>()) {
    this->_stereo_cam_matrices = try_scm.value();
  }
  // bool simrunning = drawdata._properties["simrunning"_crcu].get<bool>();
  if (auto try_def = drawdata._properties["defcammtx"_crcu].tryAs<cameramatrices_ptr_t>()) {
    this->_mono_cam_matrices = try_def.value();
  }
  if (auto try_sim = drawdata._properties["simcammtx"_crcu].tryAs<cameramatrices_ptr_t>()) {
    this->_mono_cam_matrices = try_sim.value();
  }
}

///////////////////////////////////////////////////////////////////////////////

fvec3 CompositingPassData::monoCamPos(const fmtx4& vizoffsetmtx) const {
  // vizoffsetmtx : use in visual offset cases such as the heightfield
  //   (todo: elaborate on this subject)
  fmtx4 vmono = isSinglePassStereo() ? (_stereo_cam_matrices ? _stereo_cam_matrices->VMONO() : fmtx4()): (_mono_cam_matrices ? _mono_cam_matrices->_vmatrix : fmtx4());
  auto mvmono = fmtx4::multiply_ltor(vizoffsetmtx, vmono);
  fmtx4 imvmono;
  imvmono.inverseOf(mvmono);
  return imvmono.translation();
}
///////////////////////////////////////////////////////////////////////////////
fvec2 CompositingPassData::nearAndFar() const {
  cameramatrices_constptr_t mtcs = nullptr;
  if(isSinglePassStereo()){
    if(_stereo_cam_matrices){
      mtcs = _stereo_cam_matrices->_mono;
    }
  }
  else{
    mtcs = _mono_cam_matrices;
  }
    if(mtcs){
        const auto& camdat = mtcs->_camdat;
        return fvec2(camdat.mNear, camdat.mFar);
    }
    else{
        return fvec2(0,1);
    }
}
///////////////////////////////////////////////////////////////////////////////

const Frustum& CompositingPassData::monoCamFrustum() const {
  static const Frustum gfrustum;
  return _mono_cam_matrices ? _mono_cam_matrices->_frustum : gfrustum;
}

///////////////////////////////////////////////////////////////////////////////

const fvec3& CompositingPassData::monoCamZnormal() const {
  static const fvec3 gzn(0, 0, 1);
  return _mono_cam_matrices ? _mono_cam_matrices->_camdat.zNormal() : gzn;
}

///////////////////////////////////////////////////////////////////////////////

void CompositingPassData::assignLayers(const std::string& layers) {
  if (layers.length()) {
    _layernames = SplitString(layers, ',');
  } else {
    _layernames.push_back("All");
  }
  for (auto item : _layernames)
    _layernameset.insert(item);
}

///////////////////////////////////////////////////////////////////////////////

std::vector<std::string> CompositingPassData::getLayerNames() const {
  return _layernames;
}

///////////////////////////////////////////////////////////////////////////////
void CompositingPassData::updateCompositingSize(int w, int h) {
}
bool CompositingPassData::isPicking() const {
  return _ispicking;
}
void CompositingPassData::AddLayer(const std::string& layername) {
  auto it = _layernameset.find(layername);
  if (it == _layernameset.end())
    _layernames.push_back(layername);
  _layernameset.insert(layername);
}
bool CompositingPassData::HasLayer(const std::string& layername) const {
  return (_layernameset.find(layername) != _layernameset.end());
}

// void RenderContextFrameData::PushRenderTarget(IRenderTarget* ptarg) { mRenderTargetStack.push(ptarg); }
// IRenderTarget* RenderContextFrameData::GetRenderTarget() {
//  IRenderTarget* pt = mRenderTargetStack.top();
// return pt;
//}
// void RenderContextFrameData::PopRenderTarget() { mRenderTargetStack.pop(); }

// void RenderContextFrameData::setLayerName(const char* layername) {
// lev2::rendervar_t passdata;
// passdata.set<const char*>(layername);
// setUserProperty("pass"_crc, passdata);
//}

///////////////////////////////////////////////////////////////////////////////

void CompositingPassData::addStandardLayers() {
  AddLayer("Default");
  AddLayer("A");
  AddLayer("B");
  AddLayer("C");
  AddLayer("D");
  AddLayer("E");
  AddLayer("F");
  AddLayer("G");
  AddLayer("H");
  AddLayer("I");
  AddLayer("J");
  AddLayer("K");
  AddLayer("L");
  AddLayer("M");
  AddLayer("N");
  AddLayer("O");
  AddLayer("P");
  AddLayer("Q");
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
