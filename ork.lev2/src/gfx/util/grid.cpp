////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <math.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/gfxmaterial_test.h>
#include <ork/lev2/gfx/gfxprimitives.h>
#include <ork/lev2/gfx/renderer/rendercontext.h>
#include <ork/lev2/gfx/util/grid.h>
#include <ork/math/audiomath.h>
#include <ork/math/misc_math.h>
#include <ork/pch.h>

using namespace ork::audiomath;

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

namespace ork { namespace lev2 {

Grid3d::Grid3d()
    : _visGridBase(0.3f)
    , _visGridDiv(10.0f)
    , _visGridHiliteDiv(100.0f) {
}
///////////////////////////////////////////////////////////////////////////////
void Grid3d::Calc(const CameraMatrices& camdat) {
  const fvec4& vn0 = camdat.GetFrustum().mNearCorners[0];
  const fvec4& vn1 = camdat.GetFrustum().mNearCorners[1];
  const fvec4& vn2 = camdat.GetFrustum().mNearCorners[2];
  const fvec4& vn3 = camdat.GetFrustum().mNearCorners[3];
  const fvec4& vf0 = camdat.GetFrustum().mFarCorners[0];
  const fvec4& vf1 = camdat.GetFrustum().mFarCorners[1];
  const fvec4& vf2 = camdat.GetFrustum().mFarCorners[2];
  const fvec4& vf3 = camdat.GetFrustum().mFarCorners[3];
  fvec3 vr0        = (vf0 - vn0).Normal();
  fvec3 vr1        = (vf1 - vn1).Normal();
  fvec3 vr2        = (vf2 - vn2).Normal();
  fvec3 vr3        = (vf3 - vn3).Normal();

  /////////////////////////////////////////////////////////////////
  // get center ray

  fvec3 vnc = (vn0 + vn1 + vn2 + vn3) * float(0.25f);
  fvec3 vfc = (vf0 + vf1 + vf2 + vf3) * float(0.25f);
  fvec3 vrc = (vfc - vnc).Normal();

  fray3 centerray;
  centerray.mOrigin    = vnc;
  centerray.mDirection = vrc;

  /////////////////////////////////////////////////////////////////

  float itx0 = 0.0f, itx1 = 0.0f, ity0 = 0.0f, ity1 = 0.0f;

  /////////////////////////////////////////////////////////////////
  // calc intersection of frustum and XYPlane

  float DistToPlane(0.0f);

  if (_gridMode == EGRID_XY) {
    fplane3 XYPlane(fvec3(float(0.0f), float(0.0f), float(1.0f)), fvec3::Zero());
    fvec3 vc;
    bool bc = XYPlane.Intersect(centerray, DistToPlane);
    float isect_dist;
    bc             = XYPlane.Intersect(centerray, isect_dist, vc);
    float Restrict = DistToPlane * float(1.0f);
    // extend Grid to cover full viewport
    itx0 = vc.x - Restrict;
    itx1 = vc.x + Restrict;
    ity0 = vc.y - Restrict;
    ity1 = vc.y + Restrict;
  } else if (_gridMode == EGRID_XZ) {
    fplane3 XZPlane(fvec3(float(0.0f), float(1.0f), float(0.0f)), fvec3::Zero());
    fvec3 vc;
    bool bc = XZPlane.Intersect(centerray, DistToPlane);
    float isect_dist;
    bc             = XZPlane.Intersect(centerray, isect_dist, vc);
    float Restrict = DistToPlane * float(1.0f);
    // extend Grid to cover full viewport
    itx0 = vc.x - Restrict;
    itx1 = vc.x + Restrict;
    ity0 = vc.GetZ() - Restrict;
    ity1 = vc.GetZ() + Restrict;
  }

  /////////////////////////////////////////////////////////////////
  // get params for grid

  float fLEFT   = float(itx0);
  float fRIGHT  = float(itx1);
  float fTOP    = float(ity0);
  float fBOTTOM = float(ity1);

  float fWIDTH  = fRIGHT - fLEFT;
  float fHEIGHT = fBOTTOM - fTOP;
  float fASPECT = fHEIGHT / fWIDTH;

  float fLOG   = float(log_base(_visGridDiv, DistToPlane * _visGridBase));
  float fiLOG  = float(pow_base(_visGridDiv, float(floor(fLOG))));
  _visGridSize = fiLOG / _visGridDiv;

  if (_visGridSize < 10.0f)
    _visGridSize = 10.0f;

  // if( _visGridSize<float(0.5f) ) _visGridSize = float(0.5f);
  // if( _visGridSize>float(8.0f) ) _visGridSize = float(8.0f);

  _gridDL = _visGridSize * float(floor(fLEFT / _visGridSize));
  _gridDR = _visGridSize * float(ceil(fRIGHT / _visGridSize));
  _gridDT = _visGridSize * float(floor(fTOP / _visGridSize));
  _gridDB = _visGridSize * float(ceil(fBOTTOM / _visGridSize));
}

///////////////////////////////////////////////////////////////////////////////

void Grid3d::Render(RenderContextFrameData& FrameData) const {
  Context* pTARG = FrameData.GetTarget();

  // pTARG->MTXI()->PushPMatrix( FrameData.cameraMatrices()->GetPMatrix() );
  // pTARG->MTXI()->PushVMatrix( FrameData.cameraMatrices()->GetVMatrix() );
  pTARG->MTXI()->PushMMatrix(fmtx4::Identity());
  {
    static GfxMaterial3DSolid gridmat(pTARG);
    gridmat.SetColorMode(GfxMaterial3DSolid::EMODE_MOD_COLOR);
    gridmat._rasterstate.SetBlending(Blending::ADDITIVE);

    ////////////////////////////////
    // Grid

    float GridGrey(0.10f);
    float GridHili(0.20f);
    fvec4 BaseGridColor(GridGrey, GridGrey, GridGrey);
    fvec4 HiliGridColor(GridHili, GridHili, GridHili);

    pTARG->PushModColor(BaseGridColor);

    if (_gridMode == EGRID_XY) {
      for (float fX = _gridDL; fX <= _gridDR; fX += _visGridSize) {
        bool bORIGIN = (fX == 0.0f);
        bool bhi     = fmodf(fabs(fX), _visGridHiliteDiv) < Float::Epsilon();

        pTARG->PushModColor(bORIGIN ? fcolor4::Green() : (bhi ? HiliGridColor : BaseGridColor));
        // pTARG->IMI()->DrawLine( fvec4( fX, _gridDT, 0.0f, 1.0f ), fvec4( fX, _gridDB, 0.0f, 1.0f ) );
        // pTARG->IMI()->QueFlush( false );
        pTARG->PopModColor();
      }
      for (float fY = _gridDT; fY <= _gridDB; fY += _visGridSize) {
        bool bORIGIN = (fY == 0.0f);
        bool bhi     = fmodf(fabs(fY), _visGridHiliteDiv) < Float::Epsilon();

        pTARG->PushModColor(bORIGIN ? fcolor4::Red() : (bhi ? HiliGridColor : BaseGridColor));
        // pTARG->IMI()->DrawLine( fvec4( _gridDL, fY, 0.0f, 1.0f ), fvec4( _gridDR, fY, 0.0f, 1.0f ) );
        // pTARG->IMI()->QueFlush( false );
        pTARG->PopModColor();
      }
    } else if (_gridMode == EGRID_XZ) {
      for (float fX = _gridDL; fX <= _gridDR; fX += _visGridSize) {
        bool bORIGIN = (fX == 0.0f);
        bool bhi     = fmodf(fabs(fX), _visGridHiliteDiv) < Float::Epsilon();

        pTARG->PushModColor(bORIGIN ? fcolor4::Blue() : (bhi ? HiliGridColor : BaseGridColor));
        // pTARG->IMI()->DrawLine( fvec4( fX, 0.0f, _gridDT, 1.0f ), fvec4( fX, 0.0f, _gridDB, 1.0f ) );
        // pTARG->IMI()->QueFlush( false );
        pTARG->PopModColor();
      }
      for (float fY = _gridDT; fY <= _gridDB; fY += _visGridSize) {
        bool bORIGIN = (fY == 0.0f);
        bool bhi     = fmodf(fabs(fY), _visGridHiliteDiv) < Float::Epsilon();

        pTARG->PushModColor(bORIGIN ? fcolor4::Red() : (bhi ? HiliGridColor : BaseGridColor));
        // pTARG->IMI()->DrawLine( fvec4( _gridDL, 0.0f, fY, 1.0f ), fvec4( _gridDR, 0.0f, fY, 1.0f ) );
        // pTARG->IMI()->QueFlush( false );
        pTARG->PopModColor();
      }
    }
    // FontMan::FlushQue(pTARG);
    pTARG->PopModColor();
  }
  // pTARG->MTXI()->PopPMatrix();
  // pTARG->MTXI()->PopVMatrix();
  pTARG->MTXI()->PopMMatrix();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

Grid2d::Grid2d()
    : _visGridDiv(5.0f)
    , _visGridHiliteDiv(25.0f)
    , _visGridSize(10)
    , _center(0.0f, 0.0f)
    , _extent(100.0f)
    , _zoomX(1.0f)
    , _zoomY(1.0f)
    , _snapCenter(false)
    , _bipolar(false){

    float base_grey(0.10f);
    float hilite_grey(0.40f);
    _baseColor = fvec3(base_grey, base_grey, base_grey);
    _hiliteColor = fvec3(hilite_grey, hilite_grey, hilite_grey);

}

///////////////////////////////////////////////////////////////////////////////
/*
static const int kdiv_base = 240 * 4;
static const int kfreqmult = 5;
static const float klogbias = 0.3f; // adjust to change threshold of lod switch

struct grid_calculator
{
    int ispace;
    int il;
    int ir;
    int it;
    int ib;
    int ic;
    float flog;
    float filog;
    float flogb;

    grid_calculator( const Rectangle& devviewrect, float scale_factor )
    {
        static const float div_base = kdiv_base; // base distance between gridlines at unity scale

        auto log_base = []( float base, float inp ) ->float
        {
            float rval = logf( inp ) / logf( base );
            return rval;
        };
        auto pow_base = []( float base, float inp ) ->float
        {
            float rval = powf( base, inp );
            return rval;
        };

        const float kfbase = kfreqmult;
        flogb = log_base( kfbase, div_base );
        flog = log_base( kfbase, scale_factor );
        flog = klogbias+std::round(flog*10.0f)/10.0f;
        flog = (flog<1.0f) ? 1.0f : flog;
        filog = pow_base( kfbase, flogb+std::floor(flog) );
        filog = std::round(filog/kfbase)*kfbase;

        ispace = int(filog);
        il = (int(devviewrect.x-filog)/ispace)*ispace;
        ir = (int(devviewrect.x2+filog)/ispace)*ispace;
        it = (int(devviewrect.y-filog)/ispace)*ispace;
        ib = (int(devviewrect.y2+filog)/ispace)*ispace;
        ic = (ir-il)/ispace;
        printf( "scale_factor<%f> flog<%f> filog<%f> il<%d> ir<%d> ispace<%d> ic<%d>\n", scale_factor, flog, filog, il, ir, ispace,
ic );
    }


};
*/
////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
void Grid2d::ReCalc(int iw, int ih) {
  // float scale_factor = (dev_view_rect.y2 - dev_view_rect.y) / gmastercfg.ideviceH;
  // grid_calculator the_grid_calc(dev_view_rect,scale_factor);
  float ftW     = float(iw);
  float ftH     = float(ih);
  _aspect = ftH / ftW;

  _zoomedExtentX = _extent / _zoomX;
  _zoomedExtentY = _extent / _zoomY *_aspect;

  //float itx0 = _center.x - fnext * 0.5f;
  //float itx1 = _center.x + fnext * 0.5f;
  //float ity0 = _center.y - fnext * 0.5f * fASPECT;
  //float ity1 = _center.y + fnext * 0.5f * fASPECT;

  float fLOG   = log_base(_visGridDiv, _zoomedExtentX);
  float fiLOG  = pow_base(_visGridDiv, floor(fLOG));

  //printf( "grid fLOG<%g> fiLOG<%g>\n", fLOG, fiLOG );

  /////////////////////////////////////////////////////////////////
  // get params for grid
  /////////////////////////////////////////////////////////////////

  /*float fLEFT   = itx0;
  float fRIGHT  = itx1;
  float fTOP    = ity0;
  float fBOTTOM = ity1;

  float fWIDTH  = fRIGHT - fLEFT;
  float fHEIGHT = fBOTTOM - fTOP;

  //_visGridSize = clamp(fiLOG / _visGridDiv,10.0f,100.0f);

  //printf( "grid fLEFT<%g> fRIGHT<%g> fTOP<%g> fBOTTOM<%g> fLOG<%g> fiLOG<%g>\n", fLEFT, fRIGHT, fTOP, fBOTTOM, fLOG, fiLOG );
  */

  _topLeft.x = _center.x - _zoomedExtentX*0.5f;
  _topLeft.y= _center.y - _zoomedExtentY*0.5f;

  _botRight.x = _center.x + _zoomedExtentX*0.5f;
  _botRight.y = _center.y + _zoomedExtentY*0.5f;


  //printf( "grid _topLeft<%g %g> _botRight<%g %g>\n", _topLeft.x, _topLeft.y, _botRight.x, _botRight.y );
}
///////////////////////////////////////////////////////////////////////////////

fvec2 Grid2d::Snap(fvec2 inp) const {
  float ffx  = inp.x;
  float ffy  = inp.y;
  float ffxm = fmodf(ffx, _visGridSize);
  float ffym = fmodf(ffy, _visGridSize);

  bool blx = (ffxm < (_visGridSize * 0.5f));
  bool bly = (ffym < (_visGridSize * 0.5f));

  float fnx = blx ? (ffx - ffxm) : (ffx - ffxm) + _visGridSize;
  float fny = bly ? (ffy - ffym) : (ffy - ffym) + _visGridSize;

  return fvec2(fnx, fny);
}

///////////////////////////////////////////////////////////////////////////////

void Grid2d::updateMatrices(Context* pTARG, int iw, int ih) {
  auto mtxi = pTARG->MTXI();
  ReCalc(iw, ih);
  _mtxOrtho = mtxi->Ortho(_topLeft.x, _botRight.x, _botRight.y, _topLeft.y, 0.0f, 1.0f);
}

///////////////////////////////////////////////////////////////////////////////

void Grid2d::Render(Context* pTARG, int iw, int ih) {
  auto mtxi = pTARG->MTXI();

  lev2::DynamicVertexBuffer<lev2::SVtxV12C4T16>& VB = lev2::GfxEnv::GetSharedDynamicVB();

  mtxi->PushPMatrix(_mtxOrtho);
  mtxi->PushVMatrix(fmtx4::Identity());
  mtxi->PushMMatrix(fmtx4::Identity());
  {
    static GfxMaterial3DSolid gridmat(pTARG);
    gridmat.SetColorMode(GfxMaterial3DSolid::EMODE_VERTEX_COLOR);
    gridmat._rasterstate.SetBlending(Blending::ADDITIVE);

    ////////////////////////////////
    // Grid

    int inumx = ceil(_zoomedExtentX*2.0/_visGridSize);
    int inumy = ceil(_zoomedExtentY*2.0/_visGridSize);

    float ftW     = float(iw);
    float ftH     = float(ih);
    float fASPECT = ftH / ftW;

    int count = (inumx + inumy) * 16;

    //printf( "inumx<%d> inumy<%d> count<%d>\n", inumx, inumy, count );

    if (count) {

      float x1 = floor(_topLeft.x/_visGridSize)*_visGridSize;
      float x2 = ceil(_botRight.x/_visGridSize)*_visGridSize;
      float y1 = floor(_topLeft.y/_visGridSize)*_visGridSize;
      float y2 = ceil(_botRight.y/_visGridSize)*_visGridSize;



      lev2::VtxWriter<lev2::SVtxV12C4T16> vw;
      vw.Lock(pTARG, &VB,count);

      fvec2 uv0(0.0f, 0.0f);
      for (float fx = x1; fx <= x2; fx += _visGridSize) {
        bool bORIGIN = (fabs(fx)<0.01);
        bool bhi     = fmodf(fabs(fx), _visGridHiliteDiv) < Float::Epsilon();

        auto color = bORIGIN ? fcolor3::Green()*0.5 : (bhi ? _hiliteColor : _baseColor );
        u32 ucolor = color.GetVtxColorAsU32();
        ork::lev2::SVtxV12C4T16 v0(fvec3(fx, y1, 0.0f), uv0, ucolor);
        ork::lev2::SVtxV12C4T16 v1(fvec3(fx, y2, 0.0f), uv0, ucolor);
        vw.AddVertex(v0);
        vw.AddVertex(v1);
      }
      for (float fy = y1; fy <= y2; fy += _visGridSize) {
        bool bORIGIN = (fabs(fy)<0.01);
        bool bhi     = fmodf(fabs(fy), _visGridHiliteDiv) < Float::Epsilon();

        auto color = bORIGIN ? fcolor3::Red()*0.5 : (bhi ? _hiliteColor : _baseColor );
        u32 ucolor = color.GetVtxColorAsU32();
        ork::lev2::SVtxV12C4T16 v0(fvec3(x1, fy, 0.0f), uv0, ucolor);
        ork::lev2::SVtxV12C4T16 v1(fvec3(x2, fy, 0.0f), uv0, ucolor);
        vw.AddVertex(v0);
        vw.AddVertex(v1);
      }
      vw.UnLock(pTARG);
      pTARG->GBI()->DrawPrimitive(&gridmat, vw, ork::lev2::PrimitiveType::LINES);
    }
  }
  mtxi->PopPMatrix();
  mtxi->PopVMatrix();
  mtxi->PopMMatrix();
}

}} // namespace ork::lev2
