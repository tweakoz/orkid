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

using cache_impl_t = FxPipelineCacheImpl<PBRMaterial>;

using pbrcache_impl_ptr_t = std::shared_ptr<cache_impl_t>;

static pbrcache_impl_ptr_t _getpbrcache() {
  static pbrcache_impl_ptr_t _gcache = std::make_shared<cache_impl_t>();
  return _gcache;
}

///////////////////////////////////////////////////////////////////////////////

fxpipelinecache_constptr_t PBRMaterial::_doFxPipelineCache(fxpipelinepermutation_set_constptr_t perms) const { // final
  return _getpbrcache()->getCache(this);
}

/////////////////////////////////////////////////////////////////////////

PbrMatrixBlockApplicator* PbrMatrixBlockApplicator::getApplicator() {
  static PbrMatrixBlockApplicator* _gapplicator = new PbrMatrixBlockApplicator;
  return _gapplicator;
}

///////////////////////////////////////////////////////////////////////////////

FxPipeline::statelambda_t createBasicStateLambda(const PBRMaterial* mtl) {
  return [mtl](const RenderContextInstData& RCID, int ipass) {
    //printf( "BASICLAMBDA\n");
    auto context          = RCID.rcfd()->GetTarget();
    auto MTXI             = context->MTXI();
    auto FXI              = context->FXI();
    auto RSI              = context->RSI();
    const auto& CPD       = RCID.rcfd()->topCPD();
    const auto& RCFDPROPS = RCID.rcfd()->userProperties();
    bool is_picking       = CPD.isPicking();
    bool is_stereo        = CPD.isStereoOnePass();
    auto pbrcommon        = RCID.rcfd()->_pbrcommon;

    float num_mips = pbrcommon->envSpecularTexture()->_num_mips;

    FXI->BindParamVect3(mtl->_paramAmbientLevel, pbrcommon->_ambientLevel);
    FXI->BindParamFloat(mtl->_paramSpecularLevel, pbrcommon->_specularLevel);
    FXI->BindParamFloat(mtl->_parSpecularMipBias, pbrcommon->_specularMipBias);
    FXI->BindParamFloat(mtl->_paramDiffuseLevel, pbrcommon->_diffuseLevel);
    FXI->BindParamFloat(mtl->_paramSkyboxLevel, pbrcommon->_skyboxLevel);
    FXI->BindParamCTex(mtl->_parMapSpecularEnv, pbrcommon->envSpecularTexture().get());
    FXI->BindParamCTex(mtl->_parMapDiffuseEnv, pbrcommon->envDiffuseTexture().get());
    FXI->BindParamFloat(mtl->_parEnvironmentMipBias, pbrcommon->_environmentMipBias);
    FXI->BindParamFloat(mtl->_parEnvironmentMipScale, pbrcommon->_environmentMipScale * num_mips);
    FXI->BindParamFloat(mtl->_parDepthFogDistance, pbrcommon->_depthFogDistance);
    FXI->BindParamFloat(mtl->_parDepthFogPower, pbrcommon->_depthFogPower);
    FXI->BindParamFloat(mtl->_parRoughnessPower, pbrcommon->_roughnessPower);

    /////////////////////////

    auto worldmatrix = RCID.worldMatrix();

    auto stereocams = CPD._stereoCameraMatrices;
    auto monocams   = CPD._cameraMatrices;

    FXI->BindParamMatrix(mtl->_paramM, worldmatrix);

    if (stereocams) {
      fmtx4 vrroot;
      auto vrrootprop = RCID.rcfd()->getUserProperty("vrroot"_crc);
      if (auto as_mtx = vrrootprop.tryAs<fmtx4>()) {
        vrroot = as_mtx.value();
      }

      OrkAssert(mtl->_paramVPL);
      OrkAssert(mtl->_paramVPR);

      auto VL  = stereocams->VL();
      auto VR  = stereocams->VR();
      auto VPL = stereocams->VPL();
      auto VPR = stereocams->VPR();
      if (mtl->_paramVL) {
        FXI->BindParamMatrix(mtl->_paramVL, VL);
      }
      if (mtl->_paramVR) {
        FXI->BindParamMatrix(mtl->_paramVR, VR);
      }
      FXI->BindParamMatrix(mtl->_paramVPL, VPL);
      FXI->BindParamMatrix(mtl->_paramVPR, VPR);
      FXI->BindParamMatrix(mtl->_paramMVPL, stereocams->MVPL(vrroot * worldmatrix));
      FXI->BindParamMatrix(mtl->_paramMVPR, stereocams->MVPR(vrroot * worldmatrix));

      FXI->BindParamVect3(mtl->_paramEyePostionL, VL.inverse().translation());
      FXI->BindParamVect3(mtl->_paramEyePostionR, VR.inverse().translation());

    } else if (monocams) {
      auto eye_pos = monocams->_vmatrix.inverse().translation();
      FXI->BindParamVect3(mtl->_paramEyePostion, eye_pos);
      FXI->BindParamMatrix(mtl->_paramMVP, monocams->MVPMONO(worldmatrix));

      auto VP = monocams->VPMONO();
      // FXI->BindParamMatrix(mtl->_paramP, monocams->_pmatrix);
      FXI->BindParamMatrix(mtl->_paramV, monocams->_vmatrix);
      // FXI->BindParamMatrix(mtl->_paramIV, monocams->_ivmatrix);
      FXI->BindParamMatrix(mtl->_paramVP, VP);
      FXI->BindParamMatrix(mtl->_paramIVP, VP.inverse());
    }
  };
}

///////////////////////////////////////////////////////////////////////////////

void PBRMaterial::addBasicStateLambda(fxpipeline_ptr_t pipe) {
  auto L = createBasicStateLambda(this);
  pipe->addStateLambda(L);
}

///////////////////////////////////////////////////////////////////////////////

void PBRMaterial::addBasicStateLambda() {
  auto L = createBasicStateLambda(this);
  _state_lambdas.push_back(L);
}

///////////////////////////////////////////////////////////////////////////////

fxpipeline_ptr_t PBRMaterial::_createFxPipeline(const FxPipelinePermutation& permu, const PBRMaterial* mtl) {

  fxpipeline_ptr_t pipeline;

  if (0 == strcmp(mtl->mMaterialName.c_str(), "Material.001")) {
    // printf( "yo\n");
  }

  bool is_picking = permu._is_picking;
  if (is_picking) {
    // OrkBreak();
  }

  mtl->_vars->makeValueForKey<bool>("requirePBRparams") = true;

  if( auto as_bool = mtl->_vars->typedValueForKey<bool>("from_xgm") ){
    //print("hello\n");
    //OrkBreak();
  }


  switch (mtl->_variant) {
    case 0: { 
      //////////////////////////////////////////
      // STANDARD VARIANT
      //  standard variant just means
      //  no overriding is occuring, aka default behavior
      //////////////////////////////////////////
      switch (permu._rendering_model) { // rendering/lighting model of frame
        //////////////////////////////////////////
        case "PICKING"_crcu: {
          // rendering std pbr material to pickbuffer
          pipeline = mtl->_createFxPipelinePIK(permu);
          break;
        }
        //////////////////////////////////////////
        case "FORWARD_UNLIT"_crcu:
        case 0: {
          // rendering std pbr material to forward, unlit frame
          pipeline = mtl->_createFxPipelineUNL(permu);
          break;
        }
        //////////////////////////////////////////
        case "DEFERRED_PBR"_crcu: {
          if (is_picking) {
            OrkAssert(false);
          }
          // rendering std pbr material to deferred PBR frame
          pipeline = mtl->_createFxPipelineDEF(permu);
          break;
        }
        //////////////////////////////////////////
        case "FORWARD_PBR"_crcu: {
          // rendering std pbr material to forward PBR frame
          pipeline = mtl->_createFxPipelineFWD(permu);
          break;
        }
        //////////////////////////////////////////
        case "DEPTH_PREPASS"_crcu:
          // rendering std pbr material to depth prepass frame
          pipeline = mtl->_createFxPipelineDPP(permu);
          break;
        //////////////////////////////////////////
        default:
          OrkAssert(false);
          break;
          //////////////////////////////////////////
      } 
      break;
    } 
    //////////////////////////////////////////
    // special variants
    //////////////////////////////////////////
    case "skybox.forward"_crcu: { // FORWARD SKYBOX VARIANT
      pipeline = mtl->_createFxPipelineSKY(permu);
      break;
    }
    //////////////////////////////////////////
    case "normalviz"_crcu:
      OrkAssert(false);
      break;
    //////////////////////////////////////////
    case "vertexcolor"_crcu: {
      pipeline = mtl->_createFxPipelineVTX(permu);
      break;
    }
    //////////////////////////////////////////
    case "font"_crcu:
      OrkAssert(false);
      break;
    //////////////////////////////////////////
    case "font-instanced"_crcu:
      OrkAssert(false);
      break;
    //////////////////////////////////////////
    default:
      OrkAssert(false);
      break;
  }

  /////////////////////////////////////////////////////////////////////////////
  // 
  /////////////////////////////////////////////////////////////////////////////

  if (pipeline and pipeline->_technique) {

    pipeline->bindParam(mtl->_paramMROT, "RCFD_Model_Rot"_crcsh);

    auto require_pbr = mtl->_vars->typedValueForKey<bool>("requirePBRparams");

    if (require_pbr and require_pbr.value()) {
      pipeline->bindParam(mtl->_paramMapCNMREA, mtl->_texArrayCNMREA);
      pipeline->bindParam(mtl->_parMetallicFactor, mtl->_metallicFactor);
      pipeline->bindParam(mtl->_parRoughnessFactor, mtl->_roughnessFactor);
    }

    pipeline->_parInstanceMatrixMap = mtl->_paramInstanceMatrixMap;
    pipeline->_parInstanceIdMap     = mtl->_paramInstanceIdMap;
    pipeline->_parInstanceColorMap  = mtl->_paramInstanceColorMap;

    pipeline->_material             = (GfxMaterial*)mtl;

    for (auto l : mtl->_state_lambdas) {
      pipeline->addStateLambda(l);
    }
    for (auto item : mtl->_bound_params) {
      pipeline->bindParam(item.first, item.second);
    }

  }
  /////////////////////////////////////////////////////////////////////////////
  // DEBUG pipeline creation failure
  /////////////////////////////////////////////////////////////////////////////
   else {
    std::string rmodelstr, variantstr;

    switch (permu._rendering_model) { // rendering/lighting model of frame
      case "DEFERRED_PBR"_crcu:
        rmodelstr = "DEFERRED_PBR";
        break;
      case "FORWARD_PBR"_crcu:
        rmodelstr = "FORWARD_PBR";
        break;
      case "FORWARD_UNLIT"_crcu:
        rmodelstr = "FORWARD_UNLIT";
        break;
      case "PICKING"_crcu:
        rmodelstr = "PICKING";
        break;
      case "DEPTH_PREPASS"_crcu:
        rmodelstr = "DEPTH_PREPASS";
        break;
      default:
        rmodelstr = "UNKNOWN";
        break;
    }
    switch (mtl->_variant) { // variant of material
      case 0:
        variantstr = "standard";
        break;
      case "skybox.forward"_crcu:
        variantstr = "skybox.forward";
        break;
      case "normalviz"_crcu:
        variantstr = "normalviz";
        break;
      case "vertexcolor"_crcu:
        variantstr = "vertexcolor";
        break;
      case "font"_crcu:
        variantstr = "font";
        break;
      case "font-instanced"_crcu:
        variantstr = "font-instanced";
        break;
      default:
        variantstr = "UNKNOWN";
        break;
    }
    auto shfilename = mtl->_shader->GetName();
    printf(
        "No PIPELINE for mtl<%s> shfile<%s> variant<%08x:%s> shsuffix<%s>\n",
        mtl->mMaterialName.c_str(),
        shfilename,
        mtl->_variant,
        variantstr.c_str(),
        mtl->_shader_suffix.c_str());
    printf("permu-renderingmodel<%08x:%s>\n", permu._rendering_model, rmodelstr.c_str());
    printf("permu-instanced<%d> skinned<%d> stereo<%d> picking<%d> vtxcolors<%d>\n", int(permu._instanced), int(permu._skinned), int(permu._stereo), int(permu._is_picking), int(permu._has_vtxcolors));
    printf("permu-forced_technique<%p>\n", (void*) permu._forced_technique );
    OrkAssert(false);
  }

  return pipeline;
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2 {
