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
#include <ork/reflect/properties/DirectTypedMap.hpp>
#include <ork/reflect/properties/DirectTyped.hpp>
#include <ork/reflect/enum_serializer.inl>
///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////
CompositingPassData CompositingPassData::FromRCFD(const RenderContextFrameData& RCFD) {
  lev2::rendervar_t passdata = RCFD.getUserProperty("nodes"_crc);
  auto cstack                = passdata.Get<compositingpassdatastack_t*>();
  OrkAssert(cstack != nullptr);
  return cstack->top();
}

///////////////////////////////////////////////////////////////////////////////

void CompositingPassData::defaultSetup(CompositorDrawData& drawdata) {
  this->AddLayer("All");
  this->mbDrawSource = true;
  this->mpFrameTek   = nullptr;
  this->_cameraName  = "";
  this->_layerName   = "";
  this->_clearColor  = fvec4(0, 0, 0, 0);
  int w              = drawdata._properties["OutputWidth"_crcu].Get<int>();
  int h              = drawdata._properties["OutputHeight"_crcu].Get<int>();
  ViewportRect tgt_rect(0, 0, w, h);
  this->SetDstRect(tgt_rect);
  bool stereo = drawdata._properties["StereoEnable"_crcu].Get<bool>();
  this->setStereoOnePass(stereo);
  this->_cameraMatrices       = nullptr;
  this->_stereoCameraMatrices = nullptr;
  if (auto try_scm = drawdata._properties["StereoMatrices"_crcu].TryAs<const StereoCameraMatrices*>()) {
    this->_stereoCameraMatrices = try_scm.value();
  } else {
    // bool simrunning = drawdata._properties["simrunning"_crcu].Get<bool>();
    if (auto try_def = drawdata._properties["defcammtx"_crcu].TryAs<const CameraMatrices*>()) {
      this->_cameraMatrices = try_def.value();
    }
    if (auto try_sim = drawdata._properties["simcammtx"_crcu].TryAs<const CameraMatrices*>()) {
      this->_cameraMatrices = try_sim.value();
    }
  }
}

///////////////////////////////////////////////////////////////////////////////

fvec3 CompositingPassData::monoCamPos(const fmtx4& vizoffsetmtx) const {
  // vizoffsetmtx : use in visual offset cases such as the heightfield
  //   (todo: elaborate on this subject)
  fmtx4 vmono = isStereoOnePass() ? _stereoCameraMatrices->VMONO() : _cameraMatrices->_vmatrix;
  auto mvmono = (vizoffsetmtx * vmono);
  fmtx4 imvmono;
  imvmono.inverseOf(mvmono);
  return imvmono.GetTranslation();
}

///////////////////////////////////////////////////////////////////////////////

const Frustum& CompositingPassData::monoCamFrustum() const {
  static const Frustum gfrustum;
  return _cameraMatrices ? _cameraMatrices->_frustum : gfrustum;
}

///////////////////////////////////////////////////////////////////////////////

const fvec3& CompositingPassData::monoCamZnormal() const {
  static const fvec3 gzn(0, 0, 1);
  return _cameraMatrices ? _cameraMatrices->_camdat.zNormal() : gzn;
}

///////////////////////////////////////////////////////////////////////////////

std::vector<std::string> CompositingPassData::getLayerNames() const {
  std::vector<std::string> out_layernames;
  if (_layerName.length()) {
    const char* layer_cstr = _layerName.c_str();
    char temp_buf[256];
    strncpy(&temp_buf[0], layer_cstr, sizeof(temp_buf));
    char* tok = strtok(&temp_buf[0], ",");
    while (tok != 0) {
      out_layernames.push_back(tok);
      tok = strtok(0, ",");
    }
  } else {
    out_layernames.push_back("All");
  }
  return out_layernames;
}

///////////////////////////////////////////////////////////////////////////////
void CompositingPassData::updateCompositingSize(int w, int h) {
  if (mpFrameTek)
    if (auto ftek = dynamic_cast<FrameTechniqueBase*>(mpFrameTek))
      ftek->update(*this, w, h);
}
bool CompositingPassData::isPicking() const {
  return _ispicking;
}
void CompositingPassData::ClearLayers() {
  mLayers.clear();
}
void CompositingPassData::AddLayer(const std::string& layername) {
  mLayers.insert(layername);
}
bool CompositingPassData::HasLayer(const std::string& layername) const {
  return (mLayers.find(layername) != mLayers.end());
}

// void RenderContextFrameData::PushRenderTarget(IRenderTarget* ptarg) { mRenderTargetStack.push(ptarg); }
// IRenderTarget* RenderContextFrameData::GetRenderTarget() {
//  IRenderTarget* pt = mRenderTargetStack.top();
// return pt;
//}
// void RenderContextFrameData::PopRenderTarget() { mRenderTargetStack.pop(); }

// void RenderContextFrameData::setLayerName(const char* layername) {
// lev2::rendervar_t passdata;
// passdata.Set<const char*>(layername);
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
