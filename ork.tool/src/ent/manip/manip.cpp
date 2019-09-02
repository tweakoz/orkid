////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
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
#include <ork/lev2/gfx/lev2renderer.h>
#include <ork/lev2/gfx/pickbuffer.h>
#include <ork/lev2/ui/event.h>
#include <ork/math/audiomath.h>
#include <ork/reflect/RegisterProperty.h>
#include <ork/rtti/downcast.h>

INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::CManip, "CManip");

INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::CManipTrans, "CManipTrans");
INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::CManipSingleTrans, "CManipSingleTrans");
INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::CManipDualTrans, "CManipDualTrans");
INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::CManipRot, "CManipRot");

INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::CManipTX, "CManipTX");
INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::CManipTY, "CManipTY");
INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::CManipTZ, "CManipTZ");
INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::CManipTXY, "CManipTXY");
INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::CManipTXZ, "CManipTXZ");
INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::CManipTYZ, "CManipTYZ");

INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::CManipRX, "CManipRX");
INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::CManipRY, "CManipRY");
INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::CManipRZ, "CManipRZ");

INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::CManipManager, "CManipManager");

////////////////////////////////////////////////////////////////////////////////
namespace ork { namespace lev2 {
////////////////////////////////////////////////////////////////////////////////

void CManip::Describe() {}

void CManipTrans::Describe() {}
void CManipSingleTrans::Describe() {}
void CManipDualTrans::Describe() {}
void CManipRot::Describe() {}

void CManipTX::Describe() {}
void CManipTY::Describe() {}
void CManipTZ::Describe() {}
void CManipTXY::Describe() {}
void CManipTXZ::Describe() {}
void CManipTYZ::Describe() {}

void CManipRX::Describe() {}
void CManipRY::Describe() {}
void CManipRZ::Describe() {}

////////////////////////////////////////////////////////////////////////////////

void CManipManager::Describe() {
  RegisterAutoSlot(CManipManager, ObjectDeleted);
  RegisterAutoSlot(CManipManager, ObjectSelected);
  RegisterAutoSlot(CManipManager, ObjectDeSelected);
  RegisterAutoSlot(CManipManager, ClearSelection);
}

CManipManager::CManipManager()
    : mpTXManip(0), mpTYManip(0), mpTZManip(0), mpTXYManip(0), mpTXZManip(0), mpTYZManip(0), mpRXManip(0), mpRYManip(0),
      mpRZManip(0), mpCurrentManip(0), mpHoverManip(0), meManipMode(EMANIPMODE_WORLD_TRANS), meManipEnable(EMANIPMODE_OFF),
      mbDoComponents(false), mfManipScale(1.0f), mfBaseManipSize(100.0f), mpCurrentInterface(0), mpCurrentObject(0),
      mbWorldTrans(false), mbGridSnap(false), mpManipMaterial(0), meUIMode(EUIMODE_STD), mDualAxis(false), mfViewScale(1.0f),
      ConstructAutoSlot(ObjectDeSelected), ConstructAutoSlot(ObjectSelected), ConstructAutoSlot(ObjectDeleted),
      ConstructAutoSlot(ClearSelection) {
  SetupSignalsAndSlots();
}

////////////////////////////////////////////////////////////////////////////////

void CManipManager::SlotObjectDeleted(ork::Object* pOBJ) {
  if (mpCurrentObject == pOBJ) {
    DetachObject();
  }
}
void CManipManager::SlotObjectSelected(ork::Object* pOBJ) {}
void CManipManager::SlotObjectDeSelected(ork::Object* pOBJ) {}
void CManipManager::SlotClearSelection() {}

////////////////////////////////////////////////////////////////////////////////

bool CManipManager::UIEventHandler(const ui::Event& EV) {
  bool rval = false;

  switch (EV.miEventCode) {
    case ui::UIEV_KEY:
    case ui::UIEV_KEYUP: {
      if (EV.mbSHIFT)
        mDualAxis = true;
      else
        mDualAxis = false;
    } break;
  }

  // printf( "CManipManager::UIEventHandler mpCurrentManip<%p>\n", mpCurrentManip );

  if (mpCurrentManip)
    rval = mpCurrentManip->UIEventHandler(EV);

  return rval;
}

////////////////////////////////////////////////////////////////////////////////

IntersectionRecord::IntersectionRecord()
    : mIntersectionPoint(0.0f, 0.0f, 0.0f), mOldIntersectionPoint(0.0f, 0.0f, 0.0f), mBaseIntersectionPoint(0.0f, 0.0f, 0.0f),
      mbHasItersected(false) {}

fvec4 IntersectionRecord::GetLocalSpaceDelta(const fmtx4& InvLocalMatrix) {
  return mIntersectionPoint.Transform(InvLocalMatrix) - mOldIntersectionPoint.Transform(InvLocalMatrix);
}

////////////////////////////////////////////////////////////////////////////////

CManip::CManip(CManipManager& mgr) : mManager(mgr), mActiveIntersection(0), mColor() {}

////////////////////////////////////////////////////////////////////////////////

bool CManip::CheckIntersect(void) const {
  bool bisect = (mActiveIntersection == nullptr) ? false : mActiveIntersection->mbHasItersected;
  // printf( "manip<%p> ai<%p> CheckIntersect<%d>\n", this, mActiveIntersection, int(bisect) );

  return bisect;
}

////////////////////////////////////////////////////////////////////////////////

void CManip::CalcPlanes() {
  fmtx4 CurMatrix;
  mBaseTransform.GetMatrix(CurMatrix);
  fvec4 origin = CurMatrix.GetTranslation();
  fvec4 normalX = CurMatrix.GetXNormal();
  fvec4 normalY = CurMatrix.GetYNormal();
  fvec4 normalZ = CurMatrix.GetZNormal();
  /////////////////////////////
  if (mManager.mbWorldTrans && (CManipManager::EMANIPMODE_WORLD_TRANS == mManager.GetManipMode())) {
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

fvec3 CManip::IntersectWithPlanes(const ork::fvec2& posubp) {
  fvec3 rval;

  const CCamera* cam = mManager.getActiveCamera();
  fmtx4 CurMatrix;
  mBaseTransform.GetMatrix(CurMatrix);
  /////////////////////////////
  fvec3 rayN, rayF;
  cam->GenerateDepthRay(posubp, rayN, rayF, InvMatrix);
  fvec4 rayDir = (rayF - rayN).Normal();
  fray3 ray;
  ray.mOrigin = rayN;
  ray.mDirection = rayDir;
  /////////////////////////////
  float dist;
  mIntersection[EPLANE_XZ].mbHasItersected = mPlaneXZ.Intersect(ray, dist);
  fvec4 rayOutXZ = (rayDir * dist);
  mIntersection[EPLANE_YZ].mbHasItersected = mPlaneYZ.Intersect(ray, dist);
  fvec4 rayOutYZ = (rayDir * dist);
  mIntersection[EPLANE_XY].mbHasItersected = mPlaneXY.Intersect(ray, dist);
  fvec4 rayOutXY = (rayDir * dist);
  /////////////////////////////
  mIntersection[EPLANE_XZ].mIntersectionPoint = rayN + rayOutXZ;
  mIntersection[EPLANE_YZ].mIntersectionPoint = rayN + rayOutYZ;
  mIntersection[EPLANE_XY].mIntersectionPoint = rayN + rayOutXY;
  /////////////////////////////
  rval = rayDir.xyz();
  return rval;
}

////////////////////////////////////////////////////////////////////////////////

void CManip::SelectBestPlane(const ork::fvec2& posubp) {
  bool brotmode = (mManager.GetManipMode() == CManipManager::EMANIPMODE_LOCAL_ROTATE);

  CalcPlanes();
  fvec3 RayDir = IntersectWithPlanes(posubp);
  /////////////////////////////
  mIntersection[EPLANE_XZ].mOldIntersectionPoint = mIntersection[EPLANE_XZ].mIntersectionPoint;
  mIntersection[EPLANE_YZ].mOldIntersectionPoint = mIntersection[EPLANE_YZ].mIntersectionPoint;
  mIntersection[EPLANE_XY].mOldIntersectionPoint = mIntersection[EPLANE_XY].mIntersectionPoint;
  mIntersection[EPLANE_XZ].mBaseIntersectionPoint = mIntersection[EPLANE_XZ].mIntersectionPoint;
  mIntersection[EPLANE_YZ].mBaseIntersectionPoint = mIntersection[EPLANE_YZ].mIntersectionPoint;
  mIntersection[EPLANE_XY].mBaseIntersectionPoint = mIntersection[EPLANE_XY].mIntersectionPoint;
  /////////////////////////////
  // rot manips use explicit planes
  /////////////////////////////
  if (brotmode) {
    if (mManager.mpCurrentManip->GetClass() == CManipRX::GetClassStatic()) {
      mActiveIntersection = &mIntersection[EPLANE_YZ];
    } else if (mManager.mpCurrentManip->GetClass() == CManipRY::GetClassStatic()) {
      mActiveIntersection = &mIntersection[EPLANE_XZ];
    } else if (mManager.mpCurrentManip->GetClass() == CManipRZ::GetClassStatic()) {
      mActiveIntersection = &mIntersection[EPLANE_XY];
    }
  } else {
    float dotxz = RayDir.Dot(mPlaneXZ.GetNormal());
    float dotxy = RayDir.Dot(mPlaneXY.GetNormal());
    float dotyz = RayDir.Dot(mPlaneYZ.GetNormal());
    float adotxz = CFloat::Abs(dotxz);
    float adotxy = CFloat::Abs(dotxy);
    float adotyz = CFloat::Abs(dotyz);

    // printf( "mManager.mpCurrentManip<%p>\n", mManager.mpCurrentManip );

    if (mManager.mpCurrentManip->GetClass() == CManipTX::GetClassStatic()) {
      mActiveIntersection = (adotxy > adotxz) ? &mIntersection[EPLANE_XZ] : &mIntersection[EPLANE_XY];
    } else if (mManager.mpCurrentManip->GetClass() == CManipTY::GetClassStatic()) {
      mActiveIntersection = (adotxy > adotyz) ? &mIntersection[EPLANE_XY] : &mIntersection[EPLANE_YZ];
    } else if (mManager.mpCurrentManip->GetClass() == CManipTZ::GetClassStatic()) {
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

void CManipManager::RebaseMatrices(void) {
  if (mpCurrentInterface && mpCurrentObject) {
    TransformNode Mat = mpCurrentInterface->GetTransform(mpCurrentObject);
    TransformNode MatT = mpCurrentInterface->GetTransform(mpCurrentObject);
    mCurTransform = Mat;
    mOldTransform = Mat;
    CalcObjectScale();
  }
}

///////////////////////////////////////////////////////////////////////////////

void CManipManager::AttachObject(ork::Object* pobj) {
  object::ObjectClass* pclass = rtti::safe_downcast<object::ObjectClass*>(pobj->GetClass());
  // CClass *pClass = pOBJ->GetClass();

  if (mpCurrentObject && pobj != mpCurrentObject) {
    delete mpCurrentInterface;
    mpCurrentInterface = 0;
  }

  auto anno = pclass->Description().GetClassAnnotation("editor.3dxfinterface");

  if (auto as_cstr = anno.TryAs<ConstString>()) {
    ConstString classname = as_cstr.value();
    const rtti::Class* pifclass = rtti::autocast(rtti::Class::FindClass(classname));

    if (pifclass) {
      mpCurrentInterface = rtti::autocast(pifclass->CreateObject());
      mpCurrentObject = pobj;
      RebaseMatrices();
    } else {
      mpCurrentInterface = 0;
      mpCurrentObject = 0;
    }
  }
}

///////////////////////////////////////////////////////////////////////////////

void CManipManager::DetachObject() {
  mpCurrentInterface = 0;
  mpCurrentObject = 0;
  meManipEnable = EMANIPMODE_OFF;
}

///////////////////////////////////////////////////////////////////////////////

void CManipManager::CalcObjectScale(void) {
  fvec3 pos;
  CQuaternion rot;
  float scale;
  fmtx4 ScaleMat;
  mCurTransform.GetMatrix(ScaleMat);
  ScaleMat.DecomposeMatrix(pos, rot, scale);

  mObjScale = scale;
  mObjInvScale = float(1.0f) / mObjScale;
}

///////////////////////////////////////////////////////////////////////////////

void CManipManager::ReleaseObject(void) {
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

float CManipManager::CalcViewScale(float fW, float fH, const CCameraData* camdat) const {
  fmtx4 MatW;
  mCurTransform.GetMatrix(MatW);

  //////////////////////////////////////////////////////////////
  // Calc World Scale of manip (maintain constant size)

  fvec2 VP(fW, fH);

  fvec3 Pos = MatW.GetTranslation();
  fvec3 UpVector;
  fvec3 RightVector;
  camdat->GetPixelLengthVectors(Pos, VP, UpVector, RightVector);

  float rscale = RightVector.Mag();

  // printf( "manip rscale<%f>\n", rscale );

  //////////////////////////////////////////////////////////////

  return rscale;
}

///////////////////////////////////////////////////////////////////////////////

void CManipManager::Setup(ork::lev2::Renderer* prend) {
  GfxTarget* pTARG = prend->GetTarget();

  if (mpCurrentInterface) {
    bool isshift = false; // CSystem::IsKeyDepressed(VK_SHIFT);
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

  if (0 == mpManipMaterial) {
    mpManipMaterial = new GfxMaterialManip(pTARG, *this);
    mpManipMaterial->mRasterState.SetDepthTest(EDEPTHTEST_OFF);

    if (mpTXManip == 0)
      mpTXManip = new CManipTX(*this);
    if (mpTYManip == 0)
      mpTYManip = new CManipTY(*this);
    if (mpTZManip == 0)
      mpTZManip = new CManipTZ(*this);
    if (mpTXYManip == 0)
      mpTXYManip = new CManipTXY(*this);
    if (mpTXZManip == 0)
      mpTXZManip = new CManipTXZ(*this);
    if (mpTYZManip == 0)
      mpTYZManip = new CManipTYZ(*this);
    if (mpRXManip == 0)
      mpRXManip = new CManipRX(*this);
    if (mpRYManip == 0)
      mpRYManip = new CManipRY(*this);
    if (mpRZManip == 0)
      mpRZManip = new CManipRZ(*this);
  }
}

///////////////////////////////////////////////////////////////////////////////

void CManipManager::DrawManip(CManip* pmanip, GfxTarget* pTARG) {
  if (!pmanip)
    return;

  lev2::PickBufferBase* pickBuf = pTARG->FBI()->GetCurrentPickBuffer();

  uint64_t pickID = pickBuf ? pickBuf->AssignPickId((ork::Object*)pmanip) : 0;
  fcolor4 col = pmanip->GetColor();

  pTARG->SetCurrentObject(pmanip);
  // orkprintf( "MANIP<%p>\n", pmanip );

  if (pTARG->FBI()->IsPickState()) {
    fvec4 asv4; asv4.SetRGBAU64(pickID);
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

void CManipManager::DrawCurrentManipSet(GfxTarget* pTARG) {
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

static void ManipRenderCallback(ork::lev2::RenderContextInstData& rcid, ork::lev2::GfxTarget* targ,
                                const ork::lev2::CallbackRenderable* pren) {
  CManipManager* pmanipman = pren->GetUserData0().Get<CManipManager*>();
  pmanipman->SetDrawMode(0);
  pmanipman->DrawCurrentManipSet(targ);
}

void CManipManager::Queue(ork::lev2::Renderer* prend) {
  if (mpCurrentInterface && mpCurrentObject) {
    anyp ap;
    ap.Set<CManipManager*>(this);

    CallbackRenderable& rable = prend->QueueCallback();
    rable.SetUserData0(ap);
    rable.SetSortKey(0x7fffffff);
    rable.SetRenderCallback(ManipRenderCallback);
  }
}
///////////////////////////////////////////////////////////////////////////////

void CManipManager::ApplyTransform(const TransformNode& SetMat) {
  mCurTransform = SetMat;

  if ((0 != mpCurrentInterface) && (0 != mpCurrentObject)) {
    mpCurrentInterface->SetTransform(mpCurrentObject, SetMat);
  }
}

///////////////////////////////////////////////////////////////////////////////

void CManipManager::DisableManip(void) {
  if (EMANIPMODE_ON == meManipEnable)
    ReleaseObject();

  orkprintf("Disable Manip\n");
  mpCurrentManip = 0;
  meManipEnable = EMANIPMODE_OFF;
}

void CManipManager::EnableManip(CManip* pObj) {
  orkprintf("Enable Manip\n");
  mpCurrentManip = pObj;
  meManipEnable = EMANIPMODE_ON;

  RebaseMatrices();
}
}} // namespace ork::lev2
