////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/reflect/properties/registerX.inl>
#include <ork/lev2/gfx/particle/modular_particles2.h>
#include <ork/lev2/gfx/particle/modular_forces.h>
#include <ork/lev2/gfx/particle/modular_renderers.h>
#include <ork/lev2/gfx/material_freestyle.h>
#include <ork/dataflow/module.inl>
#include <ork/dataflow/plug_data.inl>
#include <ork/util/triple_buffer.h>

using namespace ork::dataflow;

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::particle {
/////////////////////////////////////////

struct SpriteRendererInst : public ParticleModuleInst {

  using triple_buf_t     = concurrent_triple_buffer<ParticlePoolRenderBuffer>;
  using triple_buf_ptr_t = std::shared_ptr<triple_buf_t>;

  SpriteRendererInst(const SpriteRendererData* smd)
      : ParticleModuleInst(smd) {

    _triple_buf = std::make_shared<triple_buf_t>();
  }

  void onLink(GraphInst* inst) final {

    _onLink(inst);
    auto ptcl_context = inst->_impl.getShared<Context>();

    ptcl_context->_rcidlambda = [this](const RenderContextInstData& RCID) { this->_render(RCID); };

    /*if( mbImageSequence && mTexture && mTexture->IsLoaded() )
    {
        const file::Path& BaseAsset = mTexture->GetName().c_str();
        printf( "BaseAssetPath<%s>\n", BaseAsset.c_str() );
        const char* srch = strstr( BaseAsset.c_str(), "frame0000" );
        if( srch )
        {
            printf( "IMGSEQUENCE DETECTED\n" );
            for( int i=miImgSequenceBegin; i<=miImgSequenceEnd; i++ )
            {
                std::string assetname = BaseAsset.c_str();
                std::string seqidx = CreateFormattedString("%04d",i);
                OldStlSchoolFindAndReplace<std::string>( assetname, "0000", seqidx );

                ork::lev2::TextureAsset* passet = ork::asset::AssetManager<ork::lev2::TextureAsset>::load( assetname.c_str() );

                if( passet )
                {
                    ork::lev2::Texture* ptex = passet->GetTexture();
                    if( ptex )
                    {
                        printf( "AddedAsset<%p>\n", ptex );
                        mSequenceTextures.push_back(passet);
                    }
                }
            }
            mbImageSequenceOK = true;
        }

    }*/
  }
  void compute(GraphInst* inst, ui::updatedata_ptr_t updata) final {

    auto ptcl_context = inst->_impl.getShared<Context>();
    auto drawable     = ptcl_context->_drawable;
    OrkAssert(drawable);
    auto output_buffer = _triple_buf->begin_push();

    output_buffer->update(*_pool);

    _triple_buf->end_push(output_buffer);
    /*  MaterialBase* pMTLBASE = 0;
    if (mpTemplateModule) {
      SpriteRenderer* ptemplate_module                     = 0;
      ptemplate_module                                     = rtti::autocast(mpTemplateModule);
      const PoolString& active_m                           = ptemplate_module->mActiveMaterial;
      orklut<PoolString, ork::Object*>::const_iterator itM = mMaterials.find(active_m);
      if (itM != mMaterials.end()) {
        MaterialBase* pMTLBASE = rtti::autocast(itM->second);
        float ftexframe        = mPlugInpAnimFrame.GetValue();
        if (pMTLBASE) {
          pMTLBASE->Update(ftexframe);
        }
      }
    }*/
  }

  void _render(const ork::lev2::RenderContextInstData& RCID) {

    auto render_buffer          = _triple_buf->begin_pull();
    auto context                = RCID.context();
    auto RCFD                   = context->topRenderContextFrameData();
    const auto& CPD             = RCFD->topCPD();
    const CameraMatrices* cmtcs = CPD.cameraMatrices();
    const CameraData& cdata     = cmtcs->_camdat;
    const fmtx4& MVP            = context->MTXI()->RefMVPMatrix();
    auto& vertex_buffer         = GfxEnv::GetSharedDynamicVB();

    int icnt             = render_buffer->_numParticles;
    int ivertexlockcount = icnt;

    fmtx4 mtx;

    float input_rot  = 0.0f; // mPlugInpRot.GetValue();
    float anim_frame = 0.0f; // mPlugInpAnimFrame.GetValue()
    float input_size = 5.0f;

    if (icnt) {
      lev2::VtxWriter<SVtxV12C4T16> vw;
      vw.Lock(context, &vertex_buffer, ivertexlockcount);
      { // ork::fcolor4 CL;
        switch (meAlignment) {
          case ParticleItemAlignment::BILLBOARD: {
            ork::fmtx4 matrs(mtx);
            matrs.setTranslation(0.0f, 0.0f, 0.0f);
            matrs.transpose();
            fvec3 nx = cdata.xNormal().transform(matrs);
            fvec3 ny = cdata.yNormal().transform(matrs);
            fvec3 NX = nx * -1.0f;
            fvec3 PX = nx;
            fvec3 NY = ny * -1.0f;
            fvec3 PY = ny;
            NX_NY    = (NX + NY);
            PX_NY    = (PX + NY);
            PX_PY    = (PX + PY);
            NX_PY    = (NX + PY);
            break;
          }
          case ParticleItemAlignment::XZ: {
            NX_NY = fvec3(-1.0f, 0.0f, -1.0f);
            PX_NY = fvec3(+1.0f, 0.0f, -1.0f);
            PX_PY = fvec3(+1.0f, 0.0f, +1.0f);
            NX_PY = fvec3(-1.0f, 0.0f, +1.0f);
            break;
          }
          case ParticleItemAlignment::XY: {
            NX_NY = fvec3(-1.0f, -1.0f, 0.0f);
            PX_NY = fvec3(+1.0f, -1.0f, 0.0f);
            PX_PY = fvec3(+1.0f, +1.0f, 0.0f);
            NX_PY = fvec3(-1.0f, +1.0f, 0.0f);
            break;
          }
          case ParticleItemAlignment::YZ: {
            NX_NY = fvec3(0.0f, -1.0f, -1.0f);
            PX_NY = fvec3(0.0f, +1.0f, -1.0f);
            PX_PY = fvec3(0.0f, +1.0f, +1.0f);
            NX_PY = fvec3(0.0f, -1.0f, +1.0f);
            break;
          }
        }

        miTexCnt = 1;           //(miAnimTexDim*miAnimTexDim);
        mfTexs   = 1.0f / 1.0f; // float(miAnimTexDim);
        miTexCnt = (miAnimTexDim * miAnimTexDim);
        mfTexs   = 1.0f / float(miAnimTexDim);
        uvr0     = fvec2(0.0f, 0.0f);
        uvr1     = fvec2(mfTexs, 0.0f);
        uvr2     = fvec2(mfTexs, mfTexs);
        uvr3     = fvec2(0.0f, mfTexs);

        ////////////////////////////////////////////////////////////////////

        /*Gradient<fvec4>* pGRAD = 0;
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

        ////////////////////////////////////////////////////////////////////

        bool bsort = mbSort;

        if (meBlendMode >= Blending::ADDITIVE && meBlendMode <= Blending::ALPHA_SUBTRACTIVE) {
          bsort = false;
        }

        if (bsort) {
          static ork::fixedlut<float, const particle::BasicParticle*, 20000> SortedParticles(EKEYPOLICY_MULTILUT);
          SortedParticles.clear();
          for (int i = 0; i < icnt; i++) {
            auto ptcl  = render_buffer->_particles + i;
            fvec4 proj = ptcl->mPosition.transform(MVP);
            proj.perspectiveDivideInPlace();
            float fv = proj.z;
            SortedParticles.AddSorted(fv, ptcl);
          }
          for (int i = (icnt - 1); i >= 0; i--) {
            auto ptcl = SortedParticles.GetItemAtIndex(i).second;
            //////////////////////////////////////////////////////
            float fage            = ptcl->mfAge;
            float funitage        = (fage / ptcl->mfLifeSpan);
            float clamped_unitage = std::clamp<float>(funitage, 0, 1);
            float fiunitage       = (1.0f - clamped_unitage);
            float fsize           = input_size; // mPlugInpSize.GetValue();
            //////////////////////////////////////////////////////
            fvec4 color(1, 1, 1, 1);
            // if (pGRAD)
            // color = (pGRAD->Sample(mOutDataUnitAge) * mCurFGI).saturated();
            U32 ucolor = color.VtxColorAsU32();
            //////////////////////////////////////////////////////
            float fang  = input_rot * DTOR;
            float sinfr = sinf(fang) * fsize;
            float cosfr = cosf(fang) * fsize;
            fvec3 rota  = (NX_NY * cosfr) + (NX_PY * sinfr);
            fvec3 rotb  = (NX_PY * cosfr) - (NX_NY * sinfr);
            fvec3 p0    = ptcl->mPosition + rota;
            fvec3 p1    = ptcl->mPosition + rotb;
            fvec3 p2    = ptcl->mPosition - rota;
            fvec3 p3    = ptcl->mPosition - rotb;
            //////////////////////////////////////////////////////
            float flastframe = float(miTexCnt - 1);
            float ftexframe  = anim_frame * flastframe;
            ftexframe        = (ftexframe < 0.0f) ? 0.0f : (ftexframe >= flastframe) ? flastframe : ftexframe;
            ork::fvec2 uvA(funitage, 1.0f); // float(miAnimTexDim) );
            //////////////////////////////////////////////////////
            vw.AddVertex(SVtxV12C4T16(p0, uvr0, uvA, ucolor));
          }
        }
        ////////////////////////////////////////////////////////////////////
        else // no sort
        ////////////////////////////////////////////////////////////////////
        {
          auto pbase = render_buffer->_particles;

          for (int i = 0; i < icnt; i++) {
            auto ptcl             = pbase + i;
            float fage            = ptcl->mfAge;
            float flspan          = (ptcl->mfLifeSpan != 0.0f) ? ptcl->mfLifeSpan : 0.01f;
            float clamped_unitage = std::clamp<float>((fage / flspan), 0, 1);
            auto _OUTRANDOM       = ptcl->mfRandom;
            float fiunitage       = (1.0f - clamped_unitage);
            float fsize           = input_size;
            fvec4 color(1, 1, 1, clamped_unitage);
            // if (pGRAD)
            // color = (pGRAD->Sample(clamped_unitage) * mCurFGI).saturated();
            U32 ucolor = color.VtxColorAsU32();
            float fang = input_rot * DTOR;
            //////////////////////////////////////////////////////
            float flastframe = float(miTexCnt - 1);
            float ftexframe  = anim_frame * flastframe;
            ftexframe        = (ftexframe < 0.0f) ? 0.0f : (ftexframe >= flastframe) ? flastframe : ftexframe;
            bool is_texanim  = (miAnimTexDim > 1);

            fvec2 uv0(fang, fsize);
            fvec2 uv1 = is_texanim                   //
                            ? fvec2(ftexframe, 0.0f) //
                            : fvec2(clamped_unitage, _OUTRANDOM);

            vw.AddVertex(SVtxV12C4T16(ptcl->mPosition, uv0, uv1, ucolor));
          }
        }
        ////////////////////////////////////////////////////////////////////
      }
      vw.UnLock(context);

      if (nullptr == _testmaterial) {
        _testmaterial = std::make_shared<FreestyleMaterial>();
        _testmaterial->gpuInit(context,"orkshader://particle");
        _testmaterial->_rasterstate.SetBlending(Blending::ADDITIVE);
        _testmaterial->_rasterstate.SetCullTest(ECullTest::OFF);
        _testmaterial->_rasterstate.SetDepthTest(EDepthTest::OFF);
        //auto fxtechnique    = _testmaterial->technique("tparticle_nogs");
        auto fxtechnique    = _testmaterial->technique("tbasicparticle");
        auto fxparameterM = _testmaterial->param("MatM");
        auto fxparameterMVP = _testmaterial->param("MatMVP");
        auto fxparameterIV = _testmaterial->param("MatIV");
        auto fxparameterIVP = _testmaterial->param("MatIVP");
        auto fxparameterVP = _testmaterial->param("MatVP");
        auto fxparameterInvDim = _testmaterial->param("Rtg_InvDim");
        auto pipeline_cache = _testmaterial->pipelineCache();
        _pipeline = pipeline_cache->findPipeline(RCID);
        _pipeline->_technique = fxtechnique;
        _pipeline->bindParam(fxparameterMVP, "RCFD_Camera_MVP_Mono"_crcsh);
        _pipeline->bindParam(fxparameterIVP, "RCFD_Camera_IVP_Mono"_crcsh);
        _pipeline->bindParam(fxparameterVP, "RCFD_Camera_VP_Mono"_crcsh);
        _pipeline->bindParam(fxparameterIV, "RCFD_Camera_IV_Mono"_crcsh);
        _pipeline->bindParam(fxparameterM, "RCFD_M"_crcsh);
        _pipeline->bindParam(fxparameterInvDim, "CPD_Rtg_InvDim"_crcsh);
      }

      _pipeline->wrappedDrawCall(RCID, [&]() {
        //context->MTXI()->PushMMatrix(fmtx4::multiply_ltor(MatScale, mtx));
        context->GBI()->DrawPrimitiveEML(vw, ork::lev2::PrimitiveType::POINTS, ivertexlockcount);
        //context->MTXI()->PopMMatrix();
      });

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
          */
    }
    _triple_buf->end_pull(render_buffer);
  }

  //////////////////////////////////////////////////
  // outputs
  //////////////////////////////////////////////////

  // DeclareFloatOutPlug(UnitAge);
  // DeclareFloatOutPlug(PtcRandom);
  // dataflow::outplugbase* GetOutput(int idx) const final;

  //////////////////////////////////////////////////
  Blending meBlendMode              = Blending::ALPHA;
  ParticleItemAlignment meAlignment = ParticleItemAlignment::BILLBOARD;
  bool mbSort                       = false;
  int miImageFrame                  = 0;
  float mCurFGI                     = 0.0f;
  int miTexCnt                      = 0;
  float mfTexs                      = 0.0f;
  int miAnimTexDim                  = 1;

  ork::fvec2 uvr0;
  ork::fvec2 uvr1;
  ork::fvec2 uvr2;
  ork::fvec2 uvr3;
  ork::fvec3 NX_NY;
  ork::fvec3 PX_NY;
  ork::fvec3 PX_PY;
  ork::fvec3 NX_PY;

  //////////////////////////////////////////////////

  orklut<PoolString, ork::Object*> mGradients;
  orklut<PoolString, ork::Object*> mMaterials;
  PoolString mActiveGradient;
  PoolString mActiveMaterial;

  freestyle_mtl_ptr_t _testmaterial;
  fxpipeline_ptr_t _pipeline;
  triple_buf_ptr_t _triple_buf;
};

void RendererModuleData::describeX(class_t* clazz) {
}

RendererModuleData::RendererModuleData() {
}

//////////////////////////////////////////////////////////////////////////

void SpriteRendererData::describeX(class_t* clazz) {
}

//////////////////////////////////////////////////////////////////////////

SpriteRendererData::SpriteRendererData() {
}

//////////////////////////////////////////////////////////////////////////

std::shared_ptr<SpriteRendererData> SpriteRendererData::createShared() {
  auto data = std::make_shared<SpriteRendererData>();

  _initShared(data);

  createInputPlug<FloatXfPlugTraits>(data, EPR_UNIFORM, "Size");
  createInputPlug<FloatXfPlugTraits>(data, EPR_UNIFORM, "Rot");
  createInputPlug<FloatXfPlugTraits>(data, EPR_UNIFORM, "AnimFrame");
  createInputPlug<FloatXfPlugTraits>(data, EPR_UNIFORM, "GradientIntensity");

  createInputPlug<Vec3XfPlugTraits>(data, EPR_UNIFORM, "Noise0");
  createInputPlug<Vec3XfPlugTraits>(data, EPR_UNIFORM, "Noise1");
  createInputPlug<Vec3XfPlugTraits>(data, EPR_UNIFORM, "Noise2");

  return data;
}

//////////////////////////////////////////////////////////////////////////

dgmoduleinst_ptr_t SpriteRendererData::createInstance() const {
  return std::make_shared<SpriteRendererInst>(this);
}

/////////////////////////////////////////
} // namespace ork::lev2::particle
///////////////////////////////////////////////////////////////////////////////

namespace ptcl = ork::lev2::particle;

ImplementReflectionX(ptcl::RendererModuleData, "psys::RendererModuleData");
ImplementReflectionX(ptcl::SpriteRendererData, "psys::SpriteRendererData");
