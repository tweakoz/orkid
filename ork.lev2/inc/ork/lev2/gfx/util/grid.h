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
    meGridMode = egrid;
  }

  float GetVisGridBase() const {
    return mVisGridBase;
  }
  float GetVisGridDiv() const {
    return mVisGridDiv;
  }
  float GetVisGridSize() const {
    return mVisGridSize;
  }

  Grid3d();

private:
  EGrid meGridMode;
  float mVisGridBase;
  float mVisGridDiv;
  float mVisGridHiliteDiv;
  float mGridDL;
  float mGridDR;
  float mGridDB;
  float mGridDT;
  float mVisGridSize;
};

///////////////////////////////////////////////////////////////////////////////

struct Grid2d {
public:
  void updateMatrices(Context* pTARG, int iw, int ih);
  void Render(Context* pTARG, int iw, int ih);

  float GetVisGridBase() const {
    return mVisGridBase;
  }
  float GetVisGridDiv() const {
    return mVisGridDiv;
  }
  float GetVisGridSize() const {
    return mVisGridSize;
  }

  Grid2d();

  fvec2 Snap(fvec2 inp) const;

  float GetZoom() const {
    return mZoom;
  }
  float GetExtent() const {
    return mExtent;
  }
  const fvec2& GetCenter() const {
    return mCenter;
  }

  void SetZoom(float fz);
  void SetExtent(float fz);
  void SetCenter(const fvec2& ctr);

  const fmtx4& GetOrthoMatrix() const {
    return mMtxOrtho;
  }

  const fvec2& GetTopLeft() const {
    return mTopLeft;
  }
  const fvec2& GetBotRight() const {
    return mBotRight;
  }

  void ReCalc(int iw, int ih);

  fmtx4 mMtxOrtho;
  float mVisGridBase;
  float mVisGridDiv;
  float mVisGridHiliteDiv;
  // float				mGridDL;
  // float				mGridDR;
  // float				mGridDB;
  // float				mGridDT;
  float mZoom;
  float mVisGridSize;
  float mExtent;
  fvec2 mCenter;
  fvec2 mTopLeft;
  fvec2 mBotRight;
};

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
