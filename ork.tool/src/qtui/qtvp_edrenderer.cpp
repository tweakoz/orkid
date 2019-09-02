////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <orktool/qtui/qtui_tool.h>

///////////////////////////////////////////////////////////////////////////////

#include <ork/lev2/gfx/gfxmaterial_test.h>
#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/lev2/gfx/gfxprimitives.h>
#include <ork/lev2/gfx/pickbuffer.h>
#include <ork/lev2/gfx/texman.h>
#include <orktool/qtui/qtvp_edrenderer.h>
#include <orktool/toolcore/selection.h>
#include <pkg/ent/editor/editor.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace tool {
///////////////////////////////////////////////////////////////////////////////

Renderer::Renderer(ent::SceneEditorBase& ed, lev2::GfxTarget* ptarg) : lev2::Renderer(ptarg), mEditor(ed) {
  mTopSkyEnvMap = 0;
  mBotSkyEnvMap = 0;
}

///////////////////////////////////////////////////////////////////////////////

void Renderer::Init() {
  if (0 == mTopSkyEnvMap) {
    mTopSkyEnvMap = lev2::Texture::LoadUnManaged("data://yo_dualparamap_top.tga");
    mBotSkyEnvMap = lev2::Texture::LoadUnManaged("data://yo_dualparamap_bot.tga");
  }
}

///////////////////////////////////////////////////////////////////////////////

U32 Renderer::ComposeSortKey(U32 texIndex, U32 depthIndex, U32 passIndex, U32 transIndex) const { return 0; }

///////////////////////////////////////////////////////////////////////////////

void Renderer::RenderBox(const lev2::CBoxRenderable& BoxRen) const {
  CMatrix4 wmat = BoxRen.GetMatrix();

  GetTarget()->BindMaterial(lev2::GfxEnv::GetRef().GetDefault3DMaterial());
  if (GetTarget()->FBI()->IsPickState()) {
    CColor4 ObjColor;
    ObjColor.SetRGBAU32((U32)((u64)BoxRen.GetObject()));

    GetTarget()->PushModColor(ObjColor);

    GetTarget()->MTXI()->PushMMatrix(wmat);
    ork::lev2::CGfxPrimitives::RenderAxisBox(GetTarget());
    GetTarget()->MTXI()->PopMMatrix();

    CQuaternion quat;
    quat.FromAxisAngle(CVector4(0.0f, 0.0f, 1.0f, PI * -0.5f));

    GetTarget()->MTXI()->PushMMatrix(quat.ToMatrix() * wmat);
    ork::lev2::CGfxPrimitives::RenderAxisBox(GetTarget());
    GetTarget()->MTXI()->PopMMatrix();

    quat.FromAxisAngle(CVector4(1.0f, 0.0f, 0.0f, PI * 0.5f));

    GetTarget()->MTXI()->PushMMatrix(quat.ToMatrix() * wmat);
    ork::lev2::CGfxPrimitives::RenderAxisBox(GetTarget());
    GetTarget()->MTXI()->PopMMatrix();

    GetTarget()->PopModColor();
  } else {
    GetTarget()->MTXI()->PushMMatrix(wmat);
    GetTarget()->PushModColor(CColor4(0.3f, 0.7f, 0.3f));
    ork::lev2::CGfxPrimitives::RenderAxisBox(GetTarget());
    GetTarget()->MTXI()->PopMMatrix();
    GetTarget()->PopModColor();

    CQuaternion quat;
    quat.FromAxisAngle(CVector4(0.0f, 0.0f, 1.0f, PI * -0.5f));

    GetTarget()->MTXI()->PushMMatrix(quat.ToMatrix() * wmat);
    GetTarget()->PushModColor(CColor4(0.7f, 0.3f, 0.3f));
    ork::lev2::CGfxPrimitives::RenderAxisBox(GetTarget());
    GetTarget()->MTXI()->PopMMatrix();
    GetTarget()->PopModColor();

    quat.FromAxisAngle(CVector4(1.0f, 0.0f, 0.0f, PI * 0.5f));

    GetTarget()->MTXI()->PushMMatrix(quat.ToMatrix() * wmat);
    GetTarget()->PushModColor(CColor4(0.3f, 0.3f, 0.7f));
    ork::lev2::CGfxPrimitives::RenderAxisBox(GetTarget());
    GetTarget()->MTXI()->PopMMatrix();
    GetTarget()->PopModColor();
  }
}

///////////////////////////////////////////////////////////////////////////////

void Renderer::RenderModelGroup(const ork::lev2::CModelRenderable** Renderables, int inumr) const {
  // printf( "Renderer::RenderModelGroup() numr<%d>\n", inumr );

  for (int i = 0; i < inumr; i++) {
    const ork::lev2::CModelRenderable& r = *Renderables[i];
    const ork::lev2::XgmSubMesh* psub = r.GetSubMesh();
    ork::lev2::GfxMaterial* pmtl = psub->GetMaterial();
    RenderModel(r);
  }
}

///////////////////////////////////////////////////////////////////////////////

void Renderer::RenderModel(const lev2::CModelRenderable& ModelRen, ork::lev2::RenderGroupState rgs) const {
  lev2::GfxTarget* target = GetTarget();

  const auto& SelMgr = mEditor.selectionManager();

  const lev2::XgmModelInst* minst = ModelRen.GetModelInst();
  const lev2::XgmModel* model = minst->GetXgmModel();

  /////////////////////////////////////////////////////////////

  float fscale = ModelRen.GetScale();

  const ork::CVector3& offset = ModelRen.GetOffset();
  const ork::CVector3& rotate = ModelRen.GetRotate();

  CMatrix4 smat;
  CMatrix4 tmat;
  CMatrix4 rmat;

  smat.SetScale(fscale);
  tmat.SetTranslation(offset);
  rmat.SetRotateY(rotate.GetY() + rotate.GetZ());

  CMatrix4 wmat = ModelRen.GetMatrix();

  /////////////////////////////////////////////////////////////
  // compute world matrix
  /////////////////////////////////////////////////////////////

  CMatrix4 nmat = tmat * rmat * smat * wmat;

  if (minst->IsBlenderZup()) // zup to yup conversion matrix
  {
    CMatrix4 rmatx, rmaty;
    rmatx.RotateX(3.14159f * 0.5f);
    rmaty.RotateX(3.14159f);
    nmat = (rmatx * rmaty) * nmat;
  }

  ///////////////////////////////////////

  auto owner = ModelRen.GetObject();
  auto c = owner->GetClass();

  const ent::Entity* as_ent = rtti::autocast(owner);
  if (as_ent) {
    owner = &as_ent->GetEntData();
    c = owner->GetClass();
  }

  bool is_sel = (owner == nullptr) ? false : SelMgr.IsObjectSelected(owner);
  bool is_pick_state = target->FBI()->IsPickState();

  /////////////////////////////////////////////////////////////

  ork::lev2::RenderContextInstData MatCtx;

  lev2::RenderContextInstModelData MdlCtx;

  // if(ModelRen.GetEngineParamFloat(0) < 1.0f && ModelRen.GetEngineParamFloat(0) > 0.0f)
  //	orkprintf("ModelRen.GetEngineParamFloat(0) = %g\n", ModelRen.GetEngineParamFloat(0));

  for (int i = 0; i < ork::lev2::CModelRenderable::kMaxEngineParamFloats; i++)
    MatCtx.SetEngineParamFloat(i, ModelRen.GetEngineParamFloat(i));

  MatCtx.SetTopEnvMap(mTopSkyEnvMap);
  MatCtx.SetBotEnvMap(mBotSkyEnvMap);
  MatCtx.SetMaterialInst(&minst->RefMaterialInst());
  MatCtx.BindLightMap(ModelRen.GetSubMesh()->mLightMap);
  MatCtx.SetVertexLit(ModelRen.GetSubMesh()->mbVertexLit);

  MdlCtx.mMesh = ModelRen.GetMesh();
  MdlCtx.mSubMesh = ModelRen.GetSubMesh();
  MdlCtx.mCluster = ModelRen.GetCluster();
  MdlCtx.mpWorldPose = ModelRen.GetWorldPose();

  MatCtx.SetMaterialIndex(0);
  MatCtx.SetRenderer(this);

  lev2::PickBufferBase* pickBuf = target->FBI()->GetCurrentPickBuffer();

  ///////////////////////////////////////
  // select mod color
  //  if in pick state - override with colorfied object pick id
  //  if selected - override with red
  ///////////////////////////////////////

  CColor4 ObjColor = ModelRen.GetModColor();

  if (is_pick_state) {
    uint64_t pid = pickBuf
		             ? pickBuf->AssignPickId((ork::Object*)ModelRen.GetObject())
								 : 0;
    ObjColor.SetRGBAU64(pid);
  } else if (is_sel) {
    ObjColor = CColor4::Red();
  }

  ///////////////////////////////////////

  // printf( "Renderer::RenderModel() rable<%p>\n", & ModelRen );
  lev2::LightingGroup lgrp;
  lgrp.mLightManager = target->GetRenderContextFrameData()->GetLightManager();
  lgrp.mLightMask = ModelRen.GetLightMask();
  MatCtx.SetLightingGroup(&lgrp);

  MdlCtx.SetSkinned(model->IsSkinned());
  if (model->IsSkinned()) {
    model->RenderSkinned(minst, ObjColor, nmat, GetTarget(), MatCtx, MdlCtx);
  } else {
    model->RenderRigid(ObjColor, nmat, GetTarget(), MatCtx, MdlCtx);
  }
}

///////////////////////////////////////////////////////////////////////////////

void Renderer::RenderFrustum(const lev2::FrustumRenderable& FrusRen) const {
  // GetTarget()->IMI()->QueFlush();

  bool bpickbuffer = (GetTarget()->FBI()->IsOffscreenTarget());

  if (bpickbuffer)
    return;

  CColor4 ObjColor;
  u64 asu64 = (u64)mpCurrentQueueObject;
  ObjColor.SetRGBAU32((U32)asu64);

  CVector3 vScreenUp, vScreenRight;
  CVector2 vpdims(float(GetTarget()->GetW()), float(GetTarget()->GetH()));

  bool objspace = FrusRen.IsObjSpace();

  if (false == GetTarget()->FBI()->IsOffscreenTarget()) {
    static ork::lev2::GfxMaterial3DSolid StdMaterial(GetTarget());
    StdMaterial.SetColorMode(ork::lev2::GfxMaterial3DSolid::EMODE_MOD_COLOR);
    StdMaterial.mRasterState.SetBlending(ork::lev2::EBLENDING_OFF);
    StdMaterial.mRasterState.SetDepthTest(ork::lev2::EDEPTHTEST_LEQUALS);
    StdMaterial.mRasterState.SetCullTest(ork::lev2::ECULLTEST_OFF);
    StdMaterial.mRasterState.SetZWriteMask(false);

    CColor4 AlphaYellow(1.0f, 1.0f, 0.0f, 0.3f);
    if (false == objspace)
      GetTarget()->MTXI()->PushMMatrix(CMatrix4::Identity);
    {
      CVector2 uvZED;
      lev2::DynamicVertexBuffer<lev2::SVtxV12C4T16>& VB = lev2::GfxEnv::GetSharedDynamicVB();
      lev2::VtxWriter<lev2::SVtxV12C4T16> vw;

      vw.Lock(GetTarget(), &VB, 4 * 2 * 3);
      GetTarget()->PushModColor(AlphaYellow);
      GetTarget()->BindMaterial(&StdMaterial);
      const Frustum& frus = FrusRen.GetFrustum();
      {
        for (int i = 0; i < 4; i++) {
          const CVector3& p0 = frus.mNearCorners[i];
          const CVector3& p1 = frus.mFarCorners[i];
          // printf( "pN<%d> %f %f %f\n", i, p0.GetX(), p0.GetY(), p0.GetZ() );
          // printf( "pF<%d> %f %f %f\n", i, p1.GetX(), p1.GetY(), p1.GetZ() );
          vw.AddVertex(lev2::SVtxV12C4T16(p0, uvZED, 0xffffffff));
          vw.AddVertex(lev2::SVtxV12C4T16(p1, uvZED, 0xffffffff));
        }
        for (int i = 0; i < 4; i++) {
          const CVector3& p0 = frus.mNearCorners[i];
          const CVector3& p1 = frus.mNearCorners[(i + 1) % 4];
          vw.AddVertex(lev2::SVtxV12C4T16(p0, uvZED, 0xffffffff));
          vw.AddVertex(lev2::SVtxV12C4T16(p1, uvZED, 0xffffffff));
        }
        for (int i = 0; i < 4; i++) {
          const CVector3& p0 = frus.mFarCorners[i];
          const CVector3& p1 = frus.mFarCorners[(i + 1) % 4];
          vw.AddVertex(lev2::SVtxV12C4T16(p0, uvZED, 0xffffffff));
          vw.AddVertex(lev2::SVtxV12C4T16(p1, uvZED, 0xffffffff));
        }
      }
      vw.UnLock(GetTarget());
      GetTarget()->GBI()->DrawPrimitive(vw, lev2::EPRIM_LINES, 4 * 2 * 3);
      GetTarget()->PopModColor();
      GetTarget()->BindMaterial(0);
      // printf( "DRAWFRUSREND<%p>\n", & FrusRen );
    }
    if (false == objspace)
      GetTarget()->MTXI()->PopMMatrix();
  }
}

///////////////////////////////////////////////////////////////////////////////

void Renderer::RenderSphere(const lev2::SphereRenderable& SphRen) const {
  lev2::GfxTarget* pTARG = GetTarget();
  CMatrix4 mtx;
  mtx.SetTranslation(SphRen.GetPosition());
  mtx.SetScale(SphRen.GetRadius());
  GetTarget()->MTXI()->PushMMatrix(mtx);
  pTARG->PushModColor(SphRen.GetColor());
  { ork::lev2::CGfxPrimitives::GetRef().RenderTriCircle(pTARG); }
  GetTarget()->PopModColor();
  GetTarget()->MTXI()->PopMMatrix();
}

///////////////////////////////////////////////////////////////////////////////

void Renderer::RenderCallback(const lev2::CallbackRenderable& cbren) const {
  lev2::RenderContextInstData MatCtx;
  lev2::GfxTarget* pTARG = GetTarget();
  MatCtx.SetRenderer(this);

  if (cbren.GetRenderCallback()) {
    cbren.GetRenderCallback()(MatCtx, pTARG, &cbren);
  }
}

///////////////////////////////////////////////////////////////////////////////
}} // namespace ork::tool
///////////////////////////////////////////////////////////////////////////////
