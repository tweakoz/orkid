////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
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

enum class EGrid2DDrawMode { ALL = 0, NONE, ORIGIN_AXIS_ONLY };

struct Grid2d {

  void updateMatrices(Context* pTARG, int iw, int ih);
  void Render(Context* pTARG, int iw, int ih);
  void pushMatrix(Context* pTARG);
  void popMatrix(Context* pTARG);

  Grid2d();

  fvec2 Snap(fvec2 inp) const;

  void ReCalc(int iw, int ih);

  float _visGridDiv;
  float _visGridHiliteDiv;
  float _visGridSize;

  fvec2 _center;
  float _extent;

  float _zoomX;
  float _zoomY;

  bool _snapCenter;

  fmtx4 _mtxOrtho;
  fvec2 _topLeft;
  fvec2 _botRight;
  float _aspect;
  float _zoomedExtentX;
  float _zoomedExtentY;

  fvec3 _baseColor;
  fvec3 _hiliteColor;

  EGrid2DDrawMode _drawmode = EGrid2DDrawMode::ALL;
};

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
