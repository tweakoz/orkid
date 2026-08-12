////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/reflect/properties/registerX.inl>
#include <ork/lev2/gfx/particle/modular_particles2.h>
#include <ork/lev2/gfx/particle/modular_forces.h>
#include <ork/lev2/gfx/particle/modular_renderers.h>
#include <ork/lev2/gfx/material_freestyle.h>
#include <ork/lev2/gfx/material_pbr.inl>
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/pbr_common.h>
#include <ork/dataflow/module.inl>
#include <ork/dataflow/plug_data.inl>
#include <ork/lev2/gfx/gfxvtxbuf.inl>

using namespace ork::dataflow;

namespace ork::lev2 {
void gradientGeometry(
    Context* pTARG,                    //
    const ork::gradient_fvec4_t& grad, //
    VtxWriter<SVtxV16T16C16>& vw,
    int x1, //
    int y1, //
    int w,  //
    int h);
}

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::particle {
/////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
void MaterialBase::describeX(class_t* clazz) {
  // shared raster knobs — every concrete material serializes these (the
  // model-B lesson: a live material's unreflected state silently resets on
  // round-trip and the failure mode is always "renders nothing").
  clazz->directEnumProperty("depthtest", &MaterialBase::_depthtest);
}
///////////////////////////////////////////////////////////////////////////////
MaterialBase::MaterialBase() {
  // gradient LUT defaults to WHITE so non-gradient materials pass frg_clr through unchanged
  // (the shared VS multiplies nothing); GradientMaterial overwrites it from its gradient.
  for (int i = 0; i < 256; i++)
    _gradientSamples[i] = fvec4(1, 1, 1, 1);
  _vertexSetterSprite = [](sprite_vertex_writer_t& vw, //
                           const BasicParticle* ptc,   //
                           float fang,                 //
                           float size,                 //
                           uint32_t ucolor) {          //
    float fage            = ptc->mfAge;
    float flspan          = (ptc->mfLifeSpan != 0.0f) ? ptc->mfLifeSpan : 0.01f;
    float clamped_unitage = std::clamp<float>((fage / flspan), 0, 1);
    //////////////////////////////////////////////////////
    fvec2 uv0(fang, size);
    fvec2 uv1(clamped_unitage, ptc->mfRandom);
    //////////////////////////////////////////////////////
    vw.AddVertex(sprite_vtx_t(ptc->mPosition, fvec3(0), fvec3(0), uv0, uv1));
  };
  _vertexSetterStreak = [](streak_vertex_writer_t& vw, //
                           const BasicParticle* ptcl,  //
                           fvec2 LW,                   //
                           fvec3 obj_nrmz) {           //
    float fage            = ptcl->mfAge;
    float flspan          = (ptcl->mfLifeSpan != 0.0f) ? ptcl->mfLifeSpan : 0.01f;
    float clamped_unitage = std::clamp<float>((fage / flspan), 0, 1);
    //////////////////////////////////////////////////////
    vw.AddVertex(streak_vtx_t(
        ptcl->mPosition, //
        obj_nrmz,        //
        ptcl->mVelocity, //
        LW,              //
        ork::fvec2(clamped_unitage, ptcl->mfRandom)));
  };
}
///////////////////////////////////////////////////////////////////////////////////////////////////
// PRE-PASS material init (render thread, NO render pass active — called from the particle
// drawable's onGpuUpdate fan-out). gpuInit LOADS THE SHADER FILE: it JITs the passes and
// takes a process-lifetime program id off the vulkan pipeline-key counter. Running that
// lazily from _render put it inside the recording color pass — the anti-pattern that
// surfaced as a program-id budget assert on the session's first NEW shader file. Idempotent:
// the first frame the drawable is in the scenegraph pays for it, every later frame no-ops.
///////////////////////////////////////////////////////////////////////////////////////////////////
void MaterialBase::gpuInitIfNeeded(ork::lev2::Context* ctx) {
  if (_pipeline)
    return;
  auto RCFD = ctx->topRenderContextFrameData();
  if (nullptr == RCFD) {
    // no frame data on the context stack (a fan-out entry that never pushed one): synthesize.
    // Only the pipeline PERMUTATION is derived from it, and particle materials assign
    // _technique per draw (see MaterialBase::pipeline), so the permutation is inert here.
    RCFD = std::make_shared<RenderContextFrameData>(ctx);
  }
  RenderContextInstData RCID(RCFD);
  Timer gpu_init_timer;
  gpu_init_timer.Start();
  gpuInit(RCID);
  OrkAssert(_pipeline);
  printf(
      "[particles] %s pre-pass gpuInit time<%f>\n", //
      GetClass()->Name().c_str(),
      gpu_init_timer.SecsSinceStart());
}
///////////////////////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////////////////////////
// SINGLE-PASS STEREO wiring, shared by every stock particle material (A2 family iii).
//
// Particles bind MatMVP by semantic — one MONO matrix — so a stereo pass that did nothing
// here would draw the SAME image into both eye layers: zero parallax, which Vulkan
// validation cannot see and a whole-frame image diff barely can. Two halves are needed:
//
//   1) the technique. Each stock technique has a "<name>_ST" peer whose vertex stage
//      reads the per-VIEW view-projection out of ublk_stereo. A MISSING peer is announced
//      by name, once — falling back to the mono technique silently IS the regression.
//   2) the producer. ublk_stereo is per-FRAME and shared; PBRMaterial::writeStereoBlock is
//      its ONE writer, and the block is bound here so a particle draw reads the byte-identical
//      view state a PBR draw does. Guarded on the pass being stereo, so mono is untouched.
//
// The stereo stage also needs the DRAW's model matrix (ublk_stereo carries no model), hence
// the MatM semantic bind — harmless in mono, where the stage never reads it.
///////////////////////////////////////////////////////////////////////////////////////////////////

void MaterialBase::_wireStereoTechniques() {
  auto _resolve = [this](fxtechnique_constptr_t mono, const char* mononame) -> fxtechnique_constptr_t {
    if (nullptr == mono)
      return nullptr;
    std::string stereoname = std::string(mononame) + "_ST";
    auto tek               = _material->technique(stereoname);
    if (nullptr == tek) {
      printf("[SPVR] WARN particle material<%s> has no technique<%s> — stereo passes fall back to the "
             "MONO stage, which renders identical images into both eye layers (zero parallax).\n",
             _material->mMaterialName.c_str(),
             stereoname.c_str());
      fflush(stdout);
      return mono;
    }
    return tek;
  };
  _tek_sprites_stereoCI = _resolve(_tek_sprites, _tek_sprites ? _tek_sprites->_techniqueName.c_str() : "");
  _tek_streaks_stereoCI = _resolve(_tek_streaks, _tek_streaks ? _tek_streaks->_techniqueName.c_str() : "");

  if (nullptr == _pipeline)
    return;

  if (auto par_m = _material->param("MatM"))
    _pipeline->bindParam(par_m, "RCFD_M"_crcsh);

  auto stereo_block = _material->uniformBlock("ublk_stereo");
  _pipeline->addStateLambda([stereo_block](const RenderContextInstData& RCID) {
    auto rcfd = RCID.rcfd();
    if (not rcfd->hasCPD())
      return;
    const auto& CPD = rcfd->topCPD();
    if (not CPD.isSinglePassStereo())
      return;
    auto stereocams = CPD._stereo_cam_matrices;
    if (nullptr == stereocams or nullptr == stereo_block)
      return;
    auto context = rcfd->GetTarget();
    auto FXI     = context->FXI();
    PBRMaterial::writeStereoBlock(FXI, context, stereocams);
    FXI->bindUniformBuffer(stereo_block, PBRMaterial::stereoDataBuffer(context));
  });
}

///////////////////////////////////////////////////////////////////////////////////////////////////

fxpipeline_ptr_t MaterialBase::pipeline(const RenderContextInstData& RCID, bool streaks) {
  auto RCFD = RCID.rcfd();
  // AUX-CHANNEL subpass (E2B item D): the forward node's aux pass marks the
  // RCFD and publishes the active channel; materials that registered a
  // technique pair for it draw with that pipeline, everyone else returns
  // nullptr and the renderer SKIPS the draw (content opts in per channel).
  if (RCFD->_subpassID == "AUX"_crcu) {
    if (auto try_chan = RCFD->tryUserProperty<uint64_t>("AUX_CHANNEL"_crcu)) {
      auto it = _aux_teks.find(try_chan.value());
      if (it != _aux_teks.end()) {
        auto& aux = it->second;
        aux._pipeline->_technique = streaks ? aux._tek_streaks : aux._tek_sprites;
        return aux._pipeline;
      }
    }
    return nullptr;
  }
  _pipeline->_technique = (RCFD->isStereo())                                              // ?
                              ? (streaks ? _tek_streaks_stereoCI : _tek_sprites_stereoCI) // stereo
                              : (streaks ? _tek_streaks : _tek_sprites);                  // mono

  OrkAssert(_pipeline);
  OrkAssert(_pipeline->_technique);
  return _pipeline;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
void FlatMaterial::describeX(class_t* clazz) {
  clazz
      ->directProperty("color", &FlatMaterial::_color) //
      ->annotate<ConstString>("editor.ged.node.factory", "GedNodeFactoryColorV4");
  clazz->directEnumProperty("blendmode", &FlatMaterial::_blending);
}
///////////////////////////////////////////////////////////////////////////////
FlatMaterial::FlatMaterial() {
  _color = fvec4(1, .5, 0, 1);
}
///////////////////////////////////////////////////////////////////////////////
std::shared_ptr<FlatMaterial> FlatMaterial::createShared() {
  return std::make_shared<FlatMaterial>();
}
///////////////////////////////////////////////////////////////////////////////
void FlatMaterial::gpuInit(const RenderContextInstData& RCID) {
  auto context                                       = RCID.context();
  _material                                          = std::make_shared<FreestyleMaterial>();
  _material->_varmap["tflatparticle_streaks_ST"] = std::string("dump_and_exit");
  _material->gpuInit(context, "orkshader://particle");
  _material->_rasterstate->setBlendingMacro(BlendingMacro::ADDITIVE);
  _material->_rasterstate->setCullTest(ECullTest::OFF);
  _material->_rasterstate->setDepthTest(EDepthTest::LEQUALS);
  _material->_rasterstate->setWriteMaskZ(true);

  auto fxparameterIV    = _material->param("MatIV");
  auto fxparameterMVP   = _material->param("MatMVP");
  auto fxparameterColor = _material->param("modcolor");
  auto pipeline_cache   = _material->pipelineCache();

  _pipeline = pipeline_cache->findPipeline(RCID);
  _pipeline->_rasterstate = _material->_rasterstate;
  _pipeline->_material_ptr = _material.get();

  _pipeline->bindParam(fxparameterIV, "RCFD_Camera_IV_Mono"_crcsh);
  _pipeline->bindParam(fxparameterMVP, "RCFD_Camera_MVP_Mono"_crcsh);
  FxPipeline::varval_generator_t gen_color = [=]() -> FxPipeline::varval_t { return _color; };
  _pipeline->bindParam(fxparameterColor, gen_color);

  _tek_sprites          = _material->technique("tflatparticle_sprites");
  _tek_streaks          = _material->technique("tflatparticle_streaks");
  _wireStereoTechniques();

  auto FXI = context->FXI();

  // Storage-block binding slot is shared (it's just shader-side metadata).
  // The actual SSBO buffer is per-renderer-instance now — see
  // Streak/SpriteRendererInst::_cu_vertex_io_buffer.
  _cu_storage_block = _material->storageBlock("storage_particles");
}
///////////////////////////////////////////////////////////////////////////////
void FlatMaterial::update(const RenderContextInstData& RCID) {
  _material->_rasterstate->setBlendingMacro(_blending);
}
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
void GradientMaterial::describeX(class_t* clazz) {
  clazz->directObjectProperty("gradient", &GradientMaterial::_gradient);
  clazz->floatProperty("colorFactor", float_range{-10, 10}, &GradientMaterial::_gradientColorIntensity);
  clazz->floatProperty("alphaFactor", float_range{-10, 10}, &GradientMaterial::_gradientAlphaIntensity);
  clazz->directEnumProperty("blendmode", &GradientMaterial::_blending);
  clazz->directAssetProperty("modtexture", &GradientMaterial::_modulation_texture_asset)
      ->annotate<ConstString>("editor.asset.class", "lev2tex")
      ->annotate<ConstString>("editor.asset.type", "lev2tex");
}
/////////////////////////////////////////////////////////////////////////////////////////////
GradientMaterial::GradientMaterial() {
  _color    = fvec4(1, .5, 0, 1);
  _gradient = std::make_shared<gradient_fvec4_t>();
  /////////////////////////////////////////////////////////////////////////
  _vertexSetterSprite = [=](sprite_vertex_writer_t& vw, //
                            const BasicParticle* ptc,   //
                            float fang,                 //
                            float size,                 //
                            uint32_t ucolor) {          //
    float fage            = ptc->mfAge;
    float flspan          = (ptc->mfLifeSpan != 0.0f) ? ptc->mfLifeSpan : 0.01f;
    float clamped_unitage = std::clamp<float>((fage / flspan), 0, 1);
    //////////////////////////////////////////////////////
    //fvec4 color = _gradient->sample(clamped_unitage);
    //////////////////////////////////////////////////////
    fvec2 uv0(fang, size);
    fvec2 uv1(ptc->mfRandom,clamped_unitage);
    //////////////////////////////////////////////////////
    vw.AddVertex(sprite_vtx_t( //
      ptc->mPosition, //
      fvec3(0), // NRM
      fvec3(0), // VEL
      uv0, // UV0
      uv1)); // UV1
  };
  /////////////////////////////////////////////////////////////////////////
  _vertexSetterStreak = [](streak_vertex_writer_t& vw, //
                           const BasicParticle* ptc,  //
                           fvec2 LW,                   //
                           fvec3 obj_nrmz) {           //
    float fage            = ptc->mfAge;
    float flspan          = (ptc->mfLifeSpan != 0.0f) ? ptc->mfLifeSpan : 0.01f;
    float clamped_unitage = std::clamp<float>((fage / flspan), 0, 1);
    //////////////////////////////////////////////////////
    fvec2 uv0(ptc->mfRandom,clamped_unitage);
    fvec2 uv1(LW.x, LW.y);
    //////////////////////////////////////////////////////
    vw.AddVertex(streak_vtx_t(
        ptc->mPosition, // pos
        obj_nrmz,        // nrm
        ptc->mVelocity, // vel
        uv0,             // UV0
        uv1));           // UV1
  };
  /////////////////////////////////////////////////////////////////////////
}
/////////////////////////////////////////////////////////////////////////////////////////////
std::shared_ptr<GradientMaterial> GradientMaterial::createShared() {
  return std::make_shared<GradientMaterial>();
}
/////////////////////////////////////////////////////////////////////////////////////////////
void GradientMaterial::gpuInit(const RenderContextInstData& RCID) {
  auto context = RCID.context();
  ////////////////////////////////////////////////////////////////////
  // Gradient is delivered to the shader through the per-frame particle SSBO (a 256-entry LUT
  // the renderer writes from _gradientSamples), NOT a texture: the particle render path is
  // entirely inside a render pass, so a texture upload can't land (the VR/forward black-gradient
  // bug). Seed _gradientSamples here; onGpuUpdate re-samples it when the stops change.
  for (int i = 0; i < 256; i++) {
    _gradientSamples[i] = _gradient->sample(float(i) / 256.0f);
  }
  _gradient_resampled = true;
  ////////////////////////////////////////////////////////////////////
  _material = std::make_shared<FreestyleMaterial>();
  _material->gpuInit(context, "orkshader://particle");
  _material->_rasterstate->setBlendingMacro(_blending);
  _material->_rasterstate->setCullTest(ECullTest::OFF);
  _material->_rasterstate->setDepthTest(_depthtest);
  _material->_rasterstate->setWriteMaskZ(false);
  _material->_rasterstate->setDepthTest(EDepthTest::OFF);

  auto fxparameterIV          = _material->param("MatIV");
  auto fxparameterMVP         = _material->param("MatMVP");
  auto fxparameterColorFactor = _material->param("ColorFactor");
  auto fxparameterAlphaFactor = _material->param("AlphaFactor");
  _param_mod_texture          = _material->param("ColorMap");
  auto pipeline_cache         = _material->pipelineCache();

  _pipeline = pipeline_cache->findPipeline(RCID);
  _pipeline->_rasterstate = _material->_rasterstate;
  _pipeline->_material_ptr = _material.get();

  _pipeline->bindParam(fxparameterIV, "RCFD_Camera_IV_Mono"_crcsh);
  _pipeline->bindParam(fxparameterMVP, "RCFD_Camera_MVP_Mono"_crcsh);

  FxPipeline::varval_generator_t gen_tex = [=]() -> FxPipeline::varval_t {
    auto as_tex = std::dynamic_pointer_cast<TextureAsset>(_modulation_texture_asset);
    // TODO move to deserializer post actions
    if (as_tex) {
      _modulation_texture = as_tex->GetTexture();
      if (_modulation_texture->_width == 0 and as_tex->_loadAttempts < 10) {
        _modulation_texture = Texture::LoadUnManaged(as_tex->_name);
        as_tex->_loadAttempts++;
        as_tex->_texture = _modulation_texture;
      }
    }
    FxPipeline::varval_t rval = _modulation_texture;
    return rval;
  };
  _pipeline->bindParam(_param_mod_texture, gen_tex);
  //////////////////////////////////////////
  FxPipeline::varval_generator_t colorfactor = [=]() -> FxPipeline::varval_t {
    FxPipeline::varval_t rval = _gradientColorIntensity;
    //printf("GradientMaterial::gpuInit colorfactor<%f>\n", rval.get<float>());
    return rval;
  };
  _pipeline->bindParam(fxparameterColorFactor, colorfactor);
  //////////////////////////////////////////
  FxPipeline::varval_generator_t alphafactor = [=]() -> FxPipeline::varval_t {
    FxPipeline::varval_t rval = _gradientAlphaIntensity;
    return rval;
  };
  _pipeline->bindParam(fxparameterAlphaFactor, alphafactor);
  //////////////////////////////////////////
  _tek_sprites          = _material->technique("tgradparticle_sprites");
  _tek_streaks          = _material->technique("tgradparticle_streaks");
  _wireStereoTechniques();

  auto FXI = context->FXI();

  // Storage-block binding slot is shared (it's just shader-side metadata).
  // The actual SSBO buffer is per-renderer-instance now — see
  // Streak/SpriteRendererInst::_cu_vertex_io_buffer.
  _cu_storage_block = _material->storageBlock("storage_particles");
}
/////////////////////////////////////////////////////////////////////////////////////////////
void GradientMaterial::update(const RenderContextInstData& RCID) {
  // The gradient LUT texture is now uploaded in onGpuUpdate (render-pass-safe), NOT baked
  // here mid-pass via a nested render-to-texture (the retired VR/forward black-gradient bug).
  // Keep only the live rasterstate sync — _blending can change at runtime.
  _material->_rasterstate->setBlendingMacro(_blending);
  _material->_rasterstate->setWriteMaskZ(false);
  _material->_rasterstate->setDepthTest(_depthtest);
}
/////////////////////////////////////////////////////////////////////////////////////////////
// PRE-RENDER (onGpuUpdate): re-sample the gradient into _gradientSamples WHEN IT CHANGED, so
// the renderer can copy the LUT into the per-frame SSBO. CPU-only (no GPU op) — safe to run
// with a render pass active. Dirty-gated so animated stops cost a re-sample only on change.
void GradientMaterial::onGpuUpdate(ork::lev2::Context* ctx) {
  fvec4 newsamples[256];
  for (int i = 0; i < 256; i++)
    newsamples[i] = _gradient->sample(float(i) / 256.0f);
  bool changed = (not _gradient_resampled) //
                 or (memcmp(newsamples, _gradientSamples, sizeof(_gradientSamples)) != 0);
  if (not changed)
    return;
  memcpy(_gradientSamples, newsamples, sizeof(_gradientSamples));
  _averageColor      = _gradient->average();
  _gradient_resampled = true;
}
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
void GradientAtlasMaterial::describeX(class_t* clazz) {
  clazz->floatProperty("colorFactor", float_range{-10, 10}, &GradientAtlasMaterial::_gradientColorIntensity);
  clazz->floatProperty("alphaFactor", float_range{-10, 10}, &GradientAtlasMaterial::_gradientAlphaIntensity);
  clazz->directEnumProperty("blendmode", &GradientAtlasMaterial::_blending);
}
/////////////////////////////////////////////////////////////////////////////////////////////
GradientAtlasMaterial::GradientAtlasMaterial() {
  _color = fvec4(1, 1, 1, 1);
  /////////////////////////////////////////////////////////////////////////
  // Same per-particle vertex setters as GradientMaterial — the SSBO path
  // doesn't actually use these (it builds geometry on the GPU from the
  // SSBO directly), but the base class expects them to be set.
  /////////////////////////////////////////////////////////////////////////
  _vertexSetterSprite = [=](sprite_vertex_writer_t& vw,
                            const BasicParticle* ptc,
                            float fang,
                            float size,
                            uint32_t ucolor) {
    float fage            = ptc->mfAge;
    float flspan          = (ptc->mfLifeSpan != 0.0f) ? ptc->mfLifeSpan : 0.01f;
    float clamped_unitage = std::clamp<float>((fage / flspan), 0, 1);
    fvec2 uv0(fang, size);
    fvec2 uv1(ptc->mfRandom, clamped_unitage);
    vw.AddVertex(sprite_vtx_t(ptc->mPosition, fvec3(0), fvec3(0), uv0, uv1));
  };
  _vertexSetterStreak = [](streak_vertex_writer_t& vw,
                           const BasicParticle* ptc,
                           fvec2 LW,
                           fvec3 obj_nrmz) {
    float fage            = ptc->mfAge;
    float flspan          = (ptc->mfLifeSpan != 0.0f) ? ptc->mfLifeSpan : 0.01f;
    float clamped_unitage = std::clamp<float>((fage / flspan), 0, 1);
    fvec2 uv0(ptc->mfRandom, clamped_unitage);
    fvec2 uv1(LW.x, LW.y);
    vw.AddVertex(streak_vtx_t(ptc->mPosition, obj_nrmz, ptc->mVelocity, uv0, uv1));
  };
}
/////////////////////////////////////////////////////////////////////////////////////////////
std::shared_ptr<GradientAtlasMaterial> GradientAtlasMaterial::createShared() {
  return std::make_shared<GradientAtlasMaterial>();
}
/////////////////////////////////////////////////////////////////////////////////////////////
void GradientAtlasMaterial::gpuInit(const RenderContextInstData& RCID) {
  auto context = RCID.context();

  _material = std::make_shared<FreestyleMaterial>();
  _material->gpuInit(context, "orkshader://particle");
  _material->_rasterstate->setBlendingMacro(_blending);
  _material->_rasterstate->setCullTest(ECullTest::OFF);
  _material->_rasterstate->setDepthTest(_depthtest);
  _material->_rasterstate->setWriteMaskZ(false);
  _material->_rasterstate->setDepthTest(EDepthTest::OFF);

  auto fxparameterIV          = _material->param("MatIV");
  auto fxparameterMVP         = _material->param("MatMVP");
  auto fxparameterAtlas       = _material->param("GradientMap");  // same uniform slot
  auto fxparameterColorFactor = _material->param("ColorFactor");
  auto fxparameterAlphaFactor = _material->param("AlphaFactor");
  _param_atlas                = fxparameterAtlas;
  _param_mod_texture          = _material->param("ColorMap");
  auto pipeline_cache         = _material->pipelineCache();

  _pipeline                = pipeline_cache->findPipeline(RCID);
  _pipeline->_rasterstate  = _material->_rasterstate;
  _pipeline->_material_ptr = _material.get();

  _pipeline->bindParam(fxparameterIV,  "RCFD_Camera_IV_Mono"_crcsh);
  _pipeline->bindParam(fxparameterMVP, "RCFD_Camera_MVP_Mono"_crcsh);

  FxPipeline::varval_generator_t gen_atlas = [=]() -> FxPipeline::varval_t {
    // Allow swapping the atlas at runtime by re-reading _atlas each frame.
    return _atlas;
  };
  _pipeline->bindParam(fxparameterAtlas, gen_atlas);

  FxPipeline::varval_generator_t gen_modtex = [=]() -> FxPipeline::varval_t {
    return _modulation_texture;
  };
  _pipeline->bindParam(_param_mod_texture, gen_modtex);

  FxPipeline::varval_generator_t colorfactor = [=]() -> FxPipeline::varval_t {
    return _gradientColorIntensity;
  };
  _pipeline->bindParam(fxparameterColorFactor, colorfactor);

  FxPipeline::varval_generator_t alphafactor = [=]() -> FxPipeline::varval_t {
    return _gradientAlphaIntensity;
  };
  _pipeline->bindParam(fxparameterAlphaFactor, alphafactor);

  // New techniques for atlas variants — defined in particle_comshader.i2.
  _tek_sprites          = _material->technique("tgradatlasparticle_sprites");
  _tek_streaks          = _material->technique("tgradatlasparticle_streaks");
  _wireStereoTechniques();

  auto FXI         = context->FXI();
  _cu_storage_block = _material->storageBlock("storage_particles");
}
/////////////////////////////////////////////////////////////////////////////////////////////
void GradientAtlasMaterial::update(const RenderContextInstData& RCID) {
  _material->_rasterstate->setBlendingMacro(_blending);
  _material->_rasterstate->setWriteMaskZ(false);
  _material->_rasterstate->setDepthTest(_depthtest);
}
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
void TextureMaterial::describeX(class_t* clazz) {
  clazz->directProperty("color", &TextureMaterial::_color)
      ->annotate<ConstString>("editor.ged.node.factory", "GedNodeFactoryColorV4");
  clazz->directEnumProperty("blendmode", &TextureMaterial::_blending);
  clazz->directAssetProperty("texture", &TextureMaterial::_texture_asset)
      ->annotate<ConstString>("editor.asset.class", "lev2tex")
      ->annotate<ConstString>("editor.asset.type", "lev2tex");
}
///////////////////////////////////////////////////////////////////////////////
TextureMaterial::TextureMaterial() {
}
std::shared_ptr<TextureMaterial> TextureMaterial::createShared() {
  return std::make_shared<TextureMaterial>();
}
///////////////////////////////////////////////////////////////////////////////
void TextureMaterial::update(const RenderContextInstData& RCID) {
  if (_texture) {
    auto context = RCID.context();
    auto FXI     = context->FXI();
    FXI->bindParamTexture(_paramColorMap, _texture.get());
    FXI->bindParamVect4(_parammodcolor, _color);
  }
}
///////////////////////////////////////////////////////////////////////////////
void TextureMaterial::gpuInit(const RenderContextInstData& RCID) {
  auto context = RCID.context();
  _material    = std::make_shared<FreestyleMaterial>();
  _material->gpuInit(context, "orkshader://particle");
  _material->_rasterstate->setBlendingMacro(BlendingMacro::ADDITIVE);
  _material->_rasterstate->setCullTest(ECullTest::OFF);
  _material->_rasterstate->setDepthTest(_depthtest);
  auto fxparameterM      = _material->param("MatM");
  auto fxparameterMVP    = _material->param("MatMVP");
  auto fxparameterIV     = _material->param("MatIV");
  auto fxparameterIVP    = _material->param("MatIVP");
  auto fxparameterVP     = _material->param("MatVP");
  auto fxparameterInvDim = _material->param("Rtg_InvDim");
  _paramColorMap         = _material->param("ColorMap");
  _parammodcolor         = _material->param("modcolor");

  auto pipeline_cache = _material->pipelineCache();
  _pipeline           = pipeline_cache->findPipeline(RCID);
  _pipeline->_material_ptr = _material.get();
  _pipeline->_rasterstate = _material->_rasterstate;
  _pipeline->bindParam(fxparameterMVP, "RCFD_Camera_MVP_Mono"_crcsh);
  _pipeline->bindParam(fxparameterIVP, "RCFD_Camera_IVP_Mono"_crcsh);
  _pipeline->bindParam(fxparameterVP, "RCFD_Camera_VP_Mono"_crcsh);
  _pipeline->bindParam(fxparameterIV, "RCFD_Camera_IV_Mono"_crcsh);
  _pipeline->bindParam(fxparameterM, "RCFD_M"_crcsh);
  _pipeline->bindParam(fxparameterInvDim, "CPD_Rtg_InvDim"_crcsh);
  _tek_sprites          = _material->technique("ttexparticle_sprites");
  _tek_streaks          = _material->technique("ttexparticle_streaks");
  _wireStereoTechniques();

  auto FXI = context->FXI();

  // Storage-block binding slot is shared (it's just shader-side metadata).
  // The actual SSBO buffer is per-renderer-instance now — see
  // Streak/SpriteRendererInst::_cu_vertex_io_buffer.
  _cu_storage_block = _material->storageBlock("storage_particles");
}
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
void TexGridMaterial::describeX(class_t* clazz) {
  clazz->directProperty("color", &TexGridMaterial::_color)
      ->annotate<ConstString>("editor.ged.node.factory", "GedNodeFactoryColorV4");
  clazz->directEnumProperty("blendmode", &TexGridMaterial::_blending);
  clazz->floatProperty("griddim", float_range{1, 64}, &TexGridMaterial::_gridDim);
  clazz->directAssetProperty("texture", &TexGridMaterial::_texture_asset)
      ->annotate<ConstString>("editor.asset.class", "lev2tex")
      ->annotate<ConstString>("editor.asset.type", "lev2tex");
}
///////////////////////////////////////////////////////////////////////////////
TexGridMaterial::TexGridMaterial() {
}
std::shared_ptr<TexGridMaterial> TexGridMaterial::createShared() {
  return std::make_shared<TexGridMaterial>();
}
///////////////////////////////////////////////////////////////////////////////
void TexGridMaterial::update(const RenderContextInstData& RCID) {
  /*if (_texture) {
    auto context = RCID.context();
    auto FXI     = context->FXI();
    FXI->bindParamTexture(_paramColorMap, _texture.get());
    FXI->bindParamVect4(_parammodcolor, _color);
  }*/

}
///////////////////////////////////////////////////////////////////////////////
void TexGridMaterial::gpuInit(const RenderContextInstData& RCID) {
  auto context = RCID.context();
  _material    = std::make_shared<FreestyleMaterial>();
  _material->gpuInit(context, "orkshader://particle");
  _material->_rasterstate->setBlendingMacro(_blending);
  _material->_rasterstate->setCullTest(ECullTest::OFF);
  _material->_rasterstate->setDepthTest(_depthtest);
  // blended sprites TEST depth but never WRITE it (matches every other
  // particle material) — z-writing additive flipbook sprites occlude each
  // other and anything drawn after them (was only masked historically by
  // tests running with depthtest=OFF entirely).
  _material->_rasterstate->setWriteMaskZ(false);
  auto fxparameterM      = _material->param("MatM");
  auto fxparameterMVP    = _material->param("MatMVP");
  auto fxparameterIV     = _material->param("MatIV");
  auto fxparameterIVP    = _material->param("MatIVP");
  auto fxparameterVP     = _material->param("MatVP");
  auto fxparameterInvDim = _material->param("Rtg_InvDim");
  _paramColorMap         = _material->param("ColorMap");
  _parammodcolor         = _material->param("modcolor");
  _paramGridDim          = _material->param("GridDim");

  auto pipeline_cache = _material->pipelineCache();
  _pipeline           = pipeline_cache->findPipeline(RCID);
  _pipeline->_rasterstate = _material->_rasterstate;
  _pipeline->_material_ptr = _material.get();

  _pipeline->bindParam(fxparameterMVP, "RCFD_Camera_MVP_Mono"_crcsh);
  _pipeline->bindParam(fxparameterIVP, "RCFD_Camera_IVP_Mono"_crcsh);
  _pipeline->bindParam(fxparameterVP, "RCFD_Camera_VP_Mono"_crcsh);
  _pipeline->bindParam(fxparameterIV, "RCFD_Camera_IV_Mono"_crcsh);
  _pipeline->bindParam(fxparameterM, "RCFD_M"_crcsh);
  _pipeline->bindParam(fxparameterInvDim, "CPD_Rtg_InvDim"_crcsh);

  _tek_sprites          = _material->technique("ttexgridparticle_sprites");
  _tek_streaks          = _material->technique("ttexparticle_streaks");
  _wireStereoTechniques();

  FxPipeline::varval_generator_t gen_tex = [=]() -> FxPipeline::varval_t {
    // resolve the REFLECTED texture asset when no live texture was injected
    // (the deserialized/model-B path — same retry idiom as GradientMaterial's
    // modulation texture above).
    if (not _texture) {
      auto as_tex = std::dynamic_pointer_cast<TextureAsset>(_texture_asset);
      if (as_tex) {
        _texture = as_tex->GetTexture();
        if (_texture and _texture->_width == 0 and as_tex->_loadAttempts < 10) {
          _texture = Texture::LoadUnManaged(as_tex->_name);
          as_tex->_loadAttempts++;
          as_tex->_texture = _texture;
        }
      }
    }
    FxPipeline::varval_t rval = _texture;
    return rval;
  };
  _pipeline->bindParam(_paramColorMap, gen_tex);

    FxPipeline::varval_generator_t gen_clr = [=]() -> FxPipeline::varval_t {
    FxPipeline::varval_t rval = _color;
    return rval;
  };
  _pipeline->bindParam(_parammodcolor, gen_clr);

  FxPipeline::varval_generator_t gen_dim = [=]() -> FxPipeline::varval_t {
    FxPipeline::varval_t rval = _gridDim;
    return rval;
  };
  _pipeline->bindParam(_paramGridDim, gen_dim);

  auto FXI = context->FXI();

  // Storage-block binding slot is shared (it's just shader-side metadata).
  // The actual SSBO buffer is per-renderer-instance now — see
  // Streak/SpriteRendererInst::_cu_vertex_io_buffer.
  _cu_storage_block = _material->storageBlock("storage_particles");
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// FreestyleParticleMaterial — flipbook cookie x gradient ramp, PREMA-first.
// gpuInit composes the GradientMaterial bake (ramp -> 256x1 RT) with the
// TexGridMaterial cookie resolve; ONE fragment shader (ps_freestyle_ptc)
// serves the sprite AND streak techniques. _shader_path swaps the whole fxv2
// (must provide tfreestyleparticle_{sprites,streaks}) — the DSL/aux-channel
// hook.
///////////////////////////////////////////////////////////////////////////////
void FreestyleParticleMaterial::describeX(class_t* clazz) {
  clazz->directObjectProperty("gradient", &FreestyleParticleMaterial::_gradient);
  clazz->floatProperty("colorFactor", float_range{-10, 10}, &FreestyleParticleMaterial::_gradientColorIntensity);
  clazz->floatProperty("alphaFactor", float_range{-10, 10}, &FreestyleParticleMaterial::_gradientAlphaIntensity);
  clazz->directProperty("color", &FreestyleParticleMaterial::_color)
      ->annotate<ConstString>("editor.ged.node.factory", "GedNodeFactoryColorV4");
  clazz->directEnumProperty("blendmode", &FreestyleParticleMaterial::_blending);
  clazz->floatProperty("griddim", float_range{1, 64}, &FreestyleParticleMaterial::_gridDim);
  clazz->directAssetProperty("texture", &FreestyleParticleMaterial::_texture_asset)
      ->annotate<ConstString>("editor.asset.class", "lev2tex")
      ->annotate<ConstString>("editor.asset.type", "lev2tex");
  clazz->directProperty("shader_path", &FreestyleParticleMaterial::_shader_path);
  clazz->floatProperty("emission_smoothing", float_range{0, 2}, &FreestyleParticleMaterial::_emission_smoothing);
  clazz->floatProperty("emission_lum_power", float_range{0.1f, 2}, &FreestyleParticleMaterial::_emission_lum_power);
  clazz->directProperty("emission_tint", &FreestyleParticleMaterial::_emission_tint);
  clazz->floatProperty("soft_fade_distance", float_range{0, 50}, &FreestyleParticleMaterial::_soft_fade_distance);
}
///////////////////////////////////////////////////////////////////////////////
FreestyleParticleMaterial::FreestyleParticleMaterial() {
  _color    = fvec4(1, 1, 1, 1);
  _gradient = std::make_shared<gradient_fvec4_t>();
  _blending = BlendingMacro::PREMA; // the raison d'etre: one chain, additive->absorptive
}
std::shared_ptr<FreestyleParticleMaterial> FreestyleParticleMaterial::createShared() {
  return std::make_shared<FreestyleParticleMaterial>();
}
///////////////////////////////////////////////////////////////////////////////
void FreestyleParticleMaterial::gpuInit(const RenderContextInstData& RCID) {
  auto context = RCID.context();
  ////////////////////////////////////////////////////////////////////
  // gradient ramp -> 256x1 RT (the GradientMaterial bake recipe)
  ////////////////////////////////////////////////////////////////////
  _grad_render_mtl = std::make_shared<FreestyleMaterial>();
  _grad_render_mtl->gpuInit(context, "orkshader://ui2");
  FxPipelinePermutation permu;
  permu._forced_technique                = _grad_render_mtl->technique("ui_gradwalpha");
  auto grad_render_cache                 = _grad_render_mtl->pipelineCache();
  _grad_render_pipeline                  = grad_render_cache->findPipeline(permu);
  auto grad_par_mvp                      = _grad_render_mtl->param("mvp");
  auto grad_par_time                     = _grad_render_mtl->param("time");
  FxPipeline::varval_generator_t gen_mtx = [=]() -> FxPipeline::varval_t { return context->MTXI()->Ortho(0, 256, 0, 1, 0, 1); };
  _grad_render_pipeline->bindParam(grad_par_mvp, gen_mtx);
  _grad_render_pipeline->bindParam(grad_par_time, 0.0f);
  _gradient_rtgroup = std::make_shared<RtGroup>(context, 256, 1);
  auto rtb0         = _gradient_rtgroup->createRenderTarget(EBufferFormat::RGBA8);
  _gradient_texture = rtb0->_texture;
  _gradient_texture->TexSamplingMode()._texAddrModeS = TextureAddressMode::CLAMP;
  _gradient_texture->TexSamplingMode()._texAddrModeT = TextureAddressMode::CLAMP;
  context->TXI()->ApplySamplingMode(_gradient_texture.get());
  ////////////////////////////////////////////////////////////////////
  for (int i = 0; i < 256; i++) {
    _gradientSamples[i] = _gradient->sample(float(i) / 256.0f);
  }
  ////////////////////////////////////////////////////////////////////
  auto shader_path = _shader_path.empty() ? std::string("orkshader://particle") : _shader_path;
  _material        = std::make_shared<FreestyleMaterial>();
  _material->gpuInit(context, shader_path.c_str());
  _material->_rasterstate->setBlendingMacro(_blending);
  _material->_rasterstate->setCullTest(ECullTest::OFF);
  _material->_rasterstate->setDepthTest(_depthtest);
  // blended sprites TEST depth but never WRITE it (particle-material contract)
  _material->_rasterstate->setWriteMaskZ(false);

  auto fxparameterM      = _material->param("MatM");
  auto fxparameterMVP    = _material->param("MatMVP");
  auto fxparameterIV     = _material->param("MatIV");
  auto fxparameterIVP    = _material->param("MatIVP");
  auto fxparameterVP     = _material->param("MatVP");
  auto fxparameterInvDim = _material->param("Rtg_InvDim");
  auto fxparameterGradMap     = _material->param("GradientMap");
  auto fxparameterColorFactor = _material->param("ColorFactor");
  auto fxparameterAlphaFactor = _material->param("AlphaFactor");
  _param_cookie               = _material->param("ColorMap");
  _param_gridDim              = _material->param("GridDim");
  auto parammodcolor          = _material->param("modcolor");
  // scene depth + soft-particle fade knobs — ABSENT unless the shader asked for
  // them (the heat pair declares DepthMap; ctx.soft_fade() additionally declares
  // SoftFadeDistance/NearFar). Null param = that feature isn't in this shader.
  auto param_depthmap  = _material->param("DepthMap");
  auto param_nearfar   = _material->param("NearFar");
  auto param_softfade  = _material->param("SoftFadeDistance");

  auto pipeline_cache      = _material->pipelineCache();
  _pipeline                = pipeline_cache->findPipeline(RCID);
  _pipeline->_rasterstate  = _material->_rasterstate;
  _pipeline->_material_ptr = _material.get();

  _tek_sprites          = _material->technique("tfreestyleparticle_sprites");
  _tek_streaks          = _material->technique("tfreestyleparticle_streaks");
  _wireStereoTechniques();

  // OPTIONAL aux-channel technique pair (E2B item D) — present when the
  // shader (stock override or fragment-DSL generated with ctx.is_heat)
  // provides the heat variants. Own pipeline: ADDITIVE accumulation into
  // the heat RT, depth IGNORED (the aux RTG carries no depth attachment —
  // v1 limit: aux content is not occluded by scene geometry).
  auto tek_sprites_heat = _material->technique("tfreestyleparticle_sprites_heat");
  auto tek_streaks_heat = _material->technique("tfreestyleparticle_streaks_heat");
  fxpipeline_ptr_t pipeline_heat;
  if (tek_sprites_heat and tek_streaks_heat) {
    FxPipelinePermutation permu_heat;
    permu_heat._forced_technique = tek_sprites_heat;
    pipeline_heat                = pipeline_cache->findPipeline(permu_heat);
    auto heat_rs                 = _material->_rasterstate->clone();
    heat_rs->setBlendingMacro(BlendingMacro::ADDITIVE);
    heat_rs->setCullTest(ECullTest::OFF);
    heat_rs->setDepthTest(EDepthTest::OFF);
    heat_rs->setWriteMaskZ(false);
    pipeline_heat->_rasterstate  = heat_rs;
    pipeline_heat->_material_ptr = _material.get();
    AuxTekSet heatset;
    heatset._tek_sprites = tek_sprites_heat;
    heatset._tek_streaks = tek_streaks_heat;
    heatset._pipeline    = pipeline_heat;
    _aux_teks["heat"_crcu] = heatset;
  }

  // 4x4 white = depth 1.0 = infinitely far: the always-legal stand-in the
  // depth samplers fall back to when no prepass filled the real one.
  _farDepth     = context->TXI()->createColorTextureV3(fvec3(1, 1, 1), 4, 4);
  _depth_source = _farDepth;

  FxPipeline::varval_generator_t gen_depth = [=]() -> FxPipeline::varval_t {
    FxPipeline::varval_t rval = _depth_source ? _depth_source : _farDepth;
    return rval;
  };

  // bind the SAME param set on every pipeline this material drives
  auto bind_params = [&](fxpipeline_ptr_t pipe) {
    pipe->bindParam(fxparameterMVP, "RCFD_Camera_MVP_Mono"_crcsh);
    pipe->bindParam(fxparameterIVP, "RCFD_Camera_IVP_Mono"_crcsh);
    pipe->bindParam(fxparameterVP, "RCFD_Camera_VP_Mono"_crcsh);
    pipe->bindParam(fxparameterIV, "RCFD_Camera_IV_Mono"_crcsh);
    pipe->bindParam(fxparameterM, "RCFD_M"_crcsh);
    pipe->bindParam(fxparameterInvDim, "CPD_Rtg_InvDim"_crcsh);
    pipe->bindParam(fxparameterGradMap, _gradient_texture);
    // the per-frame scene depth: the heat pair manually depth-tests against it
    // (the aux RTG carries no depth attachment) and the soft-particle fade
    // reconstructs linear scene depth from it. NOT the RCFD_DEPTH_MAP provider —
    // update() picks the texture, because with no prepass the real one is still
    // a write-target attachment and binding it faults.
    if (param_depthmap)
      pipe->bindParam(param_depthmap, gen_depth);
    if (param_nearfar)
      pipe->bindParam(param_nearfar, "RCFD_MONOCAM_NEAR_FAR"_crcsh);
  };
  bind_params(_pipeline);
  if (pipeline_heat)
    bind_params(pipeline_heat);

  FxPipeline::varval_generator_t gen_tex = [=]() -> FxPipeline::varval_t {
    // resolve the REFLECTED texture asset when no live texture was injected
    // (the deserialized/model-B path — LoadUnManaged retry idiom)
    if (not _texture) {
      auto as_tex = std::dynamic_pointer_cast<TextureAsset>(_texture_asset);
      if (as_tex) {
        _texture = as_tex->GetTexture();
        if (_texture and _texture->_width == 0 and as_tex->_loadAttempts < 10) {
          _texture = Texture::LoadUnManaged(as_tex->_name);
          as_tex->_loadAttempts++;
          as_tex->_texture = _texture;
        }
      }
    }
    FxPipeline::varval_t rval = _texture;
    return rval;
  };
  FxPipeline::varval_generator_t gen_clr = [=]() -> FxPipeline::varval_t {
    FxPipeline::varval_t rval = _color;
    return rval;
  };
  FxPipeline::varval_generator_t gen_dim = [=]() -> FxPipeline::varval_t {
    FxPipeline::varval_t rval = _gridDim;
    return rval;
  };
  FxPipeline::varval_generator_t gen_cfac = [=]() -> FxPipeline::varval_t {
    FxPipeline::varval_t rval = _gradientColorIntensity;
    return rval;
  };
  FxPipeline::varval_generator_t gen_afac = [=]() -> FxPipeline::varval_t {
    FxPipeline::varval_t rval = _gradientAlphaIntensity;
    return rval;
  };
  // A8: the fade distance rides the uniform, never the generated shader text —
  // a negative value is the shader's "fade off" sentinel, which is also how
  // update() REFUSES the fade when its depth precondition is unmet.
  FxPipeline::varval_generator_t gen_softfade = [=]() -> FxPipeline::varval_t {
    FxPipeline::varval_t rval = _soft_fade_refused ? -1.0f : _soft_fade_distance;
    return rval;
  };
  auto bind_generators = [&](fxpipeline_ptr_t pipe) {
    pipe->bindParam(_param_cookie, gen_tex);
    pipe->bindParam(parammodcolor, gen_clr);
    pipe->bindParam(_param_gridDim, gen_dim);
    pipe->bindParam(fxparameterColorFactor, gen_cfac);
    pipe->bindParam(fxparameterAlphaFactor, gen_afac);
    if (param_softfade)
      pipe->bindParam(param_softfade, gen_softfade);
  };
  bind_generators(_pipeline);
  if (pipeline_heat)
    bind_generators(pipeline_heat);

  // Storage-block binding slot is shared (it's just shader-side metadata).
  // The actual SSBO buffer is per-renderer-instance — see
  // Streak/SpriteRendererInst::_cu_vertex_io_buffer.
  _cu_storage_block = _material->storageBlock("storage_particles");
}
///////////////////////////////////////////////////////////////////////////////
void FreestyleParticleMaterial::update(const RenderContextInstData& RCID) {
  auto context = RCID.context();
  auto FXI     = context->FXI();
  auto FBI     = context->FBI();
  auto GBI     = context->GBI();
  ///////////////////////////////
  // SCENE DEPTH, and the soft-particle fade's PRECONDITION on it. DEPTH_MAP only
  // holds a valid, SAMPLEABLE scene depth when the DEPTH PREPASS ran — that pass
  // is what fills the single-sample image (the MSAA resolve is a depth-WRITE-pass
  // operation) and what lets the color pass transition depth to read-only. The
  // prepass is an engine invariant, but a pass can still reach here with
  // _useDepthPrepass false (a bake or probe rig that never declared it): depth is
  // then the color pass's write target, binding it faults outright, and a fade
  // computed from it would dissolve everything or nothing. So substitute the far stand-in
  // (fade becomes a no-op, the heat pair's manual test occludes nothing) and
  // refuse the fade BY NAME — same contract as the forward node's SSAO refusal.
  ///////////////////////////////
  auto pbrcommon     = RCID.rcfd()->_pbrcommon;
  bool depth_usable  = pbrcommon and pbrcommon->_useDepthPrepass;
  _depth_source      = _farDepth;
  if (depth_usable) {
    if (auto try_depth = RCID.rcfd()->tryUserProperty<texture_ptr_t>("DEPTH_MAP"_crcu))
      _depth_source = try_depth.value();
    else
      depth_usable = false;
  }
  _soft_fade_refused = (_soft_fade_distance > 0.0f) and (not depth_usable);
  if (_soft_fade_refused and not _soft_fade_warned) {
    _soft_fade_warned = true;
    printf(
        "[PTC:SOFTFADE] ERROR FreestyleParticleMaterial soft_fade_distance<%g> needs the "
        "DEPTH PREPASS (the fade samples DEPTH_MAP); the prepass is OFF for this pass, so the "
        "fade is REFUSED — sprites render with HARD intersection edges rather than fading "
        "against undefined depth. Enable pbr_common.useDepthPrepass.\n",
        _soft_fade_distance);
    fflush(stdout);
  }
  ///////////////////////////////
  // re-bake the ramp every update (live gradient edits — GradientMaterial recipe)
  ///////////////////////////////
  if (1) {
    VtxWriter<SVtxV16T16C16> vw;
    gradientGeometry( //
        context,      //
        *_gradient,   //
        vw,
        0,   //
        0,   //
        256, //
        1);
    _grad_render_mtl->_rasterstate->setBlendingMacro(BlendingMacro::OFF);
    _grad_render_mtl->_rasterstate->setWriteMaskRGB(true);
    _grad_render_mtl->_rasterstate->setWriteMaskA(true);
    _grad_render_mtl->_rasterstate->setWriteMaskZ(true);
    _grad_render_mtl->_rasterstate->setDepthTest(EDepthTest::OFF);
    _grad_render_mtl->_rasterstate->setCullTest(ECullTest::OFF);
    /////////////////////////////////////////
    // ensure this operation is not stereo
    //  as that will mess up viewport settings
    /////////////////////////////////////////
    auto& CPD        = (CompositingPassData&)RCID.rcfd()->topCPD();
    bool prev_stereo = CPD.isSinglePassStereo();
    CPD.setSinglePassStereo(false);
    /////////////////////////////////////////
    FBI->PushRtGroup(_gradient_rtgroup.get());
    _grad_render_pipeline->wrappedDrawCall(RCID, [&]() { //
      GBI->DrawPrimitiveEML(vw, PrimitiveType::TRIANGLES);
    });
    CPD.setSinglePassStereo(prev_stereo);
    FBI->PopRtGroup();
    _averageColor = _gradient->average();
    FXI->reset();
    /////////////////////////////////////////
  }
  ///////////////////////////////
  _material->_rasterstate->setBlendingMacro(_blending);
  _material->_rasterstate->setWriteMaskZ(false);
  _material->_rasterstate->setDepthTest(_depthtest);
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void VolTexMaterial::describeX(class_t* clazz) {
  // reflect::RegisterProperty("Texture", &VolTexMaterial::GetTextureAccessor, &VolTexMaterial::SetTextureAccessor);
  // reflect::annotatePropertyForEditor<VolTexMaterial>("Texture", "editor.class", "ged.factory.assetlist");
  // reflect::annotatePropertyForEditor<VolTexMaterial>("Texture", "editor.assettype", "lev2tex");
  // reflect::annotatePropertyForEditor<VolTexMaterial>("Texture", "editor.assetclass", "lev2tex");
}

///////////////////////////////////////////////////////////////////////////////
VolTexMaterial::VolTexMaterial() {
  // auto targ = lev2::contextForCurrentThread();
  //_material               = new GfxMaterial3DSolid(targ, "orkshader://particle", "tvolumeparticle");
  //_material->SetColorMode(GfxMaterial3DSolid::EMODE_USER);
}
///////////////////////////////////////////////////////////////////////////////
std::shared_ptr<VolTexMaterial> VolTexMaterial::createShared() {
  return std::make_shared<VolTexMaterial>();
}
///////////////////////////////////////////////////////////////////////////////
void VolTexMaterial::update(const RenderContextInstData& RCID) {
}
///////////////////////////////////////////////////////////////////////////////
void VolTexMaterial::gpuInit(const RenderContextInstData& RCID) {

  /*_material->SetVolumeTexture(_texture);
  _material->SetColorMode(lev2::GfxMaterial3DSolid::EMODE_USER);
  _material->_rasterstate->setAlphaTest(lev2::EALPHATEST_GREATER, 0.0f);
  _material->_rasterstate->setDepthTest(lev2::EDepthTest::LEQUALS);
  _material->_rasterstate->setWriteMaskZ(false);
  _material->_rasterstate->setCullTest(lev2::ECullTest::OFF);
  _material->_rasterstate->setPointSize(32.0f);*/
}
/////////////////////////////////////////
} // namespace ork::lev2::particle
///////////////////////////////////////////////////////////////////////////////

namespace ptcl = ork::lev2::particle;

ImplementReflectionX(ptcl::MaterialBase, "psys::MaterialBase");
ImplementReflectionX(ptcl::FlatMaterial, "psys::FlatMaterial");
ImplementReflectionX(ptcl::GradientMaterial,      "psys::GradientMaterial");
ImplementReflectionX(ptcl::GradientAtlasMaterial, "psys::GradientAtlasMaterial");
ImplementReflectionX(ptcl::TextureMaterial, "psys::TextureMaterial");
ImplementReflectionX(ptcl::TexGridMaterial, "psys::TexGridMaterial");
ImplementReflectionX(ptcl::FreestyleParticleMaterial, "psys::FreestyleParticleMaterial");
ImplementReflectionX(ptcl::VolTexMaterial, "psys::VolTexMaterial");
