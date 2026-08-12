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

///////////////////////////////////////////////////////////////////////////////

fxpipelinecache_ptr_t _evictPbrPipelineCache(const PBRMaterial* mtl) {
  return _getpbrcache()->removeCache(mtl);
}

/////////////////////////////////////////////////////////////////////////

PbrMatrixBlockApplicator* PbrMatrixBlockApplicator::getApplicator() {
  static PbrMatrixBlockApplicator* _gapplicator = new PbrMatrixBlockApplicator;
  return _gapplicator;
}

///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// The ambient's fail-loud (owner ruling 10). TWO ways a draw can end up with no
// diffuse ambient, and neither may be silent:
//
//   no projection   the bound radiance maps carry no sky yet. Expected for the
//                   handful of warm-up frames before the first publish, and a
//                   defect after that.
//   no EnvSH param  this material's shader does not expose the ambient block at
//                   all, so the bind below is a no-op and the fragment shades
//                   against whatever the UBO happened to hold. This is the
//                   exact shape of the bug that let grid/concrete keep an old
//                   ambient under a new sky, which is why the handle is checked
//                   rather than assumed.
//
// One line per material, ever: the condition is a property of the material and
// the frame, not of the draw, and a per-draw line is a spam channel. Render
// thread only (state lambdas run nowhere else), hence the plain set.
///////////////////////////////////////////////////////////////////////////////
static void _warnNoAmbient(const PBRMaterial* mtl, bool have_sh) {
  static std::set<std::string> s_warned;
  if (not s_warned.insert(mtl->mMaterialName).second)
    return;
  printf(
      "WARNING: PBR ambient unreachable for material<%s> shader<%s>: %s. Every fragment lit by this "
      "draw has zero diffuse ambient.\n",
      mtl->mMaterialName.c_str(),
      mtl->_shaderpath.c_str(),
      (nullptr == mtl->_parEnvSH) ? "the shader exposes no EnvSH parameter (the bind is a no-op)"
                                  : "the bound radiance maps carry no sky projection");
  fflush(stdout);
}

///////////////////////////////////////////////////////////////////////////////
// SINGLE-PASS STEREO — ublk_stereo host layout and its writer.
//
// std140, one entry per VIEW. Offsets are hand-mirrored here because the block
// declaration lives in stdtools.i2 and the write lives here; the static_assert
// below is what keeps the two from drifting apart silently.
//
// The write is skipped when the matrices match the last write. That is not an
// optimization detail: this lambda runs PER DRAW, and mapping/unmapping the
// shared buffer thousands of times a frame to rewrite identical bytes would put
// a per-draw cost on a per-frame value. Two passes with different eye cameras in
// one frame still each get their write, because the guard compares CONTENT, not
// a frame index.
///////////////////////////////////////////////////////////////////////////////

static constexpr size_t k_stereo_off_vp     = 0x000; // mat4 spvr_vp[2]
static constexpr size_t k_stereo_off_invvp  = 0x080; // mat4 spvr_inv_vp[2]
static constexpr size_t k_stereo_off_eyepos = 0x100; // vec4 spvr_eyepos[2]
static constexpr size_t k_stereo_size       = 0x120;
static_assert(k_stereo_size <= PBRMaterial::kStereoDataBufferBytes, "ublk_stereo outgrew its buffer");

void PBRMaterial::writeStereoBlock(
    FxInterface* FXI,                        //
    Context* context,                        //
    const StereoCameraMatrices* stereocams) {

  struct StereoBlockShadow {
    fmtx4 _vp[2];
    fmtx4 _ivp[2];
    fvec4 _eyepos[2];
    bool _valid = false;
  };
  static StereoBlockShadow s_last;

  StereoBlockShadow next;
  next._vp[0]     = stereocams->VPL();
  next._vp[1]     = stereocams->VPR();
  next._ivp[0]    = next._vp[0].inverse();
  next._ivp[1]    = next._vp[1].inverse();
  next._eyepos[0] = fvec4(stereocams->VL().inverse().translation(), 1.0f);
  next._eyepos[1] = fvec4(stereocams->VR().inverse().translation(), 1.0f);
  next._valid     = true;

  if (s_last._valid                                                //
      and (0 == memcmp(s_last._vp, next._vp, sizeof(next._vp)))     //
      and (0 == memcmp(s_last._eyepos, next._eyepos, sizeof(next._eyepos))))
    return;

  auto buffer = PBRMaterial::stereoDataBuffer(context);
  auto mapped = FXI->mapUniformBuffer(buffer, 0, k_stereo_size);
  for (int i = 0; i < 2; i++) {
    mapped->ref<fmtx4>(k_stereo_off_vp + i * sizeof(fmtx4))     = next._vp[i];
    mapped->ref<fmtx4>(k_stereo_off_invvp + i * sizeof(fmtx4))  = next._ivp[i];
    mapped->ref<fvec4>(k_stereo_off_eyepos + i * sizeof(fvec4)) = next._eyepos[i];
  }
  mapped->unmap();
  s_last = next;
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
        printf("!! spec_array=%p sh_valid=%d\n",
               (void*)envOverride->_filtenvSpecularMapArray.get(),
               int(envOverride->_shValid));
        printf("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n\n");
        once = true;
      }
    }
    auto spec_tex = envOverride
      ? envOverride->_filtenvSpecularMapArray
      : pbrcommon->envSpecularTexture();
    // Crossfade out of the previous procedural IBL set. An override aliases
    // itself at weight 1 — it owns its maps and never refilters.
    auto spec_tex_prev = envOverride
      ? spec_tex
      : pbrcommon->envSpecularTexturePrev();
    float num_mips = spec_tex ? spec_tex->_num_mips : 1.0f;

    FXI->bindParamVect3(mtl->_paramAmbientLevel, pbrcommon->_ambientLevel);
    FXI->bindParamFloat(mtl->_paramSpecularLevel, envOverride ? 1.0f : pbrcommon->_specularLevel);
    FXI->bindParamFloat(mtl->_parSpecularMipBias, envOverride ? 0.0f : pbrcommon->_specularMipBias);
    FXI->bindParamFloat(mtl->_paramDiffuseLevel, envOverride ? 1.0f : pbrcommon->_diffuseLevel);
    FXI->bindParamFloat(mtl->_paramSkyboxLevel, envOverride ? 1.0f : pbrcommon->_skyboxLevel);
    // THE DIRECT DIFFUSE LOBE. Bound on EVERY draw for the same reason the SH
    // block below is: ublk_std_pbr is shared by every PBR consumer, so a frame
    // that skipped the write would shade against whatever the last one left.
    // An envmap override changes where the light comes from, not which lobe
    // scatters it, so it reads the scene's model unconditionally.
    FXI->bindParamInt(mtl->_paramDiffuseBrdfModel, int(pbrcommon->_diffuseBrdfModel));
    FXI->bindParamTextureArray(mtl->_parMapSpecularEnv, spec_tex.get());
    FXI->bindParamTextureArray(mtl->_parMapSpecularEnvPrev, spec_tex_prev.get());
    FXI->bindParamFloat(mtl->_parEnvBlendWeight, envOverride ? 1.0f : pbrcommon->envCrossfadeWeight());
    // an override owns its own baked maps and never went through the
    // procedural capture, so the pre-scale decode is a no-op for it.
    FXI->bindParamFloat(mtl->_parEnvCaptureScaleInv, envOverride ? 1.0f : pbrcommon->envCaptureScaleInv());

    // THE DIFFUSE AMBIENT (W4-S8, unified by W4-S9) — nine L2 coefficients, the
    // ONE source there is. Bound on EVERY draw, never conditionally: the block
    // is shared by every PBR consumer, so a frame that skipped the write would
    // shade against whatever the last one left. A per-drawable override lights
    // from ITS OWN map set's projection, which is the same ambient its
    // reflections come from.
    fvec4 sh_coeffs[9] = {};
    bool have_sh       = false;
    if (envOverride) {
      // an override's maps are authored — they never went through the
      // procedural capture, so there is no pre-scale to divide out.
      have_sh = envOverride->_shValid;
      for (int i = 0; i < 9; i++)
        sh_coeffs[i] = envOverride->_shCoeffs[i];
    } else {
      have_sh = pbrcommon->envSHCoeffs(sh_coeffs);
    }
    FXI->bindParamVect4Array(mtl->_parEnvSH, sh_coeffs, 9);
    FXI->bindParamFloat(mtl->_parEnvSHValid, have_sh ? 1.0f : 0.0f);
    // NO SILENT FALLBACK (owner ruling 10): there is one ambient pipeline, so a
    // draw that cannot reach it has no ambient at all and says so by name.
    if ((not have_sh) or (nullptr == mtl->_parEnvSH))
      _warnNoAmbient(mtl, have_sh);

    // SKYLIGHT B3: the roughness-level count indexes the specular array bound
    // above, so it must come from the SAME maps the accessors just served — the
    // procedural set once its feed has published, the baked set otherwise
    // (identical to _radiance_maps in every baked scene). The per-drawable
    // override keeps its historical behaviour of reading these from the scene's
    // maps, not the override's.
    auto active_maps = pbrcommon->activeRadianceMaps();

    float actual_roughness_levels = float(active_maps->_numRoughnessLevels);
    if (actual_roughness_levels < 1.0f) actual_roughness_levels = PBRMaterial::roughnessLevels;
    FXI->bindParamFloat(mtl->_parMapSpecularRufLevels, actual_roughness_levels);

    switch (pbrcommon->_brdftype) {
      case "BLINN"_crcu:
        FXI->bindParamTexture(mtl->_parMapBrdfIntegration, active_maps->_brdfIntegrationMapBlinn.get());
        // printf("PBRMaterial<%p> using BLINN brdf integration map\n", mtl);
        break;
      case "PHONG"_crcu:
        FXI->bindParamTexture(mtl->_parMapBrdfIntegration, active_maps->_brdfIntegrationMapPhong.get());
        // printf("PBRMaterial<%p> using PHONG brdf integration map\n", mtl);
        break;
      case "GGXVELVET"_crcu:
        FXI->bindParamTexture(mtl->_parMapBrdfIntegration, active_maps->_brdfIntegrationMapVelvet.get());
        // printf("PBRMaterial<%p> using GGXVELVET brdf integration map\n", mtl);
        break;
      case "GGXRIM"_crcu:
        FXI->bindParamTexture(mtl->_parMapBrdfIntegration, active_maps->_brdfIntegrationMapGGXRIM.get());
        // printf("PBRMaterial<%p> using GGXRIM brdf integration map\n", mtl);
        break;
      case "GGX"_crcu:
      default:
        FXI->bindParamTexture(mtl->_parMapBrdfIntegration, active_maps->_brdfIntegrationMapGGX.get());
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
      // The walker/world-root offset is folded into the eye VIEW matrices themselves
      // (Device::_updatePosesCommon composes usermtx into cmv, fed by the VR output node),
      // so every draw path — this standard one and the SSBO-custom mono paths alike —
      // transforms a true-WORLD position by the eye camera. No per-draw root compose.
      //
      // SINGLE-PASS STEREO producer: the per-view state goes into the ublk_stereo UBO,
      // which the multiview shader variant indexes by gl_ViewIndex. NOTHING per-draw goes
      // in there — mvp is rebuilt in the shader as spvr_vp[view] * m — so the write is
      // skipped whenever the matrices are unchanged from the last one, which is every
      // draw after the first of a pass.
      PBRMaterial::writeStereoBlock(FXI, context, stereocams);
      if (mtl->_parStereoBlock) {
        FXI->bindUniformBuffer(mtl->_parStereoBlock, PBRMaterial::stereoDataBuffer(context));
      }
    }
    if (monocams) {
      auto eye_pos = monocams->_vmatrix.inverse().translation();
      FXI->bindParamVect3(mtl->_paramEyePostion, eye_pos);
      auto MVP = monocams->MVPMONO(worldmatrix);
      auto MV = monocams->_vmatrix * worldmatrix;

      FXI->bindParamMatrix(mtl->_paramMVP, MVP);
      FXI->bindParamMatrix(mtl->_paramMV, MV);
      // normal matrix (object->view) = inverse-transpose of MV. mvit_rot (3x3) is what
      // the fragment uses for view-space normals; the full mat4 is kept for callers
      // that transform more than a direction. Null handle (shader doesn't declare it)
      // -> bind is a no-op, so this is free for materials that don't read them.
      auto MVIT = MV.inverse().transposed();
      FXI->bindParamMatrix(mtl->_paramMVIT, MVIT);
      FXI->bindParamMatrix(mtl->_paramMVITROT, MVIT.rotMatrix33());

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
      pipeline->bindParam(mtl->_parAlphaCutoff, mtl->_alphaCutoff);

      // PBR2 Phase 1 — 8 glTF KHR-extension lobes. bindParam tolerates
      // nullptr handles (older shaders / non-PBR pipelines that don't
      // expose these uniforms just skip). Flags default 0 so shader-side
      // libblocks short-circuit and disabled-lobe cost is one int compare.
      pipeline->bindParam(mtl->_parHasTransmission,           int(mtl->_hasTransmission ? 1 : 0));
      pipeline->bindParam(mtl->_parTransmissionFactor,        mtl->_transmissionFactor);
      pipeline->bindParam(mtl->_parHasTransmissionRoughness,  int(mtl->_hasTransmissionRoughness ? 1 : 0));
      pipeline->bindParam(mtl->_parTransmissionRoughness,     mtl->_transmissionRoughness);
      pipeline->bindParam(mtl->_parHasIor,                    int(mtl->_hasIor ? 1 : 0));
      pipeline->bindParam(mtl->_parIor,                       mtl->_ior);
      pipeline->bindParam(mtl->_parHasVolume,                 int(mtl->_hasVolume ? 1 : 0));
      pipeline->bindParam(mtl->_parVolumeThicknessFactor,     mtl->_volumeThicknessFactor);
      pipeline->bindParam(mtl->_parHasDiffuseTransmission,    int(mtl->_hasDiffuseTransmission ? 1 : 0));
      pipeline->bindParam(mtl->_parDiffuseTransmissionFactor, mtl->_diffuseTransmissionFactor);
      pipeline->bindParam(mtl->_parHasSpecular,               int(mtl->_hasSpecular ? 1 : 0));
      pipeline->bindParam(mtl->_parSpecularFactor,            mtl->_specularFactor);
      pipeline->bindParam(mtl->_parHasClearcoat,              int(mtl->_hasClearcoat ? 1 : 0));
      pipeline->bindParam(mtl->_parClearcoatFactor,           mtl->_clearcoatFactor);
      pipeline->bindParam(mtl->_parHasSheen,                  int(mtl->_hasSheen ? 1 : 0));
      pipeline->bindParam(mtl->_parSheenFactor,               mtl->_sheenFactor);
      pipeline->bindParam(mtl->_parHasIridescence,            int(mtl->_hasIridescence ? 1 : 0));
      pipeline->bindParam(mtl->_parIridescenceFactor,         mtl->_iridescenceFactor);

      // PBR2 Phase 2 — vec3 color + secondary scalar uniforms.
      pipeline->bindParam(mtl->_parSheenColor,                mtl->_sheenColor);
      pipeline->bindParam(mtl->_parSpecularColor,             mtl->_specularColor);
      pipeline->bindParam(mtl->_parAttenuationColor,          mtl->_attenuationColor);
      pipeline->bindParam(mtl->_parDiffuseTransmissionColor,  mtl->_diffuseTransmissionColor);
      pipeline->bindParam(mtl->_parClearcoatRoughness,        mtl->_clearcoatRoughness);
      pipeline->bindParam(mtl->_parSheenRoughness,            mtl->_sheenRoughness);
      pipeline->bindParam(mtl->_parAttenuationDistance,       mtl->_attenuationDistance);

      // PBR2 Phase 3 (P3.D) — subsurface scattering.
      pipeline->bindParam(mtl->_parHasSubsurface,             int(mtl->_hasSubsurface ? 1 : 0));
      pipeline->bindParam(mtl->_parSubsurfaceColor,           mtl->_subsurfaceColor);
      pipeline->bindParam(mtl->_parSubsurfaceRadius,          mtl->_subsurfaceRadius);
      pipeline->bindParam(mtl->_parSubsurfaceFactor,          mtl->_subsurfaceFactor);
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
