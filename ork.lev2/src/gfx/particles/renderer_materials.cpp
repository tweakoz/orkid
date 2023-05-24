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
#include <ork/dataflow/module.inl>
#include <ork/dataflow/plug_data.inl>

using namespace ork::dataflow;

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::particle {
/////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
void MaterialBase::describeX(class_t* clazz) {
    clazz->directProperty( "color", &MaterialBase::_color ) //
         ->annotate<ConstString>("editor.ged.node.factory", "GedNodeFactoryColorV4");
}
///////////////////////////////////////////////////////////////////////////////
MaterialBase::MaterialBase() {
  _color        = fvec4(1, .5, 0, 1);
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
}
fxpipeline_ptr_t MaterialBase::pipeline(bool streaks) {
  _pipeline->_technique = streaks ? _tek_streaks : _tek_sprites;
  return _pipeline;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void FlatMaterial::describeX(class_t* clazz) {
}
///////////////////////////////////////////////////////////////////////////////
FlatMaterial::FlatMaterial() {
}
std::shared_ptr<FlatMaterial> FlatMaterial::createShared() {
  return std::make_shared<FlatMaterial>();
}
///////////////////////////////////////////////////////////////////////////////
void FlatMaterial::gpuInit(const RenderContextInstData& RCID) {
  auto context = RCID.context();
  _material    = std::make_shared<FreestyleMaterial>();
  _material->gpuInit(context, "orkshader://particle");
  _material->_rasterstate.SetBlending(Blending::ADDITIVE);
  _material->_rasterstate.SetCullTest(ECullTest::OFF);
  _material->_rasterstate.SetDepthTest(EDepthTest::OFF);

  auto fxparameterM      = _material->param("MatM");
  auto fxparameterMVP    = _material->param("MatMVP");
  auto fxparameterIV     = _material->param("MatIV");
  auto fxparameterIVP    = _material->param("MatIVP");
  auto fxparameterVP     = _material->param("MatVP");
  auto fxparameterInvDim = _material->param("Rtg_InvDim");
  _parammodcolor         = _material->param("modcolor");
  auto pipeline_cache    = _material->pipelineCache();

  _pipeline = pipeline_cache->findPipeline(RCID);
  _pipeline->bindParam(fxparameterMVP, "RCFD_Camera_MVP_Mono"_crcsh);
  _pipeline->bindParam(fxparameterIVP, "RCFD_Camera_IVP_Mono"_crcsh);
  _pipeline->bindParam(fxparameterVP, "RCFD_Camera_VP_Mono"_crcsh);
  _pipeline->bindParam(fxparameterIV, "RCFD_Camera_IV_Mono"_crcsh);
  _pipeline->bindParam(fxparameterM, "RCFD_M"_crcsh);
  _pipeline->bindParam(fxparameterInvDim, "CPD_Rtg_InvDim"_crcsh);

  _tek_sprites = _material->technique("tflatparticle_sprites");
  ;
  _tek_streaks = _material->technique("tflatparticle_streaks");
  ;
}
void FlatMaterial::update(const RenderContextInstData& RCID) {
  auto context = RCID.context();
  auto FXI     = context->FXI();
  FXI->BindParamVect4(_parammodcolor, _color);
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void GradientMaterial::describeX(class_t* clazz) {
  /*

      MaterialBase* pMTLBASE      = 0;
      //////////////////////////////////////////
      float Scale = 1.0f;
      ork::fmtx4 MatScale;
      MatScale.setScale(Scale, Scale, Scale);
      ///////////////////////////////////////////////////////////////
      mCurFGI = mPlugInpGradientIntensity.GetValue();
      ///////////////////////////////////////////////////////////////
      // compute particle dynamic vertex buffer
      //////////////////////////////////////////

        //////////////////////////////////////////
        // setup particle material
        //////////////////////////////////////////

        if (pMTLBASE) {
          auto bound_mtl = (lev2::GfxMaterial3DSolid*)pMTLBASE->Bind(targ);

          if (bound_mtl) {
            fvec4 user0 = NX_NY;
            fvec4 user1 = NX_PY;
            fvec4 user2(float(miAnimTexDim), float(miTexCnt), 0.0f);

            bound_mtl->SetUser0(user0);
            bound_mtl->SetUser1(user1);
            bound_mtl->SetUser2(user2);

            bound_mtl->_rasterstate.SetBlending(meBlendMode);
            targ->MTXI()->PushMMatrix(fmtx4::multiply_ltor(MatScale, mtx));
            targ->GBI()->DrawPrimitive(bound_mtl, vw, ork::lev2::PrimitiveType::POINTS, ivertexlockcount);
            mpVB = 0;
            targ->MTXI()->PopMMatrix();
          }
        }

       // if( icnt )
      */      /*Gradient<fvec4>* pGRAD = 0;
      if (mpTemplateModule) {
        SpriteRenderer* ptemplate_module = 0;
        ptemplate_module                 = rtti::autocast(mpTemplateModule);
        const PoolString& active_g       = ptemplate_module->mActiveGradient;
        const PoolString& active_m       = ptemplate_module->mActiveMaterial;

        orklut<PoolString, Object*>::const_iterator itG = mGradients.find(active_g);
        if (itG != mGradients.end()) {
          pGRAD = rtti::autocast(itG->second);
        }
        orklut<PoolString, Object*>::const_iterator itM = mMaterials.find(active_m);
        if (itM != mMaterials.end()) {
          pMTLBASE = rtti::autocast(itM->second);
        }
      }*/
}
///////////////////////////////////////////////////////////////////////////////
GradientMaterial::GradientMaterial() {
  _color = fvec4(1, .5, 0, 1);
}
std::shared_ptr<GradientMaterial> GradientMaterial::createShared() {
  return std::make_shared<GradientMaterial>();
}
///////////////////////////////////////////////////////////////////////////////
void GradientMaterial::gpuInit(const RenderContextInstData& RCID) {
  auto context = RCID.context();
  _material    = std::make_shared<FreestyleMaterial>();
  _material->gpuInit(context, "orkshader://particle");
  _material->_rasterstate.SetBlending(Blending::ADDITIVE);
  _material->_rasterstate.SetCullTest(ECullTest::OFF);
  _material->_rasterstate.SetDepthTest(EDepthTest::OFF);
  // auto fxtechnique    = _material->technique("tparticle_nogs");
  auto fxtechnique       = _material->technique("tflatparticle_sprites");
  auto fxparameterM      = _material->param("MatM");
  auto fxparameterMVP    = _material->param("MatMVP");
  auto fxparameterIV     = _material->param("MatIV");
  auto fxparameterIVP    = _material->param("MatIVP");
  auto fxparameterVP     = _material->param("MatVP");
  auto fxparameterInvDim = _material->param("Rtg_InvDim");
  _parammodcolor         = _material->param("modcolor");
  auto pipeline_cache    = _material->pipelineCache();

  _pipeline = pipeline_cache->findPipeline(RCID);
  _pipeline->bindParam(fxparameterMVP, "RCFD_Camera_MVP_Mono"_crcsh);
  _pipeline->bindParam(fxparameterIVP, "RCFD_Camera_IVP_Mono"_crcsh);
  _pipeline->bindParam(fxparameterVP, "RCFD_Camera_VP_Mono"_crcsh);
  _pipeline->bindParam(fxparameterIV, "RCFD_Camera_IV_Mono"_crcsh);
  _pipeline->bindParam(fxparameterM, "RCFD_M"_crcsh);
  _pipeline->bindParam(fxparameterInvDim, "CPD_Rtg_InvDim"_crcsh);

  _tek_sprites = _material->technique("tflatparticle_sprites");
  ;
  _tek_streaks = _material->technique("tflatparticle_streaks");
  ;
}
void GradientMaterial::update(const RenderContextInstData& RCID) {
  auto context = RCID.context();
  auto FXI     = context->FXI();
  FXI->BindParamVect4(_parammodcolor, _color);
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
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
  _material->_rasterstate.SetBlending(Blending::ADDITIVE);
  _material->_rasterstate.SetCullTest(ECullTest::OFF);
  _material->_rasterstate.SetDepthTest(EDepthTest::OFF);
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
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
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
  _material->_rasterstate.SetAlphaTest(EALPHATEST_GREATER, 0.0f);
  _material->_rasterstate.SetDepthTest(EDepthTest::LEQUALS);
  _material->_rasterstate.SetZWriteMask(false);
  _material->_rasterstate.SetCullTest(ECullTest::OFF);
  _material->_rasterstate.SetPointSize(32.0f);*/
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
  _material->_rasterstate.SetAlphaTest(lev2::EALPHATEST_GREATER, 0.0f);
  _material->_rasterstate.SetDepthTest(lev2::EDepthTest::LEQUALS);
  _material->_rasterstate.SetZWriteMask(false);
  _material->_rasterstate.SetCullTest(lev2::ECullTest::OFF);
  _material->_rasterstate.SetPointSize(32.0f);*/
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
