////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
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
#include <ork/lev2/glfw/ctx_glfw.h>
#include <ork/lev2/gfx/material_pbr.inl>
#include <ork/lev2/gfx/material_freestyle.h>
#include <ork/lev2/gfx/fx_pipeline.h>
#include <ork/reflect/properties/registerX.inl>

ImplementReflectionX(ork::lev2::EzUiCam, "EzUiCam");

using namespace ork::ui;

namespace ork { namespace lev2 {

void OrkGlobalDisableMousePointer();
void OrkGlobalEnableMousePointer();

///////////////////////////////////////////////////////////////////////////////

void EzUiCam::lookAt(fvec3 eye, fvec3 tgt, fvec3 up ){
    fmtx4 VMATRIX;
    VMATRIX.lookAt(eye,tgt,up);

    if(_constrainZ){

      VMATRIX = VMATRIX.inverse();
      fvec3 znormal = VMATRIX.zNormal();

      OrkAssert(up.dotWith(fvec3(0,1,0))>0.99); // in constrainZ, up should be up

      /////////////////////////
      // compute SIGNED/ORIENTED heading
      /////////////////////////

      auto heading_n = fvec3(znormal.x,0,znormal.z).normalized();
      auto heading_ref = fvec3(0,0,-1);
      auto hrXhn = heading_ref.crossWith(heading_n);
      hrXhn.y = -fabs(hrXhn.y);
      float heading = heading_ref.orientedAngle(heading_n,hrXhn);

      /////////////////////////
      // compute SIGNED/ORIENTED elevation
      /////////////////////////

      auto znXhn = znormal.crossWith(heading_n);
      float elevation = znormal.orientedAngle(heading_n,znXhn);
      bool is_up = (znormal.y >= 0);
      elevation = fabs(elevation) * (is_up?-1:1);

      /////////////////////////

      QuatElevation.fromAxisAngle(fvec4(1,0,0,elevation));
      QuatHeading.fromAxisAngle(fvec4(0,1,0,heading));
      QuatC = QuatElevation.multiply(QuatHeading);
    }
    else{

      fvec3 xnormal = VMATRIX.xNormal();
      fvec3 ynormal = VMATRIX.yNormal();
      fvec3 znormal = VMATRIX.zNormal();

      fmtx4 matrot, imatrot;
      matrot.fromNormalVectors(xnormal, ynormal, znormal);
      imatrot.inverseOf(matrot);
      QuatC.fromMatrix(imatrot);
    }

    mfLoc = (tgt-eye).magnitude();
    mvCenter = tgt;
    updateMatrices();

}

///////////////////////////////////////////////////////////////////////////////

void EzUiCam::describeX(object::ObjectClass* clazz) {
  clazz->floatProperty("Aperature", float_range{0.0f, 90.0f}, &EzUiCam::_fov);
  clazz->floatProperty("MaxFar", float_range{1.0f, 100000.0f}, &EzUiCam::far_max);
  clazz->floatProperty("MinNear", float_range{0.01f, 10000.0f}, &EzUiCam::near_min);

  // temporary until old mox files converted
  ork::rtti::Class::CreateClassAlias("Camera_persp", GetClassStatic());
}

///////////////////////////////////////////////////////////////////////////////

struct UiCamPrivate {
  UiCamPrivate() {
    _material = std::make_shared<FreestyleMaterial>();
    FxPipelinePermutation perm;
    _materialinst_mono   = std::make_shared<FxPipeline>(perm); // _material
    perm._stereo = true;
    _materialinst_stereo = std::make_shared<FxPipeline>(perm); // _material

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
  fxpipeline_ptr_t _materialinst_mono;
  fxpipeline_ptr_t _materialinst_stereo;
};
using uicamprivate_t = std::shared_ptr<UiCamPrivate>;

///////////////////////////////////////////////////////////////////////////////

EzUiCam::EzUiCam()
    : UiCamera()
    , meRotMode(EROT_SCREENXY)
    , _fov(40*DTOR)
    , tx(0.0f)
    , ty(0.0f)
    , tz(0.0f)
    , far_max(10000.0f)
    , near_min(0.01f)
    , mDoRotate(false)
    , mDoDolly(false)
    , mDoPan(false)
    , mDoZoom(false)
    , _constrainZ(false)  {
  _camcamdata.Persp(1.0f, 1000.0f, 70.0f);
  _camcamdata.Lookat(fvec3(0.0f, 0.0f, 0.0f), fvec3(0.0f, 0.0f, 1.0f), fvec3(0.0f, 1.0f, 0.0f));
  type_name     = "Perspective";
  instance_name = "Default";

  mfWorldSizeAtLocator = 150.0f;

  auto uicampriv = std::make_shared<UiCamPrivate>();
  _private.set<uicamprivate_t>(uicampriv);

  _pushNX = fvec3(1,0,0);
  _pushNY = fvec3(0,1,0);
  _pushNZ = fvec3(0,0,1);

  fquat QuatX, QuatY;
  QuatX.fromAxisAngle(fvec4(_pushNX, -15*DTOR));
  QuatY.fromAxisAngle(fvec4(_pushNY, 180*DTOR));
  QuatElevation = QuatElevation.multiply(QuatX);
  QuatHeading = QuatHeading.multiply(QuatY);
  QuatC = QuatElevation.multiply(QuatHeading);

}

///////////////////////////////////////////////////////////////////////////////

void EzUiCam::draw(Context* context) const {
  extern fvec4 TRayN;
  extern fvec4 TRayF;

  auto priv = _private.get<uicamprivate_t>();

  priv->gpuUpdate(context);

  float CurVelMag = MeasuredCameraVelocity.magnitude();
  //////////////////////////////////////
  /*context->MTXI()->PushUIMatrix();
  {

    context->PushModColor(fcolor4::Black());
    ork::lev2::FontMan::PushFont("i14");
    FontMan::beginTextBlock(context);
    FontMan::DrawText(context, 41, 9, "Center %f %f %f", mvCenter.x, mvCenter.y, mvCenter.z);
    FontMan::DrawText(context, 41, 21, "CamLoc   %f %f %f", CamLoc.x, CamLoc.y, CamLoc.z);
    FontMan::DrawText(context, 41, 33, "zf %f", (_camcamdata.GetFar()));
    FontMan::DrawText(context, 41, 45, "zn %f", (_camcamdata.GetNear()));
    FontMan::DrawText(context, 41, 57, "zfoverzn %f", (_camcamdata.GetFar() / _camcamdata.GetNear()));
    FontMan::DrawText(context, 41, 69, "Loc(m) %f Speed(m/f) %f", mfLoc, CurVelMag);
    FontMan::DrawText(context, 41, 81, "RotMode %s", (meRotMode == EROT_SCREENZ) ? "ScreenZ" : "ScreenXY");
    FontMan::DrawText(context, 41, 93, "Aper %f", _fov);
    FontMan::DrawText(context, 41, 105, "Name %s", GetName().c_str());
    FontMan::endTextBlock(context);
    context->PopModColor();

    context->PushModColor(fcolor4::Yellow());
    FontMan::beginTextBlock(context);
    FontMan::DrawText(context, 41, 9, "Center %f %f %f", mvCenter.x, mvCenter.y, mvCenter.z);
    FontMan::DrawText(context, 41, 21, "CamLoc   %f %f %f", CamLoc.x, CamLoc.y, CamLoc.z);
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
  //_curMatrices = _camcamdata.computeMatrices(aspect);
  auto RCFD    = context->topRenderContextFrameData();
  lev2::RenderContextInstData RCID(RCFD);
  fmtx4 worldmtx;
  worldmtx.setTranslation(mvCenter);
  float Scale = mfLoc / 60.0f;
  worldmtx.scale(fvec4(Scale, Scale, Scale));
  ///////////////////////////////////////////////////////////////
  context->debugPushGroup("EzUiCam::draw");
  auto mtlinst = RCFD->isStereo() //
                     ? priv->_materialinst_stereo
                     : priv->_materialinst_mono;
  mtlinst->wrappedDrawCall(RCID, [context]() {
    auto& tricircle = GfxPrimitives::GetRef().mVtxBuf_TriCircle;
    auto& axis      = GfxPrimitives::GetRef().mVtxBuf_Axis;
    context->GBI()->DrawPrimitiveEML(tricircle,PrimitiveType::LINES);
    context->GBI()->DrawPrimitiveEML(axis,PrimitiveType::LINES);
  });
  context->debugPopGroup();
  ///////////////////////////////////////////////////////////////
}

///////////////////////////////////////////////////////////////////////////////

static fvec2 pmousepos;

///////////////////////////////////////////////////////////////////////////////

void EzUiCam::ZoomBegin(const CamEvTrackData& ed){
   //printf("ZoomBegin\n");
}
void EzUiCam::ZoomUpdate(const CamEvTrackData& ed){

}
void EzUiCam::ZoomEnd(){
   //printf("ZoomEnd\n");
}

///////////////////////////////////////////////////////////////////////////////

void EzUiCam::PanBegin(const CamEvTrackData& ed) {
   //printf("BeginPan\n");
  pmousepos = ork::lev2::logicalMousePos();
  // OrkGlobalDisableMousePointer();
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
  float fdx = -float(esx - ipushx);
  float fdy = float(esy - ipushy);

  mvCenter = ed.vPushCenter + (outx * fdx) + (outy * fdy);

  // QCursor::setPos(pmousepos);
}

///////////////////////////////////////////////////////////////////////////////

void EzUiCam::PanEnd() {
  //printf("EndPan\n");
  // QCursor::setPos(pmousepos);
  // OrkGlobalEnableMousePointer();
}

///////////////////////////////////////////////////////////////////////////////

void EzUiCam::RotBegin(const CamEvTrackData& ed) {
   //printf("BeginRot\n");

  // printf( "Rot: vPushNZ<%g %g %g>\n", vPushNZ.x, vPushNZ.y, vPushNZ.z );

  pmousepos = ork::lev2::logicalMousePos();

  // OrkGlobalDisableMousePointer();
}

///////////////////////////////////////////////////////////////////////////////

void EzUiCam::RotEnd() {
   //printf("EndRot\n");
  // QCursor::setPos(pmousepos);
  // OrkGlobalEnableMousePointer();
}

///////////////////////////////////////////////////////////////////////////////

void EzUiCam::DollyBegin(const CamEvTrackData& ed) {
   //printf("BeginDolly\n");
  pmousepos = ork::lev2::logicalMousePos();
  // OrkGlobalDisableMousePointer();
}

///////////////////////////////////////////////////////////////////////////////

void EzUiCam::DollyEnd() {
   //printf("EndDolly\n");
  // QCursor::setPos(pmousepos);
  // OrkGlobalEnableMousePointer();
  mDoDolly = false;
}

///////////////////////////////////////////////////////////////////////////////

bool EzUiCam::UIEventHandler(ui::event_constptr_t EV) {
  const ui::EventCooked& filtev = EV->mFilteredEvent;

  int evx    = filtev.miX;
  int evy    = filtev.miY;
  float fux  = filtev.mUnitX;
  float fuy  = filtev.mUnitY;
  float fpux = (fux * 2.0f) - 1.0f;
  float fpuy = (fuy * 2.0f) - 1.0f;
  fvec2 vpc = EV->xfToVpUnitCoord(fvec2(evx,evy));
  //float frad = sqrtf((vpc.x * vpc.x) + (vpc.y * vpc.y));
  //printf( "evx<%d> evy<%d> fux<%g> fuy<%g> fpux<%g> fpuy<%g>\n", evx, evy, fux, fuy, fpux, fpuy);
  fvec2 pos2D(fpux, fpuy);

  int state    = 0;
  bool isctrl  = filtev.mCTRL;
  bool isalt   = filtev.mALT;
  bool isshift = filtev.mSHIFT;
  bool ismeta  = filtev.mMETA;

  static int ipushx = 0;
  static int ipushy = 0;
  static f32 flerp  = 0.0f;

  bool do_wheel = false;

  _vpdim = EV->_vpdim;

  ///////////////////////////////////////////////////////////////

  auto on_begin = [&](){
      QuatCPushed = QuatC;

      // printf( "fx<%g %g> frad<%g>\n", fx, fy, frad);

      meRotMode = EROT_SCREENXY;

      CommonPostSetup();

      if(_constrainZ){
        _pushNX = fvec3(1,0,0);
        _pushNY = fvec3(0,1,0);
        //_pushNZ = fvec3(0,0,1);
        _pushNZ = _camcamdata.zNormal();
      }
      else{
        _pushNX = _camcamdata.xNormal();
        _pushNY = _camcamdata.yNormal();
        _pushNZ = _camcamdata.zNormal();
      }

      // printf( "nx <%g %g %g>\n", _pushNX.x, _pushNX.y, _pushNX.z );
      // printf( "ny <%g %g %g>\n", _pushNY.x, _pushNY.y, _pushNY.z );
      // printf( "nz <%g %g %g>\n", _pushNZ.x, _pushNZ.y, _pushNZ.z );

      fvec3 vrn, vrf;

      GenerateDepthRay(pos2D, vrn, vrf, _curMatrices.GetIVPMatrix());

      if (mDoRotate or mDoPan) {
        ////////////////////////////////////////////////////////
        // calculate planes with world rotation, but current view target as origin

        fvec4 Origin = mvCenter;
        _manipHandler.Init(pos2D, _curMatrices.GetIVPMatrix(), QuatC);
      }
      //////////////////////////////////////////////////

      _begin_evx = evx;
      _begin_evy = evy;
      ipushx = evx;
      ipushy = evy;

      if(mDoRotate or mDoPan or mDoZoom){
        mEvTrackData.vPushCenter = mvCenter;
        mEvTrackData.ipushX      = ipushx;
        mEvTrackData.ipushY      = ipushy;
      }

      if (mDoRotate)
        RotBegin(mEvTrackData);
      else if (mDoPan)
        PanBegin(mEvTrackData);
      else if (mDoZoom)
        ZoomBegin(mEvTrackData);

  };

  ///////////////////////////////////////////////////////////////

  auto on_end = [&](){
      CommonPostSetup();

      if (mDoDolly)
        DollyEnd();
      if (mDoPan)
        PanEnd();
      if (mDoRotate)
        RotEnd();
      if (mDoZoom)
        ZoomEnd();

      mDoPan    = false;
      mDoDolly  = false;
      mDoRotate = false;
      mDoZoom = false;


  };

  ///////////////////////////////////////////////////////////////

  switch (filtev._eventcode) {

    case EventCode::KEY_DOWN: {
      mDoPan = false;
      mDoZoom = false;
      mDoDolly = false;
      mDoRotate = false;

      switch(filtev.miKeyCode){
        case 'Z': {
          mDoRotate = true;
          isalt = true;
          on_begin();
          break;
        }
        case 'X': {
          mDoPan = true;
          on_begin();
          break;
        }
        case 'C': {
          mDoDolly = true;
          on_begin();
          break;
        }
        case 'V': {
          mDoZoom = true;
          on_begin();
          break;
        }
        default:
          break;
      }
      break;
    }
    case EventCode::KEY_UP: {
      on_end();
      break;
    }
    case EventCode::PUSH: {
      mDoRotate = filtev.mBut0;
      mDoPan = filtev.mBut1;
      mDoDolly = filtev.mBut2;
      mDoZoom = false;
      on_begin();
      break;
    }
    case EventCode::RELEASE: {
      on_end();
      break;
    }

    case EventCode::MOVE:
    case EventCode::DRAG: {

      if(_rotOnMove){
        if(mbInMotion==false){
          mbInMotion = true;
          on_begin();
        }
        mDoRotate = true;
      }

      //meRotMode = (frad > 0.35f) ? EROT_SCREENZ : EROT_SCREENXY;

      //////////////////////////////////////////////////
      // intersect ray with worlds XZ/XY/YZ planes

      //_manipHandler.Intersect(pos2D);

      float dx = float(evx - _begin_evx);
      float dy = float(evy - _begin_evy);
      float pdx = dx;
      float pdy = dy;
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

            if(not _constrainZ){
                float fvpx   = _vpdim.x;
                float fvpy   = _vpdim.y;
                float fvpwd2 = fvpx * 0.5f;
                float fvphd2 = fvpy * 0.5f;
                float fipx   = float(ipushx);
                float fipy   = float(ipushy);
                float fevx   = float(evx);
                float fevy   = float(evy);

                float fx0 = (fipx - fvpwd2) / fvpwd2;
                float fy0 = (fipy - fvphd2) / fvphd2;
                float fx1 = (fevx - fvpwd2) / fvpwd2;
                float fy1 = (fevy - fvphd2) / fvphd2;
                fvec2 v0(fx0, fy0);
                fvec2 v1(fx1, fy1);
                v0.normalizeInPlace();
                v1.normalizeInPlace();
                float ang0   = rect2pol_ang(v0.x, v0.y);
                float ang1   = rect2pol_ang(v1.x, v1.y);
                float dangle = (ang1 - ang0);
                fvec4 rotz   = fvec4(_pushNZ, -dangle);
                fquat QuatZ;
                QuatZ.fromAxisAngle(rotz);
                QuatC = QuatCPushed.multiply(QuatZ);
                // printf( "v0 <%g %g> v1<%g %g>\n", v0.x, v0.y, v1.x, v1.y );
                // printf( "ang0 <%g> ang1<%g>\n", ang0, ang1 );
                // printf( "rotz <%g %g %g %g>\n", rotz.x, rotz.y, rotz.z, rotz.w );
                // printf( "QuatZ <%g %g %g %g>\n", QuatZ.x, QuatZ.y, QuatZ.z, QuatZ.w );
                // printf( "QuatC <%g %g %g %g>\n", QuatC.x, QuatC.y, QuatC.z, QuatC.w );
            }
            break;
          }
          case EROT_SCREENXY: {

            fquat QuatX, QuatY;
            QuatX.fromAxisAngle(fvec4(_pushNX, -dy));
            QuatY.fromAxisAngle(fvec4(_pushNY, dx));

            //printf( "dx <%g> dy <%g> pdx<%g> pdy<%g>\n", dx, dy, pdx, pdy  );

            if(_constrainZ){
              QuatElevation = QuatElevation.multiply(QuatX);
              QuatHeading = QuatHeading.multiply(QuatY);
              QuatC = QuatElevation.multiply(QuatHeading);
            }
            else{
              QuatC = QuatC.multiply(QuatY);
              QuatC = QuatC.multiply(QuatX);
            }

            break;
          }
        }

        _begin_evx = evx;
        _begin_evy = evy;

      } else if (mDoDolly) {

        fvec3 outx, outy;

        _curMatrices.GetPixelLengthVectors(mvCenter, _vpdim, outx, outy);

        float fvl = ViewLengthToWorldLength(mvCenter, 1.0f);

        float fdolly = (outx.magnitude() * dy);

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

        _begin_evx = evx;
        _begin_evy = evy;
      } else if (mDoPan) {
        mEvTrackData.icurX = evx;
        mEvTrackData.icurY = evy;
        PanUpdate(mEvTrackData);
      }
      break;
    }
    case EventCode::MOUSEWHEEL: {
      mDoZoom = true;
      float zmoveamt = _base_zmoveamt;
      if (isctrl)
        zmoveamt *= 0.2f;
      else if (isshift)
        zmoveamt *= 5.0f;


      //printf( "mw - zmoveamt<%g> isalt<%d>\n", zmoveamt,int(isalt) );

      int mousedelta = EV->miMWY;

      #if defined(__APPLE__)
      if(isshift){
        mousedelta = EV->miMWX;
      }
      #endif

      if (isalt) {
        fvec4 Center = mvCenter;
        fvec4 Delta  = _pushNZ * zmoveamt * mousedelta;
        mvCenter += Delta;
      } else {
        fvec3 Pos = mvCenter;
        fvec3 UpVector;
        fvec3 RightVector;

        _curMatrices.GetPixelLengthVectors(Pos, _vpdim, UpVector, RightVector);

        //printf( "UpVector<%g %g %g>\n", UpVector.x, UpVector.y, UpVector.z );
        //printf( "RightVector<%g %g %g>\n", RightVector.x, RightVector.y, RightVector.z );

        float CameraFactor   = RightVector.magnitude() * 100.0f; // 20 pixels of movement
        mfLoc                = std::clamp(mfLoc, _loc_min, _loc_max);
        float DeltaInMeters  = float(-mousedelta) * CameraFactor * zmoveamt;
        //printf( "mousedelta<%d>\n", mousedelta );
        //printf( "CameraFactor<%g>\n", CameraFactor );
        //printf( "DeltaInMeters<%g>\n", DeltaInMeters );
        mfLoc += DeltaInMeters;
        mfLoc = std::clamp(mfLoc, _loc_min, _loc_max);

      }

      //printf( "mfLoc<%g>\n", mfLoc );

      mDoZoom = false;

      do_wheel = true;

      break;
    }
    default:
      break;
  }

  updateMatrices();

  return (mDoPan || mDoRotate || mDoDolly || do_wheel);
}

///////////////////////////////////////////////////////////////////////////////

void EzUiCam::SetFromWorldSpaceMatrix(const fmtx4& matrix) {
  ork::fvec3 xnormal = matrix.xNormal();
  ork::fvec3 ynormal = matrix.yNormal();
  ork::fvec3 znormal = matrix.zNormal();

  fmtx4 matrot, imatrot;
  matrot.fromNormalVectors(xnormal, ynormal, znormal);
  imatrot.inverseOf(matrot);

  fquat quat;
  quat.fromMatrix(imatrot);

  fvec3 pos = matrix.translation();

  printf("SetQuatc:1\n");
  QuatC    = quat;
  mvCenter = pos + fvec3(0.0f, 0.0f, mfLoc).transform3x3(matrot);

  updateMatrices();
}

///////////////////////////////////////////////////////////////////////////////

void EzUiCam::updateMatrices(void) {
  if (mfLoc < _loc_min)
    mfLoc = _loc_min;
  if (mfLoc > _loc_max)
    mfLoc = _loc_max;

  // Parameters to define
  float base_near = mfLoc*0.01; // Base near plane distance
  float near_far_ratio = 10000.0; // Ratio between far and near plane distances

  // Calculate the logarithm of the location (mfLoc) to determine the dynamic range
  //float flog10 = log10(mfLoc);

  // Calculate the interpolation index based on the logarithm, adjusted for our dynamic range
  //float flerpidx = (flog10 + 1.0f) / 6.0f;
  //float finvlerpidx = 1.0f - flerpidx;
  // Adjust calculations for neardiv and farmul using the base near and ratio
  //float neardiv = (base_near * finvlerpidx + near_far_ratio * base_near) * flerpidx;
  //float farmul = (near_far_ratio * 0.5f * finvlerpidx + 0.5f / near_far_ratio) * flerpidx;

  // Calculate the near and far plane distances
  float fnear = base_near;
  //float ffar = mfLoc * farmul;
  //float fnear = base_near;
  float ffar = fnear * near_far_ratio;
  // Enforce minimum and maximum values for near and far plane distances
  //if (ffar > far_max) ffar = far_max;
  if (fnear < near_min) fnear = near_min;
  if (ffar > far_max) ffar = far_max;
  //if (ffar < (fnear+0.0001)) ffar = (fnear+0.0001);

  //float fratio = ffar/fnear;
  //printf( "mfLoc<%g> near_min<%g> far_max<%g> fnear<%g> ffar<%g> fratio<%g>\n", mfLoc, near_min, far_max, fnear, ffar, fratio );

  ///////////////////////////////////////////////////////////////

  mRot = QuatC.toMatrix();
  mTrans.setTranslation(mvCenter * -1.0f);

  fmtx4 matxf = fmtx4::multiply_ltor(mTrans,mRot);
  fmtx4 matixf;
  matxf.inverseOf(matxf);

  fvec3 veye    = fvec3(0.0f, 0.0f, -mfLoc).transform(matxf);
  fvec3 vtarget = fvec3(0.0f, 0.0f, 0.0f).transform(matxf);
  fvec3 vup     = fvec4(0.0f, 1.0f, 0.0f, 0.0f).transform(matxf).xyz();

  veye += _position_offset;
  vtarget += _position_offset;

  _camcamdata.Persp(fnear, ffar, _fov);
  _camcamdata.Lookat(veye, vtarget, vup);

  // printf("near<%g> far<%g> mfLoc<%g>\n", fnear, ffar, mfLoc);
  // printf("mvCenter<%g %g %g>\n", mvCenter.x, mvCenter.y, mvCenter.z);
   //printf("veye<%g %g %g>\n", veye.x, veye.y, veye.z);
   //printf("vtarget<%g %g %g>\n", vtarget.x, vtarget.y, vtarget.z);
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
  float distATnear    = (frustum.mNearCorners[1] - frustum.mNearCorners[0]).magnitude();
  float distATfar     = (frustum.mFarCorners[1] - frustum.mFarCorners[0]).magnitude();
  float depthscaler   = distATfar / distATnear;

  // get pos as a lerp from near to far
  float depthN     = frustum._nearPlane.pointDistance(pos);
  float depthF     = frustum._farPlane.pointDistance(pos);
  float depthRange = (camrayF - camrayN).magnitude();
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
  float fx = pos2D.x;
  float fy = pos2D.y;
  //////////////////////////////////////////
  fvec4 vWinN(fx, fy, 0.0f);
  fvec4 vWinF(fx, fy, 1.0f);
  fmtx4::unProject(_curMatrices.GetIVPMatrix(), vWinN, rayN);
  fmtx4::unProject(_curMatrices.GetIVPMatrix(), vWinF, rayF);
  //////////////////////////////////////////
}

///////////////////////////////////////////////////////////////////////////////////////////

}} // namespace ork::lev2
