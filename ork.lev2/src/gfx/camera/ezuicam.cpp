////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
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
#include <ork/lev2/gfx/camera/uicam.h>
#include <ork/lev2/gfx/dbgfontman.h>
#include <ork/lev2/ui/touch.h>
#include <ork/lev2/ui/viewport.h>
#include <ork/math/basicfilters.h>
#include <ork/math/misc_math.h>
#include <ork/lev2/qtui/qtui.h>
#include <ork/lev2/gfx/material_pbr.inl>
#include <ork/lev2/gfx/material_freestyle.h>
#include <ork/lev2/gfx/fxstate_instance.h>
#include <ork/reflect/properties/registerX.inl>

#include <QtGui/QCursor>

ImplementReflectionX(ork::lev2::EzUiCam, "EzUiCam");

using namespace ork::ui;

namespace ork { namespace lev2 {

void OrkGlobalDisableMousePointer();
void OrkGlobalEnableMousePointer();

///////////////////////////////////////////////////////////////////////////////

void EzUiCam::describeX(object::ObjectClass* clazz) {
  clazz->floatProperty("Aperature", float_range{0.0f, 90.0f}, &EzUiCam::aper);
  clazz->floatProperty("MaxFar", float_range{1.0f, 100000.0f}, &EzUiCam::far_max);
  clazz->floatProperty("MinNear", float_range{0.01f, 10000.0f}, &EzUiCam::near_min);

  // temporary until old mox files converted
  ork::rtti::Class::CreateClassAlias("Camera_persp", GetClassStatic());
}

///////////////////////////////////////////////////////////////////////////////

struct UiCamPrivate {
  UiCamPrivate() {
    _material = std::make_shared<FreestyleMaterial>();
    FxStateInstanceConfig config;
    _materialinst_mono   = std::make_shared<FxStateInstance>(config); // _material
    _materialinst_stereo = std::make_shared<FxStateInstance>(config); // _material
  }
  void gpuUpdate(Context* ctx) {
    if (_doGpuInit) {
      auto shaderpath = file::Path("orkshader://manip");
      _material->gpuInit(ctx, shaderpath);
      //_materialinst->setInstanceMvpParams("mvp", "mvpL", "mvpR");
      _materialinst_mono->_technique   = _material->technique("std_mono");
      _materialinst_stereo->_technique = _material->technique("std_stereo");
      _doGpuInit                       = false;
    }
  }
  bool _doGpuInit = true;
  freestyle_mtl_ptr_t _material;
  fxinstance_ptr_t _materialinst_mono;
  fxinstance_ptr_t _materialinst_stereo;
};
using uicamprivate_t = std::shared_ptr<UiCamPrivate>;

///////////////////////////////////////////////////////////////////////////////

EzUiCam::EzUiCam()
    : UiCamera()
    , aper(40.0f)
    , far_max(10000.0f)
    , near_min(0.01f)
    , tx(0.0f)
    , ty(0.0f)
    , tz(0.0f)
    , meRotMode(EROT_SCREENXY)
    , mDoDolly(false)
    , mDoRotate(false)
    , mDoPan(false) {
  _camcamdata.Persp(1.0f, 1000.0f, 70.0f);
  _camcamdata.Lookat(fvec3(0.0f, 0.0f, 0.0f), fvec3(0.0f, 0.0f, 1.0f), fvec3(0.0f, 1.0f, 0.0f));
  type_name     = "Perspective";
  instance_name = "Default";
  leftbutton    = false;
  middlebutton  = false;
  rightbutton   = false;

  mfWorldSizeAtLocator = 150.0f;

  auto uicampriv = std::make_shared<UiCamPrivate>();
  _private.Set<uicamprivate_t>(uicampriv);
}

///////////////////////////////////////////////////////////////////////////////

void EzUiCam::draw(Context* context) const {
  extern fvec4 TRayN;
  extern fvec4 TRayF;

  auto priv = _private.Get<uicamprivate_t>();

  priv->gpuUpdate(context);

  float CurVelMag = MeasuredCameraVelocity.Mag();
  //////////////////////////////////////
  /*context->MTXI()->PushUIMatrix();
  {

    context->PushModColor(fcolor4::Black());
    ork::lev2::FontMan::PushFont("i14");
    FontMan::beginTextBlock(context);
    FontMan::DrawText(context, 41, 9, "Center %f %f %f", mvCenter.GetX(), mvCenter.GetY(), mvCenter.GetZ());
    FontMan::DrawText(context, 41, 21, "CamLoc   %f %f %f", CamLoc.GetX(), CamLoc.GetY(), CamLoc.GetZ());
    FontMan::DrawText(context, 41, 33, "zf %f", (_camcamdata.GetFar()));
    FontMan::DrawText(context, 41, 45, "zn %f", (_camcamdata.GetNear()));
    FontMan::DrawText(context, 41, 57, "zfoverzn %f", (_camcamdata.GetFar() / _camcamdata.GetNear()));
    FontMan::DrawText(context, 41, 69, "Loc(m) %f Speed(m/f) %f", mfLoc, CurVelMag);
    FontMan::DrawText(context, 41, 81, "RotMode %s", (meRotMode == EROT_SCREENZ) ? "ScreenZ" : "ScreenXY");
    FontMan::DrawText(context, 41, 93, "Aper %f", aper);
    FontMan::DrawText(context, 41, 105, "Name %s", GetName().c_str());
    FontMan::endTextBlock(context);
    context->PopModColor();

    context->PushModColor(fcolor4::Yellow());
    FontMan::beginTextBlock(context);
    FontMan::DrawText(context, 41, 9, "Center %f %f %f", mvCenter.GetX(), mvCenter.GetY(), mvCenter.GetZ());
    FontMan::DrawText(context, 41, 21, "CamLoc   %f %f %f", CamLoc.GetX(), CamLoc.GetY(), CamLoc.GetZ());
    FontMan::DrawText(context, 41, 33, "zf %f", (_camcamdata.GetFar()));
    FontMan::DrawText(context, 41, 45, "zn %f", (_camcamdata.GetNear()));
    FontMan::DrawText(context, 41, 57, "zfoverzn %f", (_camcamdata.GetFar() / _camcamdata.GetNear()));
    FontMan::DrawText(context, 41, 69, "Loc(m) %f Speed(m/f) %f", mfLoc, CurVelMag);
    FontMan::DrawText(context, 41, 81, "RotMode %s", (meRotMode == EROT_SCREENZ) ? "ScreenZ" : "ScreenXY");
    FontMan::DrawText(context, 41, 93, "Aper %f", aper);
    FontMan::DrawText(context, 41, 105, "Name %s", GetName().c_str());
    FontMan::endTextBlock(context);
    ork::lev2::FontMan::PopFont();
    context->PopModColor();
  }
  context->MTXI()->PopUIMatrix();*/
  ///////////////////////////////////////////////////////////////
  // printf( "CAMHUD\n" );
  float aspect = float(context->mainSurfaceWidth()) / float(context->mainSurfaceHeight());
  _curMatrices = _camcamdata.computeMatrices(aspect);
  auto RCFD    = context->topRenderContextFrameData();
  lev2::RenderContextInstData RCID(RCFD);
  fmtx4 worldmtx;
  worldmtx.SetTranslation(mvCenter);
  float Scale = mfLoc / 60.0f;
  worldmtx.Scale(fvec4(Scale, Scale, Scale));
  ///////////////////////////////////////////////////////////////
  context->debugPushGroup("EzUiCam::draw");
  auto mtlinst = RCFD->isStereo() //
                     ? priv->_materialinst_stereo
                     : priv->_materialinst_mono;
  mtlinst->wrappedDrawCall(RCID, [context]() {
    auto& tricircle = GfxPrimitives::GetRef().mVtxBuf_TriCircle;
    auto& axis      = GfxPrimitives::GetRef().mVtxBuf_Axis;
    context->GBI()->DrawPrimitiveEML(tricircle);
    context->GBI()->DrawPrimitiveEML(axis);
  });
  context->debugPopGroup();
  ///////////////////////////////////////////////////////////////
}

///////////////////////////////////////////////////////////////////////////////

static QPoint pmousepos;

///////////////////////////////////////////////////////////////////////////////

void EzUiCam::PanBegin(const CamEvTrackData& ed) {
  // printf("BeginPan\n");
  pmousepos = ork::lev2::logicalMousePos();
  // OrkGlobalDisableMousePointer();
  mDoPan = true;
}

///////////////////////////////////////////////////////////////////////////////

void EzUiCam::PanUpdate(const CamEvTrackData& ed) {
  assert(mDoPan);

  int esx    = ed.icurX;
  int esy    = ed.icurY;
  int ipushx = ed.ipushX;
  int ipushy = ed.ipushY;

  fvec3 outx, outy;

  _curMatrices.GetPixelLengthVectors(mvCenter, _vpdim, outx, outy);

  float fvl = ViewLengthToWorldLength(mvCenter, 1.0f);
  float fdx = float(esx - ipushx);
  float fdy = float(esy - ipushy);

  mvCenter = ed.vPushCenter + (outx * fdx) + (outy * fdy);

  // QCursor::setPos(pmousepos);
}

///////////////////////////////////////////////////////////////////////////////

void EzUiCam::PanEnd() {
  // printf("EndPan\n");
  // QCursor::setPos(pmousepos);
  // OrkGlobalEnableMousePointer();
  mDoPan = false;
}

///////////////////////////////////////////////////////////////////////////////

void EzUiCam::RotBegin(const CamEvTrackData& ed) {
  // printf("BeginRot\n");

  // printf( "Rot: vPushNZ<%g %g %g>\n", vPushNZ.x, vPushNZ.y, vPushNZ.z );

  pmousepos = ork::lev2::logicalMousePos();
  // OrkGlobalDisableMousePointer();
  mDoRotate = true;
}

///////////////////////////////////////////////////////////////////////////////

void EzUiCam::RotEnd() {
  // printf("EndRot\n");
  // QCursor::setPos(pmousepos);
  // OrkGlobalEnableMousePointer();
  mDoRotate = false;
}

///////////////////////////////////////////////////////////////////////////////

void EzUiCam::DollyBegin(const CamEvTrackData& ed) {
  // printf("BeginDolly\n");
  pmousepos = ork::lev2::logicalMousePos();
  // OrkGlobalDisableMousePointer();
  mDoDolly = true;
}

///////////////////////////////////////////////////////////////////////////////

void EzUiCam::DollyEnd() {
  // printf("EndDolly\n");
  // QCursor::setPos(pmousepos);
  // OrkGlobalEnableMousePointer();
  mDoDolly = false;
}

///////////////////////////////////////////////////////////////////////////////

bool EzUiCam::UIEventHandler(ui::event_constptr_t EV) {
  const ui::EventCooked& filtev = EV->mFilteredEvent;

  int esx    = filtev.miX;
  int esy    = filtev.miY;
  float fux  = filtev.mUnitX;
  float fuy  = filtev.mUnitY;
  float fpux = (fux * 2.0f) - 1.0f;
  float fpuy = (fuy * 2.0f) - 1.0f;

  fvec2 pos2D(fpux, fpuy);

  int state    = 0;
  bool isctrl  = filtev.mCTRL;
  bool isalt   = filtev.mALT;
  bool isshift = filtev.mSHIFT;
  bool ismeta  = filtev.mMETA;
  bool isleft  = filtev.mBut0;
  bool ismid   = filtev.mBut1;
  bool isright = filtev.mBut2;

  static int ipushx = 0;
  static int ipushy = 0;
  static f32 flerp  = 0.0f;

  _vpdim = EV->_vpdim;

  switch (filtev._eventcode) {
    case EventCode::PUSH: {

      QuatCPushed = QuatC;

      float fx   = float(esx) / _vpdim.x - 0.5f;
      float fy   = float(esy) / _vpdim.y - 0.5f;
      float frad = sqrtf((fx * fx) + (fy * fy));

      // printf( "fx<%g %g> frad<%g>\n", fx, fy, frad);

      meRotMode = (frad > 0.35f) ? EROT_SCREENZ : EROT_SCREENXY;

      CommonPostSetup();

      _pushNX = _camcamdata.xNormal();
      _pushNY = _camcamdata.yNormal();
      _pushNZ = _camcamdata.zNormal();

      // printf( "nx <%g %g %g>\n", _pushNX.x, _pushNX.y, _pushNX.z );
      // printf( "ny <%g %g %g>\n", _pushNY.x, _pushNY.y, _pushNY.z );
      // printf( "nz <%g %g %g>\n", _pushNZ.x, _pushNZ.y, _pushNZ.z );

      fvec3 vrn, vrf;

      GenerateDepthRay(pos2D, vrn, vrf, _curMatrices.GetIVPMatrix());

      if (isleft || isright) {
        ////////////////////////////////////////////////////////
        // calculate planes with world rotation, but current view target as origin

        fvec4 Origin = mvCenter;
        _manipHandler.Init(pos2D, _curMatrices.GetIVPMatrix(), QuatC);
      }
      //////////////////////////////////////////////////

      beginx = esx;
      beginy = esy;
      ipushx = esx;
      ipushy = esy;

      leftbutton   = filtev.mBut0;
      middlebutton = filtev.mBut1;
      rightbutton  = filtev.mBut2;

      bool filt_kpush = (filtev.mAction == "keypush");

      bool do_rot = (isleft && (isalt || filt_kpush));
      bool do_pan = (ismid && (isalt || filt_kpush));

      if (do_rot) {
        mEvTrackData.vPushCenter = mvCenter;
        mEvTrackData.ipushX      = ipushx;
        mEvTrackData.ipushY      = ipushy;
        RotBegin(mEvTrackData);
      } else if (do_pan) {
        mEvTrackData.vPushCenter = mvCenter;
        mEvTrackData.ipushX      = ipushx;
        mEvTrackData.ipushY      = ipushy;
        PanBegin(mEvTrackData);
      }

      break;
    }
    case EventCode::RELEASE: {
      CommonPostSetup();
      // could do fall-through or maybe even but this outside of switch
      leftbutton   = filtev.mBut0;
      middlebutton = filtev.mBut1;
      rightbutton  = filtev.mBut2;

      if (mDoDolly)
        DollyEnd();
      if (mDoPan)
        PanEnd();
      if (mDoRotate)
        RotEnd();

      mDoPan    = false;
      mDoDolly  = false;
      mDoRotate = false;

      break;
    }
    case EventCode::MOVE: {

      float fx   = float(esx) / _vpdim.x - 0.5f;
      float fy   = float(esy) / _vpdim.y - 0.5f;
      float frad = sqrtf((fx * fx) + (fy * fy));

      meRotMode = (frad > 0.35f) ? EROT_SCREENZ : EROT_SCREENXY;

      break;
    }
    case EventCode::DRAG: {
      //////////////////////////////////////////////////
      // intersect ray with worlds XZ/XY/YZ planes

      _manipHandler.Intersect(pos2D);

      float dx = float(esx - beginx);
      float dy = float(esy - beginy);

      // input  1 . 10 . 100
      // output .1  10 . 1000

      float flogdx = log10(fabs(float(dx)));
      float flogdy = log10(fabs(float(dy)));

      flogdx = (flogdx * float(1.5f)) - float(0.5f);
      flogdy = (flogdy * float(1.5f)) - float(0.5f);

      dx = float(powf(float(10.0f), flogdx)) * ((dx < float(0.0f)) ? float(-1.0f) : float(1.0f));
      dy = float(powf(float(10.0f), flogdy)) * ((dy < float(0.0f)) ? float(-1.0f) : float(1.0f));

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
            float fvpx   = _vpdim.x;
            float fvpy   = _vpdim.y;
            float fvpwd2 = fvpx * 0.5f;
            float fvphd2 = fvpy * 0.5f;
            float fipx   = float(ipushx);
            float fipy   = float(ipushy);
            float fesx   = float(esx);
            float fesy   = float(esy);

            float fx0 = (fipx - fvpwd2) / fvpwd2;
            float fy0 = (fipy - fvphd2) / fvphd2;
            float fx1 = (fesx - fvpwd2) / fvpwd2;
            float fy1 = (fesy - fvphd2) / fvphd2;
            fvec2 v0(fx0, fy0);
            fvec2 v1(fx1, fy1);
            v0.Normalize();
            v1.Normalize();
            float ang0   = rect2pol_ang(v0.x, v0.y);
            float ang1   = rect2pol_ang(v1.x, v1.y);
            float dangle = (ang1 - ang0);
            fvec4 rotz   = fvec4(_pushNZ, dangle);
            fquat QuatZ;
            QuatZ.fromAxisAngle(rotz);
            QuatC = QuatZ.Multiply(QuatCPushed);
            // printf( "v0 <%g %g> v1<%g %g>\n", v0.x, v0.y, v1.x, v1.y );
            // printf( "ang0 <%g> ang1<%g>\n", ang0, ang1 );
            // printf( "rotz <%g %g %g %g>\n", rotz.x, rotz.y, rotz.z, rotz.w );
            // printf( "QuatZ <%g %g %g %g>\n", QuatZ.x, QuatZ.y, QuatZ.z, QuatZ.w );
            // printf( "QuatC <%g %g %g %g>\n", QuatC.x, QuatC.y, QuatC.z, QuatC.w );

            break;
          }
          case EROT_SCREENXY: {

            fvec4 rotx = fvec4(_pushNX, -dy);
            fvec4 roty = fvec4(_pushNY, dx);

            fquat QuatX, QuatY;

            QuatX.fromAxisAngle(rotx);
            QuatY.fromAxisAngle(roty);

            QuatC = QuatY.Multiply(QuatC);
            QuatC = QuatX.Multiply(QuatC);

            break;
          }
        }

        beginx = esx;
        beginy = esy;

      } else if (mDoDolly) {

        fvec3 outx, outy;

        _curMatrices.GetPixelLengthVectors(mvCenter, _vpdim, outx, outy);

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

        fvec4 MoveVec = _pushNZ * fdolly;

        // if (HotKeyManager::IsDepressed("camera_y"))
        // MoveVec = _pushNY * fdolly;
        // else if (HotKeyManager::IsDepressed("camera_x"))
        // MoveVec = _pushNX * fdolly;

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
    case EventCode::MOUSEWHEEL: {
      float zmoveamt = 0.003f;
      if (isctrl)
        zmoveamt *= 0.2f;
      else if (isshift)
        zmoveamt *= 5.0f;
      if (isalt) {
        fvec4 Center = mvCenter;
        fvec4 Delta  = _pushNZ * zmoveamt * EV->miMWY;
        mvCenter += Delta;
      } else {
        fvec3 Pos = mvCenter;
        fvec3 UpVector;
        fvec3 RightVector;
        _curMatrices.GetPixelLengthVectors(Pos, _vpdim, UpVector, RightVector);
        float CameraFactor   = RightVector.Mag() * 20.0f; // 20 pixels of movement
        constexpr float kmin = 0.1f;
        constexpr float kmax = 20000.0f;
        mfLoc                = std::clamp(mfLoc, kmin, kmax);
        float DeltaInMeters  = float(-EV->miMWY) * CameraFactor * zmoveamt;
        mfLoc += DeltaInMeters;
        mfLoc = std::clamp(mfLoc, kmin, kmax);
      }
      break;
    }
  }

  updateMatrices();

  return (mDoPan || mDoRotate || mDoDolly);
}

///////////////////////////////////////////////////////////////////////////////

void EzUiCam::SetFromWorldSpaceMatrix(const fmtx4& matrix) {
  ork::fvec3 xnormal = matrix.GetXNormal();
  ork::fvec3 ynormal = matrix.GetYNormal();
  ork::fvec3 znormal = matrix.GetZNormal();

  fmtx4 matrot, imatrot;
  matrot.fromNormalVectors(xnormal, ynormal, znormal);
  imatrot.inverseOf(matrot);

  fquat quat;
  quat.FromMatrix(imatrot);

  fvec3 pos = matrix.GetTranslation();

  printf("SetQuatc:1\n");
  QuatC    = quat;
  mvCenter = pos + fvec3(0.0f, 0.0f, mfLoc).Transform3x3(matrot);

  updateMatrices();
}

///////////////////////////////////////////////////////////////////////////////

void EzUiCam::updateMatrices(void) {
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

  float flog10      = log10(mfLoc);
  float flerpidx    = (flog10 + 1.0f) / 6.0f;
  float finvlerpidx = float(1.0f) - flerpidx;

  float neardiv = (0.5f * finvlerpidx + 1000.0f) * flerpidx;
  float farmul  = (500.0f * finvlerpidx + 0.5f) * flerpidx;

  float fnear = mfLoc / neardiv;
  float ffar  = mfLoc * farmul;

  if (fnear < near_min)
    fnear = near_min;
  if (ffar > far_max)
    ffar = far_max;

  ///////////////////////////////////////////////////////////////

  mRot = QuatC.ToMatrix();
  mTrans.SetTranslation(mvCenter * -1.0f);

  fmtx4 matxf = (mTrans * mRot);
  fmtx4 matixf;
  matxf.inverseOf(matxf);

  fvec3 veye    = fvec3(0.0f, 0.0f, -mfLoc).Transform(matxf);
  fvec3 vtarget = fvec3(0.0f, 0.0f, 0.0f).Transform(matxf);
  fvec3 vup     = fvec4(0.0f, 1.0f, 0.0f, 0.0f).Transform(matxf).xyz();

  _camcamdata.Persp(fnear, ffar, aper);
  _camcamdata.Lookat(veye, vtarget, vup);

  // printf("near<%g> far<%g> mfLoc<%g>\n", fnear, ffar, mfLoc);
  // printf("mvCenter<%g %g %g>\n", mvCenter.x, mvCenter.y, mvCenter.z);
  // printf("veye<%g %g %g>\n", veye.x, veye.y, veye.z);
  // printf("vtarget<%g %g %g>\n", vtarget.x, vtarget.y, vtarget.z);
  // printf("vup<%g %g %g>\n", vup.x, vup.y, vup.z);

  ///////////////////////////////////////////////////////////////
  // CameraMatrices ctx = _camcamdata.computeMatrices(ctx);
  ///////////////////////////////////////////////////////////////
  CommonPostSetup();
}

///////////////////////////////////////////////////////////////////////////////
// this will return 1.0f id pos is directly at the near plane, should increase linearly furthur back

float EzUiCam::ViewLengthToWorldLength(const fvec4& pos, float ViewLength) {
  float rval = 1.0f;

  const auto& frustum = _curMatrices.GetFrustum();
  float distATnear    = (frustum.mNearCorners[1] - frustum.mNearCorners[0]).Mag();
  float distATfar     = (frustum.mFarCorners[1] - frustum.mFarCorners[0]).Mag();
  float depthscaler   = distATfar / distATnear;

  // get pos as a lerp from near to far
  float depthN     = frustum._nearPlane.pointDistance(pos);
  float depthF     = frustum._farPlane.pointDistance(pos);
  float depthRange = (camrayF - camrayN).Mag();
  if ((depthN >= float(0.0f)) && (depthF >= float(0.0f))) { // better be between near and far planes
    float lerpV  = depthN / depthRange;
    float ilerpV = float(1.0f) - lerpV;
    rval         = ((ilerpV) + (lerpV * depthscaler));
    // orkprintf( "lerpV %f dan %f daf %f dscaler %f rval %f\n", lerpV, distATnear, distATfar, depthscaler, rval );
  }

  return rval;
}

///////////////////////////////////////////////////////////////////////////////////////////

void EzUiCam::GenerateDepthRay(const fvec2& pos2D, fvec3& rayN, fvec3& rayF, const fmtx4& IMat) const {
  float fx = pos2D.GetX();
  float fy = pos2D.GetY();
  //////////////////////////////////////////
  fvec4 vWinN(fx, fy, 0.0f);
  fvec4 vWinF(fx, fy, 1.0f);
  fmtx4::UnProject(_curMatrices.GetIVPMatrix(), vWinN, rayN);
  fmtx4::UnProject(_curMatrices.GetIVPMatrix(), vWinF, rayF);
  //////////////////////////////////////////
}

///////////////////////////////////////////////////////////////////////////////////////////

}} // namespace ork::lev2
