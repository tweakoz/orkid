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
  return [mtl](const RenderContextInstData& RCID) {
    // printf( "BASICLAMBDA\n");
    auto context          = RCID.rcfd()->GetTarget();
    auto MTXI             = context->MTXI();
    auto FXI              = context->FXI();
    const auto& CPD       = RCID.rcfd()->topCPD();
    const auto& RCFDPROPS = RCID.rcfd()->userProperties();
    bool is_picking       = CPD.isPicking();
    bool is_stereo        = CPD.isSinglePassStereo();
    auto pbrcommon        = RCID.rcfd()->_pbrcommon;

    if (mtl->_commonOverride) {
      pbrcommon = mtl->_commonOverride;
    }

    // Per-drawable env map override (from DrawableData._environmentMapPath)
    auto envOverride = RCID._envmapOverride;
    if (envOverride) {
      static bool once = false;
      if (!once) {
        printf("\n!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
        printf("!! PBR STATE LAMBDA: ENVMAP OVERRIDE ACTIVE\n");
        printf("!! spec_array=%p diff=%p\n",
               (void*)envOverride->_filtenvSpecularMapArray.get(),
               (void*)envOverride->_filtenvDiffuseMap.get());
        printf("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n\n");
        once = true;
      }
    }
    auto spec_tex = envOverride
      ? envOverride->_filtenvSpecularMapArray
      : pbrcommon->envSpecularTexture();
    auto diff_tex = envOverride
      ? envOverride->_filtenvDiffuseMap
      : pbrcommon->envDiffuseTexture();
    float num_mips = spec_tex ? spec_tex->_num_mips : 1.0f;

    FXI->bindParamVect3(mtl->_paramAmbientLevel, pbrcommon->_ambientLevel);
    FXI->bindParamFloat(mtl->_paramSpecularLevel, envOverride ? 1.0f : pbrcommon->_specularLevel);
    FXI->bindParamFloat(mtl->_parSpecularMipBias, envOverride ? 0.0f : pbrcommon->_specularMipBias);
    FXI->bindParamFloat(mtl->_paramDiffuseLevel, envOverride ? 1.0f : pbrcommon->_diffuseLevel);
    FXI->bindParamFloat(mtl->_paramSkyboxLevel, envOverride ? 1.0f : pbrcommon->_skyboxLevel);
    FXI->bindParamTextureArray(mtl->_parMapSpecularEnv, spec_tex.get());
    FXI->bindParamTexture(mtl->_parMapDiffuseEnv, diff_tex.get());

    FXI->bindParamFloat(mtl->_parMapSpecularRufLevels, PBRMaterial::roughnessLevels);

    switch (pbrcommon->_brdftype) {
      case "BLINN"_crcu:
        FXI->bindParamTexture(mtl->_parMapBrdfIntegration, pbrcommon->_radiance_maps->_brdfIntegrationMapBlinn.get());
        // printf("PBRMaterial<%p> using BLINN brdf integration map\n", mtl);
        break;
      case "PHONG"_crcu:
        FXI->bindParamTexture(mtl->_parMapBrdfIntegration, pbrcommon->_radiance_maps->_brdfIntegrationMapPhong.get());
        // printf("PBRMaterial<%p> using PHONG brdf integration map\n", mtl);
        break;
      case "GGXVELVET"_crcu:
        FXI->bindParamTexture(mtl->_parMapBrdfIntegration, pbrcommon->_radiance_maps->_brdfIntegrationMapVelvet.get());
        // printf("PBRMaterial<%p> using GGXVELVET brdf integration map\n", mtl);
        break;
      case "GGXRIM"_crcu:
        FXI->bindParamTexture(mtl->_parMapBrdfIntegration, pbrcommon->_radiance_maps->_brdfIntegrationMapGGXRIM.get());
        // printf("PBRMaterial<%p> using GGXRIM brdf integration map\n", mtl);
        break;
      case "GGX"_crcu:
      default:
        FXI->bindParamTexture(mtl->_parMapBrdfIntegration, pbrcommon->_radiance_maps->_brdfIntegrationMapGGX.get());
        // printf("PBRMaterial<%p> using GGX brdf integration map\n", mtl);
        break;
    }

    FXI->bindParamFloat(mtl->_parEnvironmentMipBias, pbrcommon->_environmentMipBias);
    FXI->bindParamFloat(mtl->_parEnvironmentMipScale, pbrcommon->_environmentMipScale * num_mips);
    FXI->bindParamFloat(mtl->_parDepthFogDistance, pbrcommon->_depthFogDistance);
    FXI->bindParamFloat(mtl->_parDepthFogPower, pbrcommon->_depthFogPower);
    FXI->bindParamFloat(mtl->_parRoughnessPower, pbrcommon->_roughnessPower);

    /////////////////////////

    auto worldmatrix = RCID.worldMatrix();

    auto stereocams = CPD._stereo_cam_matrices;
    auto monocams   = CPD._mono_cam_matrices;

    FXI->bindParamMatrix(mtl->_paramM, worldmatrix);

    if (stereocams) {
      fmtx4 vrroot;
      if (auto as_mtx = RCID.rcfd()->tryUserProperty<fmtx4>("vrroot"_crc)) {
        vrroot = as_mtx.value();
      }

      OrkAssert(mtl->_paramVPL);
      OrkAssert(mtl->_paramVPR);

      auto VL  = stereocams->VL();
      auto VR  = stereocams->VR();
      auto VPL = stereocams->VPL();
      auto VPR = stereocams->VPR();
      if (mtl->_paramVL) {
        FXI->bindParamMatrix(mtl->_paramVL, VL);
      }
      if (mtl->_paramVR) {
        FXI->bindParamMatrix(mtl->_paramVR, VR);
      }
      FXI->bindParamMatrix(mtl->_paramVPL, VPL);
      FXI->bindParamMatrix(mtl->_paramVPR, VPR);
      FXI->bindParamMatrix(mtl->_paramMVPL, stereocams->MVPL(vrroot * worldmatrix));
      FXI->bindParamMatrix(mtl->_paramMVPR, stereocams->MVPR(vrroot * worldmatrix));

      FXI->bindParamVect3(mtl->_paramEyePostionL, VL.inverse().translation());
      FXI->bindParamVect3(mtl->_paramEyePostionR, VR.inverse().translation());
    }
    if (monocams) {
      auto eye_pos = monocams->_vmatrix.inverse().translation();
      FXI->bindParamVect3(mtl->_paramEyePostion, eye_pos);
      auto MVP = monocams->MVPMONO(worldmatrix);
      auto MV = monocams->_vmatrix * worldmatrix;

      FXI->bindParamMatrix(mtl->_paramMVP, MVP);
      FXI->bindParamMatrix(mtl->_paramMV, MV);

      auto VP = monocams->VPMONO();
      FXI->bindParamMatrix(mtl->_paramP, monocams->_pmatrix);
      FXI->bindParamMatrix(mtl->_paramV, monocams->_vmatrix);
      FXI->bindParamMatrix(mtl->_paramIV, monocams->_vmatrix.inverse());
      FXI->bindParamMatrix(mtl->_paramVP, VP);
      FXI->bindParamMatrix(mtl->_paramIVP, VP.inverse());
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

  if (auto as_bool = mtl->_vars->typedValueForKey<bool>("from_xgm")) {
    // print("hello\n");
    // OrkBreak();
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
    if(0)printf("pipetech:%s\n", pipeline->_technique->_techniqueName.c_str());
    pipeline->bindParam(mtl->_paramMROT, "RCFD_Model_Rot"_crcsh);

    auto require_pbr = mtl->_vars->typedValueForKey<bool>("requirePBRparams");

    if (require_pbr and require_pbr.value()) {
      pipeline->bindParam(mtl->_parMetallicFactor, mtl->_metallicFactor);
      pipeline->bindParam(mtl->_parRoughnessFactor, mtl->_roughnessFactor);
    }

    pipeline->_parInstanceBlock = mtl->_parInstanceBlock;

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
    printf(
        "permu-instanced<%d> skinned<%d> stereo<%d> picking<%d> vtxcolors<%d>\n",
        int(permu._instanced),
        int(permu._skinned),
        int(permu._stereo),
        int(permu._is_picking),
        int(permu._has_vtxcolors));
    printf("permu-forced_technique<%p>\n", (void*)permu._forced_technique);
    OrkAssert(false);
  }
  if (pipeline) {
    pipeline->_material_ptr = (GfxMaterial*)mtl;
    pipeline->_rasterstate  = mtl->_rasterstate;
  }

  return pipeline;
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
