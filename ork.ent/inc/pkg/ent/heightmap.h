#pragma once

#include <ork/file/path.h>
#include <ork/kernel/mutex.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/math/cvector4.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork {
namespace ent {
///////////////////////////////////////////////////////////////////////////////

typedef orkvector<lev2::CVtxBuffer<lev2::SVtxV12C4T16> *> TerVtxBuffersType;

struct HeightMap {
  HeightMap(int isx, int isz);
  ~HeightMap();

  bool Load(const ork::file::Path &pth);

  mutex &GetLock() { return mMutex; }

  void SetGridSize(int iw, int ih);
  void SetWorldSize(float fwsize, float fhsize) {
    mfWorldSizeX = fwsize;
    mfWorldSizeZ = fhsize;
  }

  void SetWorldHeight(float fh) { mWorldHeight = fh; }
  inline int CalcAddress(int ix, int iz) const {
    return (miGridSizeX * iz) + ix;
  }
  float GetMaxHeight() const { return mMax; }
  float GetMinHeight() const { return mMin; }
  float GetHeightRange() const { return mRange; }
  void ResetMinMax();
  int GetGridSizeX() const { return miGridSizeX; }
  int GetGridSizeZ() const { return miGridSizeZ; }

  float GetWorldSizeX() const { return mfWorldSizeX; }
  float GetWorldSizeZ() const { return mfWorldSizeZ; }

  float GetWorldHeight() const { return mWorldHeight; }

  bool CalcClosestAddress(const CVector3 &to, float &outx, float &outz) const;

  float GetHeight(int ix, int iz) const;
  void SetHeight(int ix, int iz, float fh);

  const float *GetHeightData() const { return mHeightData.data(); }

  CVector3 Min() const;
  CVector3 Max() const;
  CVector3 Range() const;
  CVector3 XYZ(int iX, int iZ) const;
  CVector3 ComputeNormal(int iX, int iZ) const;
  void ReadSurface(bool bfilter, const CVector3 &xyz, CVector3 &pos,
                   CVector3 &nrm) const;

  /////////////////////////////////////////////

  static HeightMap gdefhm;

  int miGridSizeX;
  int miGridSizeZ;
  float mfWorldSizeX;
  float mfWorldSizeZ;
  float mWorldHeight;
  orkvector<float> mHeightData;
  float mMin;
  float mMax;
  float mRange;
  mutex mMutex;
  float mIndexToUnitX;
  float mIndexToUnitZ;
};

///////////////////////////////////////////////////////////////////////////////

struct GradientSet {
  const orkmap<float, CVector4> *mGradientLo;
  const orkmap<float, CVector4> *mGradientHi;
  float mHeightLo;
  float mHeightHi;

  GradientSet()
      : mGradientLo(0), mGradientHi(0), mHeightLo(0.0f), mHeightHi(0.0f) {}

  CVector4 Lerp(float fu, float fv) const;
};

///////////////////////////////////////////////////////////////////////////////
} // namespace ent
} // namespace ork
///////////////////////////////////////////////////////////////////////////////
