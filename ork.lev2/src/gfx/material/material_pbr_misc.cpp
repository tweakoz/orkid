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
///////////////////////////////////////////////////////////////////////////////

FxPipeline::statelambda_t createForwardLightingLambda(const PBRMaterial* mtl);

fxpipeline_ptr_t PBRMaterial::_createFxPipelineSKY(const FxPipelinePermutation& permu) const {
  fxpipeline_ptr_t pipeline;
  auto basic_lambda  = createBasicStateLambda(this);
  auto skybox_lambda = [this, basic_lambda](const RenderContextInstData& RCID) {
    auto mut       = (PBRMaterial*)this;
    auto RCFD      = RCID.rcfd();
    auto context   = RCFD->GetTarget();
    auto FXI       = context->FXI();
    auto MTXI      = context->MTXI();
    auto pbrcommon = RCFD->_pbrcommon;
    auto envtex    = pbrcommon->envSpecularTexture();
    // The baked skybox never fades (a procedural sky draws through
    // FWD_SKYBOX_PROC instead), but the prev slot of the shared PBR sampler set
    // still gets a real texture here rather than whatever the last draw left.
    auto envtex_prev = pbrcommon->envSpecularTexturePrev();

    FXI->bindParamTextureArray(this->_parMapSpecularEnv, envtex.get());
    FXI->bindParamTextureArray(this->_parMapSpecularEnvPrev, envtex_prev.get());
    FXI->bindParamFloat(this->_parEnvBlendWeight, pbrcommon->envCrossfadeWeight());
    FXI->bindParamFloat(this->_parEnvCaptureScaleInv, pbrcommon->envCaptureScaleInv());

    basic_lambda(RCID);
    mut->_rasterstate->setCullTest(ECullTest::OFF);
    mut->_rasterstate->setWriteMaskZ(false);
    mut->_rasterstate->setDepthTest(EDepthTest::LEQUALS);
    mut->_rasterstate->setWriteMaskRGB(true);
    mut->_rasterstate->setWriteMaskA(true);
    //RSI->BindRasterState(this->_rasterstate);
  };
  //////////////////////////////////////////////////////////
  // SKYLIGHT lane B — procedural sky. Deliberately NOT built on
  // createBasicStateLambda: that lambda binds MapSpecularEnv, and T10 keeps the
  // procedural sky off the IBL array entirely. Everything this technique reads
  // comes from the atmosphere data plus the frame state the forward prologue
  // published alongside the LUTs it baked — so the visible sky and the analytic
  // disc share ONE sun direction, the one the sky-view LUT was baked with.
  //////////////////////////////////////////////////////////
  auto procsky_lambda = [this](const RenderContextInstData& RCID) {
    auto mut       = (PBRMaterial*)this;
    auto RCFD      = RCID.rcfd();
    auto context   = RCFD->GetTarget();
    auto FXI       = context->FXI();
    auto pbrcommon = RCFD->_pbrcommon;
    //////////////////////////////////////////////////////////
    // SINGLE-PASS STEREO producer. The _ST fragment's ONLY per-view input is
    //  ublk_stereo, and this lambda is the procedural sky's whole state -- it
    //  deliberately does not run createBasicStateLambda (see above), which is
    //  where every other stereo path gets that block written and bound. Without
    //  this, the block reads as the shared ZERO buffer: no validation error, no
    //  crash, an unprojection through a zero matrix. Same one writer as every
    //  other producer, so the sky's view state is byte-identical to the geometry's.
    //////////////////////////////////////////////////////////
    if (RCFD->hasCPD()) {
      const auto& CPD = RCFD->topCPD();
      if (CPD.isSinglePassStereo()) {
        auto stereocams = CPD._stereo_cam_matrices;
        OrkAssertI(
            stereocams != nullptr,
            "procedural sky drawn in a single-pass-stereo pass with no stereo camera "
            "matrices on the CPD -- the per-view unprojection has nothing to read");
        PBRMaterial::writeStereoBlock(FXI, context, stereocams);
        OrkAssertI(
            this->_parStereoBlock != nullptr,
            "procedural sky material declares no ublk_stereo -- FWD_SKYBOX_PROC_ST would "
            "unproject through a zero matrix, silently");
        FXI->bindUniformBuffer(this->_parStereoBlock, PBRMaterial::stereoDataBuffer(context));
      }
    }
    auto atmo      = pbrcommon ? pbrcommon->_atmosphere : nullptr;
    OrkAssertI(
        atmo != nullptr,
        "FWD_SKYBOX_PROC drawn with no SkyAtmosphereData on pbr::CommonStuff (the forward "
        "prologue attaches the default whenever the sky source is procedural)");
    auto skyframe = RCFD->userPropertyAs<pbr::skyframestate_ptr_t>("SKY_FRAME"_crcu);
    OrkAssertI(
        skyframe and skyframe->_skyViewLUT and skyframe->_transmittanceLUT,
        "FWD_SKYBOX_PROC drawn before the prologue published this frame's sky-view LUT");

    if (this->_parSkyRadii)
      FXI->bindParamVect4(
          this->_parSkyRadii, //
          fvec4(atmo->_groundRadius, atmo->topRadius(), skyframe->_viewAltitudeKm, 0.0f));
    if (this->_parSkySunDirection)
      FXI->bindParamVect4(this->_parSkySunDirection, fvec4(skyframe->_dirToSun, 0.0f));
    if (this->_parSkySunIlluminance)
      FXI->bindParamVect4(this->_parSkySunIlluminance, fvec4(atmo->_sunIlluminance, 0.0f));
    // the below-horizon floor's target color (skySampleSkyView) — the only
    // medium value this pass reads, since the LUT carries all the rest
    if (this->_parSkyGroundAlbedo)
      FXI->bindParamVect4(this->_parSkyGroundAlbedo, fvec4(atmo->_groundAlbedo, 0.0f));
    if (this->_parSkySunDisc)
      FXI->bindParamVect4(
          this->_parSkySunDisc, //
          fvec4(
              atmo->_sunDiscAngularRadius * float(DTOR), //
              atmo->_sunDiscIntensity,                   //
              atmo->_sunDiscLimbSoftness,                //
              atmo->_skyExposure));                      //
    // the moon rides the SAME frame state as the sun, so the disc and the phase
    // it is lit with can never disagree with the sky's sun. A zero _dirToMoon
    // (no declared moon) lands in .w as the shader's disable.
    bool has_moon = skyframe->_dirToMoon.magnitudeSquared() > 0.0f;
    if (this->_parSkyMoonDirection)
      FXI->bindParamVect4(
          this->_parSkyMoonDirection, //
          fvec4(has_moon ? skyframe->_dirToMoon.normalized() : fvec3(0, 1, 0), has_moon ? 1.0f : 0.0f));
    if (this->_parSkyMoonDisc)
      FXI->bindParamVect4(
          this->_parSkyMoonDisc, //
          fvec4(
              atmo->_moonDiscAngularRadius * float(DTOR), //
              atmo->_moonDiscIntensity,                   //
              atmo->_moonLimbSoftness,                    //
              atmo->_moonTerminatorSoftness));            //
    if (this->_parSkyMoonAlbedo)
      FXI->bindParamVect4(this->_parSkyMoonAlbedo, fvec4(atmo->_moonAlbedoColor, 0.0f));
    // NIGHT EMISSION. Note _skyExposure alone rides in SkySunDisc.w above — the
    // IBL snapshot's own bind is what multiplies in the capture pre-scale, and
    // the visible sky must never carry it.
    if (this->_parSkyNightEmission)
      FXI->bindParamVect4(
          this->_parSkyNightEmission, //
          fvec4(
              atmo->_airglowIntensity,     //
              atmo->_starlightIntensity,   //
              atmo->_moonRayleighStrength, //
              atmo->_airglowAltitudeKm));  //
    if (this->_parSkyMoonIlluminance)
      FXI->bindParamVect4(this->_parSkyMoonIlluminance, fvec4(skyframe->_moonIlluminance, 0.0f));
    if (this->_parSkyViewLut)
      FXI->bindParamTexture(this->_parSkyViewLut, skyframe->_skyViewLUT.get());
    if (this->_parSkyTransmittanceLut)
      FXI->bindParamTexture(this->_parSkyTransmittanceLut, skyframe->_transmittanceLUT.get());

    ////////////////////////////////////////////////////////////////////////
    // CLOUD OCCLUSION of the discs — the sun cookie's second consumer, and the
    // SAME cookie the ground shadows read (LightManager, filled by the forward
    // prologue). The lookup is resolved to ONE uv here because the eye position
    // is known on this side; the shader then costs a single fetch.
    //
    // WHICH BODY: the cookie is a projection along the CASCADE HOLDER's
    // direction, so it may only dim the body it was baked toward. At night the
    // moon holds the cascade and the moon gate lights up; by day the sun's
    // does. A body the cookie was NOT baked toward is left alone — occluding it
    // with someone else's shadow would be a fabrication, not a v1 limitation.
    ////////////////////////////////////////////////////////////////////////
    fvec4 cookie_params(0, 0, 0, 0);
    fvec4 cookie_body(0, 0, 0, 0);
    texture_ptr_t cookie_tex;
    auto CIMPL = RCFD->topCompositor();
    if (auto LMGR = CIMPL ? CIMPL->lightManager() : nullptr) {
      cookie_tex = LMGR->_sun_cookie ? LMGR->_sun_cookie : LMGR->_sun_cookie_default;
      bool armed = LMGR->_sun_cookie                                  //
                   and (LMGR->_sun_cookie_strength > 0.0f)            //
                   and (LMGR->_sun_cookie != LMGR->_sun_cookie_default);
      if (armed) {
        auto cammtx = RCFD->topCPD().cameraMatrices();
        fvec3 eye   = cammtx ? cammtx->GetIVMatrix().translation() : fvec3(0, 0, 0);
        fvec4 cuv   = fvec4(eye, 1).transform(LMGR->_sun_cookie_matrix);
        if (cuv.w > 0.0f) {
          fvec2 uv(cuv.x / cuv.w, cuv.y / cuv.w);
          // outside the projected footprint the cookie says nothing, and the
          // shader's clamp sampler would smear the border texel across the
          // whole sky — disarm instead (transmittance 1, as at the edge of a
          // finite map everywhere else).
          if (uv.x >= 0.0f and uv.x <= 1.0f and uv.y >= 0.0f and uv.y <= 1.0f) {
            // the DISC's own lod, not the ground's: a transit edge is crisp
            // (see DirectionalLightData::_cloudDiscSoftness).
            cookie_params = fvec4(uv.x, uv.y, LMGR->_sun_cookie_disc_lod, LMGR->_sun_cookie_strength);
            cookie_body.z = LMGR->_sun_cookie_extinction;
            // the cookie direction is where the light TRAVELS; the sky's are
            // directions TOWARD the body.
            fvec3 to_body = LMGR->_sun_cookie_dir * -1.0f;
            cookie_body.x = (to_body.dotWith(skyframe->_dirToSun.normalized()) > 0.999f) ? 1.0f : 0.0f;
            if (has_moon)
              cookie_body.y = (to_body.dotWith(skyframe->_dirToMoon.normalized()) > 0.999f) ? 1.0f : 0.0f;
          }
        }
      }
    }
    if (getenv("ORKID_SUN_COOKIE_TRACE")) {
      static int s_disc_trace = 0;
      if (0 == (s_disc_trace++ % 240)) {
        printf(
            "[suncookie:disc] params<%g %g %g %g> body<%g %g> tosun<%g %g %g>\n",
            cookie_params.x, cookie_params.y, cookie_params.z, cookie_params.w,
            cookie_body.x, cookie_body.y,
            skyframe->_dirToSun.x, skyframe->_dirToSun.y, skyframe->_dirToSun.z);
        fflush(stdout);
      }
    }
    if (this->_parSkyCloudCookie and cookie_tex)
      FXI->bindParamTexture(this->_parSkyCloudCookie, cookie_tex.get());
    if (this->_parSkyCookieParams)
      FXI->bindParamVect4(this->_parSkyCookieParams, cookie_params);
    if (this->_parSkyCookieBody)
      FXI->bindParamVect4(this->_parSkyCookieBody, cookie_body);

    mut->_rasterstate->setCullTest(ECullTest::OFF);
    mut->_rasterstate->setWriteMaskZ(false);
    mut->_rasterstate->setDepthTest(EDepthTest::LEQUALS);
    mut->_rasterstate->setWriteMaskRGB(true);
    mut->_rasterstate->setWriteMaskA(true);
  };
  //////////////////////////////////////////////////////////
  OrkAssert(permu._instanced == false);
  OrkAssert(permu._skinned == false);
  //////////////////////////////////////////////////////////
  // The draw site FORCES the procedural technique (RCID._forced_technique, which
  // keys the pipeline cache) when the scene's sky source is procedural: the two
  // sources therefore resolve to two separately cached pipelines and the switch
  // is live per frame, while baked mode still builds exactly the pipeline it
  // always did (L6 byte-identity).
  //////////////////////////////////////////////////////////
  // THE SKYBOX SELECTOR — ONE decision, made once: SOURCE picks the family
  // (procedural vs baked) and VIEW MODE picks within it. Not a chain of arms per
  // lane: two families x two view modes is a 2x2, and writing it as a 2x2 is what
  // keeps a later family (or a later view mode) from being bolted on beside the
  // others instead of into them.
  //
  //  - SOURCE: the draw site FORCES the procedural technique, so a forced-technique
  //    match IS the procedural request.
  //  - VIEW MODE: under single-pass stereo the sky's per-view state is the
  //    unprojection matrix, which the _ST fragment reads as spvr_inv_vp[view] out
  //    of ublk_stereo. The state lambda runs the basic-state lambda, which is what
  //    writes and binds that block, so no per-eye bind is needed here.
  //  - Each _ST slot is NULL-GUARDED: a build whose shader lacks the peer selects
  //    its mono twin rather than a null pipeline, and both stereo peers still
  //    declare ublk_std_matrices, so the mono IVP bind below is right for all four.
  //////////////////////////////////////////////////////////
  const bool want_procedural =
      this->_tek_FWD_SKYBOX_PROC and (permu._forced_technique == this->_tek_FWD_SKYBOX_PROC);

  fxtechnique_constptr_t sky_technique = nullptr;
  FxPipeline::statelambda_t sky_lambda;

  if (want_procedural) {
    sky_technique = (permu._stereo and this->_tek_FWD_SKYBOX_PROC_ST) //
                        ? this->_tek_FWD_SKYBOX_PROC_ST               //
                        : this->_tek_FWD_SKYBOX_PROC;
    sky_lambda    = procsky_lambda;
  } else {
    sky_technique = (permu._stereo and this->_tek_FWD_SKYBOX_ST) //
                        ? this->_tek_FWD_SKYBOX_ST               //
                        : this->_tek_FWD_SKYBOX_MO;
    sky_lambda    = skybox_lambda;
  }

  if (sky_technique) {
    pipeline             = std::make_shared<FxPipeline>(permu);
    pipeline->_technique = sky_technique;
    pipeline->bindParam(this->_paramIVP, "RCFD_Camera_IVP_NoTrans_Mono"_crcsh);
    pipeline->addStateLambda(sky_lambda);
    //////////////////////////////////////////////////////////
    // OBSERVABLE SELECTION, AT THE POINT OF USE. A mono skybox and a per-view
    //  one look alike in a still, so "it looked right in the window" is not
    //  evidence. Neither is a line printed HERE, at pipeline construction: that
    //  proves an intent, not a draw. So the announcement rides a state lambda —
    //  those run when the pipeline is actually BOUND for a draw — and reports the
    //  technique that draw took together with the pass's OWN stereo bit read off
    //  the live CPD, which is the fact in question. Once per pipeline, so a
    //  60Hz demo does not drown in it.
    //  Grep token: SPVR:SKYSEL
    //////////////////////////////////////////////////////////
    auto announced   = std::make_shared<bool>(false);
    auto tekname     = sky_technique->_techniqueName;
    auto sourcename  = want_procedural ? "procedural" : "baked";
    bool permu_stereo = permu._stereo;
    pipeline->addStateLambda([announced, tekname, sourcename, permu_stereo] //
                             (const RenderContextInstData& RCID) {
      if (*announced)
        return;
      *announced   = true;
      auto RCFD    = RCID.rcfd();
      bool cpd_st  = RCFD->hasCPD() ? RCFD->topCPD().isSinglePassStereo() : false;
      printf(
          "[SPVR:SKYSEL] skybox DRAW technique<%s> source<%s> permu_stereo<%d> pass_stereo<%d>\n",
          tekname.c_str(),
          sourcename,
          int(permu_stereo),
          int(cpd_st));
      fflush(stdout);
    });
  }
  if(pipeline){
    pipeline->_material_ptr = (GfxMaterial*) this;
    pipeline->_rasterstate = this->_rasterstate;
  }
  return pipeline;
}

///////////////////////////////////////////////////////////////////////////////

fxpipeline_ptr_t PBRMaterial::_createFxPipelineVTX(const FxPipelinePermutation& permu) const {
  fxpipeline_ptr_t pipeline;
  auto no_cull_stateblock = [this](const RenderContextInstData& RCID) {
    auto mut   = (PBRMaterial*)this;
    auto RCFD    = RCID.rcfd();
    auto context = RCFD->GetTarget();
    auto FXI     = context->FXI();
    auto MTXI    = context->MTXI();
    //auto RSI     = context->RSI();
    mut->_rasterstate->setCullTest(ECullTest::OFF);
    mut->_rasterstate->setDepthTest(EDepthTest::OFF);
    mut->_rasterstate->setWriteMaskZ(true);
    mut->_rasterstate->setWriteMaskRGB(true);
    mut->_rasterstate->setWriteMaskA(false);
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
    case "PICKING"_crcu: {
      if (not permu._instanced and not permu._skinned and not permu._stereo) {
        if (this->_tek_PIK_RI_NI) {
          pipeline             = std::make_shared<FxPipeline>(permu);
          pipeline->_technique = this->_tek_PIK_RI_NI;
          pipeline->bindParam(this->_paramMVP, "RCFD_Camera_Pick"_crcsh);
          pipeline->bindParam(this->_paramM, "RCFD_M"_crcsh);
          pipeline->bindParam(this->_paramMROT, "RCFD_Model_Rot"_crcsh);
          pipeline->bindParam(this->_parPickID, "RCID_PickID"_crcsh);
          OrkAssert(pipeline->_technique != nullptr);
        }
      }
      break;
    }
    default:
      break;
  }
  if(pipeline){
    pipeline->_material_ptr = (GfxMaterial*) this;
    pipeline->_rasterstate = this->_rasterstate;
  }
  return pipeline;
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
