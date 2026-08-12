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
#include <ork/lev2/gfx/renderer/NodeCompositor/sky_atmosphere.h>
#include <ork/util/logger.h>

OIIO_NAMESPACE_USING

namespace ork::lev2 {

static logchannel_ptr_t logchan_pbr_fwd = logger()->configureChannel("mtlpbrFWDPL", fvec3(0.8, 0.8, 0.1), true);

///////////////////////////////////////////////////////////////////////////////
// OBSERVABLE TECHNIQUE SELECTION, AT THE POINT OF USE.
//
// A per-view (_ST) technique and its mono twin produce eye layers a still cannot
// tell apart — a stereo pass that silently fell through to mono renders clean,
// validates clean, and looks right in a headset to anyone not measuring. So the
// question "which technique did this draw actually take" needs an answer in the
// transcript, and it has to be reported from a STATE LAMBDA: those run when the
// pipeline is BOUND for a real draw, where a line printed at pipeline
// construction would only report an intent. It also reports the pass's own
// stereo bit read off the LIVE CPD, since "the permutation said stereo" and "the
// pass was stereo" are different claims.
//
// Once per pipeline (not per frame), so a running demo does not drown in it.
// The technique name is captured BY VALUE rather than reading it back off the
// pipeline, which would make the pipeline own a lambda that owns the pipeline.
//
// Attached on the two arms a standard forward draw can take — the _ST block and
// the mono tail. The generated-material arms (SSBO / mesh / impostor) return
// earlier and are not covered here.
//
// Grep token: SPVR:FWDSEL
///////////////////////////////////////////////////////////////////////////////

static void _announceForwardTechnique(fxpipeline_ptr_t pipeline, const GfxMaterial* mtl, bool permu_stereo) {
  if ((nullptr == pipeline) or (nullptr == pipeline->_technique))
    return;
  auto announced      = std::make_shared<bool>(false);
  std::string tekname = pipeline->_technique->_techniqueName;
  std::string mtlname = mtl ? mtl->mMaterialName : std::string("<unnamed>");
  pipeline->addStateLambda([announced, tekname, mtlname, permu_stereo] //
                           (const RenderContextInstData& RCID) {
    if (*announced)
      return;
    *announced  = true;
    auto RCFD   = RCID.rcfd();
    bool cpd_st = RCFD->hasCPD() ? RCFD->topCPD().isSinglePassStereo() : false;
    printf(
        "[SPVR:FWDSEL] forward DRAW material<%s> technique<%s> permu_stereo<%d> pass_stereo<%d>\n",
        mtlname.c_str(),
        tekname.c_str(),
        int(permu_stereo),
        int(cpd_st));
    fflush(stdout);
  });
}

///////////////////////////////////////////////////////////////////////////////
// The GENERATED-MATERIAL arms (impostor / SSBO-pull / instanced / mesh-shader) each RETURN
// before the FWDSEL announcement above, so a census that greps for technique selection sees
// nothing from them — which is exactly how an unwired _ST peer stayed invisible: the shader
// text existed, no C++ ever chose it, and no line said so. Same contract as FWDSEL: fires
// from a state lambda (a real BIND, not a construction), reports the pass's own stereo bit
// off the live CPD, once per pipeline.
//
// Grep token: SPVR:GENSEL
///////////////////////////////////////////////////////////////////////////////

static void _announceGeneratedTechnique(
    fxpipeline_ptr_t pipeline,   //
    const GfxMaterial* mtl,      //
    bool permu_stereo,           //
    const char* family) {
  if ((nullptr == pipeline) or (nullptr == pipeline->_technique))
    return;
  auto announced      = std::make_shared<bool>(false);
  std::string tekname = pipeline->_technique->_techniqueName;
  std::string mtlname = mtl ? mtl->mMaterialName : std::string("<unnamed>");
  std::string fam     = family;
  pipeline->addStateLambda([announced, tekname, mtlname, fam, permu_stereo] //
                           (const RenderContextInstData& RCID) {
    if (*announced)
      return;
    *announced  = true;
    auto RCFD   = RCID.rcfd();
    bool cpd_st = RCFD->hasCPD() ? RCFD->topCPD().isSinglePassStereo() : false;
    printf(
        "[SPVR:GENSEL] generated DRAW family<%s> material<%s> technique<%s> permu_stereo<%d> pass_stereo<%d>\n",
        fam.c_str(),
        mtlname.c_str(),
        tekname.c_str(),
        int(permu_stereo),
        int(cpd_st));
    fflush(stdout);
  });
}

FxPipeline::statelambda_t createForwardLightingLambda(const PBRMaterial* mtl) {

  auto L = [mtl](const RenderContextInstData& RCID) {

    auto RCFD             = RCID.rcfd();
    auto context    = RCFD->GetTarget();
    auto FXI        = context->FXI();
    bool is_skinned = RCID._isSkinned;
    auto CIMPL = RCFD->topCompositor();
    ///////////////////////////////////////////////////////////////////////////
    // retrieve global RCFD state for PBR materials
    ///////////////////////////////////////////////////////////////////////////

    auto enumlights = RCFD->userPropertyAs<enumeratedlights_ptr_t>("enumeratedlights"_crcu);
    bool is_rendering_PROBE = RCFD->userPropertyAs<bool>("renderingPROBE"_crcu);
    bool have_PROBES = RCFD->userPropertyAs<bool>("havePROBES"_crcu);
    bool should_bind_probes = (have_PROBES);
    bool is_depth_prepass = RCFD->_renderingmodel._modelID == "DEPTH_PREPASS"_crcu;
    auto pbrcommon = RCFD->userPropertyAs<pbr::commonstuff_ptr_t>("PBR_COMMON"_crcu);

    ///////////////////////////////////////////////////////////////////////////
    // we are not lighting for depth prepass, so NOP
    ///////////////////////////////////////////////////////////////////////////

    if (is_depth_prepass)
      return;

    ///////////////////////////////////////////////////////////////////////////
    // build lighting UBO
    ///////////////////////////////////////////////////////////////////////////

    // logchan_pbr_fwd->log("fwd: all lights count<%zu>", enumlights->_alllights.size());

    auto lmgr = CIMPL->_lightmgr;
    OrkAssert(lmgr);
    OrkAssert(lmgr->_needs_gpu_init==false);

    ///////////////////////////////////////////////////////////////////////////
    // bind lighting UBO
    ///////////////////////////////////////////////////////////////////////////

    if (mtl->_parUnTexPointLightsCount) {
      FXI->bindParamInt(mtl->_parUnTexPointLightsCount, enumlights->_num_active_untextured_pointlights);
    }
    if (mtl->_parForwardLightBlock) {
      auto pl_buffer = PBRMaterial::lightingDataBuffer(context);
      if(0)printf("BINDING LIGHTING SSBO<%p> to param<%p> mtl<%s>\n", //
             (void*) pl_buffer,                          //
             mtl->_parForwardLightBlock,
             mtl->mMaterialName.c_str());


      FXI->bindStorageBuffer(mtl->_parForwardLightBlock, pl_buffer);
    }

    ///////////////////////////////////////////////////////////////////////////
    // bind spotlight cookies
    ///////////////////////////////////////////////////////////////////////////
 
    // EVERY bind below is handle-guarded. A material whose shader does not
    // declare a given sampler/param gets a null handle, and the FXI bind entry
    // points dereference the handle immediately — the lean UNLIT main-view
    // fragment (ps_ptex_unlit) exists precisely to drop declarations, so an
    // unguarded bind here is a segfault, not a no-op.
    if (mtl->_parTexSpotLightsCount) {
      FXI->bindParamInt(mtl->_parTexSpotLightsCount, enumlights->_num_active_texspotlights);
    }
    if (mtl->_parLightDepthCookies) {
      FXI->bindParamTextureArray(mtl->_parLightDepthCookies, lmgr->_cookies_spot_depth.get() );
    }
    if (mtl->_parLightColorCookies) {
      FXI->bindParamTextureArray(mtl->_parLightColorCookies, lmgr->_cookies_spot_color.get() );
    }

    ///////////////////////////////////////////////////////////////////////////
    // bind sun cascade state (SKYLIGHT lane A) — the ublk_sun UBO is written
    // once per frame by the prologue (_update_sun_cascades); here we only
    // bind the shared buffer + the cascade depth array (default tiny array
    // when no sun — shader branches on has_sun before sampling).
    ///////////////////////////////////////////////////////////////////////////

    if (mtl->_parSunBlock) {
      FXI->bindUniformBuffer(mtl->_parSunBlock, PBRMaterial::sunDataBuffer(context));
    }
    if (mtl->_parSunShadowMap) {
      FXI->bindParamTextureArray(mtl->_parSunShadowMap, lmgr->_sun_shadow_cascades.get());
    }
    if (mtl->_parSunCookie) {
      // always bound (1x1 white default when disarmed) so the descriptor set is
      // complete whether or not a deck published a cookie this frame.
      FXI->bindParamTexture(mtl->_parSunCookie, lmgr->_sun_cookie.get());
    }

    ///////////////////////////////////////////////////////////////////////////
    // bind AERIAL PERSPECTIVE state (SKYLIGHT lane B) — the haze march in
    // lib_sky. ONE binder, shared with the lighting-free pipelines that carry
    // it as their own state lambda (material_pbr_misc.cpp).
    ///////////////////////////////////////////////////////////////////////////

    bindSkyHazeState(mtl, RCID);

    ///////////////////////////////////////////////////////////////////////////
    // bind Color/Normal/Metallic/Roughness/AO Texture Array
    ///////////////////////////////////////////////////////////////////////////

    if (mtl->_paramMapCNMREA) {
      FXI->bindParamTextureArray( mtl->_paramMapCNMREA, mtl->_texArrayCNMREA.get() );
    }

    ///////////////////////////////////////////////////////////////////////////
    // bind light/environment probes
    ///////////////////////////////////////////////////////////////////////////

    //printf("should_bind_probes<%d> is_rendering_PROBE<%d>\n", int(should_bind_probes), int(is_rendering_PROBE));
    bool probe_active = false;
    if(not is_rendering_PROBE){
      // Per-draw probe override (PBR2 P0.4c — set by ParticlesGlobalSystem
      // from gendata._probe_entity_name → live LightProbe). Routes a
      // specific probe's cube to this draw, regardless of the global
      // _lightprobes[0] choice. Empty → fall through to global path.
      lightprobe_ptr_t chosen_probe;
      if (RCID._probeOverride) {
        chosen_probe = RCID._probeOverride;
      } else if (should_bind_probes && enumlights->_lightprobes.size() > 0) {
        // Global fallback: first probe wins (same as the old behavior).
        chosen_probe = enumlights->_lightprobes[0];
      }
      if (chosen_probe) {
        auto probe_tex = chosen_probe->_cubeTexture;
        if (mtl->_parProbeReflection)
          FXI->bindParamTexture(mtl->_parProbeReflection, probe_tex.get());
        if (mtl->_parProbeRadiance)
          FXI->bindParamTexture(mtl->_parProbeRadiance,   probe_tex.get());
        probe_active = (probe_tex != nullptr);
      }
    }
    if(not probe_active){
      //printf( "NOT BINDING PROBES black<%p>!\n", mtl->_texCubeBlack.get() );
      if (mtl->_parProbeReflection)
        FXI->bindParamTexture(mtl->_parProbeReflection, pbrcommon->_texCubeBlack.get() );
      if (mtl->_parProbeRadiance)
        FXI->bindParamTexture(mtl->_parProbeRadiance, pbrcommon->_texCubeBlack.get() );
    }
    // has_reflection_probe drives the spec-env vs probe swap in fwdtools.i2.
    // Inactive path keeps the black cube bound so the sampler is always valid;
    // shader branches on the flag, not on sampler-null.
    if(mtl->_parHasReflectionProbe){
      FXI->bindParamInt(mtl->_parHasReflectionProbe, probe_active ? 1 : 0);
    }
    // PBR2 Phase 2 — rendering_probe gates refractive lobes during cubemap
    // capture. is_rendering_PROBE comes off RCFD["renderingPROBE"], set on
    // the per-face CPD in fwdnode_impl_sub.cpp before each probe pass.
    if(mtl->_parRenderingProbe){
      FXI->bindParamInt(mtl->_parRenderingProbe, is_rendering_PROBE ? 1 : 0);
    }

    ///////////////////////////////////////////////////////////////////////////
    // lightmaps
    ///////////////////////////////////////////////////////////////////////////

    /*for( int i=0; i<8; i++ ){
      auto C = mtl->_lightmapColors[i];
      printf( "lightmapcolor<%d> = <%f %f %f>\n", i, C.x, C.y, C.z );
    }*/
    if(mtl->_texLightMapArray){
      //printf("binding lightmap array\n");
      if (mtl->_parMapLightMapArray)
        FXI->bindParamTextureArray(mtl->_parMapLightMapArray, mtl->_texLightMapArray.get());
      if (mtl->_paramLightMapColors)
        FXI->bindParamVect3Array(mtl->_paramLightMapColors, mtl->_lightmapColors,8);
    }
    else{
      //printf("binding white lightmap array\n");
      //printf("mtl->_parMapLightMapArray<%p>\n", mtl->_parMapLightMapArray);
      //printf("mtl->_texWhiteLightMapArray<%p>\n", mtl->_texWhiteLightMapArray.get());
      if (mtl->_parMapLightMapArray)
        FXI->bindParamTextureArray(mtl->_parMapLightMapArray, pbrcommon->_texWhiteLightMapArray.get());
      if (mtl->_paramLightMapColors)
        FXI->bindParamVect3Array(mtl->_paramLightMapColors, mtl->_lightmapColors,8);
      //printf("OK...\n");
    }

    ///////////////////////////////////////////////////////////////////////////

    // ModColor: per-frame post-multiplicative tint (fades, hover, etc.).
    // Applied AFTER lighting computation. Must NOT carry albedo or it
    // would multiply through the env-IBL specular path.
    //
    // ModAlbedo: per-material multiplicative albedo tint. Applied at the
    // light-input boundary (modulates the textured CNMREA color sample).
    // Used by lighting math (diffuse term, F0 metallic mix), not as a
    // post-multiplier.
    auto modcolor = context->RefModColor();
    if (mtl->_parModColor)
      FXI->bindParamVect4(mtl->_parModColor,  modcolor);
    if (mtl->_parModAlbedo)
      FXI->bindParamVect4(mtl->_parModAlbedo, mtl->_baseColor);
  };
  return L;
}

void PBRMaterial::addLightingLambda(fxpipeline_ptr_t pipe) {
  auto L = createForwardLightingLambda(this);
  pipe->addStateLambda(L);
}
void PBRMaterial::addLightingLambda() {
  auto L = createForwardLightingLambda(this);
  _state_lambdas.push_back(L);
}

///////////////////////////////////////////////////////////////////////////////

fxpipeline_ptr_t PBRMaterial::_createFxPipelineFWD(const FxPipelinePermutation& permu) const {
  fxpipeline_ptr_t pipeline;

  ////////////////////////////////////////////////
  // set raster state
  ////////////////////////////////////////////////
  auto l_rsi = [this](const RenderContextInstData& RCID) {
    auto mut = const_cast<PBRMaterial*>(this);
    auto RCFD    = RCID.rcfd();
    auto context = RCFD->GetTarget();
    //auto RSI     = context->RSI();
    //this->_rasterstate->setBlendingMacro(BlendingMacro::ADDITIVE);
    bool is_rendering_PROBE = RCFD->userPropertyAs<bool>("renderingPROBE"_crcu);

    ECullTest culltest = this->_doubleSided ? ECullTest::OFF : ECullTest::PASS_FRONT;
    if(is_rendering_PROBE){
      culltest = ECullTest::OFF;
    }

    // Bump priority above the technique state block when the material wants
    // to override its cull (e.g. mtl.doubleSided / PROBE rendering).
    mut->_rasterstate->_priority = (this->_doubleSided || is_rendering_PROBE) ? (1 << 20) : 0;
    mut->_rasterstate->setCullTest(culltest);
    mut->_rasterstate->setDepthTest(EDepthTest::LEQUALS);
    if (this->_alphaMode == 2) { // BLEND
      mut->_rasterstate->setBlendingMacro(BlendingMacro::ALPHA);
      mut->_rasterstate->setWriteMaskZ(false); // transparent objects don't write depth
    } else {
      mut->_rasterstate->setWriteMaskZ(true);
    }
    mut->_rasterstate->setWriteMaskRGB(true);
    mut->_rasterstate->setWriteMaskA(true);
    mut->_rasterstate->setAlphaToCoverage(false); // default off; the impostor branch turns it on (no leak to meshes)
  };
  ////////////////////////////////////////////////
  // ssao lambda
  ////////////////////////////////////////////////
  auto l_ssao = [this](const RenderContextInstData& RCID) {
    auto RCFD    = RCID.rcfd();
    auto context = RCFD->GetTarget();
    auto FXI              = context->FXI();
      if( RCFD->_renderingmodel._modelID == "DEPTH_PREPASS"_crcu ){
        //FXI->bindParamTexture(this->_paramSSAOTexture, nullptr );
        FXI->bindParamVect2(this->_parInvViewSize, fvec2(1,1) );
      }
      else {
          auto pbrcommon = RCFD->userPropertyAs<pbr::commonstuff_ptr_t>("PBR_COMMON"_crcu);
          auto ssaotexture = RCFD->userPropertyAs<texture_ptr_t>("SSAO_MAP"_crcu);
          auto depthtexture = RCFD->userPropertyAs<texture_ptr_t>("DEPTH_MAP"_crcu);

          //FXI->bindParamTexture(this->_paramMapDepth, depthtexture.get() );

          auto near_far = RCFD->userPropertyAs<fvec2>("NEAR_FAR"_crcu);
          auto ssaoDIM = RCFD->userPropertyAs<fvec2>("SSAO_DIM"_crcu);
          auto pmatrix = RCFD->userPropertyAs<fmtx4>("PMATRIX"_crcu);
          auto ipmatrix = RCFD->userPropertyAs<fmtx4>("IPMATRIX"_crcu);
          fvec2 ivpdim = fvec2(1.0f / ssaoDIM.x, 1.0f / ssaoDIM.y);
          FXI->bindParamTexture(this->_paramSSAOTexture, ssaotexture.get() );
          FXI->bindParamVect2(this->_parInvViewSize, ivpdim );
          FXI->bindParamVect2(this->_paramNearFar, near_far );
          FXI->bindParamMatrix(this->_paramP, pmatrix);
          FXI->bindParamMatrix(this->_paramIP, ipmatrix);

      }
  };
  /////////////////////////////////////////////////////////////
  // CLOUD-SHADOW FILL (FWD_SUNCOOKIE) — the sun-cookie pass renders this surface into a
  // transmittance map and consumes its ALPHA only, so an UNLIT material (which produces both
  // color and alpha without asking the scene for light) gets a technique that never DECLARES the
  // lit path's sampler sets. A descriptor-set layout counts declarations, not reads: the forward
  // technique's layout is 17 combined samplers where Metal/MoltenVK reports
  // maxPerStageDescriptorSamplers = 16 (VUID-VkPipelineLayoutCreateInfo-descriptorType-03016,
  // which traps under validation), and 16 of those 17 are lighting the fill has no use for.
  // NO lighting/ssao lambdas here — nothing in the alpha-only fragment reads them.
  // Checked FIRST so it wins over the vertex variants; RIGID MONO only, because the lean
  // technique pairs the attribute vertex shader with a mono cookie camera. Anything else (and
  // every material without the technique, e.g. stock PBR) falls through to the full forward
  // pipeline below, leaving the pass exactly as it was.
  /////////////////////////////////////////////////////////////
  if (permu._is_sun_cookie and this->_tek_FWD_SUNCOOKIE and                    //
      not(permu._stereo or permu._skinned or permu._instanced or               //
          permu._instanced_matrices_only or permu._is_vertex_ssbo or           //
          permu._is_impostor or permu._is_mesh_shader)) {
    pipeline             = std::make_shared<FxPipeline>(permu);
    pipeline->_technique = this->_tek_FWD_SUNCOOKIE;
    pipeline->bindParam(this->_paramMVP, "RCFD_Camera_MVP_Mono"_crcsh);
    pipeline->addStateLambda(createBasicStateLambda(this));
    pipeline->addStateLambda(l_rsi);
    // AERIAL PERSPECTIVE, on the ONE forward pipeline with no lighting lambda to
    // fold it into. The opt-in is DECLARATION: a generated surface that calls
    // skyAerialPerspective inherits lib_sky into every one of its fragments,
    // cookie fragment included, so SkyRadii resolving is exactly the question
    // "does this material's shader have a sky block to feed". This pass runs
    // BEFORE the sky LUTs are published, so the binder lands on its disarmed
    // path here by construction (gate 0, default LUT textures) — which is the
    // right answer for the cookie anyway: it measures surface-to-sun
    // transmittance, and haze between the VIEWER and the surface is not part of
    // that.
    if (this->_parSkyRadii)
      pipeline->addStateLambda(createSkyHazeStateLambda(this));
    pipeline->_material_ptr = (GfxMaterial*)this;
    pipeline->_rasterstate  = this->_rasterstate;
    return pipeline;
  }
  // NOTE: the impostor atlas + ImpCenter/ImpGrid are bound PER-DRAW by the drawable's bucket loop
  // (ComputeDrawable::_renderIndirect) — NOT here as a material state lambda. The base PBRMaterial is SHARED
  // across all variants (one "tree_bark"), so a material-level bind would collapse every impostor to the
  // last-baked atlas; each bucket binds its OWN variant's atlas just before its draw.
  /////////////////////////////////////////////////////////////
  // LOD IMPOSTOR (FWD_SSBO_CUSTOM_IMPOSTOR): billboard quad built in the VS from gl_VertexID + the SSBO
  // per-instance matrix; surface() samples the baked atlas. Checked FIRST so an impostor tier never falls
  // through to the mesh SSBO branches. Gets the IDENTICAL forward state (basic raster + lighting + ssao +
  // MVP) — the whole point: the impostor color-matches the mesh because it runs the same lighting.
  /////////////////////////////////////////////////////////////
  // SINGLE-PASS STEREO: the _ST peer is the same quad, the same billboard basis and the same
  //  atlas tile — per-view CLIP TRANSFORM only (spvr_vp[view] out of ublk_stereo, written by
  //  the basic-state lambda, hence no mono MVP bind on that arm). Null-guarded, so a material
  //  whose template predates the peer falls through to the mono technique exactly as before.
  //  This arm is checked HERE and not with the other _ST arms further down because the impostor
  //  branch returns before them: without it, every impostor-LOD instance took the mono
  //  technique inside the stereo pass and wrote the SAME image into both eye layers.
  const bool impostor_stereo = permu._stereo and (this->_tek_FWD_SSBO_CUSTOM_IMPOSTOR_ST != nullptr);
  auto tek_impostor          = impostor_stereo ? this->_tek_FWD_SSBO_CUSTOM_IMPOSTOR_ST //
                                               : this->_tek_FWD_SSBO_CUSTOM_IMPOSTOR;
  if (permu._is_impostor and tek_impostor) {
    pipeline             = std::make_shared<FxPipeline>(permu);
    pipeline->_technique = tek_impostor;
    if (not impostor_stereo)
      pipeline->bindParam(this->_paramMVP, "RCFD_Camera_MVP_Mono"_crcsh); // model=identity -> = VP
    // the billboard basis stays MONO on BOTH peers, by construction in the template.
    pipeline->bindParam(this->_paramIV,  "RCFD_Camera_IV_Mono"_crcsh);  // inverse-view: the billboard basis
    pipeline->addStateLambda(createBasicStateLambda(this));
    pipeline->addStateLambda(createForwardLightingLambda(this)); // SAME forward lighting as the mesh tiers
    pipeline->addStateLambda(l_rsi);
    pipeline->addStateLambda(l_ssao);
    // (atlas + ImpCenter/ImpGrid bound per-draw in the bucket loop — see note above; shared material.)
    // A2C (the impostor FS writes silhouette coverage as alpha -> MSAA coverage mask) is declared by the
    // technique's OWN state block, sb_ptex_impostor. It cannot be set here: l_rsi leaves the material
    // rasterstate at priority 0 and the effective-state resolution gives an equal-priority state block the
    // win, so a state lambda setting it was discarded every draw — and it mutated the SHARED material
    // rasterstate, which a doubleSided material (priority 1<<20) would then have carried onto its MESH tiers.
    pipeline->_material_ptr = (GfxMaterial*)this;
    pipeline->_rasterstate  = this->_rasterstate;
    // the generated-material arms return before the FWDSEL announcement below, so they carry
    //  their own. Zero parallax IS this arm's failure mode, so "which technique did the draw
    //  take" has to be answerable from a transcript.
    _announceGeneratedTechnique(pipeline, (const GfxMaterial*)this, permu._stereo, "impostor");
    return pipeline;
  }
  /////////////////////////////////////////////////////////////
  // SSBO-SOURCED VERTICES (FWD_SSBO_CUSTOM): a first-class vertex variant, like rigid/
  // instanced/skinned — selected by permu._is_vertex_ssbo, picking the _tek_FWD_SSBO_CUSTOM
  // technique (compute-generated geometry pulled from an SSBO). Gets the SAME full forward
  // state (basic raster + lighting + ssao + MVP). Mono only for now. See project_fwd_ssbo_custom.
  /////////////////////////////////////////////////////////////
  // SSBO geometry x PER-INSTANCE MATRIX (FWD_SSBO_CUSTOM_INSTANCED): same SSBO-pull vertices, but each is
  // placed by storage_inst_mtx[gl_InstanceIndex] (the matrices SSBO is bound via the drawable's graphics
  // storage, like the geometry channels). One indirect draw, instanceCount instances. Checked BEFORE the
  // plain SSBO case so instanced+ssbo picks the instanced technique.
  /////////////////////////////////////////////////////////////
  // MESH-SHADER GEOMETRY (FWD_SSBO_CUSTOM_MESH): the taskless VK_EXT_mesh_shader twin of the SSBO
  // pull path — meshlet workgroups generate + self-cull the geometry, so the draw is
  // DrawMeshTasksEML instead of an indirect pull. Checked BEFORE the SSBO branches: the drawable
  // still flags _isSSBOSourced (the same storage feeds both), so mesh must win or the mesh draw
  // would issue against a vertex-stage pipeline. Identical forward state — same lighting, same
  // fragment, so the two paths color-match.
  /////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////
  // SINGLE-PASS STEREO arms for the SSBO-sourced family. The per-view clip
  //  transform lives in the _ST technique's vertex/mesh stage (spvr_vp[view] out
  //  of ublk_stereo, written by the basic-state lambda) -- hence NO MVP bind here,
  //  exactly as the hand-authored _ST arms below. These techniques come from the
  //  GENERATED-MATERIAL TEMPLATE; each arm is null-guarded so a tree whose template
  //  has not grown them yet falls straight through to the mono arm underneath.
  /////////////////////////////////////////////////////////////
  if (permu._stereo and permu._is_mesh_shader and this->_tek_FWD_SSBO_CUSTOM_MESH_ST) {
    pipeline             = std::make_shared<FxPipeline>(permu);
    pipeline->_technique = this->_tek_FWD_SSBO_CUSTOM_MESH_ST;
    pipeline->addStateLambda(createBasicStateLambda(this));
    pipeline->addStateLambda(createForwardLightingLambda(this));
    pipeline->addStateLambda(l_rsi);
    pipeline->addStateLambda(l_ssao);
    pipeline->_material_ptr = (GfxMaterial*)this;
    pipeline->_rasterstate  = this->_rasterstate;
    _announceGeneratedTechnique(pipeline, (const GfxMaterial*)this, permu._stereo, "ssbo_mesh");
    return pipeline;
  }
  if (permu._stereo and permu._is_vertex_ssbo and permu._instanced and this->_tek_FWD_SSBO_CUSTOM_INSTANCED_ST) {
    pipeline             = std::make_shared<FxPipeline>(permu);
    pipeline->_technique = this->_tek_FWD_SSBO_CUSTOM_INSTANCED_ST;
    pipeline->addStateLambda(createBasicStateLambda(this));
    pipeline->addStateLambda(createForwardLightingLambda(this));
    pipeline->addStateLambda(l_rsi);
    pipeline->addStateLambda(l_ssao);
    pipeline->_material_ptr = (GfxMaterial*)this;
    pipeline->_rasterstate  = this->_rasterstate;
    _announceGeneratedTechnique(pipeline, (const GfxMaterial*)this, permu._stereo, "ssbo_instanced");
    return pipeline;
  }
  if (permu._stereo and permu._is_vertex_ssbo and this->_tek_FWD_SSBO_CUSTOM_ST) {
    pipeline             = std::make_shared<FxPipeline>(permu);
    pipeline->_technique = this->_tek_FWD_SSBO_CUSTOM_ST;
    pipeline->addStateLambda(createBasicStateLambda(this));
    pipeline->addStateLambda(createForwardLightingLambda(this));
    pipeline->addStateLambda(l_rsi);
    pipeline->addStateLambda(l_ssao);
    pipeline->_material_ptr = (GfxMaterial*)this;
    pipeline->_rasterstate  = this->_rasterstate;
    _announceGeneratedTechnique(pipeline, (const GfxMaterial*)this, permu._stereo, "ssbo_pull");
    return pipeline;
  }
  if (permu._is_mesh_shader and this->_tek_FWD_SSBO_CUSTOM_MESH) {
    pipeline             = std::make_shared<FxPipeline>(permu);
    pipeline->_technique = this->_tek_FWD_SSBO_CUSTOM_MESH;
    pipeline->bindParam(this->_paramMVP, "RCFD_Camera_MVP_Mono"_crcsh);
    pipeline->addStateLambda(createBasicStateLambda(this));
    pipeline->addStateLambda(createForwardLightingLambda(this));
    pipeline->addStateLambda(l_rsi);
    pipeline->addStateLambda(l_ssao);
    pipeline->_material_ptr = (GfxMaterial*)this;
    pipeline->_rasterstate  = this->_rasterstate;
    _announceGeneratedTechnique(pipeline, (const GfxMaterial*)this, permu._stereo, "ssbo_mesh");
    return pipeline;
  }
  if (permu._is_vertex_ssbo and permu._instanced and this->_tek_FWD_SSBO_CUSTOM_INSTANCED) {
    pipeline             = std::make_shared<FxPipeline>(permu);
    pipeline->_technique = this->_tek_FWD_SSBO_CUSTOM_INSTANCED;
    pipeline->bindParam(this->_paramMVP, "RCFD_Camera_MVP_Mono"_crcsh);
    pipeline->addStateLambda(createBasicStateLambda(this));
    pipeline->addStateLambda(createForwardLightingLambda(this));
    pipeline->addStateLambda(l_rsi);
    pipeline->addStateLambda(l_ssao);
    pipeline->_material_ptr = (GfxMaterial*)this;
    pipeline->_rasterstate  = this->_rasterstate;
    _announceGeneratedTechnique(pipeline, (const GfxMaterial*)this, permu._stereo, "ssbo_instanced");
    return pipeline;
  }
  if (permu._is_vertex_ssbo and this->_tek_FWD_SSBO_CUSTOM) {
    pipeline             = std::make_shared<FxPipeline>(permu);
    pipeline->_technique = this->_tek_FWD_SSBO_CUSTOM;
    pipeline->bindParam(this->_paramMVP, "RCFD_Camera_MVP_Mono"_crcsh);
    pipeline->addStateLambda(createBasicStateLambda(this));
    pipeline->addStateLambda(createForwardLightingLambda(this));
    pipeline->addStateLambda(l_rsi);
    pipeline->addStateLambda(l_ssao);
    pipeline->_material_ptr = (GfxMaterial*)this;
    pipeline->_rasterstate  = this->_rasterstate;
    _announceGeneratedTechnique(pipeline, (const GfxMaterial*)this, permu._stereo, "ssbo_pull");
    return pipeline;
  }
  /////////////////////////////////////////////////////////////
  // MATRICES-ONLY INSTANCED (FWD_CT_NM_IM_NI_MO): a first-class instanced variant that pulls per-
  // instance matrices from the dynamic storage_inst_mtx block (no per-instance color). Same forward
  // state as the other branches. See project_fwd_ssbo_custom (instancing sibling).
  /////////////////////////////////////////////////////////////
  // This branch returns before the stereo block below, so it carries its own _ST
  //  arm; without one a matrices-only instanced draw in a stereo pass would get
  //  the mono technique and both eye layers would come out identical.
  auto tek_im = (permu._stereo and this->_tek_FWD_CT_NM_IM_NI_ST) //
                    ? this->_tek_FWD_CT_NM_IM_NI_ST
                    : this->_tek_FWD_CT_NM_IM_NI_MO;
  if (permu._instanced_matrices_only and tek_im) {
    pipeline             = std::make_shared<FxPipeline>(permu);
    pipeline->_technique = tek_im;
    pipeline->bindParam(this->_paramMVP, "RCFD_Camera_MVP_Mono"_crcsh);
    pipeline->addStateLambda(createBasicStateLambda(this));
    pipeline->addStateLambda(createForwardLightingLambda(this));
    pipeline->addStateLambda(l_rsi);
    pipeline->addStateLambda(l_ssao);
    pipeline->_material_ptr = (GfxMaterial*)this;
    pipeline->_rasterstate  = this->_rasterstate;
    return pipeline;
  }
  /////////////////////////////////////////////////////////////
  // SINGLE-PASS STEREO — the _ST technique family (authored in pbr.fxv2 beside its
  //  _MO twin; same fragment stage, per-view clip transform in the vertex stage).
  //  Every arm is null-guarded: a missing _ST technique falls through to the mono
  //  branch below rather than producing a pipeline whose _technique is null, which
  //  is what used to detonate at the first stereo draw.
  /////////////////////////////////////////////////////////////
  if (permu._stereo) {
    // VERTEX COLORS have their own technique family (FWD_CV_*), and the CT_* peers
    //  below declare a DIFFERENT vertex format. Routing a vtxcolor permutation into
    //  a CT_ST technique is a misroute, not a fallback, and falling through to the
    //  mono block is the zero-parallax regression this whole branch exists to stop —
    //  so a missing CV _ST peer fails HERE, by name, rather than rendering wrong.
    if (permu._has_vtxcolors) {
      auto tek_cv_st = permu._instanced ? this->_tek_FWD_CV_NM_RI_IN_ST : this->_tek_FWD_CV_NM_RI_NI_ST;
      if (permu._is_alpha) {
        auto tek_cv_st_alpha =
            permu._instanced ? this->_tek_FWD_CV_NM_RI_IN_ST_ALPHA : this->_tek_FWD_CV_NM_RI_NI_ST_ALPHA;
        if (tek_cv_st_alpha)
          tek_cv_st = tek_cv_st_alpha;
      }
      OrkAssert(tek_cv_st != nullptr); // no FWD_CV_*_ST peer for a stereo vertex-color draw
      pipeline             = std::make_shared<FxPipeline>(permu);
      pipeline->_technique = tek_cv_st;
    } else if (permu._skinned) {
      if (this->_tek_FWD_CT_NM_SK_NI_ST) {
        pipeline             = std::make_shared<FxPipeline>(permu);
        pipeline->_technique = this->_tek_FWD_CT_NM_SK_NI_ST;
      }
    } else if (permu._instanced) {
      // vtxcolor arms mirror the mono block's shape below, permutation for
      //  permutation — a stereo draw that fell through to the mono technique
      //  because its _ST peer was missing would render the SAME image into both
      //  eye layers, which is a regression neither validation nor a still-frame
      //  oracle can see.
      if (permu._has_vtxcolors) {
        if (permu._is_alpha and this->_tek_FWD_CV_NM_RI_IN_ST_ALPHA) {
          pipeline             = std::make_shared<FxPipeline>(permu);
          pipeline->_technique = this->_tek_FWD_CV_NM_RI_IN_ST_ALPHA;
        } else if (this->_tek_FWD_CV_NM_RI_IN_ST) {
          pipeline             = std::make_shared<FxPipeline>(permu);
          pipeline->_technique = this->_tek_FWD_CV_NM_RI_IN_ST;
        }
      } else if (this->_tek_FWD_CT_NM_RI_IN_ST) {
        pipeline             = std::make_shared<FxPipeline>(permu);
        pipeline->_technique = this->_tek_FWD_CT_NM_RI_IN_ST;
      }
    } else {
      if (permu._has_vtxcolors) {
        if (permu._is_alpha and this->_tek_FWD_CV_NM_RI_NI_ST_ALPHA) {
          pipeline             = std::make_shared<FxPipeline>(permu);
          pipeline->_technique = this->_tek_FWD_CV_NM_RI_NI_ST_ALPHA;
        } else if (this->_tek_FWD_CV_NM_RI_NI_ST) {
          pipeline             = std::make_shared<FxPipeline>(permu);
          pipeline->_technique = this->_tek_FWD_CV_NM_RI_NI_ST;
        }
      } else if (this->_tek_FWD_CT_NM_RI_NI_ST) {
        pipeline             = std::make_shared<FxPipeline>(permu);
        pipeline->_technique = this->_tek_FWD_CT_NM_RI_NI_ST;
      }
    }
    if (pipeline) {
      // no per-eye matrix binds: the per-view state is the ublk_stereo UBO, written by
      //  the basic-state lambda and read through ofx_viewIndex in the vertex stage.
      pipeline->addStateLambda(createBasicStateLambda(this));
      pipeline->addStateLambda(createForwardLightingLambda(this));
      pipeline->addStateLambda(l_rsi);
      pipeline->addStateLambda(l_ssao);
      pipeline->_material_ptr = (GfxMaterial*)this;
      pipeline->_rasterstate  = this->_rasterstate;
      _announceForwardTechnique(pipeline, (const GfxMaterial*)this, permu._stereo);
      return pipeline;
    }
  }
  /////////////////////////////////////////////////////////////
  // FORWARD_PBR::MONO
  /////////////////////////////////////////////////////////////
  {
    if (permu._skinned) {
      if (permu._instanced) {
        if (this->_tek_FWD_CT_NM_SK_IN_MO) {
          pipeline             = std::make_shared<FxPipeline>(permu);
          pipeline->_technique = this->_tek_FWD_CT_NM_SK_IN_MO;
          //printf( "got fwdtek FWD_CT_NM_SK_IN_MO\n");
        }
      } else { // not instanced
        if (this->_tek_FWD_CT_NM_SK_NI_MO) {
          pipeline             = std::make_shared<FxPipeline>(permu);
          pipeline->_technique = this->_tek_FWD_CT_NM_SK_NI_MO;
          //printf( "got fwdtek FWD_CT_NM_SK_NI_MO\n");
        }
      }
    } else { // not skinned
      if (permu._instanced) {
        if (permu._has_vtxcolors) {
          if (permu._is_alpha && this->_tek_FWD_CV_NM_RI_IN_MO_ALPHA) {
            pipeline             = std::make_shared<FxPipeline>(permu);
            pipeline->_technique = this->_tek_FWD_CV_NM_RI_IN_MO_ALPHA;
          } else if (this->_tek_FWD_CV_NM_RI_IN_MO) {
            pipeline             = std::make_shared<FxPipeline>(permu);
            pipeline->_technique = this->_tek_FWD_CV_NM_RI_IN_MO;
          }
        } else if (this->_tek_FWD_CT_NM_RI_IN_MO) {
          pipeline             = std::make_shared<FxPipeline>(permu);
          pipeline->_technique = this->_tek_FWD_CT_NM_RI_IN_MO;
        }
      } else {
        if( permu._has_vtxcolors ){
          if (permu._is_alpha && this->_tek_FWD_CV_NM_RI_NI_MO_ALPHA) {
            pipeline             = std::make_shared<FxPipeline>(permu);
            pipeline->_technique = this->_tek_FWD_CV_NM_RI_NI_MO_ALPHA;
          }
          else if (this->_tek_FWD_CV_NM_RI_NI_MO) {
            pipeline             = std::make_shared<FxPipeline>(permu);
            pipeline->_technique = this->_tek_FWD_CV_NM_RI_NI_MO;
          }
        }
        else{
          if (this->_tek_FWD_CT_NM_RI_NI_MO) {
            pipeline             = std::make_shared<FxPipeline>(permu);
            pipeline->_technique = this->_tek_FWD_CT_NM_RI_NI_MO;
            //printf( "got fwdtek FWD_CT_NM_RI_NI_MO\n");
          }
        }
      }
    }
    if(pipeline){
      pipeline->bindParam(this->_paramMVP, "RCFD_Camera_MVP_Mono"_crcsh);
      pipeline->addStateLambda(createBasicStateLambda(this));
      pipeline->addStateLambda(createForwardLightingLambda(this));
      pipeline->addStateLambda(l_rsi);
      pipeline->addStateLambda(l_ssao);
    }
  }
  if(pipeline){
    pipeline->_material_ptr = (GfxMaterial*) this;
    pipeline->_rasterstate = this->_rasterstate;
    // reached with permu._stereo TRUE only when the _ST block above found no peer
    //  -- i.e. the silent-mono fallback. That is exactly the case this line exists
    //  to make visible, so it is attached here and not only on the stereo arm.
    _announceForwardTechnique(pipeline, (const GfxMaterial*) this, permu._stereo);
  }
  // OrkAssert(pipeline->_technique != nullptr);
  return pipeline;
}

} // namespace ork::lev2
