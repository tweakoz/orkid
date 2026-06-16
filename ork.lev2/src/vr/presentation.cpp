////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/lev2/vr/vr.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/dwi.h>
#include <ork/lev2/gfx/material_freestyle.h>
#include <ork/lev2/gfx/renderer/rendercontext.h>

////////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::orkidvr {
////////////////////////////////////////////////////////////////////////////////

static constexpr float kChromaEps = 1e-6f;

////////////////////////////////////////////////////////////////////////////////

void StandardVrPresentation::_resolve() const {
  if (_resolved)
    return;
  if (nullptr == _material)
    return;

  ////////////////////////////////////////
  // resolve the shader contract from the host-supplied material (once)
  ////////////////////////////////////////

  _tekAchromatic  = _material->technique(_techniqueAchromatic);
  _tekChromatic   = _material->technique(_techniqueChromatic);
  _parMVP         = _material->param("MatMVP");
  _parColorMap    = _material->param("ColorMap");
  _parLensCenter  = _material->param("LensCenter");
  _parDistortionR = _material->param("DistortionR");
  _parDistortionG = _material->param("DistortionG");
  _parDistortionB = _material->param("DistortionB");
  _resolved       = true;
}

////////////////////////////////////////////////////////////////////////////////

distortion_lambda_t StandardVrPresentation::genLambda() const {

  // the presentation is owned by the device for the app lifetime; the output
  //  node re-installs (or clears) this lambda when device()->_presentation
  //  changes, so the raw back-reference is valid while installed. Values are
  //  read live, so host-side param tweaks reflect on the next frame.
  auto self = this;

  return [self](rcfd_ptr_t RCFD, DistortionRect drect) {
    if (not self->_enable)
      return;
    auto mtl = self->_material;
    if (nullptr == mtl)
      return;
    self->_resolve();
    if (nullptr == self->_tekAchromatic)
      return;

    auto ctx = RCFD->context();
    auto fbi = ctx->FBI();
    auto dwi = ctx->DWI();
    int eye  = (drect._eye == 'L') ? 0 : 1;

    ////////////////////////////////////////
    // achromatic (r==g==b) vs chromatic technique select — CPU side, per eye.
    //  never a per-pixel branch; the achromatic shader compiles to one sample.
    ////////////////////////////////////////

    bool achroma = ((self->_distortionR - self->_distortionG).magnitudeSquared() < kChromaEps) and
                   ((self->_distortionR - self->_distortionB).magnitudeSquared() < kChromaEps);

    auto tek = (achroma or (nullptr == self->_tekChromatic)) //
                   ? self->_tekAchromatic
                   : self->_tekChromatic;

    ////////////////////////////////////////
    // present the eye : place via the per-eye viewport, rotate via MatMVP,
    //  distort + sample in the fragment shader.
    ////////////////////////////////////////

    const auto& vp = drect._out_vprect;
    ViewportRect extents(vp.miX, vp.miY, vp.miW, vp.miH);
    fbi->pushViewport(extents);
    fbi->pushScissor(extents);

    mtl->begin(tek, RCFD);
    if (self->_parColorMap)
      mtl->bindParamTexture(self->_parColorMap, drect._inp_tex);
    if (self->_parMVP)
      mtl->bindParamMatrix(self->_parMVP, self->_eyeTransform[eye]);
    if (self->_parLensCenter)
      mtl->bindParamVec2(self->_parLensCenter, self->_lensCenter[eye]);
    if (self->_parDistortionR)
      mtl->bindParamVec4(self->_parDistortionR, self->_distortionR);
    if (not achroma) {
      if (self->_parDistortionG)
        mtl->bindParamVec4(self->_parDistortionG, self->_distortionG);
      if (self->_parDistortionB)
        mtl->bindParamVec4(self->_parDistortionB, self->_distortionB);
    }
    dwi->fullscreenQuad();
    mtl->end(RCFD);

    fbi->popViewport();
    fbi->popScissor();
  };
}

////////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::orkidvr
////////////////////////////////////////////////////////////////////////////////
