#pragma once

#include <ork/file/path.h>
#include <ork/kernel/mutex.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/math/cvector4.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork::ent {
///////////////////////////////////////////////////////////////////////////////

typedef orkvector<lev2::CVtxBuffer<lev2::SVtxV12C4T16>*> TerVtxBuffersType;

struct HeightMap {
  HeightMap(int isx, int isz);
  ~HeightMap();

  bool Load(const ork::file::Path& pth);

  mutex& GetLock() { return mMutex; }

  void SetGridSize(int iw, int ih);
  void SetWorldSize(float fwsize, float fhsize) {
    mfWorldSizeX = fwsize;
    mfWorldSizeZ = fhsize;
  }

  void SetWorldHeight(float fh) { mWorldHeight = fh; }
  inline int CalcAddress(int ix, int iz) const { return (miGridSizeX * iz) + ix; }
  float GetMaxHeight() const { return mMax; }
  float GetMinHeight() const { return mMin; }
  float GetHeightRange() const { return mRange; }
  void ResetMinMax();
  int GetGridSizeX() const { return miGridSizeX; }
  int GetGridSizeZ() const { return miGridSizeZ; }

  float GetWorldSizeX() const { return mfWorldSizeX; }
  float GetWorldSizeZ() const { return mfWorldSizeZ; }

  float GetWorldHeight() const { return mWorldHeight; }

  bool CalcClosestAddress(const fvec3& to, float& outx, float& outz) const;

  float GetHeight(int ix, int iz) const;
  void SetHeight(int ix, int iz, float fh);

  const float* GetHeightData() const { return mHeightData.data(); }

  fvec3 Min() const;
  fvec3 Max() const;
  fvec3 Range() const;
  fvec3 XYZ(int iX, int iZ) const;
  fvec3 ComputeNormal(int iX, int iZ) const;
  void ReadSurface(bool bfilter, const fvec3& xyz, fvec3& pos, fvec3& nrm) const;

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
  uint16_t* _pu16 = nullptr;
};

///////////////////////////////////////////////////////////////////////////////

struct GradientSet {
  const orkmap<float, fvec4>* mGradientLo;
  const orkmap<float, fvec4>* mGradientHi;
  float mHeightLo;
  float mHeightHi;

  GradientSet() : mGradientLo(0), mGradientHi(0), mHeightLo(0.0f), mHeightHi(0.0f) {}

  fvec4 Lerp(float fu, float fv) const;
};

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::ent
///////////////////////////////////////////////////////////////////////////////
