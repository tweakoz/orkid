////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

////////////////////////////////////////////////////////////////////////////////

#include <ork/math/frustum.h>
#include <ork/math/cmatrix4.h>
#include <ork/lev2/lev2_types.h>

////////////////////////////////////////////////////////////////////////////////

namespace ork::lev2 {
class UiCamera;
struct CameraMatrices;

////////////////////////////////////////////////////////////////////////////////

struct CameraData {

  CameraData();

  CameraMatrices computeMatrices(float faspect = 1.0f) const;

  const fvec3& GetEye() const;
  const fvec3& GetTarget() const;
  const fvec3& GetUp() const;
  float GetNear() const;
  float GetFar() const;
  float GetAperature() const;
  const fvec3& xNormal() const;
  const fvec3& yNormal() const;
  const fvec3& zNormal() const;
  void setXNormal(const fvec3& n);
  void setYNormal(const fvec3& n);
  void setZNormal(const fvec3& n);

  void Lookat(const fvec3& eye, const fvec3& tgt, const fvec3& up);

  lev2::UiCamera* getUiCamera() const;
  void setUiCamera(lev2::UiCamera* pcam);

  void Persp(float fnear, float ffar, float faper /*degrees*/);
  void PerspH(float fnear, float ffar, float faperh /*degrees*/);

  void Ortho(float left, float right, float top, float bottom,float fnear, float ffar);

  fvec3 mEye;
  fvec3 mTarget;
  fvec3 mUp;

  fvec3 _xnormal;
  fvec3 _ynormal;
  fvec3 _znormal;

  float _left = -1;
  float _right = 1;
  float _top = -1;
  float _bottom = 1;

  lev2::UiCamera* _uiCamera = nullptr;
  float mAper               = 0.0f;
  float mHorizAper          = 0.0f;
  float mNear               = 0.0f;
  float mFar                = 0.0f;

  bool _is_ortho = false;
};

////////////////////////////////////////////////////////////////////////////////

struct CameraDataLut{


  CameraDataLut();
  ~CameraDataLut();

  using map_t = std::unordered_map<std::string, cameradata_constptr_t>;

  cameradata_ptr_t create(std::string named);

  cameradata_constptr_t& operator[] (const std::string& named);
  cameradata_constptr_t find(const std::string& named) const;
  size_t size() const;
  void clear();
  map_t::iterator begin();
  map_t::const_iterator begin() const;
  map_t::iterator end();
  map_t::const_iterator end() const;
  std::atomic<int> _state;

  map_t _lut;
};

////////////////////////////////////////////////////////////////////////////////

struct CameraMatrices {
  ////////////////////////////////////////////////////////////////////
  const Frustum& GetFrustum() const;
  Frustum& GetFrustum();
  ////////////////////////////////////////////////////////////////////
  // Get vectors whos length equals one pixel
  //  given a worldpos (hopefully within the camera's frustum)
  //  given viewport dimensions in
  ////////////////////////////////////////////////////////////////////
  void GetPixelLengthVectors(const fvec3& Pos, const fvec2& vp, fvec3& OutX, fvec3& OutY) const;
  ////////////////////////////////////////////////////////////////////
  // generate direction vector/origin (from 2d normalized screen coordinate)
  ////////////////////////////////////////////////////////////////////
  void projectDepthRay(const fvec2& v2d, fvec3& vdir, fvec3& vori) const;
  void projectDepthRay(const fvec2& v2d, fray3& ray_out) const;
  ////////////////////////////////////////////////////////////////////
  const fmtx4& GetIVMatrix() const;
  const fmtx4& GetVMatrix() const;
  const fmtx4& GetPMatrix() const;
  const fmtx4& GetIVPMatrix() const;
  const fmtx4& GetVPMatrix() const;
  ////////////////////////////////////////////////////////////////////
  float GetAspect() const;
  fmtx4 VPMONO() const;
  fmtx4 MVPMONO(const fmtx4& M) const;
  ////////////////////////////////////////////////////////////////////
  void setCustomView(const ork::fmtx4& view);
  void setCustomProjection(const ork::fmtx4& proj);
  ////////////////////////////////////////////////////////////////////
  bool _explicitProjectionMatrix = false;
  bool _explicitViewMatrix       = false;
  float _aspectRatio             = 1.0f;
  CameraData _camdat;
  fmtx4 _vmatrix;
  fmtx4 _pmatrix;
  fmtx4 _vpmatrix;
  fmtx4 _ivpmatrix;
  fmtx4 _ivmatrix;
  fmtx4 _ipmatrix;
  Frustum _frustum;
};

////////////////////////////////////////////////////////////////////////////////

struct StereoCameraMatrices {
  ////////////////////////////////////////////////////////////////////
  fmtx4 VL() const;
  fmtx4 VR() const;
  fmtx4 PL() const;
  fmtx4 PR() const;
  fmtx4 VPL() const;
  fmtx4 VPR() const;
  fmtx4 VMONO() const;
  fmtx4 PMONO() const;
  fmtx4 VPMONO() const;
  ////////////////////////////////////////////////////////////////////
  fmtx4 MVPL(const fmtx4& M) const;
  fmtx4 MVPR(const fmtx4& M) const;
  fmtx4 MVPMONO(const fmtx4& M) const;
  ////////////////////////////////////////////////////////////////////
  const CameraMatrices* _left  = nullptr;
  const CameraMatrices* _right = nullptr;
  const CameraMatrices* _mono  = nullptr;
};

////////////////////////////////////////////////////////////////////////////////

} // namespace ork::lev2
