////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

namespace ork::lev2{

/// ////////////////////////////////////////////////////////////////////////////
/// ////////////////////////////////////////////////////////////////////////////
/// Matrix Stack State Interface
/// ////////////////////////////////////////////////////////////////////////////
/// ////////////////////////////////////////////////////////////////////////////

class MatrixStackInterface {
public:
  MatrixStackInterface(Context& target);

  fmtx4 uiMatrix(float w, float h) const;

  void PushMMatrix(const fmtx4& rMat);
  void PushVMatrix(const fmtx4& rMat);
  void PushPMatrix(const fmtx4& rMat);
  void SetMMatrix(const fmtx4& rMat);

  void PopMMatrix(void);
  void PopVMatrix(void);
  void PopPMatrix(void);

  const fmtx4& RefMMatrix(void) const;
  const fmtx3& RefR3Matrix(void) const;
  const fmtx4& RefVMatrix(void) const;
  const fmtx4& RefVITMatrix(void) const;
  const fmtx4& RefVITIYMatrix(void) const;
  const fmtx4& RefVITGMatrix(void) const;
  const fmtx4& RefPMatrix(void) const;

  const fmtx4& RefMVMatrix(void) const;
  const fmtx4& RefVPMatrix(void) const;
  const fmtx4& RefMVPMatrix(void) const;

  void OnMMatrixDirty(void);
  void OnVMatrixDirty(void);
  void OnPMatrixDirty(void);

  int GetMStackDepth(void) {
    return int(miMatrixStackIndexM);
  }
  int GetVStackDepth(void) {
    return int(miMatrixStackIndexV);
  }
  int GetPStackDepth(void) {
    return int(miMatrixStackIndexP);
  }

  fmtx4& GetUIOrthoProjectionMatrix(void) {
    return mUIOrthoProjectionMatrix;
  }

  virtual fmtx4 Ortho(float left, float right, float top, float bottom, float fnear, float ffar) = 0;
  virtual fmtx4 Persp(float fovy /*degrees*/, float aspect, float fnear, float ffar);
  virtual fmtx4 Frustum(float left, float right, float top, float bottom, float zn, float zf);
  virtual fmtx4 LookAt(const fvec3& eye, const fvec3& tgt, const fvec3& up) const;

  const fmtx4& GetOrthoMatrix(void) const {
    return mMatOrtho;
  }
  void SetOrthoMatrix(const fmtx4& mtx) {
    mMatOrtho = mtx;
  }

  ///////////////////////////////////////////////////////////////////////

  const fvec4& GetScreenRightNormal(void) {
    return mVectorScreenRightNormal;
  }
  const fvec4& GetScreenUpNormal(void) {
    return mVectorScreenUpNormal;
  }

  ///////////////////////////////////////////////////////////////////////

  void PushUIMatrix();
  void PushUIMatrix(int iw, int ih);
  void PopUIMatrix();

  ///////////////////////////////////////////////////////////////////////

protected:
  static const int kiMatrixStackMax = 16;

  Context& _target;

  int miMatrixStackIndexP;
  int miMatrixStackIndexV;
  int miMatrixStackIndexM;
  int miMatrixStackIndexUI;

  fmtx4 maMatrixStackP[kiMatrixStackMax];
  fmtx4 maMatrixStackM[kiMatrixStackMax];
  fmtx4 maMatrixStackV[kiMatrixStackMax];
  fmtx4 maMatrixStackUI[kiMatrixStackMax];

  fmtx4 mMatrixVIT;
  fmtx4 mMatrixVITIY;
  fmtx4 mMatrixVITG;

  fmtx4 mMatOrtho;

  fmtx3 mmR3Matrix;
  fmtx4 mmMVMatrix;
  fmtx4 mmVPMatrix;
  fmtx4 mmMVPMatrix;

  fmtx4 mUIOrthoProjectionMatrix;

  fvec4 mVectorScreenRightNormal;
  fvec4 mVectorScreenUpNormal;
};

} //namespace ork::lev2{
