////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
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
#include <orktool/toolcore/selection.h>
#include <pkg/ent/editor/editor.h>
#include <ork/lev2/gfx/lighting/gfx_lighting.h>
#include "vpRenderer.h"

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace tool {
///////////////////////////////////////////////////////////////////////////////

Renderer::Renderer(ent::SceneEditorBase& ed, lev2::Context* ptarg)
    : lev2::IRenderer(ptarg)
    , mEditor(ed) {
}

///////////////////////////////////////////////////////////////////////////////

void Renderer::RenderModelGroup(const modelgroup_t& mdlgroup) const {
  for (auto r : mdlgroup)
    RenderModel(*r);
}

///////////////////////////////////////////////////////////////////////////////

void Renderer::RenderModel(const lev2::ModelRenderable& ModelRen, ork::lev2::RenderGroupState rgs) const {
  lev2::Context* target = GetTarget();

  const auto& SelMgr = mEditor.selectionManager();

  auto minst                  = ModelRen.GetModelInst();
  const lev2::XgmModel* model = minst->xgmModel();

  target->debugPushGroup(FormatString("toolrenderer::RenderModel model<%p> minst<%p>", model, minst.get()));

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
    rmatx.RotateX(3.14159f * -0.5f);
    rmaty.RotateX(3.14159f);
    nmat = (rmatx * rmaty) * nmat;
  }

  ///////////////////////////////////////

  auto owner = ModelRen.GetObject();
  auto c     = owner->GetClass();

  const ent::Entity* as_ent = rtti::autocast(owner);
  if (as_ent) {
    owner = as_ent->data();
    c     = owner->GetClass();
  }

  bool is_sel        = (owner == nullptr) ? false : SelMgr.IsObjectSelected(owner);
  bool is_pick_state = target->FBI()->isPickState();

  /////////////////////////////////////////////////////////////

  ork::lev2::RenderContextInstData MatCtx;

  lev2::RenderContextInstModelData MdlCtx;

  // if(ModelRen.GetEngineParamFloat(0) < 1.0f && ModelRen.GetEngineParamFloat(0) > 0.0f)
  //	orkprintf("ModelRen.GetEngineParamFloat(0) = %g\n", ModelRen.GetEngineParamFloat(0));

  for (int i = 0; i < ork::lev2::ModelRenderable::kMaxEngineParamFloats; i++)
    MatCtx.SetEngineParamFloat(i, ModelRen.GetEngineParamFloat(i));

  MatCtx.SetMaterialInst(&minst->RefMaterialInst());

  MdlCtx.mMesh    = ModelRen.mesh();
  MdlCtx.mSubMesh = ModelRen.subMesh();
  MdlCtx._cluster = ModelRen.GetCluster();

  MatCtx.SetMaterialIndex(0);
  MatCtx.SetRenderer(this);

  lev2::PickBuffer* pickBuf = target->FBI()->currentPickBuffer();

  ///////////////////////////////////////
  // select mod color
  //  if in pick state - override with colorfied object pick id
  //  if selected - override with red
  ///////////////////////////////////////

  fcolor4 ObjColor = ModelRen.GetModColor();

  if (is_pick_state) {
    uint64_t pid = pickBuf ? pickBuf->AssignPickId((ork::Object*)ModelRen.GetObject()) : 0;
    printf("AssignPickId<%zx>\n", pid);
    ObjColor.SetRGBAU64(pid);
  } else if (is_sel) {
    ObjColor = fcolor4::Red();
  }

  target->debugMarker(FormatString("toolrenderer::RenderModel isskinned<%d> owner_as_ent<%p>", int(model->isSkinned()), as_ent));

  ///////////////////////////////////////

  // printf( "Renderer::RenderModel() rable<%p>\n", & ModelRen );
  bool model_is_skinned = model->isSkinned();
  MatCtx._isSkinned     = model_is_skinned;
  MdlCtx.SetSkinned(model_is_skinned);
  MdlCtx.SetModelInst(minst);
  if (model_is_skinned) {
    model->RenderSkinned(minst.get(), ObjColor, nmat, GetTarget(), MatCtx, MdlCtx);
  } else {
    model->RenderRigid(ObjColor, nmat, GetTarget(), MatCtx, MdlCtx);
  }

  target->debugPopGroup();
}

///////////////////////////////////////////////////////////////////////////////
}} // namespace ork::tool
///////////////////////////////////////////////////////////////////////////////
