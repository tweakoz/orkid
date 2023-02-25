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
#include <ork/dataflow/module.inl>
#include <ork/dataflow/plug_data.inl>

using namespace ork::dataflow;

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::particle {
/////////////////////////////////////////

struct SpriteRendererInst : public DgModuleInst {

public:
  SpriteRendererInst(const SpriteRendererData* smd)
      : DgModuleInst(smd) {
  }

  void onLink(GraphInst* inst) final {

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

  void _drawParticle(const ork::lev2::particle::BasicParticle* ptcl) {
  }

  void _render(
      const fmtx4& mtx,
      const ork::lev2::RenderContextInstData& rcid,
      const ParticlePoolRenderBuffer& buffer,
      ork::lev2::Context* targ) {

/*
    gtarg = targ;

    auto RCFD                   = targ->topRenderContextFrameData();
    const auto& CPD             = RCFD->topCPD();
    const CameraMatrices* cmtcs = CPD.cameraMatrices();
    const CameraData& cdata     = cmtcs->_camdat;
    MaterialBase* pMTLBASE      = 0;
    //////////////////////////////////////////
    mpVB        = &GfxEnv::GetSharedDynamicVB();
    float Scale = 1.0f;
    ork::fmtx4 MatScale;
    MatScale.setScale(Scale, Scale, Scale);
    const fmtx4& MVP = targ->MTXI()->RefMVPMatrix();
    ///////////////////////////////////////////////////////////////
    mCurFGI = mPlugInpGradientIntensity.GetValue();
    ///////////////////////////////////////////////////////////////
    // compute particle dynamic vertex buffer
    //////////////////////////////////////////
    int icnt             = buffer.miNumParticles;
    int ivertexlockcount = icnt;
    // int ivertexlockbase = mpVB->GetNumVertices();
    if (icnt) {
      lev2::VtxWriter<SVtxV12C4T16> vw;
      vw.Lock(targ, mpVB, ivertexlockcount);
      { // ork::fcolor4 CL;
        switch (meAlignment) {
          case ParticleItemAlignment::BILLBOARD: {
            ork::fmtx4 matrs(mtx);
            matrs.setTranslation(0.0f, 0.0f, 0.0f);
            matrs.transpose();
            ork::fvec3 nx = cdata.xNormal().transform(matrs);
            ork::fvec3 ny = cdata.yNormal().transform(matrs);
            ork::fvec3 NX = nx * -1.0f;
            ork::fvec3 PX = nx;
            ork::fvec3 NY = ny * -1.0f;
            ork::fvec3 PY = ny;
            NX_NY         = (NX + NY);
            PX_NY         = (PX + NY);
            PX_PY         = (PX + PY);
            NX_PY         = (NX + PY);
            break;
          }
          case ParticleItemAlignment::XZ: {
            NX_NY = ork::fvec3(-1.0f, 0.0f, -1.0f);
            PX_NY = ork::fvec3(+1.0f, 0.0f, -1.0f);
            PX_PY = ork::fvec3(+1.0f, 0.0f, +1.0f);
            NX_PY = ork::fvec3(-1.0f, 0.0f, +1.0f);
            break;
          }
          case ParticleItemAlignment::XY: {
            NX_NY = ork::fvec3(-1.0f, -1.0f, 0.0f);
            PX_NY = ork::fvec3(+1.0f, -1.0f, 0.0f);
            PX_PY = ork::fvec3(+1.0f, +1.0f, 0.0f);
            NX_PY = ork::fvec3(-1.0f, +1.0f, 0.0f);
            break;
          }
          case ParticleItemAlignment::YZ: {
            NX_NY = ork::fvec3(0.0f, -1.0f, -1.0f);
            PX_NY = ork::fvec3(0.0f, +1.0f, -1.0f);
            PX_PY = ork::fvec3(0.0f, +1.0f, +1.0f);
            NX_PY = ork::fvec3(0.0f, -1.0f, +1.0f);
            break;
          }
        }

        miTexCnt = 1;           //(miAnimTexDim*miAnimTexDim);
        mfTexs   = 1.0f / 1.0f; // float(miAnimTexDim);
        miTexCnt = (miAnimTexDim * miAnimTexDim);
        mfTexs   = 1.0f / float(miAnimTexDim);
        uvr0     = ork::fvec2(0.0f, 0.0f);
        uvr1     = ork::fvec2(mfTexs, 0.0f);
        uvr2     = ork::fvec2(mfTexs, mfTexs);
        uvr3     = ork::fvec2(0.0f, mfTexs);

        ////////////////////////////////////////////////////////////////////

        ork::Gradient<ork::fvec4>* pGRAD = 0;
        if (mpTemplateModule) {
          SpriteRenderer* ptemplate_module = 0;
          ptemplate_module                 = rtti::autocast(mpTemplateModule);
          const PoolString& active_g       = ptemplate_module->mActiveGradient;
          const PoolString& active_m       = ptemplate_module->mActiveMaterial;

          orklut<PoolString, ork::Object*>::const_iterator itG = mGradients.find(active_g);
          if (itG != mGradients.end()) {
            pGRAD = rtti::autocast(itG->second);
          }
          orklut<PoolString, ork::Object*>::const_iterator itM = mMaterials.find(active_m);
          if (itM != mMaterials.end()) {
            pMTLBASE = rtti::autocast(itM->second);
          }
        }
        ////////////////////////////////////////////////////////////////////

        bool bsort = mbSort;

        if (meBlendMode >= ork::lev2::Blending::ADDITIVE && meBlendMode <= Blending::ALPHA_SUBTRACTIVE) {
          bsort = false;
        }

        if (bsort) {
          static ork::fixedlut<float, const ork::lev2::particle::BasicParticle*, 20000> SortedParticles(EKEYPOLICY_MULTILUT);
          SortedParticles.clear();
          for (int i = 0; i < icnt; i++) {
            const ork::lev2::particle::BasicParticle* ptcl = buffer.mpParticles + i;
            {
              fvec4 proj = ptcl->mPosition.transform(MVP);
              proj.perspectiveDivideInPlace();
              float fv = proj.z;
              SortedParticles.AddSorted(fv, ptcl);
            }
          }
          for (int i = (icnt - 1); i >= 0; i--) {
            const ork::lev2::particle::BasicParticle* __restrict ptcl = SortedParticles.GetItemAtIndex(i).second;
            //////////////////////////////////////////////////////
            float fage      = ptcl->mfAge;
            float funitage  = (fage / ptcl->mfLifeSpan);
            mOutDataUnitAge = funitage;
            mOutDataUnitAge = (mOutDataUnitAge < 0.0f) ? 0.0f : mOutDataUnitAge;
            mOutDataUnitAge = (mOutDataUnitAge > 1.0f) ? 1.0f : mOutDataUnitAge;
            float fiunitage = (1.0f - mOutDataUnitAge);
            float fsize     = mPlugInpSize.GetValue();
            //////////////////////////////////////////////////////
            fvec4 color;
            if (pGRAD)
              color = (pGRAD->Sample(mOutDataUnitAge) * mCurFGI).saturated();
            U32 ucolor = color.VtxColorAsU32();
            //////////////////////////////////////////////////////
            float fang  = mPlugInpRot.GetValue() * DTOR;
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
            float ftexframe  = mPlugInpAnimFrame.GetValue() * flastframe;
            ftexframe        = (ftexframe < 0.0f) ? 0.0f : (ftexframe >= flastframe) ? flastframe : ftexframe;
            ork::fvec2 uvA(funitage, 1.0f); // float(miAnimTexDim) );
            //////////////////////////////////////////////////////
            vw.AddVertex(ork::lev2::SVtxV12C4T16(p0, uvr0, uvA, ucolor));
            // vw.AddVertex( ork::lev2::SVtxV12C4T16(p1,uvr1,uvA, ucolor) );
            // vw.AddVertex( ork::lev2::SVtxV12C4T16(p2,uvr2,uvA, ucolor) );
            // vw.AddVertex( ork::lev2::SVtxV12C4T16(p0,uvr0,uvA, ucolor) );
            // vw.AddVertex( ork::lev2::SVtxV12C4T16(p2,uvr2,uvA, ucolor) );
            // vw.AddVertex( ork::lev2::SVtxV12C4T16(p3,uvr3,uvA, ucolor) );
          }
        }
        ////////////////////////////////////////////////////////////////////
        else // no sort
        ////////////////////////////////////////////////////////////////////
        {
          const ork::lev2::particle::BasicParticle* __restrict pbase = buffer.mpParticles;

          for (int i = 0; i < icnt; i++) {
            const ork::lev2::particle::BasicParticle* __restrict ptcl = pbase + i;
            float fage                                                = ptcl->mfAge;
            float flspan                                              = (ptcl->mfLifeSpan != 0.0f) ? ptcl->mfLifeSpan : 0.01f;
            mOutDataUnitAge                                           = (fage / flspan);
            mOutDataUnitAge                                           = (mOutDataUnitAge < 0.0f) ? 0.0f : mOutDataUnitAge;
            mOutDataUnitAge                                           = (mOutDataUnitAge > 1.0f) ? 1.0f : mOutDataUnitAge;
            mOutDataPtcRandom                                         = ptcl->mfRandom;
            float fiunitage                                           = (1.0f - mOutDataUnitAge);
            float fsize                                               = mPlugInpSize.GetValue();
            fvec4 color;
            if (pGRAD)
              color = (pGRAD->Sample(mOutDataUnitAge) * mCurFGI).saturated();
            U32 ucolor = color.VtxColorAsU32();
            float fang = mPlugInpRot.GetValue() * DTOR;
            //////////////////////////////////////////////////////
            float flastframe = float(miTexCnt - 1);
            float ftexframe  = mPlugInpAnimFrame.GetValue() * flastframe;
            ftexframe        = (ftexframe < 0.0f) ? 0.0f : (ftexframe >= flastframe) ? flastframe : ftexframe;
            bool is_texanim  = (miAnimTexDim > 1);

            ork::fvec2 uv0(fang, fsize);
            ork::fvec2 uv1 = is_texanim ? ork::fvec2(ftexframe, 0.0f) : ork::fvec2(mOutDataUnitAge, mOutDataPtcRandom);

            vw.AddVertex(ork::lev2::SVtxV12C4T16(ptcl->mPosition, uv0, uv1, ucolor));
          }
        }
        ////////////////////////////////////////////////////////////////////
      }
      vw.UnLock(targ);

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

    } // if( icnt )
    */
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
  ork::lev2::CVtxBuffer<ork::lev2::SVtxV12C4T16>* mpVB;

  //////////////////////////////////////////////////

  orklut<PoolString, ork::Object*> mGradients;
  orklut<PoolString, ork::Object*> mMaterials;
  PoolString mActiveGradient;
  PoolString mActiveMaterial;
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

  createInputPlug<ParticleBufferPlugTraits>(data, EPR_UNIFORM, "ParticleBuffer");

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
