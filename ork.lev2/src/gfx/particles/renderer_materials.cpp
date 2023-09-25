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
}
///////////////////////////////////////////////////////////////////////////////
MaterialBase::MaterialBase() {
  _vertexSetter = [](vertex_writer_t& vw,      //
                     const BasicParticle* ptc, //
                     float fang,               //
                     float size,               //
                     uint32_t ucolor) {        //
    float fage            = ptc->mfAge;
    float flspan          = (ptc->mfLifeSpan != 0.0f) ? ptc->mfLifeSpan : 0.01f;
    float clamped_unitage = std::clamp<float>((fage / flspan), 0, 1);
    //////////////////////////////////////////////////////
    fvec2 uv0(fang, size);
    fvec2 uv1(clamped_unitage, ptc->mfRandom);
    //////////////////////////////////////////////////////
    vw.AddVertex(vertex_t(ptc->mPosition, uv0, uv1, ucolor));
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
  _pipeline->_technique = (RCID._RCFD->isStereo())                                    //
                              ? (streaks ? _tek_streaks_stereoCI : _tek_sprites_stereo) //
                              : (streaks ? _tek_streaks : _tek_sprites);
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
  //_material->_rasterstate->SetBlending(Blending::ADDITIVE);
  //_material->_rasterstate->SetCullTest(ECullTest::OFF);
  //_material->_rasterstate->SetDepthTest(EDepthTest::LEQUALS);
  //_material->_rasterstate->SetZWriteMask(false);

  auto fxparameterMVP      = _material->param("MatMVP");
  auto fxparameterColor  = _material->param("modcolor");
  auto pipeline_cache    = _material->pipelineCache();

  _pipeline = pipeline_cache->findPipeline(RCID);
  _pipeline->bindParam(fxparameterMVP, "RCFD_Camera_MVP_Mono"_crcsh);
  FxPipeline::varval_generator_t gen_color = [=]() -> FxPipeline::varval_t { return _color; };
  _pipeline->bindParam(fxparameterColor, gen_color);

  _tek_sprites        = _material->technique("tflatparticle_sprites");
  _tek_streaks        = _material->technique("tflatparticle_streaks");
  _tek_sprites_stereo = _material->technique("tflatparticle_sprites_stereo");

#if defined(ENABLE_COMPUTE_SHADERS)

  auto FXI = context->FXI();
  auto CI  = context->CI();

  _streakcu_vertex_io_buffer = CI->createStorageBuffer(8 << 20);
  _streakcu_shader           = _material->computeShader("compute_streaks");

  _tek_streaks_stereoCI  = _material->technique("tflatparticle_streaks_stereoCI");

#endif
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
  clazz->floatProperty("colorFactor", float_range{-10,10}, &GradientMaterial::_gradientColorIntensity);
  clazz->floatProperty("alphaFactor", float_range{-10,10}, &GradientMaterial::_gradientAlphaIntensity);
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
  _vertexSetter = [=](vertex_writer_t& vw,      //
                      const BasicParticle* ptc, //
                      float fang,               //
                      float size,               //
                      uint32_t ucolor) {        //
    float fage            = ptc->mfAge;
    float flspan          = (ptc->mfLifeSpan != 0.0f) ? ptc->mfLifeSpan : 0.01f;
    float clamped_unitage = std::clamp<float>((fage / flspan), 0, 1);
    //////////////////////////////////////////////////////
    fvec4 color = _gradient->sample(clamped_unitage);
    //////////////////////////////////////////////////////
    fvec2 uv0(fang, size);
    fvec2 uv1(clamped_unitage, ptc->mfRandom);
    //////////////////////////////////////////////////////
    vw.AddVertex(vertex_t(ptc->mPosition, uv0, uv1, color.vertexColorU32()));
  };
  /////////////////////////////////////////////////////////////////////////
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
  ////////////////////////////////////////////////////////////////////
  _material = std::make_shared<FreestyleMaterial>();
  _material->gpuInit(context, "orkshader://particle");
  _material->_rasterstate->setBlendingMacro(BlendingMacro::ADDITIVE);
  //_material->_rasterstate->SetCullTest(ECullTest::OFF);
  //_material->_rasterstate->SetDepthTest(EDepthTest::LEQUALS);
  //_material->_rasterstate->SetZWriteMask(false);
  //_material->_rasterstate->SetDepthTest(EDepthTest::OFF);

  auto fxparameterMVP     = _material->param("MatMVP");
  auto fxparameterGradMap = _material->param("GradientMap");
  auto fxparameterColorFactor = _material->param("ColorFactor");
  auto fxparameterAlphaFactor = _material->param("AlphaFactor");
  _param_mod_texture      = _material->param("ColorMap");
  auto pipeline_cache     = _material->pipelineCache();

  _pipeline = pipeline_cache->findPipeline(RCID);
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
  _tek_sprites = _material->technique("tgradparticle_sprites");
  _tek_streaks = _material->technique("tgradparticle_streaks");

#if defined(ENABLE_COMPUTE_SHADERS)

  auto FXI = context->FXI();
  auto CI  = context->CI();

  _streakcu_vertex_io_buffer = CI->createStorageBuffer(8 << 20);
  _streakcu_shader           = _material->computeShader("compute_streaks");

  _tek_streaks_stereoCI  = _material->technique("tgradparticle_streaks_stereo");

#endif

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
    //_grad_render_mtl->_rasterstate->SetRGBAWriteMask(true, true);
    /////////////////////////////////////////
    // ensure this operation is not stereo
    //  as that will mess up viewport settings
    /////////////////////////////////////////
    auto& CPD = (CompositingPassData&) RCID._RCFD->topCPD();
    bool prev_stereo = CPD.isStereoOnePass();
    CPD.setStereoOnePass(false);
    /////////////////////////////////////////
    FBI->PushRtGroup(_gradient_rtgroup);
    _grad_render_pipeline->wrappedDrawCall(RCID, [&]() { //
      GBI->DrawPrimitiveEML(vw, PrimitiveType::TRIANGLES);
    });
    CPD.setStereoOnePass(prev_stereo);
    FBI->PopRtGroup();
    /////////////////////////////////////////
  }
  ///////////////////////////////
  _material->_rasterstate->setBlendingMacro(_blending);
}
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
void TextureMaterial::describeX(class_t* clazz) {
  // ork::reflect::RegisterProperty("Texture", &TextureMaterial::GetTextureAccessor, &TextureMaterial::SetTextureAccessor);
  // ork::reflect::annotatePropertyForEditor<TextureMaterial>("Texture", "editor.class", "ged.factory.assetlist");
  // ork::reflect::annotatePropertyForEditor<TextureMaterial>("Texture", "editor.assettype", "lev2tex");
  // ork::reflect::annotatePropertyForEditor<TextureMaterial>("Texture", "editor.assetclass", "lev2tex");
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
    FXI->BindParamCTex(_paramColorMap, _texture.get());
    FXI->BindParamVect4(_parammodcolor, _color);
  }
}
///////////////////////////////////////////////////////////////////////////////
void TextureMaterial::gpuInit(const RenderContextInstData& RCID) {
  auto context = RCID.context();
  _material    = std::make_shared<FreestyleMaterial>();
  _material->gpuInit(context, "orkshader://particle");
  _material->_rasterstate->setBlendingMacro(BlendingMacro::ADDITIVE);
  //_material->_rasterstate->SetCullTest(ECullTest::OFF);
  //_material->_rasterstate->SetDepthTest(EDepthTest::OFF);
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
  _pipeline->bindParam(fxparameterMVP, "RCFD_Camera_MVP_Mono"_crcsh);
  _pipeline->bindParam(fxparameterIVP, "RCFD_Camera_IVP_Mono"_crcsh);
  _pipeline->bindParam(fxparameterVP, "RCFD_Camera_VP_Mono"_crcsh);
  _pipeline->bindParam(fxparameterIV, "RCFD_Camera_IV_Mono"_crcsh);
  _pipeline->bindParam(fxparameterM, "RCFD_M"_crcsh);
  _pipeline->bindParam(fxparameterInvDim, "CPD_Rtg_InvDim"_crcsh);

  _tek_sprites = _material->technique("ttexparticle_sprites");
  _tek_streaks = _material->technique("ttexparticle_streaks");
}
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
void TexGridMaterial::describeX(class_t* clazz) {
  // ork::reflect::RegisterProperty("Texture", &TexGridMaterial::GetTextureAccessor, &TexGridMaterial::SetTextureAccessor);
  // ork::reflect::annotatePropertyForEditor<TexGridMaterial>("Texture", "editor.class", "ged.factory.assetlist");
  // ork::reflect::annotatePropertyForEditor<TexGridMaterial>("Texture", "editor.assettype", "lev2tex");
  // ork::reflect::annotatePropertyForEditor<TexGridMaterial>("Texture", "editor.assetclass", "lev2tex");
}
///////////////////////////////////////////////////////////////////////////////
TexGridMaterial::TexGridMaterial() {
  // ork::lev2::Context* targ = lev2::contextForCurrentThread();
  //_material               = new GfxMaterial3DSolid(targ, "orkshader://particle", "tbasicparticle");
  //_material->SetColorMode(GfxMaterial3DSolid::EMODE_USER);
}
std::shared_ptr<TexGridMaterial> TexGridMaterial::createShared() {
  return std::make_shared<TexGridMaterial>();
}
///////////////////////////////////////////////////////////////////////////////
void TexGridMaterial::update(const RenderContextInstData& RCID) {
  /*
      float flastframe = float(miTexCnt - 1);
      float ftexframe  = anim_frame * flastframe;
      ftexframe        = (ftexframe < 0.0f) ? 0.0f : (ftexframe >= flastframe) ? flastframe : ftexframe;
  miTexCnt = 1;           //(miAnimTexDim*miAnimTexDim);
  mfTexs   = 1.0f / 1.0f; // float(miAnimTexDim);
  miTexCnt = (miAnimTexDim * miAnimTexDim);
  mfTexs   = 1.0f / float(miAnimTexDim);
  uvr0     = fvec2(0.0f, 0.0f);
  uvr1     = fvec2(mfTexs, 0.0f);
  uvr2     = fvec2(mfTexs, mfTexs);
  uvr3     = fvec2(0.0f, mfTexs);
      float flastframe = float(miTexCnt - 1);
      float ftexframe  = anim_frame * flastframe;
      ftexframe        = (ftexframe < 0.0f) ? 0.0f : (ftexframe >= flastframe) ? flastframe : ftexframe;

      bool is_texanim  = (miAnimTexDim > 1);

      fvec2 uv0(fang, fsize);
      fvec2 uv1 = is_texanim                   //
                      ? fvec2(ftexframe, 0.0f) //
                      : fvec2(clamped_unitage, _OUTRANDOM);

if (gtarg && _texture) {
lev2::TextureAnimationBase* texanim = _texture->GetTexAnim();

if (texanim) {
  TextureAnimationInst tai(texanim);
  tai.SetCurrentTime(ftexframe);
  gtarg->TXI()->UpdateAnimatedTexture(_texture, &tai);
}
}*/
}
///////////////////////////////////////////////////////////////////////////////
void TexGridMaterial::gpuInit(const RenderContextInstData& RCID) {

  /*_material->SetTexture(_texture);
  _material->SetColorMode(GfxMaterial3DSolid::EMODE_USER);
  _material->_rasterstate->SetAlphaTest(EALPHATEST_GREATER, 0.0f);
  _material->_rasterstate->SetDepthTest(EDepthTest::LEQUALS);
  _material->_rasterstate->SetZWriteMask(false);
  _material->_rasterstate->SetCullTest(ECullTest::OFF);
  _material->_rasterstate->SetPointSize(32.0f);*/
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
  /*if (gtarg && _texture) {
    lev2::TextureAnimationBase* texanim = _texture->GetTexAnim();

    if (texanim) {
      TextureAnimationInst tai(texanim);
      tai.SetCurrentTime(ftexframe);
      gtarg->TXI()->UpdateAnimatedTexture(_texture, &tai);
    }
  }*/
}
///////////////////////////////////////////////////////////////////////////////
void VolTexMaterial::gpuInit(const RenderContextInstData& RCID) {

  /*_material->SetVolumeTexture(_texture);
  _material->SetColorMode(lev2::GfxMaterial3DSolid::EMODE_USER);
  _material->_rasterstate->SetAlphaTest(lev2::EALPHATEST_GREATER, 0.0f);
  _material->_rasterstate->SetDepthTest(lev2::EDepthTest::LEQUALS);
  _material->_rasterstate->SetZWriteMask(false);
  _material->_rasterstate->SetCullTest(lev2::ECullTest::OFF);
  _material->_rasterstate->SetPointSize(32.0f);*/
}
/////////////////////////////////////////
} // namespace ork::lev2::particle
///////////////////////////////////////////////////////////////////////////////

namespace ptcl = ork::lev2::particle;

ImplementReflectionX(ptcl::MaterialBase, "psys::MaterialBase");
ImplementReflectionX(ptcl::FlatMaterial, "psys::FlatMaterial");
ImplementReflectionX(ptcl::GradientMaterial, "psys::GradientMaterial");
ImplementReflectionX(ptcl::TextureMaterial, "psys::TextureMaterial");
ImplementReflectionX(ptcl::TexGridMaterial, "psys::TexGridMaterial");
ImplementReflectionX(ptcl::VolTexMaterial, "psys::VolTexMaterial");
