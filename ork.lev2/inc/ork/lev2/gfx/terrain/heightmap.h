#pragma once

#include <ork/file/path.h>
#include <ork/kernel/mutex.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/math/cvector4.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////

typedef orkvector<CVtxBuffer<SVtxV12C4T16>*> TerVtxBuffersType;

struct HeightMap {
  HeightMap(int isx, int isz);
  ~HeightMap();

  bool Load(const ork::file::Path& pth);

  mutex& GetLock() {
    return mMutex;
  }

  void SetGridSize(int iw, int ih);
  void SetWorldSize(float fwsize, float fhsize) {
    mfWorldSizeX = fwsize;
    mfWorldSizeZ = fhsize;
  }

  void SetWorldHeight(float fh) {
    mWorldHeight = fh;
  }
  inline int CalcAddress(int ix, int iz) const {
    return (miGridSizeX * iz) + ix;
  }
  float GetMaxHeight() const {
    return mMax;
  }
  float GetMinHeight() const {
    return mMin;
  }
  float GetHeightRange() const {
    return mRange;
  }
  void ResetMinMax();
  int GetGridSizeX() const {
    return miGridSizeX;
  }
  int GetGridSizeZ() const {
    return miGridSizeZ;
  }

  float GetWorldSizeX() const {
    return mfWorldSizeX;
  }
  float GetWorldSizeZ() const {
    return mfWorldSizeZ;
  }

  float GetWorldHeight() const {
    return mWorldHeight;
  }

  float GetHeight(int ix, int iz) const;
  void SetHeight(int ix, int iz, float fh);

  const float* GetHeightData() const {
    return mHeightData.data();
  }

  fvec3 Min() const;
  fvec3 Max() const;
  fvec3 Range() const;
  fvec3 XYZ(int iX, int iZ) const;
  fvec3 ComputeNormal(int iX, int iZ) const;
  void ReadSurface(bool bfilter, const fvec3& xyz, fvec3& pos, fvec3& nrm) const;

  /////////////////////////////////////////////

  static HeightMap gdefhm;

  uint16_t* _pu16 = nullptr;
  uint64_t _hash  = 0;

  int miGridSizeX     = 0;
  int miGridSizeZ     = 0;
  float mfWorldSizeX  = 1.0f;
  float mfWorldSizeZ  = 1.0f;
  float mWorldHeight  = 1.0f;
  float mMin          = 0.0f;
  float mMax          = 0.0f;
  float mRange        = 0.0f;
  float mIndexToUnitX = 1.0f;
  float mIndexToUnitZ = 1.0f;

  mutex mMutex;
  orkvector<float> mHeightData;
};

///////////////////////////////////////////////////////////////////////////////

struct GradientSet {
  const orkmap<float, fvec4>* mGradientLo;
  const orkmap<float, fvec4>* mGradientHi;
  float mHeightLo;
  float mHeightHi;

  GradientSet()
      : mGradientLo(0)
      , mGradientHi(0)
      , mHeightLo(0.0f)
      , mHeightHi(0.0f) {
  }

  fvec4 lerp(float fu, float fv) const;
};

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
///////////////////////////////////////////////////////////////////////////////
