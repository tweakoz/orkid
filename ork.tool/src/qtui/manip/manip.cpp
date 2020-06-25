////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/gfxmodel.h>
#include <orktool/manip/manip.h>
#include <orktool/orktool_pch.h>
//
#include <ork/kernel/prop.h>
#include <ork/lev2/gfx/dbgfontman.h>
#include <ork/lev2/gfx/gfxmaterial_test.h>
#include <ork/lev2/gfx/gfxprimitives.h>
#include <ork/lev2/gfx/pickbuffer.h>
#include <ork/lev2/gfx/renderer/rendercontext.h>
#include <ork/lev2/ui/event.h>
#include <ork/math/audiomath.h>
#include <ork/reflect/properties/register.h>
#include <ork/rtti/downcast.h>

INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::Manip, "Manip");

INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::ManipTrans, "ManipTrans");
INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::ManipSingleTrans, "ManipSingleTrans");
INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::ManipDualTrans, "ManipDualTrans");
INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::ManipRot, "ManipRot");

INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::ManipTX, "ManipTX");
INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::ManipTY, "ManipTY");
INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::ManipTZ, "ManipTZ");
INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::ManipTXY, "ManipTXY");
INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::ManipTXZ, "ManipTXZ");
INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::ManipTYZ, "ManipTYZ");

INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::ManipRX, "ManipRX");
INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::ManipRY, "ManipRY");
INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::ManipRZ, "ManipRZ");

INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::ManipManager, "ManipManager");

////////////////////////////////////////////////////////////////////////////////
namespace ork { namespace lev2 {
////////////////////////////////////////////////////////////////////////////////

void Manip::Describe() {
}

void ManipTrans::Describe() {
}
void ManipSingleTrans::Describe() {
}
void ManipDualTrans::Describe() {
}
void ManipRot::Describe() {
}

void ManipTX::Describe() {
}
void ManipTY::Describe() {
}
void ManipTZ::Describe() {
}
void ManipTXY::Describe() {
}
void ManipTXZ::Describe() {
}
void ManipTYZ::Describe() {
}

void ManipRX::Describe() {
}
void ManipRY::Describe() {
}
void ManipRZ::Describe() {
}

////////////////////////////////////////////////////////////////////////////////

void ManipManager::Describe() {
  RegisterAutoSlot(ManipManager, ObjectDeleted);
  RegisterAutoSlot(ManipManager, ObjectSelected);
  RegisterAutoSlot(ManipManager, ObjectDeSelected);
  RegisterAutoSlot(ManipManager, ClearSelection);
}

ManipManager::ManipManager()
    : meManipMode(EMANIPMODE_WORLD_TRANS)
    , meManipEnable(EMANIPMODE_OFF)
    , meUIMode(EUIMODE_STD)
    , ConstructAutoSlot(ObjectDeSelected)
    , ConstructAutoSlot(ObjectSelected)
    , ConstructAutoSlot(ObjectDeleted)
    , ConstructAutoSlot(ClearSelection) {
  SetupSignalsAndSlots();
}

////////////////////////////////////////////////////////////////////////////////

void ManipManager::SlotObjectDeleted(ork::Object* pOBJ) {
  if (mpCurrentObject == pOBJ) {
    DetachObject();
  }
}
void ManipManager::SlotObjectSelected(ork::Object* pOBJ) {
}
void ManipManager::SlotObjectDeSelected(ork::Object* pOBJ) {
}
void ManipManager::SlotClearSelection() {
}

////////////////////////////////////////////////////////////////////////////////

bool ManipManager::UIEventHandler(ui::event_constptr_t EV) {
  bool rval = false;

  switch (EV->_eventcode) {
    case ui::EventCode::KEY:
    case ui::EventCode::KEYUP: {
      if (EV->mbSHIFT)
        mDualAxis = true;
      else
        mDualAxis = false;
    } break;
  }

  // printf( "ManipManager::UIEventHandler mpCurrentManip<%p>\n", mpCurrentManip );

  if (mpCurrentManip)
    rval = mpCurrentManip->UIEventHandler(EV);

  return rval;
}

////////////////////////////////////////////////////////////////////////////////

IntersectionRecord::IntersectionRecord()
    : mIntersectionPoint(0.0f, 0.0f, 0.0f)
    , mOldIntersectionPoint(0.0f, 0.0f, 0.0f)
    , mBaseIntersectionPoint(0.0f, 0.0f, 0.0f)
    , mbHasItersected(false) {
}

fvec4 IntersectionRecord::GetLocalSpaceDelta(const fmtx4& InvLocalMatrix) {
  return mIntersectionPoint.Transform(InvLocalMatrix) - mOldIntersectionPoint.Transform(InvLocalMatrix);
}

////////////////////////////////////////////////////////////////////////////////

Manip::Manip(ManipManager& mgr)
    : mManager(mgr)
    , mActiveIntersection(0)
    , mColor() {
}

////////////////////////////////////////////////////////////////////////////////

bool Manip::CheckIntersect(void) const {
  bool bisect = (mActiveIntersection == nullptr) ? false : mActiveIntersection->mbHasItersected;
  // printf( "manip<%p> ai<%p> CheckIntersect<%d>\n", this, mActiveIntersection, int(bisect) );

  return bisect;
}

////////////////////////////////////////////////////////////////////////////////

void Manip::CalcPlanes() {
  fmtx4 CurMatrix;
  mBaseTransform.GetMatrix(CurMatrix);
  fvec4 origin  = CurMatrix.GetTranslation();
  fvec4 normalX = CurMatrix.GetXNormal();
  fvec4 normalY = CurMatrix.GetYNormal();
  fvec4 normalZ = CurMatrix.GetZNormal();
  /////////////////////////////
  if (mManager.mbWorldTrans && (ManipManager::EMANIPMODE_WORLD_TRANS == mManager.GetManipMode())) {
    normalX = fvec4(1.0f, 0.0f, 0.0f);
    normalY = fvec4(0.0f, 1.0f, 0.0f);
    normalZ = fvec4(0.0f, 0.0f, 1.0f);
  }
  /////////////////////////////
  mPlaneXZ.CalcFromNormalAndOrigin(normalY, origin);
  mPlaneYZ.CalcFromNormalAndOrigin(normalX, origin);
  mPlaneXY.CalcFromNormalAndOrigin(normalZ, origin);
}

////////////////////////////////////////////////////////////////////////////////

fvec3 Manip::IntersectWithPlanes(const ork::fvec2& posubp) {
  fvec3 rval;

  const UiCamera* cam = mManager.getActiveCamera();
  fmtx4 CurMatrix;
  mBaseTransform.GetMatrix(CurMatrix);
  /////////////////////////////
  fvec3 rayN, rayF;
  cam->GenerateDepthRay(posubp, rayN, rayF, InvMatrix);
  fvec4 rayDir = (rayF - rayN).Normal();
  fray3 ray;
  ray.mOrigin    = rayN;
  ray.mDirection = rayDir;
  /////////////////////////////
  float dist;
  mIntersection[EPLANE_XZ].mbHasItersected = mPlaneXZ.Intersect(ray, dist);
  fvec4 rayOutXZ                           = (rayDir * dist);
  mIntersection[EPLANE_YZ].mbHasItersected = mPlaneYZ.Intersect(ray, dist);
  fvec4 rayOutYZ                           = (rayDir * dist);
  mIntersection[EPLANE_XY].mbHasItersected = mPlaneXY.Intersect(ray, dist);
  fvec4 rayOutXY                           = (rayDir * dist);
  /////////////////////////////
  mIntersection[EPLANE_XZ].mIntersectionPoint = rayN + rayOutXZ;
  mIntersection[EPLANE_YZ].mIntersectionPoint = rayN + rayOutYZ;
  mIntersection[EPLANE_XY].mIntersectionPoint = rayN + rayOutXY;
  /////////////////////////////
  rval = rayDir.xyz();
  return rval;
}

////////////////////////////////////////////////////////////////////////////////

void Manip::SelectBestPlane(const ork::fvec2& posubp) {
  bool brotmode = (mManager.GetManipMode() == ManipManager::EMANIPMODE_LOCAL_ROTATE);

  CalcPlanes();
  fvec3 RayDir = IntersectWithPlanes(posubp);
  /////////////////////////////
  mIntersection[EPLANE_XZ].mOldIntersectionPoint  = mIntersection[EPLANE_XZ].mIntersectionPoint;
  mIntersection[EPLANE_YZ].mOldIntersectionPoint  = mIntersection[EPLANE_YZ].mIntersectionPoint;
  mIntersection[EPLANE_XY].mOldIntersectionPoint  = mIntersection[EPLANE_XY].mIntersectionPoint;
  mIntersection[EPLANE_XZ].mBaseIntersectionPoint = mIntersection[EPLANE_XZ].mIntersectionPoint;
  mIntersection[EPLANE_YZ].mBaseIntersectionPoint = mIntersection[EPLANE_YZ].mIntersectionPoint;
  mIntersection[EPLANE_XY].mBaseIntersectionPoint = mIntersection[EPLANE_XY].mIntersectionPoint;
  /////////////////////////////
  // rot manips use explicit planes
  /////////////////////////////
  if (brotmode) {
    if (mManager.mpCurrentManip->GetClass() == ManipRX::GetClassStatic()) {
      mActiveIntersection = &mIntersection[EPLANE_YZ];
    } else if (mManager.mpCurrentManip->GetClass() == ManipRY::GetClassStatic()) {
      mActiveIntersection = &mIntersection[EPLANE_XZ];
    } else if (mManager.mpCurrentManip->GetClass() == ManipRZ::GetClassStatic()) {
      mActiveIntersection = &mIntersection[EPLANE_XY];
    }
  } else {
    float dotxz  = RayDir.Dot(mPlaneXZ.GetNormal());
    float dotxy  = RayDir.Dot(mPlaneXY.GetNormal());
    float dotyz  = RayDir.Dot(mPlaneYZ.GetNormal());
    float adotxz = fabs(dotxz);
    float adotxy = fabs(dotxy);
    float adotyz = fabs(dotyz);

    // printf( "mManager.mpCurrentManip<%p>\n", mManager.mpCurrentManip );

    if (mManager.mpCurrentManip->GetClass() == ManipTX::GetClassStatic()) {
      mActiveIntersection = (adotxy > adotxz) ? &mIntersection[EPLANE_XZ] : &mIntersection[EPLANE_XY];
    } else if (mManager.mpCurrentManip->GetClass() == ManipTY::GetClassStatic()) {
      mActiveIntersection = (adotxy > adotyz) ? &mIntersection[EPLANE_XY] : &mIntersection[EPLANE_YZ];
    } else if (mManager.mpCurrentManip->GetClass() == ManipTZ::GetClassStatic()) {
      mActiveIntersection = (adotxz > adotyz) ? &mIntersection[EPLANE_XZ] : &mIntersection[EPLANE_YZ];
    }
  }
  /////////////////////////////
  if (mActiveIntersection == &mIntersection[EPLANE_XZ]) {
    printf("Manip<%s> using XZ plane\n", GetClass()->Name().c_str());
  } else if (mActiveIntersection == &mIntersection[EPLANE_XY]) {
    printf("Manip<%s> using XY plane\n", GetClass()->Name().c_str());
  } else if (mActiveIntersection == &mIntersection[EPLANE_YZ]) {
    printf("Manip<%s> using YZ plane\n", GetClass()->Name().c_str());
  }
  /////////////////////////////
}

///////////////////////////////////////////////////////////////////////////////

void ManipManager::RebaseMatrices(void) {
  if (mpCurrentInterface && mpCurrentObject) {
    TransformNode Mat  = mpCurrentInterface->GetTransform(mpCurrentObject);
    TransformNode MatT = mpCurrentInterface->GetTransform(mpCurrentObject);
    mCurTransform      = Mat;
    mOldTransform      = Mat;
    CalcObjectScale();
  }
}

///////////////////////////////////////////////////////////////////////////////

void ManipManager::AttachObject(ork::Object* pobj) {
  object::ObjectClass* pclass = rtti::safe_downcast<object::ObjectClass*>(pobj->GetClass());
  // CClass *pClass = pOBJ->GetClass();

  if (mpCurrentObject && pobj != mpCurrentObject) {
    delete mpCurrentInterface;
    mpCurrentInterface = 0;
  }

  auto anno = pclass->Description().classAnnotation("editor.3dxfinterface");

  if (auto as_cstr = anno.TryAs<ConstString>()) {
    ConstString classname       = as_cstr.value();
    const rtti::Class* pifclass = rtti::autocast(rtti::Class::FindClass(classname));

    if (pifclass) {
      mpCurrentInterface = rtti::autocast(pifclass->CreateObject());
      mpCurrentObject    = pobj;
      RebaseMatrices();
    } else {
      mpCurrentInterface = 0;
      mpCurrentObject    = 0;
    }
  }
}

///////////////////////////////////////////////////////////////////////////////

void ManipManager::DetachObject() {
  mpCurrentInterface = 0;
  mpCurrentObject    = 0;
  meManipEnable      = EMANIPMODE_OFF;
}

///////////////////////////////////////////////////////////////////////////////

void ManipManager::CalcObjectScale(void) {
  fvec3 pos;
  fquat rot;
  float scale;
  fmtx4 ScaleMat;
  mCurTransform.GetMatrix(ScaleMat);
  ScaleMat.decompose(pos, rot, scale);

  mObjScale    = scale;
  mObjInvScale = float(1.0f) / mObjScale;
}

///////////////////////////////////////////////////////////////////////////////

void ManipManager::ReleaseObject(void) {
  static bool bPushIgnore = false;
  if (bPushIgnore) {
    bPushIgnore = false;
  }
  if (mpCurrentInterface && mpCurrentObject && (EMANIPMODE_ON == meManipEnable)) {
    mpCurrentInterface->Detach(mpCurrentObject);
  }
  meManipEnable = EMANIPMODE_OFF;
}

///////////////////////////////////////////////////////////////////////////////

float ManipManager::CalcViewScale(float fW, float fH, const CameraMatrices* cammatrices) const {
  fmtx4 MatW;
  mCurTransform.GetMatrix(MatW);

  //////////////////////////////////////////////////////////////
  // Calc World Scale of manip (maintain constant size)

  fvec2 VP(fW, fH);

  fvec3 Pos = MatW.GetTranslation();
  fvec3 UpVector;
  fvec3 RightVector;
  cammatrices->GetPixelLengthVectors(Pos, VP, UpVector, RightVector);

  float rscale = RightVector.Mag();

  // printf( "manip rscale<%f>\n", rscale );

  //////////////////////////////////////////////////////////////

  return rscale;
}

void ManipManager::materialBegin(Context* targ) {
  auto RCFD       = targ->topRenderContextFrameData();
  auto FBI        = targ->FBI();
  auto FXI        = targ->FXI();
  auto MTXI       = targ->MTXI();
  auto CIMPL      = RCFD->_cimpl;
  const auto& CPD = CIMPL->topCPD();
  auto mtxi       = targ->MTXI();
  bool is_pick    = FBI->isPickState();
  bool is_stereo  = CPD.isStereoOnePass();

  _material->begin(_tek_modcolor, *RCFD);
  _material->bindParamVec4(_par_modcolor, targ->RefModColor());

  const auto& world = MTXI->RefMMatrix();
  if (is_stereo and CPD._stereoCameraMatrices) {
    auto stereomtx = CPD._stereoCameraMatrices;
    auto MVPL      = stereomtx->MVPL(world);
    auto MVPR      = stereomtx->MVPR(world);
    // todo fix for stereo..
    // FXI->BindParamMatrix( _paramMVPL, MVPL);
    // FXI->BindParamMatrix( _paramMVPR, MVPR);
  } else if (CPD._cameraMatrices) {
    auto mcams = CPD._cameraMatrices;
    auto MVP   = world * mcams->_vmatrix * mcams->_pmatrix;
    _material->bindParamMatrix(_par_mvp, MVP);
  } else {
    auto MVP = MTXI->RefMVPMatrix();
    _material->bindParamMatrix(_par_mvp, MVP);
  }
  //_material->setupCamera(*RCFD);
}
////////////////////////////////////////////////////////////////////////////////
void ManipManager::materialEnd(Context* targ) {
  auto RCFD = targ->topRenderContextFrameData();
  _material->end(*RCFD);
}

///////////////////////////////////////////////////////////////////////////////

void ManipManager::Setup(ork::lev2::IRenderer* prend) {
  Context* pTARG = prend->GetTarget();

  if (mpCurrentInterface) {
    bool isshift = false; // OldSchool::IsKeyDepressed(VK_SHIFT);
    if (isshift) {
      mCurTransform = mpCurrentInterface->GetTransform(mpCurrentObject);
    }
  }

  fmtx4 MatW;
  mCurTransform.GetMatrix(MatW);

  const fvec4& ScreenXNorm = pTARG->MTXI()->GetScreenRightNormal();

  const fvec4 V0 = MatW.GetTranslation();
  const fvec4 V1 = V0 + ScreenXNorm * float(30.0f);

  mfManipScale = float(mfBaseManipSize) * mfViewScale;

  //////////////////////////////////////////////////////////////

  if (0 == _material) {
    _material = new FreestyleMaterial;
    _material->gpuInit(pTARG, "orkshader://solid");
    _tek_modcolor = _material->technique("mmodcolor");
    _par_modcolor = _material->param("modcolor");
    _par_mvp      = _material->param("MatMVP");
    _material->_rasterstate.SetDepthTest(EDEPTHTEST_OFF);
    _material->_rasterstate.SetAlphaTest(EALPHATEST_OFF);
    _material->_rasterstate.SetCullTest(ECULLTEST_OFF);

    if (mpTXManip == 0)
      mpTXManip = new ManipTX(*this);
    if (mpTYManip == 0)
      mpTYManip = new ManipTY(*this);
    if (mpTZManip == 0)
      mpTZManip = new ManipTZ(*this);
    if (mpTXYManip == 0)
      mpTXYManip = new ManipTXY(*this);
    if (mpTXZManip == 0)
      mpTXZManip = new ManipTXZ(*this);
    if (mpTYZManip == 0)
      mpTYZManip = new ManipTYZ(*this);
    if (mpRXManip == 0)
      mpRXManip = new ManipRX(*this);
    if (mpRYManip == 0)
      mpRYManip = new ManipRY(*this);
    if (mpRZManip == 0)
      mpRZManip = new ManipRZ(*this);
  }
}

///////////////////////////////////////////////////////////////////////////////

void ManipManager::DrawManip(Manip* pmanip, Context* pTARG) {

  if (!pmanip)
    return;

  lev2::PickBuffer* pickBuf = pTARG->FBI()->currentPickBuffer();

  uint64_t pickID = pickBuf ? pickBuf->AssignPickId((ork::Object*)pmanip) : 0;
  fcolor4 col     = pmanip->GetColor();

  pTARG->SetCurrentObject(pmanip);
  // orkprintf( "MANIP<%p>\n", pmanip );

  if (pTARG->FBI()->isPickState()) {
    fvec4 asv4;
    asv4.SetRGBAU64(pickID);
    pTARG->PushModColor(asv4);
    pmanip->Draw(pTARG);
    pTARG->PopModColor();
  } else {
    fcolor4 outcolor = (GetHover() == pmanip) ? fcolor4::Yellow() : col;
    outcolor.SetW(0.6f);

    pTARG->PushModColor(outcolor);
    pmanip->Draw(pTARG);
    pTARG->PopModColor();
  }
}

///////////////////////////////////////////////////////////////////////////////

void ManipManager::DrawCurrentManipSet(Context* pTARG) {
  switch (meManipMode) {
    case EMANIPMODE_WORLD_TRANS: {
      if (mDualAxis) {
        // sm - dual axis disabled atm
        DrawManip(mpTXYManip, pTARG);
        DrawManip(mpTXZManip, pTARG);
        DrawManip(mpTYZManip, pTARG);
      } else {
        DrawManip(mpTXManip, pTARG);
        DrawManip(mpTYManip, pTARG);
        DrawManip(mpTZManip, pTARG);
      }
    } break;

    case EMANIPMODE_LOCAL_ROTATE: {
      DrawManip(mpRXManip, pTARG);
      DrawManip(mpRYManip, pTARG);
      DrawManip(mpRZManip, pTARG);
    } break;
  }
}

////////////////////////////////////////////////////////////////////////////////

static void ManipRenderCallback(const ork::lev2::RenderContextInstData& RCID) {
  auto context    = RCID.context();
  auto renderable = dynamic_cast<const lev2::CallbackRenderable*>(RCID._dagrenderable);
  auto manipmgr   = renderable->GetUserData0().Get<ManipManager*>();
  manipmgr->DrawCurrentManipSet(context);
}

void ManipManager::Queue(ork::lev2::IRenderer* prend) {
  if (mpCurrentInterface && mpCurrentObject) {
    CallbackRenderable::var_t ap;
    ap.Set<ManipManager*>(this);

    CallbackRenderable& rable = prend->enqueueCallback();
    rable.SetUserData0(ap);
    rable.SetSortKey(0x7fffffff);
    rable.SetRenderCallback(ManipRenderCallback);
  }
}
///////////////////////////////////////////////////////////////////////////////

void ManipManager::ApplyTransform(const TransformNode& SetMat) {
  mCurTransform = SetMat;

  if ((0 != mpCurrentInterface) && (0 != mpCurrentObject)) {
    mpCurrentInterface->SetTransform(mpCurrentObject, SetMat);
  }
}

///////////////////////////////////////////////////////////////////////////////

void ManipManager::DisableManip(void) {
  if (EMANIPMODE_ON == meManipEnable)
    ReleaseObject();

  orkprintf("Disable Manip\n");
  mpCurrentManip = 0;
  meManipEnable  = EMANIPMODE_OFF;
}

void ManipManager::EnableManip(Manip* pObj) {
  orkprintf("Enable Manip\n");
  mpCurrentManip = pObj;
  meManipEnable  = EMANIPMODE_ON;

  RebaseMatrices();
}
}} // namespace ork::lev2
