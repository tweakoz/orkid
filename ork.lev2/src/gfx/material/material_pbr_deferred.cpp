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

static logchannel_ptr_t logchan_pbr_fwd = logger()->createChannel("mtlpbrDEF", fvec3(0.8, 0.8, 0.1), true);

fxpipeline_ptr_t PBRMaterial::_createFxPipelineDEF(const FxPipelinePermutation& permu) const {
  fxpipeline_ptr_t pipeline;

  fxtechnique_constptr_t tek;
  ////////////////////////////////////////////////////////////////////////////////////////////
  auto common_lambda = [this](const RenderContextInstData& RCID, int ipass) {
    auto mut = const_cast<PBRMaterial*>(this);
    auto RCFD       = RCID.rcfd();
    auto context    = RCFD->GetTarget();
    auto RSI        = context->RSI();
    const auto& CPD = RCFD->topCPD();
    auto FXI        = context->FXI();
    auto modcolor   = RCID._modColor;
    mut->_rasterstate.SetCullTest(ECullTest::PASS_FRONT);
    mut->_rasterstate.SetDepthTest(EDepthTest::LEQUALS);
    mut->_rasterstate.SetZWriteMask(true);
    mut->_rasterstate.SetRGBAWriteMask(true, true);
    RSI->BindRasterState(this->_rasterstate);
    FXI->BindParamVect4(this->_parModColor, modcolor);
    FXI->BindParamVect4(this->_parBaseColor, this->_baseColor);
    printf( "OK1.. mc<%g %g %g %g> bc<%g %g %g %g>\n", modcolor.x, modcolor.y, modcolor.z, modcolor.w, this->_baseColor.x, this->_baseColor.y, this->_baseColor.z, this->_baseColor.w );
  };
  ////////////////////////////////////////////////////////////////////////////////////////////
  if (permu._stereo) {                         // stereo
    if (permu._instanced) {                    // stereo-instanced
      tek = permu._skinned                     //
                ? this->_tek_GBU_CT_NM_SK_IN_ST //
                : this->_tek_GBU_CT_NM_RI_IN_ST;
    } else {                                   // stereo-non-instanced
      tek = permu._skinned                     //
                ? this->_tek_GBU_CT_NM_SK_NI_ST //
                : this->_tek_GBU_CT_NM_RI_NI_ST;
    }
    //////////////////////////////////
    if (tek) {
      pipeline             = std::make_shared<FxPipeline>(permu);
      pipeline->_technique = tek;
      pipeline->addStateLambda(common_lambda);
      pipeline->addStateLambda([this](const RenderContextInstData& RCID, int ipass) {
        auto RCFD        = RCID.rcfd();
        auto context     = RCFD->GetTarget();
        auto MTXI        = context->MTXI();
        auto FXI         = context->FXI();
        auto RSI         = context->RSI();
        const auto& CPD  = RCFD->topCPD();
        auto stereocams  = CPD._stereoCameraMatrices;
        auto worldmatrix = RCID.worldMatrix();
        auto modcolor    = context->RefModColor();
        fmtx4 vrroot;
        auto vrrootprop = RCFD->getUserProperty("vrroot"_crc);
        if (auto as_mtx = vrrootprop.tryAs<fmtx4>()) {
          vrroot = as_mtx.value();
        }
        FXI->BindParamMatrix(this->_paramMVPL, stereocams->MVPL(vrroot * worldmatrix));
        FXI->BindParamMatrix(this->_paramMVPR, stereocams->MVPR(vrroot * worldmatrix));
      });
    } else {
      OrkAssert(false);
    }
    //////////////////////////////////
  }
  ///// mono ///////////////////////////////////////////////////////////////////////////////////
  else {
    // printf( "OK2.. permu._instanced<%d> skinned<%d>\n", int(permu._instanced), int(permu._skinned) );

    if (permu._instanced) {                    // mono-instanced
      tek = permu._skinned                     //
                ? this->_tek_GBU_CT_NM_SK_IN_MO //
                : this->_tek_GBU_CT_NM_RI_IN_MO;
    } else {                                   // mono-non-instanced
      tek = permu._skinned                     //
                ? this->_tek_GBU_CT_NM_SK_NI_MO //
                : this->_tek_GBU_CT_NM_RI_NI_MO;
    }
    // printf( "OK3.. mtl<%p> tek<%p>\n", mtl, tek );
    //////////////////////////////////
    if (tek) {
      pipeline             = std::make_shared<FxPipeline>(permu);
      pipeline->_technique = tek;
      pipeline->addStateLambda(common_lambda);
      pipeline->addStateLambda([this](const RenderContextInstData& RCID, int ipass) {
        auto RCFD        = RCID.rcfd();
        auto context     = RCFD->GetTarget();
        auto FXI         = context->FXI();
        auto MTXI        = context->MTXI();
        auto RSI         = context->RSI();
        const auto& CPD  = RCFD->topCPD();
        auto monocams    = CPD._cameraMatrices;
        auto worldmatrix = RCID.worldMatrix();
        auto eye_pos     = monocams->_vmatrix.inverse().translation();
        FXI->BindParamVect3(this->_paramEyePostion, eye_pos);
        FXI->BindParamMatrix(this->_paramM, worldmatrix);
        FXI->BindParamMatrix(this->_paramMVP, monocams->MVPMONO(worldmatrix));
      });
    }
  }
  // OrkAssert(pipeline->_technique != nullptr);
  return pipeline;
}

} // namespace ork::lev2
