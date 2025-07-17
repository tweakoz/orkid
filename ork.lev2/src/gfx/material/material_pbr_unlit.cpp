////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/application/application.h>
#include <ork/kernel/opq.h>
#include <ork/kernel/prop.h>
#include <ork/kernel/prop.hpp>
#include <ork/util/crc.h>
#include <ork/file/path.h>
#include <ork/lev2/gfx/camera/uicam.h>
#include <ork/lev2/gfx/dbgfontman.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/gfxenv_enum.h>
#include <ork/lev2/gfx/gfxmaterial.h>
#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/lev2/gfx/shadman.h>
#include <ork/lev2/gfx/lighting/gfx_lighting.h>
#include <ork/lev2/gfx/material_freestyle.h>
#include <ork/gfx/brdf.inl>
#include <ork/pch.h>
#include <ork/lev2/gfx/rtgroup.h>
#include <OpenImageIO/imageio.h>
#include <ork/kernel/datacache.h>
#include <ork/reflect/properties/registerX.inl>
//
#include <ork/lev2/gfx/material_pbr.inl>
#include <ork/lev2/gfx/renderer/NodeCompositor/pbr_common.h>
#include <ork/util/logger.h>

OIIO_NAMESPACE_USING

namespace ork::lev2 {

static logchannel_ptr_t logchan_pbr_unl = logger()->configureChannel("mtlpbrUNL", fvec3(0.8, 0.8, 0.1), true);

fxpipeline_ptr_t PBRMaterial::_createFxPipelineUNL(const FxPipelinePermutation& permu) const {
  fxpipeline_ptr_t pipeline;
  if (this->_tek_FWD_UNLIT_NI_MO) {
    pipeline             = std::make_shared<FxPipeline>(permu);
    pipeline->_technique = this->_tek_FWD_UNLIT_NI_MO;
    //////////////////////////////////
    pipeline->addStateLambda([this](const RenderContextInstData& RCID) {
      auto mut = const_cast<PBRMaterial*>(this);
      auto RCFD        = RCID.rcfd();
      auto context     = RCFD->GetTarget();
      auto FXI         = context->FXI();
      auto MTXI        = context->MTXI();
      //auto RSI         = context->RSI();
      const auto& CPD  = RCFD->topCPD();
      auto monocams    = CPD._mono_cam_matrices;
      auto worldmatrix = RCID.worldMatrix();
      auto modcolor    = context->RefModColor();
      FXI->bindParamVect4(this->_parModColor, modcolor * this->_baseColor);
      FXI->bindParamMatrix(this->_paramMVP, monocams->MVPMONO(worldmatrix));
      mut->_rasterstate->setCullTest(ECullTest::PASS_FRONT);
      mut->_rasterstate->setDepthTest(EDepthTest::LEQUALS);
      mut->_rasterstate->setWriteMaskZ(true);
      mut->_rasterstate->setWriteMaskRGB(true);
      mut->_rasterstate->setWriteMaskA(true);
      //RSI->BindRasterState(this->_rasterstate);
    });
  }
  return pipeline;
}

} // namespace ork::lev2 {
