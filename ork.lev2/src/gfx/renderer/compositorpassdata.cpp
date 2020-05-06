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
#include <ork/reflect/DirectObjectMapPropertyType.hpp>
#include <ork/reflect/DirectObjectPropertyType.hpp>
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
  this->AddLayer("All"_pool);
  this->mbDrawSource = true;
  this->mpFrameTek   = nullptr;
  this->mpCameraName = nullptr;
  this->mpLayerName  = nullptr;
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

std::vector<PoolString> CompositingPassData::getLayerNames() const {
  std::vector<PoolString> LayerNames;
  if (mpLayerName) {
    const char* layername = mpLayerName->c_str();
    if (layername) {
      char temp_buf[256];
      strncpy(&temp_buf[0], layername, sizeof(temp_buf));
      char* tok = strtok(&temp_buf[0], ",");
      while (tok != 0) {
        LayerNames.push_back(AddPooledString(tok));
        tok = strtok(0, ",");
      }
    }
  } else {
    LayerNames.push_back(AddPooledLiteral("All"));
  }
  return LayerNames;
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
void CompositingPassData::AddLayer(const PoolString& layername) {
  mLayers.insert(layername);
}
bool CompositingPassData::HasLayer(const PoolString& layername) const {
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
  AddLayer("Default"_pool);
  AddLayer("A"_pool);
  AddLayer("B"_pool);
  AddLayer("C"_pool);
  AddLayer("D"_pool);
  AddLayer("E"_pool);
  AddLayer("F"_pool);
  AddLayer("G"_pool);
  AddLayer("H"_pool);
  AddLayer("I"_pool);
  AddLayer("J"_pool);
  AddLayer("K"_pool);
  AddLayer("L"_pool);
  AddLayer("M"_pool);
  AddLayer("N"_pool);
  AddLayer("O"_pool);
  AddLayer("P"_pool);
  AddLayer("Q"_pool);
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
