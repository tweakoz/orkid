////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/lev2/gfx/camera/cameradata.h>

namespace ork::lev2 {

struct RenderContextFrameData;
struct CameraMatrices;

///////////////////////////////////////////////////////////////////////////////

class Grid3d {
public:
  enum EGrid {
    EGRID_XY = 0,
    EGRID_XZ,
    EGRID_END,
  };

  void Calc(const CameraMatrices& matrices);
  void Render(RenderContextFrameData& RCFD) const;

  void SetGridMode(EGrid egrid) {
    _gridMode = egrid;
  }

  Grid3d();

private:
  EGrid _gridMode;
  float _visGridBase;
  float _visGridDiv;
  float _visGridHiliteDiv;
  float _gridDL;
  float _gridDR;
  float _gridDB;
  float _gridDT;
  float _visGridSize;
};

///////////////////////////////////////////////////////////////////////////////

struct Grid2d {

  void updateMatrices(Context* pTARG, int iw, int ih);
  void Render(Context* pTARG, int iw, int ih);

  Grid2d();

  fvec2 Snap(fvec2 inp) const;

  void ReCalc(int iw, int ih);

  bool _snapCenter;
  fmtx4 _mtxOrtho;
  float _visGridDiv;
  float _visGridHiliteDiv;
  float _zoomX;
  float _zoomY;
  float _visGridSize;
  float _extent;
  fvec2 _center;
  fvec2 _topLeft;
  fvec2 _botRight;
  float _aspect;
  float _zoomedExtentX;
  float _zoomedExtentY;
  bool _bipolar;
  fvec3 _baseColor;
  fvec3 _hiliteColor;

};

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
