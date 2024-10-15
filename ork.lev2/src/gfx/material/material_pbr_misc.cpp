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
///////////////////////////////////////////////////////////////////////////////

fxpipeline_ptr_t PBRMaterial::_createFxPipelineSKY(const FxPipelinePermutation& permu) const {
  fxpipeline_ptr_t pipeline;
  auto basic_lambda  = createBasicStateLambda(this);
  auto skybox_lambda = [this, basic_lambda](const RenderContextInstData& RCID, int ipass) {
    auto mut       = (PBRMaterial*)this;
    auto RCFD      = RCID.rcfd();
    auto context   = RCFD->GetTarget();
    auto FXI       = context->FXI();
    auto MTXI      = context->MTXI();
    auto RSI       = context->RSI();
    auto pbrcommon = RCFD->_pbrcommon;
    auto envtex    = pbrcommon->envSpecularTexture();

    FXI->BindParamCTex(this->_parMapSpecularEnv, envtex.get());

    basic_lambda(RCID, ipass);
    mut->_rasterstate.SetCullTest(ECullTest::OFF);
    mut->_rasterstate.SetZWriteMask(false);
    mut->_rasterstate.SetDepthTest(EDepthTest::LEQUALS);
    mut->_rasterstate.SetRGBAWriteMask(true, true);
    RSI->BindRasterState(this->_rasterstate);
  };
  //////////////////////////////////////////////////////////
  OrkAssert(permu._instanced == false);
  OrkAssert(permu._skinned == false);
  //////////////////////////////////////////////////////////
  if (permu._stereo and (not permu._vr_mono) and this->_tek_FWD_SKYBOX_ST) {
    auto pipeline_stereo        = std::make_shared<FxPipeline>(permu);
    pipeline_stereo->_technique = this->_tek_FWD_SKYBOX_ST;
    pipeline_stereo->bindParam(this->_paramIVPL, "RCFD_Camera_IVP_Left"_crcsh);
    pipeline_stereo->bindParam(this->_paramIVPR, "RCFD_Camera_IVP_Right"_crcsh);
    pipeline_stereo->addStateLambda(skybox_lambda);
    pipeline_stereo->_material = (GfxMaterial*)this;
    pipeline                   = pipeline_stereo;
  } else if (this->_tek_FWD_SKYBOX_MO) {
    auto pipeline_stereo        = std::make_shared<FxPipeline>(permu);
    pipeline_stereo->_technique = this->_tek_FWD_SKYBOX_MO;
    pipeline_stereo->bindParam(this->_paramIVP, "RCFD_Camera_IVP_Mono"_crcsh);
    pipeline_stereo->addStateLambda(skybox_lambda);
    pipeline_stereo->_material = (GfxMaterial*)this;
    pipeline                   = pipeline_stereo;
  }

  return pipeline;
}

///////////////////////////////////////////////////////////////////////////////

fxpipeline_ptr_t PBRMaterial::_createFxPipelineVTX(const FxPipelinePermutation& permu) const {
  fxpipeline_ptr_t pipeline;
  auto no_cull_stateblock = [this](const RenderContextInstData& RCID, int ipass) {
    auto mut   = (PBRMaterial*)this;
    auto RCFD    = RCID.rcfd();
    auto context = RCFD->GetTarget();
    auto FXI     = context->FXI();
    auto MTXI    = context->MTXI();
    auto RSI     = context->RSI();
    mut->_rasterstate.SetCullTest(ECullTest::OFF);
    mut->_rasterstate.SetDepthTest(EDepthTest::OFF);
    mut->_rasterstate.SetZWriteMask(true);
    mut->_rasterstate.SetRGBAWriteMask(true, false);
    RSI->BindRasterState(this->_rasterstate);
  };
  switch (permu._rendering_model) {
    case "FORWARD_PBR"_crcu: {
      if (not permu._instanced and not permu._skinned and not permu._stereo) {
        if (this->_tek_FWD_CV_EMI_RI_NI_MO) {
          pipeline             = std::make_shared<FxPipeline>(permu);
          pipeline->_technique = this->_tek_FWD_CV_EMI_RI_NI_MO;
          pipeline->bindParam(this->_paramMVP, "RCFD_Camera_MVP_Mono"_crcsh);
          pipeline->addStateLambda(createForwardLightingLambda(this));
          pipeline->addStateLambda(createBasicStateLambda(this));
          pipeline->addStateLambda(no_cull_stateblock);
          OrkAssert(pipeline->_technique != nullptr);
        }
      }
      break;
    }
    case "DEFERRED_PBR"_crcu: {
      if (not permu._instanced and not permu._skinned and not permu._stereo) {
        if (this->_tek_GBU_CV_EMI_RI_NI_MO) {
          pipeline             = std::make_shared<FxPipeline>(permu);
          pipeline->_technique = this->_tek_GBU_CV_EMI_RI_NI_MO;
          pipeline->bindParam(this->_paramMVP, "RCFD_Camera_MVP_Mono"_crcsh);
          pipeline->addStateLambda(createBasicStateLambda(this));
          pipeline->addStateLambda(no_cull_stateblock);
          OrkAssert(pipeline->_technique != nullptr);
        }
      }
      break;
    }
    case "PICKING"_crcu: {
      if (not permu._instanced and not permu._skinned and not permu._stereo) {
        if (this->_tek_PIK_RI_NI) {
          pipeline             = std::make_shared<FxPipeline>(permu);
          pipeline->_technique = this->_tek_PIK_RI_NI;
          pipeline->bindParam(this->_paramMVP, "RCFD_Camera_Pick"_crcsh);
          OrkAssert(pipeline->_technique != nullptr);
        }
      }
      break;
    }
    default:
      break;
  }
  return pipeline;
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
