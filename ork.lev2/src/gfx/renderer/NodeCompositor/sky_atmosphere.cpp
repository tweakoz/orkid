////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/sky_atmosphere.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/reflect/properties/DirectTyped.hpp>
#include <ork/reflect/properties/registerX.inl>
#include <ork/util/crc64.h>

ImplementReflectionX(ork::lev2::pbr::SkyAtmosphereData, "pbr::SkyAtmosphereData");

////////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::pbr {
////////////////////////////////////////////////////////////////////////////////

void SkyAtmosphereData::describeX(class_t* c) {
  // SKYLIGHT lane B — Hillaire medium tunables (A8: every artist knob reflected).
  c->floatProperty("GroundRadius", float_range{100.0f, 100000.0f}, &SkyAtmosphereData::_groundRadius);
  c->floatProperty("AtmosphereThickness", float_range{1.0f, 1000.0f}, &SkyAtmosphereData::_atmosphereThickness);

  c->directProperty("RayleighScattering", &SkyAtmosphereData::_rayleighScattering);
  c->floatProperty("RayleighScaleHeight", float_range{0.1f, 100.0f}, &SkyAtmosphereData::_rayleighScaleHeight);

  c->floatProperty("MieScattering", float_range{0.0f, 1.0f}, &SkyAtmosphereData::_mieScattering);
  c->floatProperty("MieExtinction", float_range{0.0f, 1.0f}, &SkyAtmosphereData::_mieExtinction);
  c->floatProperty("MieScaleHeight", float_range{0.01f, 100.0f}, &SkyAtmosphereData::_mieScaleHeight);
  c->floatProperty("MiePhaseG", float_range{-0.99f, 0.99f}, &SkyAtmosphereData::_miePhaseG);

  c->directProperty("OzoneAbsorption", &SkyAtmosphereData::_ozoneAbsorption);
  c->floatProperty("OzoneCenterAltitude", float_range{0.0f, 100.0f}, &SkyAtmosphereData::_ozoneCenterAltitude);
  c->floatProperty("OzoneTentHalfWidth", float_range{0.1f, 100.0f}, &SkyAtmosphereData::_ozoneTentHalfWidth);

  c->directProperty("GroundAlbedo", &SkyAtmosphereData::_groundAlbedo);
  c->directProperty("SunIlluminance", &SkyAtmosphereData::_sunIlluminance);

  c->floatProperty("KilometersPerWorldUnit", float_range{1.0e-6f, 1.0f}, &SkyAtmosphereData::_kilometersPerWorldUnit);
  c->floatProperty("MinViewAltitude", float_range{0.0f, 10.0f}, &SkyAtmosphereData::_minViewAltitude);

  // visible-sky presentation (slice B2) — no LUT dependency, see mediumHash()
  c->floatProperty("SunDiscAngularRadius", float_range{0.0f, 30.0f}, &SkyAtmosphereData::_sunDiscAngularRadius);
  c->floatProperty("SunDiscIntensity", float_range{0.0f, 10000.0f}, &SkyAtmosphereData::_sunDiscIntensity);
  c->floatProperty("SunDiscLimbSoftness", float_range{0.0f, 8.0f}, &SkyAtmosphereData::_sunDiscLimbSoftness);
  c->floatProperty("SkyExposure", float_range{0.0f, 10000.0f}, &SkyAtmosphereData::_skyExposure);

  // moon disc — presentation, same tier as the sun disc
  c->floatProperty("MoonDiscAngularRadius", float_range{0.0f, 30.0f}, &SkyAtmosphereData::_moonDiscAngularRadius);
  c->floatProperty("MoonDiscIntensity", float_range{0.0f, 10000.0f}, &SkyAtmosphereData::_moonDiscIntensity);
  c->floatProperty("MoonLimbSoftness", float_range{0.0f, 8.0f}, &SkyAtmosphereData::_moonLimbSoftness);
  c->floatProperty("MoonTerminatorSoftness", float_range{0.0f, 2.0f}, &SkyAtmosphereData::_moonTerminatorSoftness);
  c->directProperty("MoonAlbedoColor", &SkyAtmosphereData::_moonAlbedoColor);

  // night emission — same presentation tier; see the derivation in the header
  c->floatProperty("AirglowIntensity", float_range{0.0f, 1.0f}, &SkyAtmosphereData::_airglowIntensity);
  c->floatProperty("StarlightIntensity", float_range{0.0f, 1.0f}, &SkyAtmosphereData::_starlightIntensity);
  c->floatProperty("MoonRayleighStrength", float_range{0.0f, 1000.0f}, &SkyAtmosphereData::_moonRayleighStrength);
  c->floatProperty("AirglowAltitudeKm", float_range{1.0f, 1000.0f}, &SkyAtmosphereData::_airglowAltitudeKm);

  // aerial perspective / ground haze — presentation tier, outside mediumHash()
  c->directProperty("AerialPerspectiveEnable", &SkyAtmosphereData::_aerialPerspectiveEnable);
  c->floatProperty("HazeDensity", float_range{0.0f, 10.0f}, &SkyAtmosphereData::_hazeDensity);
  c->floatProperty("HazeScaleHeight", float_range{0.01f, 10.0f}, &SkyAtmosphereData::_hazeScaleHeight);
  c->floatProperty("HazePhaseG", float_range{-0.99f, 0.99f}, &SkyAtmosphereData::_hazePhaseG);
  c->directProperty("HazeScatterTint", &SkyAtmosphereData::_hazeScatterTint);
  c->directProperty("HazeInscatterTint", &SkyAtmosphereData::_hazeInscatterTint);
  c->floatProperty("HazeMaxDistanceKm", float_range{1.0f, 1000.0f}, &SkyAtmosphereData::_hazeMaxDistanceKm);
  // 0/1/2 = off / inline / quarter-res (the header names the modes)
  c->floatProperty("HazeSunShadow", float_range{0.0f, 2.0f}, &SkyAtmosphereData::_hazeSunShadow);
  // artistic accentuation of the shafts; 1 = physical. The authoring range is
  // advisory — the shader clamps only the sign-critical end.
  c->floatProperty("HazeSunShadowGain", float_range{0.0f, 4.0f}, &SkyAtmosphereData::_hazeSunShadowGain);

  // IBL feed (slice B3) — likewise outside mediumHash(); see the header.
  c->intProperty("IblSnapshotWidth", int_range{16, 4096}, &SkyAtmosphereData::_iblSnapshotWidth);
  c->intProperty("IblSnapshotHeight", int_range{8, 2048}, &SkyAtmosphereData::_iblSnapshotHeight);
  c->intProperty("IblSpecularSamples", int_range{16, 8192}, &SkyAtmosphereData::_iblSpecularSamples);
  c->floatProperty("IblRefilterAngleDeg", float_range{0.0f, 180.0f}, &SkyAtmosphereData::_iblRefilterAngleDeg);
  c->intProperty("IblCrossfadeFrames", int_range{0, 240}, &SkyAtmosphereData::_iblCrossfadeFrames);
  c->floatProperty("IblCrossfadeMaxSecs", float_range{0.0f, 60.0f}, &SkyAtmosphereData::_iblCrossfadeMaxSecs);
  c->directProperty("IblFeedEnable", &SkyAtmosphereData::_iblFeedEnable);
  c->directProperty("IblContinuousChain", &SkyAtmosphereData::_iblContinuousChain);
  c->floatProperty("IblChainMinAngleDeg", float_range{0.0f, 180.0f}, &SkyAtmosphereData::_iblChainMinAngleDeg);
  c->floatProperty("IblChainMaxHz", float_range{0.0f, 240.0f}, &SkyAtmosphereData::_iblChainMaxHz);
  c->floatProperty("IblSnapshotInterval", float_range{0.0f, 600.0f}, &SkyAtmosphereData::_iblSnapshotInterval);
  c->floatProperty("IblCaptureScale", float_range{1.0f, 65504.0f}, &SkyAtmosphereData::_iblCaptureScale);
  // granularity — 0 is the UNSET sentinel and must stay inside every range
  c->intProperty("IblLevelBatches", int_range{0, 256}, &SkyAtmosphereData::_iblLevelBatches);
  c->intProperty("IblSlicesPerFrame", int_range{0, 256}, &SkyAtmosphereData::_iblSlicesPerFrame);
  c->intProperty("IblMipChainBudgetPx", int_range{0, 16777216}, &SkyAtmosphereData::_iblMipChainBudgetPx);
}

///////////////////////////////////////////////////////////

uint64_t SkyAtmosphereData::mediumHash() const {
  // Only the medium description participates — the sky-view LUT rebuilds every
  // frame anyway, so view/sun state deliberately does NOT invalidate the bake.
  boost::Crc64 hasher;
  hasher.init();
  hasher.accumulateItem(_groundRadius);
  hasher.accumulateItem(_atmosphereThickness);
  hasher.accumulateItem(_rayleighScattering);
  hasher.accumulateItem(_rayleighScaleHeight);
  hasher.accumulateItem(_mieScattering);
  hasher.accumulateItem(_mieExtinction);
  hasher.accumulateItem(_mieScaleHeight);
  hasher.accumulateItem(_miePhaseG);
  hasher.accumulateItem(_ozoneAbsorption);
  hasher.accumulateItem(_ozoneCenterAltitude);
  hasher.accumulateItem(_ozoneTentHalfWidth);
  hasher.accumulateItem(_groundAlbedo);
  hasher.accumulateItem(_sunIlluminance);
  hasher.finish();
  return hasher.result();
}

///////////////////////////////////////////////////////////

uint64_t SkyAtmosphereData::hazePresentationHash() const {
  // Everything the snapshot's haze overlay reads, and nothing else: the medium
  // it also reads is already covered by mediumHash(), and the exposure pair
  // (_skyExposure / _iblCaptureScale) scales the snapshot uniformly, so folding
  // them in here would start a refilter cycle on a knob the decode divides back
  // out anyway.
  boost::Crc64 hasher;
  hasher.init();
  hasher.accumulateItem(_aerialPerspectiveEnable);
  hasher.accumulateItem(_hazeDensity);
  hasher.accumulateItem(_hazeScaleHeight);
  hasher.accumulateItem(_hazePhaseG);
  hasher.accumulateItem(_hazeScatterTint);
  hasher.accumulateItem(_hazeInscatterTint);
  hasher.accumulateItem(_hazeMaxDistanceKm);
  hasher.accumulateItem(_hazeSunShadow);
  hasher.finish();
  return hasher.result();
}

////////////////////////////////////////////////////////////////////////////////

hillairesky_ptr_t HillaireSky::create(Context* ctx) {
  auto sky = std::make_shared<HillaireSky>();
  sky->_init(ctx);
  return sky;
}

///////////////////////////////////////////////////////////

void HillaireSky::_init(Context* ctx) {
  if (_material)
    return;

  auto make_lut = [ctx](int w, int h, const char* name) -> rtgroup_ptr_t {
    auto rtg    = std::make_shared<RtGroup>(ctx, w, h);
    rtg->_name  = name;
    auto buffer = rtg->createRenderTarget(EBufferFormat::RGBA16F);
    buffer->_debugName = name;
    // CLAMP, not the default WRAP: every one of these LUTs has a MEANINGFUL
    // edge (u=0 is the sun azimuth, v=0 the zenith, u=1 the horizon-grazing
    // ray). Wrapping folds the opposite, near-black edge into the bilinear tap
    // and draws a dark seam straight through the sun on the visible sky.
    // Recorded here, PUSHED to the sampler in _applyLutSampling — an RTG's
    // texture is handed the context's base sampler when it is realized
    // (vulkan_txi_from_rtg), so the mode has to be re-applied after that.
    auto& sampling         = buffer->_texture->TexSamplingMode();
    sampling._texAddrModeS = TextureAddressMode::CLAMP;
    sampling._texAddrModeT = TextureAddressMode::CLAMP;
    sampling._texAddrModeR = TextureAddressMode::CLAMP;
    return rtg;
  };

  _rtgTransmittance = make_lut(kTransmittanceW, kTransmittanceH, "SkyTransmittanceLUT");
  _rtgMultiScatter  = make_lut(kMultiScatterW, kMultiScatterH, "SkyMultiScatterLUT");
  _rtgSkyView       = make_lut(kSkyViewW, kSkyViewH, "SkyViewLUT");

  _material = std::make_shared<FreestyleMaterial>();
  _material->gpuInit(ctx, "orkshader://sky");
  _material->_rasterstate->setBlendingMacro(BlendingMacro::OFF);
  _material->_rasterstate->setDepthTest(EDepthTest::OFF);
  _material->_rasterstate->setCullTest(ECullTest::OFF);
  _material->_rasterstate->setWriteMaskZ(false);
  _material->_rasterstate->setWriteMaskRGB(true);
  _material->_rasterstate->setWriteMaskA(true);

  _tekTransmittance = _material->technique("sky_transmittance");
  _tekMultiScatter  = _material->technique("sky_multiscatter");
  _tekSkyView       = _material->technique("sky_skyview");
  _tekEquirect      = _material->technique("sky_equirect_snapshot");
  OrkAssertI(
      _tekTransmittance and _tekMultiScatter and _tekSkyView and _tekEquirect,
      "orkshader://sky must declare sky_transmittance / sky_multiscatter / sky_skyview / sky_equirect_snapshot");

  _parRayleighScatter  = _material->param("SkyRayleighScatter");
  _parMieScatter       = _material->param("SkyMieScatter");
  _parOzoneAbsorb      = _material->param("SkyOzoneAbsorb");
  _parOzoneTent        = _material->param("SkyOzoneTent");
  _parRadii            = _material->param("SkyRadii");
  _parGroundAlbedo     = _material->param("SkyGroundAlbedo");
  _parSunIlluminance   = _material->param("SkySunIlluminance");
  _parSunDirection     = _material->param("SkySunDirection");
  _parLutDims          = _material->param("SkyLutDims");
  _parSunDisc          = _material->param("SkySunDisc");
  _parMoonDirection    = _material->param("SkyMoonDirection");
  _parMoonDisc         = _material->param("SkyMoonDisc");
  _parNightEmission    = _material->param("SkyNightEmission");
  _parMoonIlluminance  = _material->param("SkyMoonIlluminance");
  _parTransmittanceLut = _material->param("SkyTransmittanceLUT");
  _parMultiScatterLut  = _material->param("SkyMultiScatterLUT");
  _parSkyViewLut       = _material->param("SkyViewLUT");
  _parHazeDensity       = _material->param("SkyHazeDensity");
  _parHazeScatterTint   = _material->param("SkyHazeScatterTint");
  _parHazeInscatterTint = _material->param("SkyHazeInscatterTint");
  _parHazeGeom          = _material->param("SkyHazeGeom");
  OrkAssertI(_parRadii and _parLutDims, "ublk_sky_atmo params missing from orkshader://sky");
}

///////////////////////////////////////////////////////////
// A8: the whole medium travels through ublk_sky_atmo. The only per-pass value
// that is NOT a user knob is SkyLutDims (the raster extent of the target LUT).
///////////////////////////////////////////////////////////

void HillaireSky::_bindAtmosphere(skyatmospheredata_ptr_t atmo, int lut_w, int lut_h) {
  _material->bindParamVec4(_parRayleighScatter, fvec4(atmo->_rayleighScattering, atmo->_rayleighScaleHeight));
  _material->bindParamVec4(
      _parMieScatter,
      fvec4(atmo->_mieScattering, atmo->_mieExtinction, atmo->_mieScaleHeight, atmo->_miePhaseG));
  _material->bindParamVec4(_parOzoneAbsorb, fvec4(atmo->_ozoneAbsorption, 0.0f));
  _material->bindParamVec4(_parOzoneTent, fvec4(atmo->_ozoneCenterAltitude, atmo->_ozoneTentHalfWidth, 0.0f, 0.0f));
  _material->bindParamVec4(_parGroundAlbedo, fvec4(atmo->_groundAlbedo, 0.0f));
  _material->bindParamVec4(_parSunIlluminance, fvec4(atmo->_sunIlluminance, 0.0f));
  _material->bindParamVec4(_parLutDims, fvec4(float(lut_w), float(lut_h), 1.0f / float(lut_w), 1.0f / float(lut_h)));
  // NIGHT EMISSION reaches every pass through this one bind. Only the snapshot
  // pass samples it (skySampleSkyView), but the block is shared with the LUT
  // bakes, and an unbound member is whatever the last pass left there.
  _material->bindParamVec4(
      _parNightEmission,
      fvec4(atmo->_airglowIntensity, atmo->_starlightIntensity, atmo->_moonRayleighStrength, atmo->_airglowAltitudeKm));
  _material->bindParamVec4(_parMoonDisc, fvec4(atmo->_moonDiscAngularRadius * float(DTOR), 0.0f, 0.0f, 0.0f));
  // MOON OFF by default (.w is the gate), overridden by renderEquirectSnapshot,
  // which is the only pass with a moon to scatter.
  _material->bindParamVec4(_parMoonDirection, fvec4(0.0f, 1.0f, 0.0f, 0.0f));
  _material->bindParamVec4(_parMoonIlluminance, fvec4(0.0f, 0.0f, 0.0f, 0.0f));
  // HAZE OFF by default, same shape as the moon above: .w is the ARMED gate and
  // renderEquirectSnapshot is the only pass that may raise it. The three LUT
  // bakes describe the MEDIUM, and the artist haze layer is not in it — a bake
  // that read these lanes would be baking a look into a physical table.
  if (_parHazeDensity)
    _material->bindParamVec4(_parHazeDensity, fvec4(0.0f, 1.0f, 0.0f, 0.0f));
}

///////////////////////////////////////////////////////////

// Push -> begin -> bind -> quad -> end -> pop is the order the other offscreen
// fullscreen passes use (_render_ssao_linearize_depth, exportEquirectangular):
// begin() applies raster state into the ALREADY ACTIVE render pass.
void HillaireSky::_renderLut(
    Context* ctx,                     //
    rcfd_ptr_t RCFD,                  //
    const FxShaderTechnique* tek,     //
    rtgroup_ptr_t rtg,                //
    const void_lambda_t& bind_params) { //

  auto FBI             = ctx->FBI();
  rtg->_autoclear      = true;
  rtg->_clearMaskColor = true;
  rtg->_clearMaskDepth = false;
  FBI->PushRtGroup(rtg.get());
  _material->begin(tek, RCFD);
  bind_params();
  ctx->DWI()->fullscreenQuad();
  _material->end(RCFD);
  FBI->PopRtGroup();
}

///////////////////////////////////////////////////////////

bool HillaireSky::bakeStaticLuts(Context* ctx, rcfd_ptr_t RCFD, skyatmospheredata_ptr_t atmo) {
  OrkAssertI(atmo != nullptr, "HillaireSky::bakeStaticLuts requires a SkyAtmosphereData");
  _init(ctx);

  // NOTE (slice B2): the sun-disc / exposure knobs are NOT in mediumHash — they
  // only affect how the LUTs are presented, so editing them must not re-bake.
  uint64_t hash = atmo->mediumHash();
  if (_staticLutsValid and hash == _bakedMediumHash)
    return false;

  auto radii = fvec4(atmo->_groundRadius, atmo->topRadius(), 0.0f, 0.0f);

  ctx->debugPushGroup("HillaireSky::transmittance");
  _renderLut(ctx, RCFD, _tekTransmittance, _rtgTransmittance, [&]() {
    _bindAtmosphere(atmo, kTransmittanceW, kTransmittanceH);
    _material->bindParamVec4(_parRadii, radii);
  });
  ctx->debugPopGroup();

  // The LUT images can be left in COLOR_ATTACHMENT layout by whatever last
  // touched them (a capture's _transitionToRenderTarget does exactly that), and
  // bindParamTexture asserts loudly on a non-sampleable layout. Transition here,
  // OUTSIDE any active render pass — inside one it would be illegal.
  ctx->FBI()->rtGroupTransitionToTexture(_rtgTransmittance.get());

  ctx->debugPushGroup("HillaireSky::multiscatter");
  _renderLut(ctx, RCFD, _tekMultiScatter, _rtgMultiScatter, [&]() {
    _bindAtmosphere(atmo, kMultiScatterW, kMultiScatterH);
    _material->bindParamVec4(_parRadii, radii);
    _material->bindParamTexture(_parTransmittanceLut, _rtgTransmittance->texture(0).get());
  });
  ctx->debugPopGroup();

  _bakedMediumHash = hash;
  _staticLutsValid = true;
  return true;
}

///////////////////////////////////////////////////////////

void HillaireSky::updateSkyView(
    Context* ctx,                  //
    rcfd_ptr_t RCFD,               //
    skyatmospheredata_ptr_t atmo,  //
    const fvec3& dir_to_sun,       //
    float view_altitude_km) {      //

  OrkAssertI(atmo != nullptr, "HillaireSky::updateSkyView requires a SkyAtmosphereData");
  // Self-defending: the sky-view LUT reads both static LUTs, so bake them if a
  // caller reached here first (or if the medium changed since the last bake).
  bakeStaticLuts(ctx, RCFD, atmo);

  float alt = std::max(view_altitude_km, atmo->_minViewAltitude);

  // see the note in bakeStaticLuts — both source LUTs must be sampleable before
  // the sky-view render pass opens.
  ctx->FBI()->rtGroupTransitionToTexture(_rtgTransmittance.get());
  ctx->FBI()->rtGroupTransitionToTexture(_rtgMultiScatter.get());

  ctx->debugPushGroup("HillaireSky::skyview");
  _renderLut(ctx, RCFD, _tekSkyView, _rtgSkyView, [&]() {
    _bindAtmosphere(atmo, kSkyViewW, kSkyViewH);
    _material->bindParamVec4(_parRadii, fvec4(atmo->_groundRadius, atmo->topRadius(), alt, 0.0f));
    _material->bindParamVec4(_parSunDirection, fvec4(dir_to_sun.normalized(), 0.0f));
    _material->bindParamTexture(_parTransmittanceLut, _rtgTransmittance->texture(0).get());
    _material->bindParamTexture(_parMultiScatterLut, _rtgMultiScatter->texture(0).get());
  });
  ctx->debugPopGroup();

  // the sky-view LUT is CONSUMED this same frame (skybox pass, probe captures)
  // — hand it over sampleable, outside any render pass, exactly as the two
  // static LUTs are handed to the passes that read them.
  ctx->FBI()->rtGroupTransitionToTexture(_rtgSkyView.get());

  _applyLutSampling(ctx);
}

///////////////////////////////////////////////////////////
// IBL SNAPSHOT (slice B3). The §2 snapshot law lives in the CALLER's cycle gate
// — by the time control reaches here no refilter job may be consuming
// _rtgEquirect, which is exactly what makes re-rendering (and re-allocating) it
// legal.
///////////////////////////////////////////////////////////

void HillaireSky::renderEquirectSnapshot(
    Context* ctx,                    //
    rcfd_ptr_t RCFD,                 //
    skyatmospheredata_ptr_t atmo,    //
    const fvec3& dir_to_sun,         //
    float view_altitude_km,          //
    const fvec3& dir_to_moon,        //
    const fvec3& moon_illuminance) { //

  OrkAssertI(atmo != nullptr, "HillaireSky::renderEquirectSnapshot requires a SkyAtmosphereData");
  _init(ctx);

  int w = atmo->_iblSnapshotWidth;
  int h = atmo->_iblSnapshotHeight;
  OrkAssertIFMT(
      w >= 16 and h >= 8,
      "sky IBL snapshot extent %dx%d is too small to carry a hemisphere - check IblSnapshotWidth/Height",
      w,
      h);

  ////////////////////////////////////////

  if ((nullptr == _rtgEquirect) or (w != _snapshotW) or (h != _snapshotH)) {
    auto rtg           = std::make_shared<RtGroup>(ctx, w, h);
    rtg->_name         = "SkyEquirectSnapshot";
    auto buffer        = rtg->createRenderTarget(EBufferFormat::RGBA16F);
    buffer->_debugName = "SkyEquirectSnapshot";
    // the TEXTURE's name (not the buffer's) is what the prefilter names its
    // microtask and its published maps after — without it the sky's IBL shows up
    // in traces as an anonymous "rtg0".
    buffer->_texture->_debugName = "SkyEquirectSnapshot";
    // EQUIRECT sampling, not the LUTs' all-CLAMP: u wraps at the +-180 meridian
    // (a continuous azimuth), v/r clamp at the poles. Recorded here, PUSHED by
    // RadianceMapCache::refilterFromTexture right after this render — an RTG's
    // texture is handed the context's base sampler when it is realized, so the
    // mode has to be re-applied after that (same seam as _applyLutSampling).
    auto& sampling         = buffer->_texture->TexSamplingMode();
    sampling._texAddrModeS = TextureAddressMode::WRAP;
    sampling._texAddrModeT = TextureAddressMode::CLAMP;
    sampling._texAddrModeR = TextureAddressMode::CLAMP;
    _rtgEquirect = rtg;
    _snapshotW   = w;
    _snapshotH   = h;
  }

  ////////////////////////////////////////

  // self-defending, exactly as updateSkyView is: the snapshot reads the sky-view
  // LUT, which reads both static ones. It reads the TRANSMITTANCE LUT directly
  // as well — the night emission is extincted along the view ray — so that one
  // has to be sampleable here too.
  bakeStaticLuts(ctx, RCFD, atmo);
  ctx->FBI()->rtGroupTransitionToTexture(_rtgTransmittance.get());
  // the haze overlay folds multi-scatter into its source term (so a thick layer
  // does not go black in shadow), so this pass now samples the MS LUT too.
  ctx->FBI()->rtGroupTransitionToTexture(_rtgMultiScatter.get());
  ctx->FBI()->rtGroupTransitionToTexture(_rtgSkyView.get());

  float alt = std::max(view_altitude_km, atmo->_minViewAltitude);

  ctx->debugPushGroup("HillaireSky::equirect_snapshot");
  _renderLut(ctx, RCFD, _tekEquirect, _rtgEquirect, [&]() {
    _bindAtmosphere(atmo, w, h);
    _material->bindParamVec4(_parRadii, fvec4(atmo->_groundRadius, atmo->topRadius(), alt, 0.0f));
    _material->bindParamVec4(_parSunDirection, fvec4(dir_to_sun.normalized(), 0.0f));
    // only .w (the output scale) is read by the snapshot fragment. The disc
    // terms ride along because they share the block.
    //
    // THE CAPTURE PRE-SCALE ENCODE SEAM, and the only one there is. .w carries
    // the visible sky's exposure TIMES the capture gain, so the snapshot is
    // written hot enough for a night sky to clear the fp16 minimum normal; the
    // shader divides the gain straight back out at every env read
    // (stdtools lib_env_decode). The visible skybox binds _skyExposure ALONE
    // (material_pbr_misc procsky_lambda), which is what keeps the two apart.
    _material->bindParamVec4(
        _parSunDisc, //
        fvec4(
            atmo->_sunDiscAngularRadius * float(DTOR),      //
            atmo->_sunDiscIntensity,                        //
            atmo->_sunDiscLimbSoftness,                     //
            atmo->_skyExposure * atmo->_iblCaptureScale));  //
    // the moon the night emission scatters — the SKY_FRAME one, so the snapshot
    // and the visible sky can never disagree about where the moon is or how
    // bright it is. A zero direction is "none declared" and lands in .w as the
    // shader's gate.
    bool has_moon = dir_to_moon.magnitudeSquared() > 0.0f;
    _material->bindParamVec4(
        _parMoonDirection, //
        fvec4(has_moon ? dir_to_moon.normalized() : fvec3(0, 1, 0), has_moon ? 1.0f : 0.0f));
    _material->bindParamVec4(_parMoonIlluminance, fvec4(moon_illuminance, 0.0f));
    // THE ARTIST HAZE LAYER, on the IBL tier. This pass IS the sky producer:
    // the LUTs it samples are its own instance's and the sun it was handed is
    // the SKY_FRAME one, so none of the forward binder's conditions (SKY_FRAME
    // presence, probe capture) have anything to say here — armed is exactly
    // "there is an atmosphere and aerial perspective is enabled". The shader's
    // own .x gate keeps a purely geophysical scene on the untaken branch.
    //
    // bindSkyHazeState is NOT reusable here: it binds through a PBRMaterial's
    // resolved param handles and an RCID, and this is a FreestyleMaterial pass
    // with neither.
    bool haze_armed = atmo->_aerialPerspectiveEnable;
    if (_parHazeDensity)
      _material->bindParamVec4(
          _parHazeDensity, //
          haze_armed ? fvec4(atmo->_hazeDensity, atmo->_hazeScaleHeight, atmo->_hazePhaseG, 1.0f)
                     : fvec4(0.0f, 1.0f, 0.0f, 0.0f));
    if (haze_armed) {
      if (_parHazeScatterTint)
        _material->bindParamVec4(_parHazeScatterTint, fvec4(atmo->_hazeScatterTint, 0.0f));
      if (_parHazeInscatterTint)
        _material->bindParamVec4(_parHazeInscatterTint, fvec4(atmo->_hazeInscatterTint, 0.0f));
      // .y IS the pass's output scale — the same SkySunDisc.w bound above,
      // capture gain included. The block's standing convention (SkyHazeGeom.y
      // == SkySunDisc.w) is what makes the hazed snapshot equal the visible
      // hazed sky times _iblCaptureScale, term for term. .x (km per world unit)
      // is inert in the overlay, which marches in km from SkyRadii alone. .w is
      // the SHAFT GAIN in the forward binder and stays zero here on purpose:
      // this overlay is skyHazeSkyOverlay, which has no cascade taps to gain.
      if (_parHazeGeom)
        _material->bindParamVec4(
            _parHazeGeom, //
            fvec4(
                atmo->_kilometersPerWorldUnit,                 //
                atmo->_skyExposure * atmo->_iblCaptureScale,   //
                atmo->_hazeMaxDistanceKm,                      //
                0.0f));
    }
    _material->bindParamTexture(_parSkyViewLut, _rtgSkyView->texture(0).get());
    // the night emission's own extinction sampler. An UNBOUND sampler reads
    // zero here rather than failing, which is exactly how a silently black
    // night sky gets shipped — so this bind is load-bearing, not defensive.
    _material->bindParamTexture(_parTransmittanceLut, _rtgTransmittance->texture(0).get());
    // the haze overlay's multi-scatter source. ALWAYS bound (descriptor
    // completeness), armed or not: the fragment declares the sampler either way.
    _material->bindParamTexture(_parMultiScatterLut, _rtgMultiScatter->texture(0).get());
  });
  ctx->debugPopGroup();

  // the prefilter samples it starting NEXT frame (the scheduler drains in
  // beginFrame), so hand it over sampleable outside any render pass.
  ctx->FBI()->rtGroupTransitionToTexture(_rtgEquirect.get());
}

///////////////////////////////////////////////////////////

// All three LUTs exist on the GPU only after their first render pass, and that
// realization hands each one the context's BASE sampler regardless of the mode
// recorded on the texture. Re-apply the recorded (CLAMP) mode once, after the
// first full LUT chain — see the seam note in _init.
void HillaireSky::_applyLutSampling(Context* ctx) {
  if (_lutSamplingApplied)
    return;
  auto TXI = ctx->TXI();
  auto apply = [TXI](rtgroup_ptr_t rtg) {
    if (rtg and rtg->texture(0))
      TXI->ApplySamplingMode(rtg->texture(0).get());
  };
  apply(_rtgTransmittance);
  apply(_rtgMultiScatter);
  apply(_rtgSkyView);
  _lutSamplingApplied = true;
}

///////////////////////////////////////////////////////////

texture_ptr_t HillaireSky::transmittanceTexture() const {
  return _rtgTransmittance ? _rtgTransmittance->texture(0) : nullptr;
}
texture_ptr_t HillaireSky::multiScatterTexture() const {
  return _rtgMultiScatter ? _rtgMultiScatter->texture(0) : nullptr;
}
texture_ptr_t HillaireSky::skyViewTexture() const {
  return _rtgSkyView ? _rtgSkyView->texture(0) : nullptr;
}
texture_ptr_t HillaireSky::equirectSnapshotTexture() const {
  return _rtgEquirect ? _rtgEquirect->texture(0) : nullptr;
}

////////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::pbr
////////////////////////////////////////////////////////////////////////////////
