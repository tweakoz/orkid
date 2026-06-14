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
#include <ork/lev2/gfx/rtgroup.h>
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
  _material->_varmap["tflatparticle_streaks_stereo"] = std::string("dump_and_exit");
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
  // SSBO-based rendering uses same techniques for stereo (camera vectors differ, not shaders)
  _tek_sprites_stereoCI = _tek_sprites;
  _tek_streaks_stereoCI = _tek_streaks;

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
  for( int i=0; i<256; i++ ){
    _gradientSamples[i] = _gradient->sample(float(i)/256.0f);
  }
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
  auto fxparameterGradMap     = _material->param("GradientMap");
  auto fxparameterColorFactor = _material->param("ColorFactor");
  auto fxparameterAlphaFactor = _material->param("AlphaFactor");
  _param_mod_texture          = _material->param("ColorMap");
  auto pipeline_cache         = _material->pipelineCache();

  _pipeline = pipeline_cache->findPipeline(RCID);
  _pipeline->_rasterstate = _material->_rasterstate;
  _pipeline->_material_ptr = _material.get();

  _pipeline->bindParam(fxparameterIV, "RCFD_Camera_IV_Mono"_crcsh);
  _pipeline->bindParam(fxparameterMVP, "RCFD_Camera_MVP_Mono"_crcsh);
  _pipeline->bindParam(fxparameterGradMap, _gradient_texture);

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
  // SSBO-based rendering uses same techniques for stereo (camera vectors differ, not shaders)
  _tek_sprites_stereoCI = _tek_sprites;
  _tek_streaks_stereoCI = _tek_streaks;

  auto FXI = context->FXI();

  // Storage-block binding slot is shared (it's just shader-side metadata).
  // The actual SSBO buffer is per-renderer-instance now — see
  // Streak/SpriteRendererInst::_cu_vertex_io_buffer.
  _cu_storage_block = _material->storageBlock("storage_particles");
}
/////////////////////////////////////////////////////////////////////////////////////////////
void GradientMaterial::update(const RenderContextInstData& RCID) {

  auto context = RCID.context();
  auto FXI     = context->FXI();
  auto FBI     = context->FBI();
  auto GBI     = context->GBI();
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
    _grad_render_pipeline->_debugPrint = false;
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
  _tek_sprites_stereoCI = _tek_sprites;
  _tek_streaks_stereoCI = _tek_streaks;

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
  // SSBO-based rendering uses same techniques for stereo (camera vectors differ, not shaders)
  _tek_sprites_stereoCI = _tek_sprites;
  _tek_streaks_stereoCI = _tek_streaks;

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
  // SSBO-based rendering uses same techniques for stereo (camera vectors differ, not shaders)
  _tek_sprites_stereoCI = _tek_sprites;
  _tek_streaks_stereoCI = _tek_streaks;

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

  auto pipeline_cache      = _material->pipelineCache();
  _pipeline                = pipeline_cache->findPipeline(RCID);
  _pipeline->_rasterstate  = _material->_rasterstate;
  _pipeline->_material_ptr = _material.get();

  _tek_sprites          = _material->technique("tfreestyleparticle_sprites");
  _tek_streaks          = _material->technique("tfreestyleparticle_streaks");
  // SSBO-based rendering uses same techniques for stereo (camera vectors differ, not shaders)
  _tek_sprites_stereoCI = _tek_sprites;
  _tek_streaks_stereoCI = _tek_streaks;

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
    // scene-depth occlusion: the generated heat FS declares DepthMap and
    // manually depth-tests (no depth attachment on the aux RTG); bind it
    // from the per-frame RCFD "DEPTH_MAP" property (the same read-only
    // prepass depth the color pass exposes for water translucency).
    if (auto param_depth = _material->param("DepthMap"))
      pipeline_heat->bindParam(param_depth, "RCFD_DEPTH_MAP"_crcsh);
    AuxTekSet heatset;
    heatset._tek_sprites = tek_sprites_heat;
    heatset._tek_streaks = tek_streaks_heat;
    heatset._pipeline    = pipeline_heat;
    _aux_teks["heat"_crcu] = heatset;
  }

  // bind the SAME param set on every pipeline this material drives
  auto bind_params = [&](fxpipeline_ptr_t pipe) {
    pipe->bindParam(fxparameterMVP, "RCFD_Camera_MVP_Mono"_crcsh);
    pipe->bindParam(fxparameterIVP, "RCFD_Camera_IVP_Mono"_crcsh);
    pipe->bindParam(fxparameterVP, "RCFD_Camera_VP_Mono"_crcsh);
    pipe->bindParam(fxparameterIV, "RCFD_Camera_IV_Mono"_crcsh);
    pipe->bindParam(fxparameterM, "RCFD_M"_crcsh);
    pipe->bindParam(fxparameterInvDim, "CPD_Rtg_InvDim"_crcsh);
    pipe->bindParam(fxparameterGradMap, _gradient_texture);
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
  auto bind_generators = [&](fxpipeline_ptr_t pipe) {
    pipe->bindParam(_param_cookie, gen_tex);
    pipe->bindParam(parammodcolor, gen_clr);
    pipe->bindParam(_param_gridDim, gen_dim);
    pipe->bindParam(fxparameterColorFactor, gen_cfac);
    pipe->bindParam(fxparameterAlphaFactor, gen_afac);
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
