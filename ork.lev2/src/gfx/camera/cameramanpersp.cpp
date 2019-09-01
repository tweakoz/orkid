////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#include <ork/util/stl_ext.h>

#include <ork/lev2/gfx/ctxbase.h>
#include <ork/lev2/gfx/gfxenv.h>

#include <ork/lev2/gfx/gfxprimitives.h>
//
#include <math.h>
#include <ork/file/tinyxml/tinyxml.h>
#include <ork/kernel/string/string.h>
#include <ork/lev2/gfx/camera/cameraman.h>
#include <ork/lev2/gfx/dbgfontman.h>
#include <ork/lev2/ui/touch.h>
#include <ork/lev2/ui/viewport.h>
#include <ork/math/basicfilters.h>
#include <ork/math/misc_math.h>

#include <QtGui/QCursor>

INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::CCamera_persp, "CCamera_persp");

using namespace ork::ui;

namespace ork { namespace lev2 {

	void OrkGlobalDisableMousePointer();
	void OrkGlobalEnableMousePointer();

///////////////////////////////////////////////////////////////////////////////

void CCamera_persp::Describe() {
  ork::reflect::RegisterProperty("Aperature", &CCamera_persp::aper);
  ork::reflect::AnnotatePropertyForEditor<CCamera_persp>("Aperature", "editor.range.min", "0.0f");
  ork::reflect::AnnotatePropertyForEditor<CCamera_persp>("Aperature", "editor.range.max", "90.0f");

  ork::reflect::RegisterProperty("MaxFar", &CCamera_persp::far_max);
  ork::reflect::AnnotatePropertyForEditor<CCamera_persp>("MaxFar", "editor.range.min", "1.0f");
  ork::reflect::AnnotatePropertyForEditor<CCamera_persp>("MaxFar", "editor.range.max", "100000.0f");

  ork::reflect::RegisterProperty("MinNear", &CCamera_persp::near_min);
  ork::reflect::AnnotatePropertyForEditor<CCamera_persp>("MinNear", "editor.range.min", "0.1f");
  ork::reflect::AnnotatePropertyForEditor<CCamera_persp>("MinNear", "editor.range.max", "10000.0f");
}

///////////////////////////////////////////////////////////////////////////////

CCamera_persp::CCamera_persp()
    : CCamera(), aper(40.0f), far_max(10000.0f), near_min(0.1f), tx(0.0f), ty(0.0f), tz(0.0f), player_rx(0.0f), player_ry(0.0f),
      player_rz(0.0f), move_vel(0.0f), mMoveVelocity(0.0f, 0.0f, 0.0f), meRotMode(EROT_SCREENXY), mDoDolly(false), mDoRotate(false),
      mDoPan(false) {
  // InitInstance(CCamera_persp::GetClassStatic());
  mCameraData.Persp(1.0f, 1000.0f, 70.0f);
  mCameraData.Lookat(fvec3(0.0f, 0.0f, 0.0f), fvec3(0.0f, 0.0f, 1.0f), fvec3(0.0f, 1.0f, 0.0f));
  // mCameraData.SetFar( 1000.0f );
  type_name = "Perspective";
  instance_name = "Default";

  leftbutton = false;
  middlebutton = false;
  rightbutton = false;

  mfWorldSizeAtLocator = 150.0f;

  mHK_AperPlus = HotKeyManager::GetHotKey("camera_aper+");
  mHK_AperMinus = HotKeyManager::GetHotKey("camera_aper-");

  mHK_Origin = HotKeyManager::GetHotKey("camera_origin");
  mHK_ReAlign = HotKeyManager::GetHotKey("camera_realign");

  mHK_Pik2Foc = HotKeyManager::GetHotKey("camera_pick2focus");
  mHK_Foc2Pik = HotKeyManager::GetHotKey("camera_focus2pick");

  mHK_In = HotKeyManager::GetHotKey("camera_in");
  mHK_Out = HotKeyManager::GetHotKey("camera_out");

  mHK_MovL = HotKeyManager::GetHotKey("camera_left");
  mHK_MovR = HotKeyManager::GetHotKey("camera_right");
  mHK_MovF = HotKeyManager::GetHotKey("camera_fwd");
  mHK_MovB = HotKeyManager::GetHotKey("camera_bwd");
  mHK_MovU = HotKeyManager::GetHotKey("camera_up");
  mHK_MovD = HotKeyManager::GetHotKey("camera_down");

  mHK_RotZ = HotKeyManager::GetHotKey("camera_z");
  mHK_RotL = HotKeyManager::GetHotKey("camera_rotl");
  mHK_RotR = HotKeyManager::GetHotKey("camera_rotr");
  mHK_RotU = HotKeyManager::GetHotKey("camera_rotu");
  mHK_RotD = HotKeyManager::GetHotKey("camera_rotd");

  mHK_MouseRot = HotKeyManager::GetHotKey("camera_mouse_rot");
}

///////////////////////////////////////////////////////////////////////////////

void CCamera_persp::draw(GfxTarget* pT) {
  extern fvec4 TRayN;
  extern fvec4 TRayF;

  CMatrix4 MatP, MatV, MatT;

  float CurVelMag = MeasuredCameraVelocity.Mag();

  //////////////////////////////////////
  pT->MTXI()->PushUIMatrix();
  {

    pT->PushModColor(CColor4::Black());
    ork::lev2::CFontMan::PushFont("i14");
    CFontMan::BeginTextBlock(pT);
    CFontMan::DrawText(pT, 41, 9, "CamFocus %f %f %f", mvCenter.GetX(), mvCenter.GetY(), mvCenter.GetZ());
    CFontMan::DrawText(pT, 41, 21, "CamLoc   %f %f %f", CamLoc.GetX(), CamLoc.GetY(), CamLoc.GetZ());
    CFontMan::DrawText(pT, 41, 33, "zf %f", (mCameraData.GetFar()));
    CFontMan::DrawText(pT, 41, 45, "zn %f", (mCameraData.GetNear()));
    CFontMan::DrawText(pT, 41, 57, "zfoverzn %f", (mCameraData.GetFar() / mCameraData.GetNear()));
    CFontMan::DrawText(pT, 41, 69, "Loc(m) %f Speed(m/f) %f", mfLoc, CurVelMag);
    CFontMan::DrawText(pT, 41, 81, "RotMode %s", (meRotMode == EROT_SCREENZ) ? "ScreenZ" : "ScreenXY");
    CFontMan::DrawText(pT, 41, 93, "Aper %f", aper);
    CFontMan::DrawText(pT, 41, 105, "Name %s", GetName().c_str());
    CFontMan::EndTextBlock(pT);
    pT->PopModColor();

    pT->PushModColor(CColor4::Yellow());
    CFontMan::BeginTextBlock(pT);
    CFontMan::DrawText(pT, 41, 9, "CamFocus %f %f %f", mvCenter.GetX(), mvCenter.GetY(), mvCenter.GetZ());
    CFontMan::DrawText(pT, 41, 21, "CamLoc   %f %f %f", CamLoc.GetX(), CamLoc.GetY(), CamLoc.GetZ());
    CFontMan::DrawText(pT, 41, 33, "zf %f", (mCameraData.GetFar()));
    CFontMan::DrawText(pT, 41, 45, "zn %f", (mCameraData.GetNear()));
    CFontMan::DrawText(pT, 41, 57, "zfoverzn %f", (mCameraData.GetFar() / mCameraData.GetNear()));
    CFontMan::DrawText(pT, 41, 69, "Loc(m) %f Speed(m/f) %f", mfLoc, CurVelMag);
    CFontMan::DrawText(pT, 41, 81, "RotMode %s", (meRotMode == EROT_SCREENZ) ? "ScreenZ" : "ScreenXY");
    CFontMan::DrawText(pT, 41, 93, "Aper %f", aper);
    CFontMan::DrawText(pT, 41, 105, "Name %s", GetName().c_str());
    CFontMan::EndTextBlock(pT);
    ork::lev2::CFontMan::PopFont();
    pT->PopModColor();
  }
  pT->MTXI()->PopUIMatrix();
  ///////////////////////////////////////////////////////////////
  // printf( "CAMHUD\n" );
  CCameraData camdat = mCameraData;
  camdat.BindGfxTarget(pT);
  camdat.SetVisibilityCamDat(0);
  CameraCalcContext ctx;
  camdat.CalcCameraData(ctx);
  //////////////////////////////////////////
  // this is necessary to get UI based rotations working correctly
  //////////////////////////////////////////
  mCameraData = camdat;
  // FrameData.SetCameraData( pcamdata );
  ///////////////////////////////////////////////////////////////

  // MatP =camdat.GetPMatrix();
  // MatV = camdat.GetVMatrix();
  // pT->MTXI()->PushPMatrix( MatP );
  // pT->MTXI()->PushVMatrix( MatV );
  pT->BindMaterial(GfxEnv::GetDefault3DMaterial());
  {
    LengthReal lux(mvCenter.GetX(), LengthUnit::Meters());
    LengthReal luy(mvCenter.GetY(), LengthUnit::Meters());
    LengthReal luz(mvCenter.GetZ(), LengthUnit::Meters());

    fvec3 vt;
    vt.SetX(lux.GetValue(LengthUnit::GetProjectUnits()));
    vt.SetY(luy.GetValue(LengthUnit::GetProjectUnits()));
    vt.SetZ(luz.GetValue(LengthUnit::GetProjectUnits()));

    MatT.SetTranslation(vt);
    float Scale = mfLoc / 30.0f;
    MatT.Scale(fvec4(Scale, Scale, Scale));
    pT->MTXI()->PushMMatrix(MatT);
    {
      CGfxPrimitives::GetRef().RenderTriCircle(pT);
      CGfxPrimitives::GetRef().RenderAxis(pT);

      // pT->IMI()->QueFlush();
      pT->PushModColor(CColor4::Red());
      // pT->IMI()->DrawLine( fvec4( float(0.0f), float(0.0f), float(0.0f) ), fvec4( float(10.0f), float(0.0f), float(0.0f) ) );
      // pT->IMI()->QueFlush();
      pT->PopModColor();
      pT->PushModColor(CColor4::Green());
      // pT->IMI()->DrawLine( fvec4( float(0.0f), float(0.0f), float(0.0f) ), fvec4( float(0.0f), float(10.0f), float(0.0f) ) );
      // pT->IMI()->QueFlush();
      pT->PopModColor();
      pT->PushModColor(CColor4::Blue());
      // pT->IMI()->DrawLine( fvec4( float(0.0f), float(0.0f), float(0.0f) ), fvec4( float(0.0f), float(0.0f), float(10.0f) ) );
      // pT->IMI()->QueFlush();
      pT->PopModColor();

      // pT->IMI()->QueFlush();
      pT->PushModColor(CColor4::Red() * float(0.5f));
      // pT->IMI()->DrawLine( fvec4( float(-10.0f), float(0.0f), float(0.0f) ), fvec4( float(0.0f), float(0.0f), float(0.0f) ) );
      // pT->IMI()->QueFlush();
      pT->PopModColor();
      pT->PushModColor(CColor4::Green() * float(0.5f));
      // pT->IMI()->DrawLine( fvec4( float(0.0f), float(-10.0f), float(0.0f) ), fvec4( float(0.0f), float(0.0f), float(0.0f) ) );
      // pT->IMI()->QueFlush();
      pT->PopModColor();
      pT->PushModColor(CColor4::Blue() * float(0.5f));
      // pT->IMI()->DrawLine( fvec4( float(0.0f), float(0.0f), float(-10.0f) ), fvec4( float(0.0f), float(0.0f), float(0.0f) ) );
      // pT->IMI()->QueFlush();
      pT->PopModColor();
    }
    pT->MTXI()->PopMMatrix();

    ///////////////////////////////
    lux.SetValue(CamFocus.GetX(), LengthUnit::Meters());
    luy.SetValue(CamFocus.GetY(), LengthUnit::Meters());
    luz.SetValue(CamFocus.GetZ(), LengthUnit::Meters());
    vt.SetX(lux.GetValue(LengthUnit::GetProjectUnits()));
    vt.SetY(luy.GetValue(LengthUnit::GetProjectUnits()));
    vt.SetZ(luz.GetValue(LengthUnit::GetProjectUnits()));

    MatT.SetTranslation(vt);
    pT->MTXI()->PushMMatrix(MatT);
    { CGfxPrimitives::GetRef().RenderTriCircle(pT); }
    pT->MTXI()->PopMMatrix();
    ///////////////////////////////
  }
  // pT->MTXI()->PopVMatrix();
  // pT->MTXI()->PopPMatrix();
}

///////////////////////////////////////////////////////////////////////////////

static QPoint pmousepos;

///////////////////////////////////////////////////////////////////////////////

void CCamera_persp::PanBegin(const CamEvTrackData& ed) {
  printf("BeginPan\n");
  pmousepos = QCursor::pos();
  //OrkGlobalDisableMousePointer();
  mDoPan = true;
}

///////////////////////////////////////////////////////////////////////////////

void CCamera_persp::PanUpdate(const CamEvTrackData& ed) {
  assert(mDoPan);

  int esx = ed.icurX;
  int esy = ed.icurY;
  int ipushx = ed.ipushX;
  int ipushy = ed.ipushY;

  CVector2 VP(float(mpViewport->GetW()), float(mpViewport->GetH()));

  fvec3 outx, outy;

  mCameraData.GetPixelLengthVectors(mvCenter, VP, outx, outy);

  float fvl = ViewLengthToWorldLength(mvCenter, 1.0f);
  float fdx = float(esx - ipushx);
  float fdy = float(esy - ipushy);

  mvCenter = ed.vPushCenter - (outx * fdx) - (outy * fdy);

  //QCursor::setPos(pmousepos);
}

///////////////////////////////////////////////////////////////////////////////

void CCamera_persp::PanEnd() {
  printf("EndPan\n");
  //QCursor::setPos(pmousepos);
  //OrkGlobalEnableMousePointer();
  mDoPan = false;
}

///////////////////////////////////////////////////////////////////////////////

static fvec4 vPushNZ, vPushNX, vPushNY;

void CCamera_persp::RotBegin(const CamEvTrackData& ed) {
  printf("BeginRot\n");
  vPushNX = mCameraData.GetXNormal();
  vPushNY = mCameraData.GetYNormal();
  vPushNZ = mCameraData.GetZNormal();
  pmousepos = QCursor::pos();
  //OrkGlobalDisableMousePointer();
  mDoRotate = true;
}
void CCamera_persp::RotUpdate(const CamEvTrackData& ed) {
  static int gupdatec = 0;
  printf("RotUpdate<%d>\n", gupdatec++);
  fvec4 RotX = mCameraData.GetXNormal();
  fvec4 RotY = mCameraData.GetYNormal();
  fvec4 RotZ = mCameraData.GetZNormal();

  assert(mDoRotate);

  int esx = ed.icurX;
  int esy = ed.icurY;
  int ipushx = ed.ipushX;
  int ipushy = ed.ipushY;

  CVector2 VP(float(mpViewport->GetW()), float(mpViewport->GetH()));

  fvec3 outx, outy;

  mCameraData.GetPixelLengthVectors(mvCenter, VP, outx, outy);

  float fvl = ViewLengthToWorldLength(mvCenter, 1.0f);
  float fdx = float(esx - ipushx);
  float fdy = float(esy - ipushy);

  fdx *= 0.5f;
  fdy *= 0.5f;

  float frotamt = 0.005f;

  if (mfWorldSizeAtLocator < float(0.5f))
    frotamt = 0.001f;

  fdx *= frotamt;
  fdy *= frotamt;

  switch (meRotMode) {
    case EROT_SCREENZ: {
      float fvpx = float(mpViewport->GetX());
      float fvpy = float(mpViewport->GetY());
      float fvpwd2 = float(mpViewport->GetW() / 2);
      float fvphd2 = float(mpViewport->GetH() / 2);
      float fipx = float(ipushx);
      float fipy = float(ipushy);
      float fesx = float(esx);
      float fesy = float(esy);

      float fx0 = (fipx - fvpx) - fvpwd2;
      float fy0 = (fipy - fvpy) - fvphd2;
      float fx1 = (fesx - fvpx) - fvpwd2;
      float fy1 = (fesy - fvpy) - fvphd2;
      fvec4 v0(fx0, fy0, 0.0f);
      fvec4 v1(fx1, fy1, 0.0f);
      v0.Normalize();
      v1.Normalize();
      float ang0 = rect2pol_ang(v0.GetX(), v0.GetY());
      float ang1 = rect2pol_ang(v1.GetX(), v1.GetY());
      float dangle = (ang1 - ang0);
      fquat QuatZ;
      vPushNZ.SetW(dangle);
      QuatZ.FromAxisAngle(vPushNZ);
      QuatC = QuatZ.Multiply(ManipHandler.Quat);

      break;
    }
    case EROT_SCREENXY: {

      RotY.SetW(-fdx);
      fquat QuatY;
      QuatY.FromAxisAngle(RotY);
      QuatC = QuatY.Multiply(QuatC);

      RotX.SetW(fdy);
      fquat QuatX;
      QuatX.FromAxisAngle(RotX);
      QuatC = QuatX.Multiply(QuatC);

      break;
    }
  }
  //QCursor::setPos(pmousepos);
}
void CCamera_persp::RotEnd() {
  printf("EndRot\n");
  //QCursor::setPos(pmousepos);
  //OrkGlobalEnableMousePointer();
  mDoRotate = false;
}

///////////////////////////////////////////////////////////////////////////////

void CCamera_persp::DollyBegin(const CamEvTrackData& ed) {
  printf("BeginDolly\n");
  pmousepos = QCursor::pos();
  //OrkGlobalDisableMousePointer();
  mDoDolly = true;
}
void CCamera_persp::DollyUpdate(const CamEvTrackData& ed) {
  int esx = ed.icurX;
  int esy = ed.icurY;
  int ipushx = ed.ipushX;
  int ipushy = ed.ipushY;

  assert(mDoDolly);
  CVector2 VP(float(mpViewport->GetW()), float(mpViewport->GetH()));

  fvec3 outx, outy;

  float fux = float(esx) / float(mpViewport->GetW());

  mCameraData.GetPixelLengthVectors(mvCenter, VP, outx, outy);

  float fvl = ViewLengthToWorldLength(mvCenter, 1.0f);
  float fdx = float(esx - ipushx);
  float fdy = float(esy - ipushy);

  float fdolly = (outx.Mag() * -fdy * 3.0f * powf(fux, 1.5f));

  fvec4 RotX = mCameraData.GetXNormal();
  fvec4 RotY = mCameraData.GetYNormal();
  fvec4 RotZ = mCameraData.GetZNormal();

  fvec4 MoveVec = RotZ * fdolly;

  if (mpViewport->IsHotKeyDepressed("camera_y"))
    MoveVec = RotY * fdolly;
  else if (mpViewport->IsHotKeyDepressed("camera_x"))
    MoveVec = RotX * fdolly;

  mvCenter += MoveVec;

  //QCursor::setPos(pmousepos);
}
void CCamera_persp::DollyEnd() {
  printf("EndDolly\n");
  //QCursor::setPos(pmousepos);
  //OrkGlobalEnableMousePointer();
  mDoDolly = false;
}

///////////////////////////////////////////////////////////////////////////////

bool CCamera_persp::UIEventHandler(const ui::Event& EV) {
  const ui::EventCooked& filtev = EV.mFilteredEvent;

  ui::Viewport* pVP = GetViewport();

  int esx = filtev.miX;
  int esy = filtev.miY;
  float fux = filtev.mUnitX;
  float fuy = filtev.mUnitY;
  float fpux = (fux * 2.0f) - 1.0f;
  float fpuy = (fuy * 2.0f) - 1.0f;

  CVector2 pos2D(fpux, fpuy);

  int state = 0;
  bool isctrl = filtev.mCTRL;
  bool isalt = filtev.mALT;
  bool isshift = filtev.mSHIFT;
  bool ismeta = filtev.mMETA;
  bool isleft = filtev.mBut0;
  bool ismid = filtev.mBut1;
  bool isright = filtev.mBut2;

  bool bcamL = HotKeyManager::IsDepressed(mHK_MovL);
  bool bcamR = HotKeyManager::IsDepressed(mHK_MovR);
  bool bcamU = HotKeyManager::IsDepressed(mHK_MovU);
  bool bcamD = HotKeyManager::IsDepressed(mHK_MovD);

  static int ipushx = 0;
  static int ipushy = 0;
  static f32 flerp = 0.0f;

  fvec4 RotX = mCameraData.GetXNormal();
  fvec4 RotY = mCameraData.GetYNormal();
  fvec4 RotZ = mCameraData.GetZNormal();

  switch (filtev.miEventCode) {
    case UIEV_PUSH: {
      if (pVP == 0)
        break;

      float fx = float(esx) / float(pVP->GetW()) - 0.5f;
      float fy = float(esy) / float(pVP->GetH()) - 0.5f;
      float frad = ork::sqrtf((fx * fx) + (fy * fy));

      meRotMode = (frad > 0.35f) ? EROT_SCREENZ : EROT_SCREENXY;

      CommonPostSetup();

      fvec3 vrn, vrf;

      GenerateDepthRay(pos2D, vrn, vrf, mCameraData.GetIVPMatrix());

      if (isleft || isright) {
        ////////////////////////////////////////////////////////
        // calculate planes with world rotation, but current view target as origin

        fvec4 Origin = mvCenter;
        ManipHandler.Init(pos2D, mCameraData.GetIVPMatrix(), QuatC);
      }
      //////////////////////////////////////////////////

      beginx = esx;
      beginy = esy;
      ipushx = esx;
      ipushy = esy;

      leftbutton = filtev.mBut0;
      middlebutton = filtev.mBut1;
      rightbutton = filtev.mBut2;

      vPushNX = mCameraData.GetXNormal();
      vPushNY = mCameraData.GetYNormal();
      vPushNZ = mCameraData.GetZNormal();

      bool filt_kpush = (filtev.mAction == "keypush");

      bool do_rot = (isleft && (isalt || filt_kpush));
      bool do_pan = (ismid && (isalt || filt_kpush));

      if (do_rot) {
        mEvTrackData.vPushCenter = mvCenter;
        mEvTrackData.ipushX = ipushx;
        mEvTrackData.ipushY = ipushy;
        RotBegin(mEvTrackData);
      } else if (do_pan) {
        mEvTrackData.vPushCenter = mvCenter;
        mEvTrackData.ipushX = ipushx;
        mEvTrackData.ipushY = ipushy;
        PanBegin(mEvTrackData);
      }

      break;
    }
    case UIEV_RELEASE: {
      CommonPostSetup();
      // could do fall-through or maybe even but this outside of switch
      leftbutton = filtev.mBut0;
      middlebutton = filtev.mBut1;
      rightbutton = filtev.mBut2;

      if (mDoDolly)
        DollyEnd();
      if (mDoPan)
        PanEnd();
      if (mDoRotate)
        RotEnd();

      mDoPan = false;
      mDoDolly = false;
      mDoRotate = false;

      break;
    }
    case UIEV_MOVE: {
      if (0 == pVP)
        break;

      float fx = float(esx) / float(pVP->GetW()) - 0.5f;
      float fy = float(esy) / float(pVP->GetH()) - 0.5f;
      float frad = ork::sqrtf((fx * fx) + (fy * fy));

      meRotMode = (frad > 0.35f) ? EROT_SCREENZ : EROT_SCREENXY;

      break;
    }
    case UIEV_DRAG: {
      //////////////////////////////////////////////////
      // intersect ray with worlds XZ/XY/YZ planes

      ManipHandler.Intersect(pos2D);

      float dx = float(esx - beginx);
      float dy = float(esy - beginy);

      // input  1 . 10 . 100
      // output .1  10 . 1000

      float flogdx = log10(fabs(float(dx)));
      float flogdy = log10(fabs(float(dy)));

      flogdx = (flogdx * float(1.5f)) - float(0.5f);
      flogdy = (flogdy * float(1.5f)) - float(0.5f);

      dx = float(CFloat::Pow(float(10.0f), flogdx)) * ((dx < float(0.0f)) ? float(-1.0f) : float(1.0f));
      dy = float(CFloat::Pow(float(10.0f), flogdy)) * ((dy < float(0.0f)) ? float(-1.0f) : float(1.0f));

      //////////////////////////////////////////////////

      float tmult = 1.0f;

      //////////////////////////////////////////////////
      if (mDoRotate) {
        dx *= 0.5f;
        dy *= 0.5f;

        float frotamt = 0.005f;

        if (mfWorldSizeAtLocator < float(0.5f))
          frotamt = 0.001f;

        dx *= frotamt;
        dy *= frotamt;

        switch (meRotMode) {
          case EROT_SCREENZ: {
            float fvpx = float(pVP->GetX());
            float fvpy = float(pVP->GetY());
            float fvpwd2 = float(pVP->GetW() / 2);
            float fvphd2 = float(pVP->GetH() / 2);
            float fipx = float(ipushx);
            float fipy = float(ipushy);
            float fesx = float(esx);
            float fesy = float(esy);

            float fx0 = (fipx - fvpx) - fvpwd2;
            float fy0 = (fipy - fvpy) - fvphd2;
            float fx1 = (fesx - fvpx) - fvpwd2;
            float fy1 = (fesy - fvpy) - fvphd2;
            fvec4 v0(fx0, fy0, 0.0f);
            fvec4 v1(fx1, fy1, 0.0f);
            v0.Normalize();
            v1.Normalize();
            float ang0 = rect2pol_ang(v0.GetX(), v0.GetY());
            float ang1 = rect2pol_ang(v1.GetX(), v1.GetY());
            float dangle = (ang1 - ang0);
            fquat QuatZ;
            vPushNZ.SetW(dangle);
            QuatZ.FromAxisAngle(vPushNZ);
            QuatC = QuatZ.Multiply(ManipHandler.Quat);

            break;
          }
          case EROT_SCREENXY: {

            RotY.SetW(-dx);
            fquat QuatY;
            QuatY.FromAxisAngle(RotY);
            QuatC = QuatY.Multiply(QuatC);

            RotX.SetW(dy);
            fquat QuatX;
            QuatX.FromAxisAngle(RotX);
            QuatC = QuatX.Multiply(QuatC);

            break;
          }
        }

        beginx = esx;
        beginy = esy;

      } else if (mDoDolly) {
        CVector2 VP(float(mpViewport->GetW()), float(mpViewport->GetH()));

        fvec3 outx, outy;

        mCameraData.GetPixelLengthVectors(mvCenter, VP, outx, outy);

        float fvl = ViewLengthToWorldLength(mvCenter, 1.0f);
        float fdx = float(esx - beginx);
        float fdy = float(esy - beginy);

        float fdolly = (outx.Mag() * fdx);

        if (isshift) {
          fdolly *= 3.0f;
          fdolly *= 3.0f;
        } else if (isctrl) {
          fdolly *= 0.3f;
          fdolly *= 0.3f;
        }

        fvec4 MoveVec = RotZ * fdolly;

        if (pVP->IsHotKeyDepressed("camera_y"))
          MoveVec = RotY * fdolly;
        else if (pVP->IsHotKeyDepressed("camera_x"))
          MoveVec = RotX * fdolly;

        mvCenter += MoveVec;

        beginx = esx;
        beginy = esy;
      } else if (mDoPan) {
        mEvTrackData.icurX = esx;
        mEvTrackData.icurY = esy;
        PanUpdate(mEvTrackData);
      }
      break;
    }
    case UIEV_MULTITOUCH: {
      static bool gbInMtDolly = false;
      static bool gbInMtPan = false;
      static bool gbInMtRot = false;
      struct PointProc {
        static CVector2 unitPos(const ui::MultiTouchPoint& pnt, ui::Viewport* pVP) {
          CVector2 rval;
          rval.SetX(pnt.mfCurrX / float(pVP->GetW()));
          rval.SetY(pnt.mfCurrY / float(pVP->GetH()));
          return rval;
        }
      };

      const ui::MultiTouchPoint* mp[3];
      CVector2 upos[3];
      if (EV.miNumMultiTouchPoints) {
        int inumpushed = 0;
        int inumdown = 0;

        int inumtopgate = 0;
        int inumbotgate = 0;
        int inummot = 0;
        int imotidx = -1;
        bool bmotpushed = false;
        bool bmotreleased = false;
        bool bmotdown = false;

        for (int i = 0; i < EV.miNumMultiTouchPoints; i++) {
          mp[i] = &EV.mMultiTouchPoints[i];
          upos[i] = PointProc::unitPos(*mp[i], pVP);

          bool btopgate = (upos[i].GetX() < 0.2f) && (upos[i].GetY() < 0.4f);
          bool bbotgate = (upos[i].GetX() < 0.2f) && (upos[i].GetY() >= 0.4f);
          bool bismot = (upos[i].GetX() >= 0.2f);

          inumtopgate += int(btopgate);
          inumbotgate += int(bbotgate);

          inummot += int(bismot);

          inumpushed += int(mp[i]->mState == MultiTouchPoint::PS_PUSHED);
          inumdown += int(mp[i]->mState == MultiTouchPoint::PS_DOWN);

          if (bismot) {
            imotidx = i;
            bmotpushed |= (mp[i]->mState == MultiTouchPoint::PS_PUSHED);
            bmotreleased |= (mp[i]->mState == MultiTouchPoint::PS_RELEASED);
            bmotdown |= (mp[i]->mState == MultiTouchPoint::PS_DOWN);
          }
        }

        int iX = (imotidx >= 0) ? int(mp[imotidx]->mfCurrX) : 0;
        int iY = (imotidx >= 0) ? int(mp[imotidx]->mfCurrY) : 0;

        // printf( "nmp<%d> int<%d> inb<%d> imi<%d> ix<%d> iy<%d>\n", EV.miNumMultiTouchPoints, inumtopgate, inumbotgate, imotidx,
        // iX, iY );

        ////////////////////////
        // pan
        ////////////////////////
        if ((inumtopgate == 1) && (inumbotgate == 0) && (imotidx >= 0)) {
          if (bmotpushed) {
            mEvTrackData.vPushCenter = mvCenter;
            mEvTrackData.ipushX = iX;
            mEvTrackData.ipushY = iY;
            DollyBegin(mEvTrackData);
            gbInMtDolly = true;
          } else if (bmotdown) {
            mEvTrackData.icurX = iX;
            mEvTrackData.icurY = iY;
            DollyUpdate(mEvTrackData);
            mEvTrackData.ipushX = iX;
            mEvTrackData.ipushY = iY;
            gbInMtDolly = true;
          }
        }
        ////////////////////////
        // dolly
        ////////////////////////
        if ((inumtopgate == 2) && (inumbotgate == 0) && (imotidx >= 0)) {
          if (bmotpushed) {
            mEvTrackData.vPushCenter = mvCenter;
            mEvTrackData.ipushX = iX;
            mEvTrackData.ipushY = iY;
            PanBegin(mEvTrackData);
            gbInMtPan = true;
          } else if (bmotdown) {
            mEvTrackData.icurX = iX;
            mEvTrackData.icurY = iY;
            PanUpdate(mEvTrackData);
            gbInMtPan = true;
          }
        }
        ////////////////////////
        // rot
        ////////////////////////
        if ((inumtopgate == 0) && (inumbotgate == 1) && (imotidx >= 0)) {
          if (bmotpushed) {
            mEvTrackData.vPushCenter = mvCenter;
            mEvTrackData.ipushX = iX;
            mEvTrackData.ipushY = iY;
            RotBegin(mEvTrackData);
            gbInMtRot = true;
          } else if (bmotdown) {
            mEvTrackData.icurX = iX;
            mEvTrackData.icurY = iY;
            RotUpdate(mEvTrackData);
            mEvTrackData.ipushX = iX;
            mEvTrackData.ipushY = iY;
            gbInMtRot = true;
          }
        }
      } else // miNumMultiTouchPoints==0 (end mt event)
      {
        if (gbInMtDolly)
          DollyEnd();
        if (gbInMtPan)
          PanEnd();
        if (gbInMtRot)
          RotEnd();
      }
      // printf( "cam<%p> recieved mt event\n" );
      break;
    }
    case UIEV_MOUSEWHEEL: {
      float zmoveamt = 0.003f;
      if (isctrl)
        zmoveamt *= 0.2f;
      else if (isshift)
        zmoveamt *= 5.0f;
      if (isalt) {
        fvec4 Center = mvCenter;
        fvec4 Delta = RotZ * zmoveamt * EV.miMWY;
        mvCenter += Delta;
      } else {
        CVector2 VP(float(pVP->GetW()), float(pVP->GetH()));
        fvec3 Pos = mvCenter;
        fvec3 UpVector;
        fvec3 RightVector;
        mCameraData.GetPixelLengthVectors(Pos, VP, UpVector, RightVector);
        float CameraFactor = RightVector.Mag() * float(20.0f); // 20 pixels of movement
        const float kmin = 0.1f;
        const float kmax = 20000.0f;
        if (mfLoc < kmin)
          mfLoc = kmin;
        if (mfLoc > kmax)
          mfLoc = kmax;
        float DeltaInMeters = float(-EV.miMWY) * CameraFactor * zmoveamt;
        mfLoc += DeltaInMeters;
        if (mfLoc < kmin)
          mfLoc = kmin;
        if (mfLoc > kmax)
          mfLoc = kmax;
      }
      break;
    }
  }

  UpdateMatrices();
  // CommonPostSetup();

  return (mDoPan || mDoRotate || mDoDolly);
}

///////////////////////////////////////////////////////////////////////////////

void CCamera_persp::SetFromWorldSpaceMatrix(const CMatrix4& matrix) {
  ork::fvec3 xnormal = matrix.GetXNormal();
  ork::fvec3 ynormal = matrix.GetYNormal();
  ork::fvec3 znormal = matrix.GetZNormal();

  CMatrix4 matrot, imatrot;
  matrot.NormalVectorsIn(xnormal, ynormal, znormal);
  imatrot.GEMSInverse(matrot);

  fquat quat;
  quat.FromMatrix(imatrot);

  fvec3 pos = matrix.GetTranslation();

  printf("SetQuatc:1\n");
  QuatC = quat;
  mvCenter = pos + fvec3(0.0f, 0.0f, mfLoc).Transform3x3(matrot);

  UpdateMatrices();
}

///////////////////////////////////////////////////////////////////////////////

void CCamera_persp::RenderUpdate(void) {
  OrkAssert(GetViewport());
  //////////////////////////////////////
  //////////////////////////////////////
  //////////////////////////////////////

  auto pVP = GetViewport();

  bool isctrl = false;  // pVP->IsKeyDepressed( VK_CONTROL );
  bool isshift = false; // pVP->IsKeyDepressed( VK_SHIFT );
  bool isalt = false;   // pVP->IsKeyDepressed( VK_LMENU );

  bool iscaps = false; //(GetKeyState( VK_CAPITAL ) & 1);

  bool iscapsorshift = isshift | iscaps;

  // orkprintf( "IsCaps %08X\n", int(iscaps) );

  isshift |= false; // pVP->IsKeyDepressed(VK_NUMPAD5);

  // OrkAssert(false);

  //////////////////////////////////////
  // motion state tracking

  LastMeasuredCameraVelocity = MeasuredCameraVelocity;
  MeasuredCameraVelocity = (CamLoc - PrevCamLoc);

  float CurVelMag = MeasuredCameraVelocity.Mag();
  float LastVelMag = LastMeasuredCameraVelocity.Mag();

  CVector2 VP;
  if (pVP) {
    VP.SetX((float)pVP->GetW());
    VP.SetY((float)pVP->GetH());
  }
  fvec3 Pos = mvCenter;
  fvec3 UpVector;
  fvec3 RightVector;
  mCameraData.GetPixelLengthVectors(Pos, VP, UpVector, RightVector);
  float CameraMotionThresh = RightVector.Mag() / float(1000.0f);

  static ork::lev2::CTXBASE::ERefreshPolicy glastpolicy = CTXBASE::EREFRESH_WHENDIRTY;
  if (mbInMotion) {
    if (CurVelMag < CameraMotionThresh) // start motion
    {
      if (GetViewport() && GetViewport()->GetTarget()) {
        GetViewport()->GetTarget()->GetCtxBase()->SetRefreshPolicy(glastpolicy);
        mbInMotion = false;
      }
    }
  } else {
    if ((LastVelMag < CameraMotionThresh) && (CurVelMag > CameraMotionThresh)) // start motion
    {
      if (GetViewport() && GetViewport()->GetTarget()) {
        glastpolicy = GetViewport()->GetTarget()->GetCtxBase()->GetRefreshPolicy();
        mbInMotion = true;
        GetViewport()->GetTarget()->GetCtxBase()->SetRefreshPolicy(CTXBASE::EREFRESH_FASTEST);
      }
    }
  }

  CheckMotion();

  /////////////////////////////

  GfxTarget* pT = pVP->GetTarget();

  fvec4 CamZ(CamFocusZNormal * -mfLoc);

  mfWorldSizeAtLocator = ViewLengthToWorldLength(mvCenter, float(1.0f));

  static F32 fRotZ = 0.0f;

  fRotZ *= 0.96f;

  fvec4 RotZ(0.0f, 1.0f, 0.0f, 1.0f);
  RotZ = mCameraData.GetZNormal();
  RotZ.SetW(fRotZ);
  fquat QuatZ;
  QuatZ.FromAxisAngle(RotZ);
  QuatC = QuatZ.Multiply(QuatC);

  //////////////////////////////////////////

  if (pVP && pVP->IsHotKeyDepressed(mHK_RotL)) {
    fquat QuatY = VerticalRot(-0.02f);
    QuatC = QuatY.Multiply(QuatC);
  } else if (pVP && pVP->IsHotKeyDepressed(mHK_RotR)) {
    fquat QuatY = VerticalRot(0.02f);
    QuatC = QuatY.Multiply(QuatC);
  } else if (pVP && pVP->IsHotKeyDepressed(mHK_RotD)) {
    fvec4 RotY = mCameraData.GetXNormal();
    RotY.SetW(0.01f);
    fquat QuatY;
    QuatY.FromAxisAngle(RotY);
    QuatC = QuatY.Multiply(QuatC);
  } else if (pVP && pVP->IsHotKeyDepressed(mHK_RotU)) {
    fvec4 RotY = mCameraData.GetXNormal();
    RotY.SetW(-0.01f);
    fquat QuatY;
    QuatY.FromAxisAngle(RotY);
    QuatC = QuatY.Multiply(QuatC);
  }

  //////////////////////////////////////////
  // save focus to pick

  else if (pVP && pVP->IsHotKeyDepressed(mHK_Pik2Foc)) {
    CamFocus = mvCenter;
  } else if (pVP && pVP->IsHotKeyDepressed(mHK_Foc2Pik)) {
    mvCenter = CamFocus;
  }

  //////////////////////////////////////////
  // center on origin

  else if (pVP && pVP->IsHotKeyDepressed(mHK_Origin)) {
    fvec4 Center = mvCenter * float(-1.0f);
    fvec4 Delta = Center * float(0.9f);

    mvCenter += Delta;
  }

  //////////////////////////////////////////
  // re align camera

  if (pVP && pVP->IsHotKeyDepressed(mHK_ReAlign)) {
    f32 fApproachSpeed = 0.92f; // not reall approach speed, the closer to zero the faster

    fvec4 AxisAngleA = QuatC.ToAxisAngle();

    f32 fangle = AxisAngleA.GetW();

    if (fangle > 0.5f) {
      f32 fangle2 = 1.0f - fangle;

      fangle2 *= fApproachSpeed;
      fangle2 = 1.0f - fangle2;

      if (fangle2 > 0.999f)
        fangle2 = 0.0f;

      AxisAngleA.SetW(fangle2);
      QuatC.FromAxisAngle(AxisAngleA);
    } else {
      fangle *= fApproachSpeed;

      AxisAngleA.SetW(fangle);
      QuatC.FromAxisAngle(AxisAngleA);
    }
  }

  //////////////////////////////////////////

  if (pVP && pVP->IsHotKeyDepressed(mHK_In)) {
    mfLoc = mfLoc * float(0.999f);
  }

  else if (pVP && pVP->IsHotKeyDepressed(mHK_Out)) {
    mfLoc = mfLoc + 0.001f;
  }

  //////////////////////////////////////////

  if (pVP && pVP->IsHotKeyDepressed(mHK_AperPlus))
    aper += 0.1f;
  else if (pVP && pVP->IsHotKeyDepressed(mHK_AperMinus))
    aper -= 0.1f;

  //////////////////////////////////////////

  fvec4 MoveNrmX, MoveNrmZ;

  static f32 LastTimeStep = CSystem::GetRef().GetLoResTime();
  f32 ThisTimeStep = CSystem::GetRef().GetLoResTime();

  float timestep = float(ThisTimeStep - LastTimeStep);
  LastTimeStep = ThisTimeStep;

  static const float MetersPerTick(1.0f / 100.0f);
  float fmovescale = timestep * MetersPerTick;

  if (isctrl)
    fmovescale *= 0.3f;
  else if (isshift)
    fmovescale *= 3.0f;

  float fmovscalXZ = fmovescale * mfLoc;

  ////////////////////////////////////////

  static fvec4 Velocity;

  Velocity = Velocity * float(0.85f);

  if (pVP && pVP->IsHotKeyDepressed(mHK_MovD)) {
    // log( 10 ) = 1 log( 100 ) = 2

    // loc += logf( loc ) * 0.03f;
    Velocity -= fvec4(0.0f, 1.0f, 0.0f) * fmovscalXZ;
  }
  if (pVP && pVP->IsHotKeyDepressed(mHK_MovU)) {
    // loc -= logf( loc ) * 0.03f;

    // if( loc < float(0.0f) )
    //	loc = float(0.0f);
    Velocity += fvec4(0.0f, 1.0f, 0.0f) * fmovscalXZ;
  }
  if (pVP && pVP->IsHotKeyDepressed(mHK_MovR) && iscapsorshift) {
    fvec4 Direction = mCameraData.GetXNormal();
    if (IsYVertical()) {
      Direction.SetY(0.0f);
    } else if (IsZVertical()) {
      Direction.SetZ(0.0f);
    } else if (IsXVertical()) {
      Direction.SetX(0.0f);
    }
    Velocity += Direction * fmovscalXZ;
  }
  if (pVP && pVP->IsHotKeyDepressed(mHK_MovL) && iscapsorshift) {
    fvec4 Direction = mCameraData.GetXNormal();
    if (IsYVertical()) {
      Direction.SetY(0.0f);
    } else if (IsZVertical()) {
      Direction.SetZ(0.0f);
    } else if (IsXVertical()) {
      Direction.SetX(0.0f);
    }
    Velocity += Direction * (-fmovscalXZ);
  }
  if (pVP && pVP->IsHotKeyDepressed(mHK_MovF) && iscapsorshift) {
    fvec4 Direction = mCameraData.GetZNormal();
    if (IsYVertical()) {
      Direction.SetY(0.0f);
    } else if (IsZVertical()) {
      Direction.SetZ(0.0f);
    } else if (IsXVertical()) {
      Direction.SetX(0.0f);
    }
    Velocity += Direction * fmovscalXZ;
  }
  if (pVP && pVP->IsHotKeyDepressed(mHK_MovB) && iscapsorshift) {
    fvec4 Direction = mCameraData.GetZNormal();
    if (IsYVertical()) {
      Direction.SetY(0.0f);
    } else if (IsZVertical()) {
      Direction.SetZ(0.0f);
    } else if (IsXVertical()) {
      Direction.SetX(0.0f);
    }
    Velocity += Direction * (-fmovscalXZ);
  }

  mvCenter += Velocity;

  //////////////////////////////////////

  PrevCamLoc = CamLoc;
  CamLoc = mvCenter + (mCameraData.GetZNormal() * -mfLoc);

  ////////////////////////////////////////

  UpdateMatrices();
}

///////////////////////////////////////////////////////////////////////////////

void CCamera_persp::UpdateMatrices(void) {
  if (mfLoc < 0.001f)
    mfLoc = 0.001f;

  ///////////////////////////////////////////////////////////////
  // setup dynamic near / far	(this increases depth buffer effective resolution based on context)

  //	loc			log10	near		far			ratio

  //	0.01		-2		0.001		1.0			1000.0
  //	0.1			-1		0.01		10.0		1000.0
  //	1.0			0		0.1			100.0		1000.0
  //	10.0		1		1.0			1000.0		1000.0
  //	100.0		2		10.0		10000.0		1000.0
  //	1000.0		3		100.0		100000.0	1000.0
  //	10000.0		4		1000.0		1000000.0	1000.0
  //	100000.0	5		10000.0		10000000.0	1000.0

  float flog10 = log10(mfLoc);
  float flerpidx = (flog10 + float(1.0f)) / float(6.0f);
  float finvlerpidx = float(1.0f) - flerpidx;

  float neardiv = (float(0.5f) * finvlerpidx) + (float(100.0f) * flerpidx);
  float farmul = (float(500.0f) * finvlerpidx) + (float(0.5f) * flerpidx);

  float fnear = mfLoc / neardiv;
  float ffar = mfLoc * farmul;

  if (fnear < near_min)
    fnear = near_min;
  if (ffar > far_max)
    ffar = far_max;

  ///////////////////////////////////////////////////////////////

  mRot = QuatC.ToMatrix();
  mTrans.SetTranslation(mvCenter * float(-1.0f));

  CMatrix4 matxf = (mTrans * mRot);
  CMatrix4 matixf;
  matxf.GEMSInverse(matxf);

  fvec3 veye = fvec3(0.0f, 0.0f, -mfLoc).Transform(matxf);
  fvec3 vtarget = fvec3(0.0f, 0.0f, 0.0f).Transform(matxf);
  fvec3 vup = fvec4(0.0f, 1.0f, 0.0f, 0.0f).Transform(matxf).GetXYZ();

  mCameraData.Persp(fnear, ffar, aper);
  mCameraData.Lookat(veye, vtarget, vup);

  ///////////////////////////////////////////////////////////////
  CameraCalcContext ctx;
  mCameraData.CalcCameraData(ctx);
  ///////////////////////////////////////////////////////////////
  CommonPostSetup();
}

///////////////////////////////////////////////////////////////////////////////
// this will return 1.0f id pos is directly at the near plane, should increase linearly furthur back

float CCamera_persp::ViewLengthToWorldLength(const fvec4& pos, float ViewLength) {
  float rval = 1.0f;

  float distATnear = (mCameraData.GetFrustum().mNearCorners[1] - mCameraData.GetFrustum().mNearCorners[0]).Mag();
  float distATfar = (mCameraData.GetFrustum().mFarCorners[1] - mCameraData.GetFrustum().mFarCorners[0]).Mag();
  float depthscaler = distATfar / distATnear;

  // get pos as a lerp from near to far
  float depthN = mCameraData.GetFrustum().mNearPlane.GetPointDistance(pos);
  float depthF = mCameraData.GetFrustum().mFarPlane.GetPointDistance(pos);
  float depthRange = (camrayF - camrayN).Mag();
  if ((depthN >= float(0.0f)) && (depthF >= float(0.0f))) { // better be between near and far planes
    float lerpV = depthN / depthRange;
    float ilerpV = float(1.0f) - lerpV;
    rval = ((ilerpV) + (lerpV * depthscaler));
    // orkprintf( "lerpV %f dan %f daf %f dscaler %f rval %f\n", lerpV, distATnear, distATfar, depthscaler, rval );
  }

  return rval;
}

///////////////////////////////////////////////////////////////////////////////////////////

void CCamera_persp::GenerateDepthRay(const CVector2& pos2D, fvec3& rayN, fvec3& rayF, const CMatrix4& IMat) const {
  float fx = pos2D.GetX();
  float fy = pos2D.GetY();
  //////////////////////////////////////////
  fvec4 vWinN(fx, fy, 0.0f);
  fvec4 vWinF(fx, fy, 1.0f);
  CMatrix4::UnProject(mCameraData.GetIVPMatrix(), vWinN, rayN);
  CMatrix4::UnProject(mCameraData.GetIVPMatrix(), vWinF, rayF);
  //////////////////////////////////////////
}

///////////////////////////////////////////////////////////////////////////////////////////

}} // namespace ork::lev2
