////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
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
#include <pkg/ent/drawable.h>
#include <pkg/ent/entity.h>
#include <pkg/ent/entity.hpp>
#include <pkg/ent/event/MeshEvent.h>
#include <pkg/ent/scene.h>
///////////////////////////////////////////////////////////////////////////////
#include <ork/gfx/camera.h>
#include <ork/kernel/opq.h>
#include <ork/reflect/AccessorObjectPropertyType.hpp>
#include <ork/reflect/DirectObjectMapPropertyType.hpp>
#include <ork/reflect/DirectObjectPropertyType.hpp>
#include <ork/reflect/enum_serializer.h>
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
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::ProcTexOutputBake, "ProcTexOutputBake");
///////////////////////////////////////////////////////////////////////////////
namespace ork {

file::Path SaveFileRequester(const std::string& title, const std::string& ext);

namespace ent {
///////////////////////////////////////////////////////////////////////////////

void ProcTexArchetype::Describe() {}

ProcTexArchetype::ProcTexArchetype() {}

///////////////////////////////////////////////////////////////////////////////

void ProcTexArchetype::DoLinkEntity(SceneInst* psi, Entity* pent) const {
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
void ProcTexControllerData::OutputSetter(ork::rtti::ICastable* const& val) {
  ork::rtti::ICastable* ptr = val;
  mOutput = ((ptr == 0) ? 0 : rtti::safe_downcast<ProcTexOutputBase*>(ptr));
}

void ProcTexControllerData::Describe() {
  ork::reflect::RegisterProperty("Output", &ProcTexControllerData::OutputGetter, &ProcTexControllerData::OutputSetter);

  ork::reflect::AnnotatePropertyForEditor<ProcTexControllerData>("Output", "editor.factorylistbase", "ProcTexOutputBase");

  // ork::reflect::RegisterProperty("SkyBox", &ProcTexControllerData::mSkybox);

  ork::reflect::RegisterProperty("Template", &ProcTexControllerData::TemplateAccessor);

  ork::reflect::RegisterProperty("BufferDim", &ProcTexControllerData::mBufferDim);
  ork::reflect::AnnotatePropertyForEditor<ProcTexControllerData>("BufferDim", "editor.range.min", "16");
  ork::reflect::AnnotatePropertyForEditor<ProcTexControllerData>("BufferDim", "editor.range.max", "8192");

  ork::reflect::RegisterProperty("FrameRate", &ProcTexControllerData::mMaxFrameRate);
  ork::reflect::AnnotatePropertyForEditor<ProcTexControllerData>("FrameRate", "editor.range.min", "0");
  ork::reflect::AnnotatePropertyForEditor<ProcTexControllerData>("FrameRate", "editor.range.max", "60");

  ////////////////////////////////////////

  static const char* edgstr = "grp://Main BufferDim FrameRate Output Template ";
  reflect::AnnotateClassForEditor<ProcTexControllerData>("editor.prop.groups", edgstr);

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

  reflect::AnnotateClassForEditor<ProcTexControllerData>("editor.object.ops", opm);
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
  mTimer.Start();
}

///////////////////////////////////////////////////////////////////////////////

ent::ComponentInst* ProcTexControllerData::createComponent(ent::Entity* pent) const {
  return OrkNew ProcTexControllerInst(*this, pent);
}

///////////////////////////////////////////////////////////////////////////////

void ProcTexControllerInst::DoUpdate(ent::SceneInst* sinst) {

  //	mPhase += mCD.GetSpinRate()*sinst->GetDeltaTime();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void ProcTexOutputBase::Describe() {}
///////////////////////////////////////////////////////////////////////////////
void ProcTexOutputQuad::Describe() {
  ork::reflect::RegisterProperty("Scale", &ProcTexOutputQuad::mScale);
  ork::reflect::AnnotatePropertyForEditor<ProcTexOutputQuad>("Scale", "editor.range.min", "-1000");
  ork::reflect::AnnotatePropertyForEditor<ProcTexOutputQuad>("Scale", "editor.range.max", "1000");
}
ProcTexOutputQuad::ProcTexOutputQuad() : mScale(1.0f), mMaterial(nullptr) {}
void ProcTexOutputQuad::DoLinkEntity(SceneInst* psi, Entity* pent) const {
  auto l_render_quad = [=](lev2::RenderContextInstData& rcid, lev2::GfxTarget* targ, const lev2::CallbackRenderable* pren) {
    bool IsPickState = targ->FBI()->IsPickState();
    if (IsPickState)
      return;

    auto quad = pren->GetDrawableDataA().Get<const ProcTexOutputQuad*>();

    if (0 == quad->mMaterial) {
      quad->mMaterial = new lev2::GfxMaterial3DSolid(targ);
      quad->mMaterial->SetColorMode(lev2::GfxMaterial3DSolid::EMODE_TEX_COLOR);
      quad->mMaterial->mRasterState.SetZWriteMask(true);
    }

    auto mtl = quad->mMaterial;
    auto frame_data = targ->GetRenderContextFrameData();

    auto ssci = pren->GetDrawableDataB().Get<ProcTexControllerInst*>();
    auto ent = ssci->GetEntity();
    auto psi = ent->GetSceneInst();
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
      ork::CVector2 uv0(0.0f, 1.0f);
      ork::CVector2 uv1(1.0f, 1.0f);
      ork::CVector2 uv2(1.0f, 0.0f);
      ork::CVector2 uv3(0.0f, 0.0f);
      ork::CVector3 vv0(x0, y0, fZ);
      ork::CVector3 vv1(x1, y0, fZ);
      ork::CVector3 vv2(x1, y1, fZ);
      ork::CVector3 vv3(x0, y1, fZ);

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

    const CMatrix4& mtx = pren->GetMatrix();

    targ->GBI()->DrawPrimitive(vw, lev2::EPRIM_TRIANGLES, 6);

    targ->MTXI()->PopMMatrix();
    targ->PopMaterial();
  };

  ProcTexControllerInst* ssci = pent->GetTypedComponent<ProcTexControllerInst>();
  const ProcTexControllerData& cd = ssci->GetCD();

  CallbackDrawable* pdrw = new CallbackDrawable(pent);
  pent->AddDrawable(AddPooledLiteral("Default"), pdrw);
  pdrw->SetRenderCallback(l_render_quad);
  pdrw->SetOwner(&pent->GetEntData());
  pdrw->SetSortKey(0);
  anyp ap;
  ap.Set<const ProcTexOutputQuad*>(this);
  pdrw->SetUserDataA(ap);
  pdrw->SetUserDataB(ssci);
}
///////////////////////////////////////////////////////////////////////////////
void ProcTexOutputSkybox::Describe() {
  ork::reflect::RegisterProperty("VerticalAdjust", &ProcTexOutputSkybox::mVerticalAdjust);
  ork::reflect::AnnotatePropertyForEditor<ProcTexOutputSkybox>("VerticalAdjust", "editor.range.min", "-10000.0");
  ork::reflect::AnnotatePropertyForEditor<ProcTexOutputSkybox>("VerticalAdjust", "editor.range.max", "10000.0");

  ork::reflect::RegisterProperty("Scale", &ProcTexOutputSkybox::mScale);
  ork::reflect::AnnotatePropertyForEditor<ProcTexOutputSkybox>("Scale", "editor.range.min", "0.1");
  ork::reflect::AnnotatePropertyForEditor<ProcTexOutputSkybox>("Scale", "editor.range.max", "10000.0");
}
ProcTexOutputSkybox::ProcTexOutputSkybox() : mVerticalAdjust(0.0f), mMaterial(nullptr), mScale(1.0f) {}
void ProcTexOutputSkybox::DoLinkEntity(SceneInst* psi, Entity* pent) const {
  auto l_render_skybox = [=](lev2::RenderContextInstData& rcid, lev2::GfxTarget* targ, const lev2::CallbackRenderable* pren) {
    bool IsPickState = targ->FBI()->IsPickState();
    if (IsPickState)
      return;

    auto skybox = pren->GetDrawableDataA().Get<const ProcTexOutputSkybox*>();

    if (0 == skybox->mMaterial) {
      skybox->mMaterial = new lev2::GfxMaterial3DSolid(targ);
      skybox->mMaterial->SetColorMode(lev2::GfxMaterial3DSolid::EMODE_TEX_COLOR);
      skybox->mMaterial->mRasterState.SetZWriteMask(false);
    }

    auto mtl = skybox->mMaterial;
    auto frame_data = targ->GetRenderContextFrameData();
    float fscale = skybox->mScale;
    float fphase = 0.0f;
    CVector3 pos = frame_data->GetCameraData()->GetEye();
    pos.SetY(pos.GetY() + skybox->mVerticalAdjust);
    CMatrix4 mtxSPIN;
    mtxSPIN.RotateY(fphase);
    CMatrix4 mtxSKY;
    mtxSKY.SetScale(fscale);
    mtxSKY.SetTranslation(pos);
    mtxSKY = mtxSPIN * mtxSKY;
    // rcid.ForceNoZWrite( true );
    auto ssci = pren->GetDrawableDataB().Get<ProcTexControllerInst*>();
    auto ent = ssci->GetEntity();
    auto psi = ent->GetSceneInst();
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
    ork::lev2::Texture* ptx = templ.ResultTexture();
    mtl->SetTexture(ptx);
    targ->PushMaterial(mtl);
    targ->MTXI()->PushMMatrix(mtxSKY);
    lev2::CGfxPrimitives::GetRef().RenderSkySphere(targ);
    targ->MTXI()->PopMMatrix();
    targ->PopMaterial();
  };

  ProcTexControllerInst* ssci = pent->GetTypedComponent<ProcTexControllerInst>();
  const ProcTexControllerData& cd = ssci->GetCD();

  CallbackDrawable* pdrw = new CallbackDrawable(pent);
  pent->AddDrawable(AddPooledLiteral("Default"), pdrw);
  pdrw->SetRenderCallback(l_render_skybox);
  pdrw->SetOwner(&pent->GetEntData());
  pdrw->SetSortKey(0);
  anyp ap;
  ap.Set<const ProcTexOutputSkybox*>(this);
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
void ProcTexOutputDynTex::DoLinkEntity(SceneInst* psi, Entity* pent) const {
  auto l_compute = [=](lev2::RenderContextInstData& rcid, lev2::GfxTarget* targ, const lev2::CallbackRenderable* pren) {
    bool IsPickState = targ->FBI()->IsPickState();
    if (IsPickState)
      return;

    auto dyn = pren->GetDrawableDataA().Get<const ProcTexOutputDynTex*>();

    auto ssci = pren->GetDrawableDataB().Get<ProcTexControllerInst*>();
    auto ent = ssci->GetEntity();
    auto psi = ent->GetSceneInst();
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
    targ->TXI()->generateMipMaps(templ.ResultTexture());
    if (dyn->mAsset)
      dyn->mAsset->SetTexture(templ.ResultTexture());
  };

  ProcTexControllerInst* ssci = pent->GetTypedComponent<ProcTexControllerInst>();
  const ProcTexControllerData& cd = ssci->GetCD();

  CallbackDrawable* pdrw = new CallbackDrawable(pent);
  pent->AddDrawable(AddPooledLiteral("Default"), pdrw);
  pdrw->SetRenderCallback(l_compute);
  pdrw->SetOwner(&pent->GetEntData());
  pdrw->SetSortKey(0);
  anyp ap;
  ap.Set<const ProcTexOutputDynTex*>(this);
  pdrw->SetUserDataA(ap);
  pdrw->SetUserDataB(ssci);
}
///////////////////////////////////////////////////////////////////////////////
void ProcTexOutputBake::Describe() {
  ork::reflect::RegisterProperty("NumFrames", &ProcTexOutputBake::mNumExportFrames);
  ork::reflect::AnnotatePropertyForEditor<ProcTexOutputBake>("NumFrames", "editor.range.min", "1");
  ork::reflect::AnnotatePropertyForEditor<ProcTexOutputBake>("NumFrames", "editor.range.max", "3600");
  // ork::reflect::RegisterProperty("DynTexName", &ProcTexOutputDynTex::mDynTexPath);

  auto opm = new ork::reflect::OpMap;
  opm->mLambdaMap["BakeProcTex"] = [=](Object* pobj) {
    ProcTexOutputBake* as_ptcd = rtti::autocast(pobj);
    if (as_ptcd) {
      // Op([=](){}).QueueASync(MainThreadOpQ());
      // assert(false);

      auto fname = SaveFileRequester("Export ProcTex Image Sequence", "PNG (*.png)");

      if (fname.length()) {
        printf("Starting Bake num_frames<%d>\n", as_ptcd->mNumExportFrames);

        // as_ptcd->mPerformingBake = true;
        // as_ptcd->mBakeFrameIndex = 0;
        // as_ptcd->mWritePath = fname.c_str();
      }
    }
  };
  reflect::AnnotateClassForEditor<ProcTexOutputBake>("editor.object.ops", opm);
}
ProcTexOutputBake::ProcTexOutputBake()
    : mNumExportFrames(1), mPerformingBake(false), mBakeFrameIndex(0), mWritePath(AddPooledLiteral("yo.png")) {}
#if 0
void ProcTexControllerData::IncrementFrame() const
{
	mBakeFrameIndex++;
	printf( "ProcTexControllerData::IncrementFrame() mBakeFrameIndex<%d>\n", mBakeFrameIndex );
	if( mBakeFrameIndex>=mNumExportFrames )
	{
		printf( "bake done nframes<%d>\n", mNumExportFrames );
		mPerformingBake = false;
	}
}
#endif
///////////////////////////////////////////////////////////////////////////////
void ProcTexOutputBake::DoLinkEntity(SceneInst* psi, Entity* pent) const {}

///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
} // namespace ent
} // namespace ork
