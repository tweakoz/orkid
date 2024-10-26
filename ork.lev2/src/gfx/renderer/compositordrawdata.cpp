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
///////////////////////////////////////////////////////////////////////////////

CompositorDrawData::CompositorDrawData(rcfd_ptr_t rcfd){  
  _RCFD = rcfd ? rcfd : std::make_shared<RenderContextFrameData>(nullptr);
}
rcfd_ptr_t CompositorDrawData::RCFD() const {
  return _RCFD;
}
Context* CompositorDrawData::context() const {
  return _RCFD->GetTarget();
}

const svar16_t& CompositorDrawData::property(uint64_t key) const{
  static const svar16_t NONE;
  auto it = _properties.find(key);
  return (it != _properties.end()) ? it->second : NONE;
}

///////////////////////////////////////////////////////////////////////////////

ViewData CompositorDrawData::computeViewData() const {
  auto CIMPL        = this->_cimpl;
  const auto TOPCPD = CIMPL->topCPD();
  ViewData VD;
  VD._isStereo   = TOPCPD.isStereoOnePass();
  VD._camposmono = TOPCPD.monoCamPos(fmtx4());

  auto nf = TOPCPD.nearAndFar();

  VD._near = nf.x;
  VD._far = nf.y;
  VD._time = _RCFD->getUserProperty("time"_crc).get<float>();

  if (VD._isStereo) {
    auto L = TOPCPD._stereoCameraMatrices->_left;
    auto R = TOPCPD._stereoCameraMatrices->_right;
    auto M = TOPCPD._stereoCameraMatrices->_mono;

    VD.VM  = M->_vmatrix;
    VD.PM  = M->_pmatrix;
    VD.VPM = fmtx4::multiply_ltor(VD.VM,VD.PM);

    VD.VL  = L->_vmatrix;
    VD.PL  = L->_pmatrix;
    VD.VPL = fmtx4::multiply_ltor(VD.VL,VD.PL);

    VD.VR  = R->_vmatrix;
    VD.PR  = R->_pmatrix;
    VD.VPR = fmtx4::multiply_ltor(VD.VR,VD.PR);
    // VR projection matrix
    //[ +0.7842  +0  +0  +0 ] [ +0  +0.7048  +0  +0 ] [ -0.05671  +0.0023  -1  -1 ] [ +0  +0  -0.1  +0 ]   axis<-0.001 -0.04 -0>
  } else {
    auto M = TOPCPD._cameraMatrices;
    VD.VM  = M->_vmatrix;
    VD.PM  = M->_pmatrix;
    VD.VL  = VD.VM;
    VD.VR  = VD.VM;
    VD.PL  = VD.PM;
    VD.PR  = VD.PM;
    VD.VPM = fmtx4::multiply_ltor(VD.VM,VD.PM);
    VD.VPL = VD.VPM;
    VD.VPR = VD.VPM;
    // editor projection matrix
    //[ -1.433  +0  +0  +0 ] [ +0  +2.748  +0  +0 ] [ +0  +0  +1  +1 ] [ +0  +0  -17.01  +0 ]   axis<-0 -0 -0> angle<72>
    // printf("%g %g\n", VD._zndc2eye.x, VD._zndc2eye.y);
  }
  VD.IVPM.inverseOf(VD.VPM);
  VD.IVPL.inverseOf(VD.VPL);
  VD.IVPR.inverseOf(VD.VPR);
  VD._v[0]   = VD.VL;
  VD._v[1]   = VD.VR;
  VD._p[0]   = VD.PL; //_p[0].Transpose();
  VD._p[1]   = VD.PR; //_p[1].Transpose();
  VD._ivp[0] = VD.IVPL;
  VD._ivp[1] = VD.IVPR;

  fmtx4 IVL;
  IVL.inverseOf(VD.VL);
  VD._camposmono = IVL.column(3).xyz();

  const auto& PZ = VD._p[0];
  VD._zndc2eye   = fvec2(PZ.elemXY(3, 2), PZ.elemXY(2, 2));

  return VD;
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
///////////////////////////////////////////////////////////////////////////////
