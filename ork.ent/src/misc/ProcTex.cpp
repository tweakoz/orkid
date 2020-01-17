////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/asset/DynamicAssetLoader.h>
#include <ork/kernel/string/string.h>
#include <ork/lev2/gfx/gfxmaterial_fx.h>
#include <ork/lev2/gfx/gfxmaterial_test.h>
#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/lev2/gfx/gfxprimitives.h>
#include <ork/lev2/gfx/proctex/proctex.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/pch.h>
#include <ork/reflect/RegisterProperty.h>
#include <ork/rtti/downcast.h>

///////////////////////////////////////////////////////////////////////////////
#include <ork/lev2/gfx/renderer/drawable.h>
#include <pkg/ent/entity.h>
#include <pkg/ent/entity.hpp>
#include <pkg/ent/event/MeshEvent.h>
#include <pkg/ent/scene.h>
///////////////////////////////////////////////////////////////////////////////
#include <ork/lev2/gfx/camera/cameradata.h>
#include <ork/kernel/opq.h>
#include <ork/reflect/AccessorObjectPropertyType.hpp>
#include <ork/reflect/DirectObjectMapPropertyType.hpp>
#include <ork/reflect/DirectObjectPropertyType.hpp>
#include <ork/reflect/enum_serializer.inl>
///////////////////////////////////////////////////////////////////////////////
#include "ProcTex.h"
///////////////////////////////////////////////////////////////////////////////
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::ProcTexArchetype, "ProcTexArchetype");
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::ProcTexControllerInst, "ProcTexControllerInst");
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::ProcTexControllerData, "ProcTexControllerData");
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::ProcTexOutputBase, "ProcTexOutputBase");
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::ProcTexOutputQuad, "ProcTexOutputQuad");
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::ProcTexOutputSkybox, "ProcTexOutputSkybox");
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::ProcTexOutputDynTex, "ProcTexOutputDynTex");
///////////////////////////////////////////////////////////////////////////////
namespace ork {

file::Path SaveFileRequester(const std::string& title, const std::string& ext);

namespace ent {
///////////////////////////////////////////////////////////////////////////////

void ProcTexArchetype::Describe() {}

ProcTexArchetype::ProcTexArchetype() {}

///////////////////////////////////////////////////////////////////////////////

void ProcTexArchetype::DoLinkEntity(Simulation* psi, Entity* pent) const {
  ProcTexControllerInst* ssci = pent->GetTypedComponent<ProcTexControllerInst>();
  const ProcTexControllerData& cd = ssci->GetCD();
  auto output = cd.GetOutput();

  if (nullptr == output)
    return;

  output->DoLinkEntity(psi, pent);
}

///////////////////////////////////////////////////////////////////////////////

void ProcTexArchetype::DoCompose(ork::ent::ArchComposer& composer) { composer.Register<ProcTexControllerData>(); }

///////////////////////////////////////////////////////////////////////////////

void ProcTexControllerData::OutputGetter(ork::rtti::ICastable*& val) const {
  auto nonconst = const_cast<ProcTexOutputBase*>(mOutput);
  val = nonconst;
}

///////////////////////////////////////////////////////////////////////////////

void ProcTexControllerData::OutputSetter(ork::rtti::ICastable* const& val) {
  ork::rtti::ICastable* ptr = val;
  mOutput = ((ptr == 0) ? 0 : rtti::safe_downcast<ProcTexOutputBase*>(ptr));
}

///////////////////////////////////////////////////////////////////////////////

void ProcTexControllerData::Describe() {
  ork::reflect::RegisterProperty("Output", &ProcTexControllerData::OutputGetter, &ProcTexControllerData::OutputSetter);

  ork::reflect::annotatePropertyForEditor<ProcTexControllerData>("Output", "editor.factorylistbase", "ProcTexOutputBase");

  // ork::reflect::RegisterProperty("SkyBox", &ProcTexControllerData::mSkybox);

  ork::reflect::RegisterProperty("Template", &ProcTexControllerData::TemplateAccessor);

  ork::reflect::RegisterProperty("BufferDim", &ProcTexControllerData::mBufferDim);
  ork::reflect::annotatePropertyForEditor<ProcTexControllerData>("BufferDim", "editor.range.min", "16");
  ork::reflect::annotatePropertyForEditor<ProcTexControllerData>("BufferDim", "editor.range.max", "8192");

  ork::reflect::RegisterProperty("FrameRate", &ProcTexControllerData::mMaxFrameRate);
  ork::reflect::annotatePropertyForEditor<ProcTexControllerData>("FrameRate", "editor.range.min", "0");
  ork::reflect::annotatePropertyForEditor<ProcTexControllerData>("FrameRate", "editor.range.max", "60");

  ////////////////////////////////////////

  static const char* edgstr = "grp://Main BufferDim FrameRate Output Template ";
  reflect::annotateClassForEditor<ProcTexControllerData>("editor.prop.groups", edgstr);

  ////////////////////////////////////////
  // ops map
  ////////////////////////////////////////

  auto opm = new ork::reflect::OpMap;
  opm->mLambdaMap["Refresh"] = [=](Object* pobj) {
    ProcTexControllerData* as_ptcd = rtti::autocast(pobj);
    if (as_ptcd) {
      as_ptcd->mNeedsRefresh = true;
    }
  };

  reflect::annotateClassForEditor<ProcTexControllerData>("editor.object.ops", opm);
}

///////////////////////////////////////////////////////////////////////////////

ProcTexControllerData::ProcTexControllerData() : mBufferDim(256), mMaxFrameRate(0.0), mNeedsRefresh(true), mOutput(nullptr) {}

ProcTexControllerData::~ProcTexControllerData() {
  if (mOutput)
    delete mOutput;
}

///////////////////////////////////////////////////////////////////////////////

void ProcTexControllerInst::Describe() {}

///////////////////////////////////////////////////////////////////////////////

ProcTexControllerInst::ProcTexControllerInst(const ProcTexControllerData& data, ent::Entity* pent)
    : ork::ent::ComponentInst(&data, pent), mCD(data) {
}

///////////////////////////////////////////////////////////////////////////////

ent::ComponentInst* ProcTexControllerData::createComponent(ent::Entity* pent) const {
  return OrkNew ProcTexControllerInst(*this, pent);
}

///////////////////////////////////////////////////////////////////////////////

void ProcTexControllerInst::DoUpdate(ent::Simulation* sinst) {

  //	mPhase += mCD.GetSpinRate()*sinst->GetDeltaTime();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void ProcTexOutputBase::Describe() {}
///////////////////////////////////////////////////////////////////////////////
void ProcTexOutputQuad::Describe() {
  ork::reflect::RegisterProperty("Scale", &ProcTexOutputQuad::mScale);
  ork::reflect::annotatePropertyForEditor<ProcTexOutputQuad>("Scale", "editor.range.min", "-1000");
  ork::reflect::annotatePropertyForEditor<ProcTexOutputQuad>("Scale", "editor.range.max", "1000");
}
ProcTexOutputQuad::ProcTexOutputQuad() : mScale(1.0f), mMaterial(nullptr) {}
///////////////////////////////////////////////////////////////////////////////
void ProcTexOutputQuad::OnLinkEntity(Simulation* psi, Entity* pent) {
  auto l_render_quad = [=](lev2::RenderContextInstData& rcid, lev2::Context* targ, const lev2::CallbackRenderable* pren) {
    bool isPickState = targ->FBI()->isPickState();
    if (isPickState)
      return;

    auto quad = pren->GetDrawableDataA().Get<const ProcTexOutputQuad*>();

    if (0 == quad->mMaterial) {
      quad->mMaterial = new lev2::GfxMaterial3DSolid(targ);
      quad->mMaterial->SetColorMode(lev2::GfxMaterial3DSolid::EMODE_TEX_COLOR);
      quad->mMaterial->_rasterstate.SetZWriteMask(true);
    }

    auto mtl = quad->mMaterial;
    auto RCFD = targ->topRenderContextFrameData();

    auto ssci = pren->GetDrawableDataB().Get<ProcTexControllerInst*>();
    auto ent = ssci->GetEntity();
    auto psi = ent->simulation();
    const ProcTexControllerData& cd = ssci->GetCD();
    proctex::ProcTex& templ = cd.GetTemplate();
    // auto md5 = templ.CalcMd5().hex_digest();
    // printf( "ptex MD5<%s>\n", md5.c_str() );
    float fcurtime = psi->GetGameTime();
    ssci->mContext.SetBufferDim(cd.GetBufferDim());
    ssci->mContext.mTarget = targ;
    ssci->mContext.mdflowctx.Clear();
    ssci->mContext.mCurrentTime = fcurtime;
    ssci->mContext.mWriteFrames = false;
    ssci->mContext.mWritePath = ""; // cd.GetWritePath();
    templ.compute(ssci->mContext);
    cd.DidRefresh();
    mtl->SetTexture(templ.ResultTexture());
    targ->PushMaterial(mtl);

    targ->MTXI()->PushMMatrix(ent->GetEffectiveMatrix());

    lev2::VtxWriter<lev2::SVtxV12C4T16> vw;

    vw.Lock(targ, &lev2::GfxEnv::GetSharedDynamicVB(), 6);
    {
      float fZ = 0.0f;
      u32 ucolor = 0xffffffff;
      f32 x0 = -quad->mScale;
      f32 y0 = -quad->mScale;
      f32 x1 = quad->mScale;
      f32 y1 = quad->mScale;
      ork::fvec2 uv0(0.0f, 1.0f);
      ork::fvec2 uv1(1.0f, 1.0f);
      ork::fvec2 uv2(1.0f, 0.0f);
      ork::fvec2 uv3(0.0f, 0.0f);
      ork::fvec3 vv0(x0, y0, fZ);
      ork::fvec3 vv1(x1, y0, fZ);
      ork::fvec3 vv2(x1, y1, fZ);
      ork::fvec3 vv3(x0, y1, fZ);

      lev2::SVtxV12C4T16 v0(vv0, uv0, ucolor);
      lev2::SVtxV12C4T16 v1(vv1, uv1, ucolor);
      lev2::SVtxV12C4T16 v2(vv2, uv2, ucolor);
      lev2::SVtxV12C4T16 v3(vv3, uv3, ucolor);

      vw.AddVertex(v0);
      vw.AddVertex(v1);
      vw.AddVertex(v2);

      vw.AddVertex(v2);
      vw.AddVertex(v3);
      vw.AddVertex(v0);
    }
    vw.UnLock(targ);

    const fmtx4& mtx = pren->GetMatrix();

    targ->GBI()->DrawPrimitive(vw, lev2::EPRIM_TRIANGLES, 6);

    targ->MTXI()->PopMMatrix();
    targ->PopMaterial();
  };

  ProcTexControllerInst* ssci = pent->GetTypedComponent<ProcTexControllerInst>();
  const ProcTexControllerData& cd = ssci->GetCD();

  auto pdrw = new lev2::CallbackDrawable(pent);
  pent->addDrawableToDefaultLayer(pdrw);
  pdrw->SetRenderCallback(l_render_quad);
  pdrw->SetOwner(pent->data());
  pdrw->SetSortKey(400000000);
  lev2::Drawable::var_t ap;
  ap.Set<const ProcTexOutputQuad*>(this);
  pdrw->SetUserDataA(ap);
  pdrw->SetUserDataB(ssci);
}
///////////////////////////////////////////////////////////////////////////////
void ProcTexOutputSkybox::Describe() {
  ork::reflect::RegisterProperty("VerticalAdjust", &ProcTexOutputSkybox::mVerticalAdjust);
  ork::reflect::annotatePropertyForEditor<ProcTexOutputSkybox>("VerticalAdjust", "editor.range.min", "-10000.0");
  ork::reflect::annotatePropertyForEditor<ProcTexOutputSkybox>("VerticalAdjust", "editor.range.max", "10000.0");

  ork::reflect::RegisterProperty("Scale", &ProcTexOutputSkybox::mScale);
  ork::reflect::annotatePropertyForEditor<ProcTexOutputSkybox>("Scale", "editor.range.min", "0.1");
  ork::reflect::annotatePropertyForEditor<ProcTexOutputSkybox>("Scale", "editor.range.max", "10000.0");
}
///////////////////////////////////////////////////////////////////////////////
ProcTexOutputSkybox::ProcTexOutputSkybox() : mVerticalAdjust(0.0f), mMaterial(nullptr), mScale(1.0f) {}
///////////////////////////////////////////////////////////////////////////////
void ProcTexOutputSkybox::OnLinkEntity(Simulation* psi, Entity* pent) {
  auto l_render_skybox = [=](lev2::RenderContextInstData& rcid, lev2::Context* targ, const lev2::CallbackRenderable* pren) {
    bool isPickState = targ->FBI()->isPickState();
    if (isPickState)
      return;

      auto skybox = pren->GetDrawableDataA().Get<ProcTexOutputSkybox*>();
    if( skybox->_timer.SecsSinceStart() > skybox->_timeperupdate ){
        printf( "ProcTexOutputSkybox UPDATE\n");
        skybox->_timer.Start();

        if (0 == skybox->mMaterial) {
          skybox->mMaterial = new lev2::GfxMaterial3DSolid(targ);
          skybox->mMaterial->SetColorMode(lev2::GfxMaterial3DSolid::EMODE_TEX_COLOR);
          skybox->mMaterial->_rasterstate.SetZWriteMask(false);
        }

        auto mtl = skybox->mMaterial;
        auto RCFD = targ->topRenderContextFrameData();
        const auto& CPD = RCFD->topCPD();
        float fscale = skybox->mScale;
        float fphase = 0.0f;
        fvec3 pos = CPD.cameraMatrices()->_camdat.GetEye();
        pos.SetY(pos.GetY() + skybox->mVerticalAdjust);
        fmtx4 mtxSPIN;
        mtxSPIN.RotateY(fphase);
        fmtx4 mtxSKY;
        mtxSKY.SetScale(fscale);
        mtxSKY.SetTranslation(pos);
        mtxSKY = mtxSPIN * mtxSKY;
        // rcid.ForceNoZWrite( true );
        auto ssci = pren->GetDrawableDataB().Get<ProcTexControllerInst*>();
        auto ent = ssci->GetEntity();
        auto psi = ent->simulation();
        const ProcTexControllerData& cd = ssci->GetCD();
        proctex::ProcTex& templ = cd.GetTemplate();
        // auto md5 = templ.CalcMd5().hex_digest();
        // printf( "ptex MD5<%s>\n", md5.c_str() );
        float fcurtime = psi->GetGameTime();
        ssci->mContext.SetBufferDim(cd.GetBufferDim());
        ssci->mContext.mTarget = targ;
        ssci->mContext.mdflowctx.Clear();
        ssci->mContext.mCurrentTime = fcurtime;
        ssci->mContext.mWriteFrames = false;
        ssci->mContext.mWritePath = ""; // cd.GetWritePath();
        templ.compute(ssci->mContext);
        cd.DidRefresh();
        lev2::Texture* ptx = templ.ResultTexture();
        mtl->SetTexture(ptx);
        targ->PushMaterial(mtl);
        targ->MTXI()->PushMMatrix(mtxSKY);
        lev2::GfxPrimitives::GetRef().RenderSkySphere(targ);
        targ->MTXI()->PopMMatrix();
        targ->PopMaterial();
    }
  };

  ProcTexControllerInst* ssci = pent->GetTypedComponent<ProcTexControllerInst>();
  const ProcTexControllerData& cd = ssci->GetCD();

  auto pdrw = new lev2::CallbackDrawable(pent);
  pent->addDrawableToDefaultLayer(pdrw);
  pdrw->SetRenderCallback(l_render_skybox);
  pdrw->SetOwner(pent->data());
  pdrw->SetSortKey(400000000);
  lev2::Drawable::var_t ap;
  ap.Set<ProcTexOutputSkybox*>(this);
  pdrw->SetUserDataA(ap);
  pdrw->SetUserDataB(ssci);
}
///////////////////////////////////////////////////////////////////////////////
//
///////////////////////////////////////////////////////////////////////////////
typedef std::set<ProcTexOutputDynTex*> dyntex_set_t;
ork::LockedResource<dyntex_set_t> gdyntexset;

///////////////////////////////////////////////////////////////////////////////
void ProcTexOutputDynTex::Describe() {
  ork::reflect::RegisterProperty("DynTexName", &ProcTexOutputDynTex::mDynTexPath);

  auto ptex_loader = new asset::DynamicAssetLoader;

  ptex_loader->mEnumFn = [=]() {
    printf( "PTEX enumerate()\n");
    std::set<file::Path> rval;
    gdyntexset.atomicOp([&](dyntex_set_t& dset) {

     printf( "NumPTEX<%zu>\n",dset.size());

      for (auto item : dset) {
        std::string pstr("ptex://");
        pstr += item->mDynTexPath.c_str();
        file::Path p = pstr.c_str();
        rval.insert(p);
      }
    });
    return rval;
  };
  ptex_loader->mCheckFn = [=](const PieceString& name) {
      return ork::IsSubStringPresent("ptex://", name.data());
  };
  ptex_loader->mLoadFn = [=](asset::Asset* asset) {
    auto asset_name = asset->GetName().c_str();
    lev2::TextureAsset* as_tex = rtti::autocast(asset);
    gdyntexset.atomicOp([&](dyntex_set_t& dset) {
      for (auto item : dset) {
        std::string pstr("ptex://");
        pstr += item->mDynTexPath.c_str();

        printf("LOADDYNPTEX pstr<%s> anam<%s>\n", pstr.c_str(), asset_name);
        if (pstr == asset_name) {
          item->mAsset = rtti::autocast(asset);
        }
      }
    });
    return true;
  };

  lev2::TextureAsset::GetClassStatic()->AddLoader(ptex_loader);
}
ProcTexOutputDynTex::ProcTexOutputDynTex() : mAsset(nullptr) {
  gdyntexset.atomicOp([&](dyntex_set_t& dset) { dset.insert(this); });
}
ProcTexOutputDynTex::~ProcTexOutputDynTex() {
  if (mAsset)
    mAsset->SetTexture(new lev2::Texture);

  gdyntexset.atomicOp([&](dyntex_set_t& dset) {
    auto it = dset.find(this);
    dset.erase(it);
  });
}
///////////////////////////////////////////////////////////////////////////////
void ProcTexOutputDynTex::OnLinkEntity(Simulation* psi, Entity* pent) {


  auto l_compute = [=](lev2::RenderContextInstData& rcid, lev2::Context* targ, const lev2::CallbackRenderable* pren) {
    bool isPickState = targ->FBI()->isPickState();
    if (isPickState)
      return;

    auto dyn = pren->GetDrawableDataA().Get<ProcTexOutputDynTex*>();

    if( dyn->_timer.SecsSinceStart() > dyn->_timeperupdate ){
        printf( "ProcTexUPDATE\n");
        dyn->_timer.Start();
        auto ssci = pren->GetDrawableDataB().Get<ProcTexControllerInst*>();
        auto ent = ssci->GetEntity();
        auto psi = ent->simulation();
        const ProcTexControllerData& cd = ssci->GetCD();
        proctex::ProcTex& templ = cd.GetTemplate();
        float fcurtime = psi->GetGameTime();
        ssci->mContext.SetBufferDim(cd.GetBufferDim());
        ssci->mContext.mTarget = targ;
        ssci->mContext.mdflowctx.Clear();
        ssci->mContext.mCurrentTime = fcurtime;
        ssci->mContext.mWriteFrames = false;
        ssci->mContext.mWritePath = ""; // cd.GetWritePath();
        templ.compute(ssci->mContext);
        cd.DidRefresh();
        targ->TXI()->generateMipMaps(templ.ResultTexture());
        if (dyn->mAsset)
          dyn->mAsset->SetTexture(templ.ResultTexture());
    }
  };

  ProcTexControllerInst* ssci = pent->GetTypedComponent<ProcTexControllerInst>();

  auto pdrw = new lev2::CallbackDrawable(pent);
  pent->addDrawableToDefaultLayer(pdrw);
  pdrw->SetRenderCallback(l_compute);
  pdrw->SetOwner(pent->data());
  pdrw->SetSortKey(400000000);
  lev2::Drawable::var_t ap;
  ap.Set<ProcTexOutputDynTex*>(this);
  pdrw->SetUserDataA(ap);
  pdrw->SetUserDataB(ssci);
}

///////////////////////////////////////////////////////////////////////////////
void ProcTexOutputBase::DoLinkEntity( Simulation* psi, Entity *pent ){
  ProcTexControllerInst* ssci = pent->GetTypedComponent<ProcTexControllerInst>();
  const ProcTexControllerData& cd = ssci->GetCD();
  if( cd.GetMaxFrameRate() == 0.0f )
    _timeperupdate = 0.0f;
  else
    _timeperupdate = 1.0f / cd.GetMaxFrameRate();
  OnLinkEntity(psi,pent);
}
ProcTexOutputBase::ProcTexOutputBase(){
  _timer.Start();
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ent
} // namespace ork
