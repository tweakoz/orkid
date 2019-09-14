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

void Renderer::RenderModelGroup(const ork::lev2::ModelRenderable** Renderables, int inumr) const {
  // printf( "Renderer::RenderModelGroup() numr<%d>\n", inumr );

  for (int i = 0; i < inumr; i++) {
    const ork::lev2::ModelRenderable& r = *Renderables[i];
    const ork::lev2::XgmSubMesh* psub = r.GetSubMesh();
    ork::lev2::GfxMaterial* pmtl = psub->GetMaterial();
    RenderModel(r);
  }
}

///////////////////////////////////////////////////////////////////////////////

void Renderer::RenderModel(const lev2::ModelRenderable& ModelRen, ork::lev2::RenderGroupState rgs) const {
  lev2::GfxTarget* target = GetTarget();

  const auto& SelMgr = mEditor.selectionManager();

  const lev2::XgmModelInst* minst = ModelRen.GetModelInst();
  const lev2::XgmModel* model = minst->GetXgmModel();

  /////////////////////////////////////////////////////////////

  float fscale = ModelRen.GetScale();

  const ork::fvec3& offset = ModelRen.GetOffset();
  const ork::fvec3& rotate = ModelRen.GetRotate();

  fmtx4 smat;
  fmtx4 tmat;
  fmtx4 rmat;

  smat.SetScale(fscale);
  tmat.SetTranslation(offset);
  rmat.SetRotateY(rotate.GetY() + rotate.GetZ());

  fmtx4 wmat = ModelRen.GetMatrix();

  /////////////////////////////////////////////////////////////
  // compute world matrix
  /////////////////////////////////////////////////////////////

  fmtx4 nmat = tmat * rmat * smat * wmat;

  if (minst->IsBlenderZup()) // zup to yup conversion matrix
  {
    fmtx4 rmatx, rmaty;
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

  for (int i = 0; i < ork::lev2::ModelRenderable::kMaxEngineParamFloats; i++)
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

  fcolor4 ObjColor = ModelRen.GetModColor();

  if (is_pick_state) {
    uint64_t pid = pickBuf
		             ? pickBuf->AssignPickId((ork::Object*)ModelRen.GetObject())
								 : 0;
    ObjColor.SetRGBAU64(pid);
  } else if (is_sel) {
    ObjColor = fcolor4::Red();
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
