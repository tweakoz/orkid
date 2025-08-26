////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once
///////////////////////////////////////////////////////////////////////////////
#include <ork/lev2/gfx/gfxmaterial_test.h>

namespace ork::lev2 {

class PrimitivesInterface {
public:
  PrimitivesInterface(Context* pTarg);
  void gpuInit();

  ///////////////////////////////////////////////////////////////////////////////

  void RenderGridX100();
  void RenderCone();
  void RenderAxis();
  void RenderTriCircle();
  void RenderDiamond();
  void RenderRotManip();
  void RenderEQSphere();
  void RenderSkySphere();
  void RenderGroundPlane();
  void RenderPerlinTerrain();
  void RenderCircleStrip();
  void RenderCircleStripUI();
  void RenderCircleUI();
  void RenderDirCone();

  void RenderCylinder(bool drawoutline = false);
  void RenderCapsule(float radius);
  void RenderBox(bool drawoutline = false);
  void RenderAxisLineCone();
  void RenderAxisBox();

  CVtxBuffer<SVtxV12N12B12T8C4>& GetAxisVB(void) {
    return mVtxBuf_Axis;
  }
  CVtxBuffer<SVtxV12C4T16>& GetConeVB(void) {
    return mVtxBuf_Cone;
  }
  CVtxBuffer<SVtxV12C4T16>& GetGridVB(void) {
    return mVtxBuf_GridX100;
  }
  CVtxBuffer<SVtxV12C4T16>& GetGroundVB(void) {
    return mVtxBuf_GroundPlane;
  }
  CVtxBuffer<SVtxV12N12B12T8C4>& GetPerlinVB(void) {
    return mVtxBuf_PerlinTerrain;
  }
  CVtxBuffer<SVtxV12C4T16>& GetTriCircleVB(void) {
    return mVtxBuf_TriCircle;
  }
  CVtxBuffer<SVtxV12C4T16>& GetDiamondVB(void) {
    return mVtxBuf_Diamond;
  }
  CVtxBuffer<SVtxV12C4T16>& GetCircleStripVB(void) {
    return mVtxBuf_CircleStrip;
  }
  CVtxBuffer<SVtxV12C4T16>& GetCircleStripUI(void) {
    return mVtxBuf_CircleStripUI;
  }
  CVtxBuffer<SVtxV12C4T16>& GetCircleUI(void) {
    return mVtxBuf_CircleUI;
  }

  CVtxBuffer<SVtxV12N12B12T8C4>& GetAxisLineVB(void) {
    return mVtxBuf_AxisLine;
  }
  CVtxBuffer<SVtxV12N12B12T8C4>& GetAxisConeVB(void) {
    return mVtxBuf_AxisCone;
  }
  CVtxBuffer<SVtxV12N12B12T8C4>& GetAxisBoxVB(void) {
    return mVtxBuf_AxisBox;
  }
  CVtxBuffer<SVtxV12C4T16>& GetEQSphere(void) {
    return mVtxBuf_EQSphere;
  }
  CVtxBuffer<SVtxV12C4T16>& GetFullSphere(void) {
    return mVtxBuf_FullSphere;
  }

  ///////////////////////////////////////////////////////////////////////////////
  // other types of prims

  void RenderOrthoQuad(f32 fX1, f32 fX2, f32 fY1, f32 fY2, f32 iminU, f32 imaxU, f32 iminV, f32 imaxV);
  void RenderQuadAtX(f32 fY1, f32 fY2, f32 fZ1, f32 fZ2, f32 fX, f32 iminU, f32 imaxU, f32 iminV, f32 imaxV);
  void RenderQuadAtY(f32 fX1, f32 fX2, f32 fZ1, f32 fZ2, f32 fY, f32 iminU, f32 imaxU, f32 iminV, f32 imaxV);
  void RenderQuadAtZ(
      GfxMaterial* mtl, //
      f32 fX1,
      f32 fX2,
      f32 fY1,
      f32 fY2,
      f32 fZ,
      f32 iminU,
      f32 imaxU,
      f32 iminV,
      f32 imaxV,
      bool debug = false);
  void RenderQuadAtZV16T16C16(
      GfxMaterial* mtl, //
      f32 fX1,
      f32 fX2,
      f32 fY1,
      f32 fY2,
      f32 fZ,
      f32 iminU,
      f32 imaxU,
      f32 iminV,
      f32 imaxV);
  void RenderEMLQuadAtZV16T16C16(
      f32 fX1,
      f32 fX2,
      f32 fY1,
      f32 fY2,
      f32 fZ,
      f32 iminU,
      f32 imaxU,
      f32 iminV,
      f32 imaxV);

  void RenderQuad(fvec4& V0, fvec4& V1, fvec4& V2, fvec4& V3);

  ///////////////////////////////////////////////////////////////////////////////

private:
  Context* _context;

public:
  StaticVertexBuffer<SVtxV12N12B12T8C4> mVtxBuf_Axis;
  StaticVertexBuffer<SVtxV12N12B12T8C4> mVtxBuf_AxisBox;
  StaticVertexBuffer<SVtxV12N12B12T8C4> mVtxBuf_AxisCone;
  StaticVertexBuffer<SVtxV12N12B12T8C4> mVtxBuf_AxisLine;

  StaticVertexBuffer<SVtxV12C4T16> mVtxBuf_Box;

  StaticVertexBuffer<SVtxV12C4T16> mVtxBuf_Capsule;
  StaticVertexBuffer<SVtxV12C4T16> mVtxBuf_CircleStrip;
  StaticVertexBuffer<SVtxV12C4T16> mVtxBuf_CircleStripUI;
  StaticVertexBuffer<SVtxV12C4T16> mVtxBuf_CircleUI;
  StaticVertexBuffer<SVtxV12C4T16> mVtxBuf_Cone;
  StaticVertexBuffer<SVtxV12C4T16> mVtxBuf_Cylinder;

  StaticVertexBuffer<SVtxV12C4T16> mVtxBuf_Diamond;
  StaticVertexBuffer<SVtxV12C4T16> mVtxBuf_DirCone;
  StaticVertexBuffer<SVtxV12C4T16> mVtxBuf_Dome;

  StaticVertexBuffer<SVtxV12C4T16> mVtxBuf_EQSphere;
  StaticVertexBuffer<SVtxV12C4T16> mVtxBuf_FullSphere;
  StaticVertexBuffer<SVtxV12C4T16> mVtxBuf_GridX100;
  StaticVertexBuffer<SVtxV12C4T16> mVtxBuf_GroundPlane;

  StaticVertexBuffer<SVtxV12N12B12T8C4> mVtxBuf_PerlinTerrain;
  StaticVertexBuffer<SVtxV12C4T16> mVtxBuf_SkySphere;
  StaticVertexBuffer<SVtxV12C4T16> mVtxBuf_TriCircle;

  StaticVertexBuffer<SVtxV12C4T16> mVtxBuf_WireFrameBox;
  StaticVertexBuffer<SVtxV12C4T16> mVtxBuf_WireFrameCapsule;
  StaticVertexBuffer<SVtxV12C4T16> mVtxBuf_WireFrameCylinder;
  StaticVertexBuffer<SVtxV12C4T16> mVtxBuf_WireFrameDome;

  std::unique_ptr<GfxMaterial3DSolid> mMaterial;
};

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::lev2
