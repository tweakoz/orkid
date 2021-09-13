////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/renderer/rendercontext.h>

namespace ork { namespace lev2 {

MatrixStackInterface::MatrixStackInterface(Context& target)
    : mTarget(target)
    , miMatrixStackIndexP(0)
    , miMatrixStackIndexM(0)
    , miMatrixStackIndexV(0)
    , miMatrixStackIndexUI(0) {
}

///////////////////////////////////////////////////////////////////////////////

fmtx4 MatrixStackInterface::uiMatrix(float fw, float fh) const {
  if (mTarget.hiDPI()) {
    // iw /=2;
    // ih /=2;
  }
  return mTarget.MTXI()->Ortho(0.0f, fw, 0.0f, fh, 0.0f, 1.0f);
}

///////////////////////////////////////////////////////////////////////////////

void MatrixStackInterface::PushUIMatrix() {
  const RenderContextFrameData* pfdata = mTarget.topRenderContextFrameData();
  assert(pfdata);
  const auto& CPD = pfdata->topCPD();
  float fw        = float(CPD.GetDstRect()._w);
  float fh        = float(CPD.GetDstRect()._h);
  if (mTarget.hiDPI()) {
    fw *= 0.5f;
    fh *= 0.5f;
  }
  ork::fmtx4 mtxMVP = mTarget.MTXI()->Ortho(0.0f, fw, 0.0f, fh, 0.0f, 1.0f);
  PushPMatrix(mtxMVP);
  PushVMatrix(ork::fmtx4::Identity());
  PushMMatrix(ork::fmtx4::Identity());
}

///////////////////////////////////////////////////////////////////////////////

void MatrixStackInterface::PushUIMatrix(int iw, int ih) {
  ork::fmtx4 mtxMVP = uiMatrix(iw, ih);
  PushPMatrix(mtxMVP);
  PushVMatrix(ork::fmtx4::Identity());
  PushMMatrix(ork::fmtx4::Identity());
}

///////////////////////////////////////////////////////////////////////////////

void MatrixStackInterface::PopUIMatrix() {
  PopPMatrix();
  PopVMatrix();
  PopMMatrix();
}

///////////////////////////////////////////////////////////////////////////////

void MatrixStackInterface::OnMMatrixDirty(void) {
  const fmtx4& wmat = RefMMatrix();
  //
  mmMVMatrix  = wmat * RefVMatrix();
  mmMVPMatrix = wmat * mmVPMatrix;
  mmR3Matrix  = mmMVMatrix.rotMatrix33();
  // mmMVPMatrix.Transpose();
}

///////////////////////////////////////////////////////////////////////////////

void MatrixStackInterface::OnVMatrixDirty(void) {
  //////////////////////////////////////////////////////
  const fmtx4& VMatrix = RefVMatrix();
  mmMVMatrix           = RefMMatrix() * VMatrix;
  mmVPMatrix           = RefVMatrix() * RefPMatrix();
  //////////////////////////////////////////////////////
  const float* pfmatrix    = VMatrix.asArray();
  mVectorScreenRightNormal = fvec4(pfmatrix[0], pfmatrix[4], pfmatrix[8]);
  mVectorScreenUpNormal    = fvec4(pfmatrix[1], pfmatrix[5], pfmatrix[9]);
  //////////////////////////////////////////////////////
  mMatrixVIT.inverseOf(VMatrix);
  mMatrixVIT.Transpose();
  //////////////////////////////////////////////////////
  fmtx4 matiy;
  matiy.Scale(1.0f, -1.0f, 1.0f);
  mMatrixVITIY = mMatrixVIT * matiy;
  //////////////////////////////////////////////////////
  mMatrixVITG.inverseOf(VMatrix);
  mMatrixVITG.Transpose();
  //////////////////////////////////////////////////////
}

///////////////////////////////////////////////////////////////////////////////

void MatrixStackInterface::OnPMatrixDirty(void) {
  mmVPMatrix  = RefVMatrix() * RefPMatrix();
  mmMVPMatrix = RefMMatrix() * mmVPMatrix;
}

///////////////////////////////////////////////////////////////////////
// Matrix Stack

void MatrixStackInterface::PushMMatrix(const fmtx4& rMat) {
  OrkAssert(miMatrixStackIndexM < (kiMatrixStackMax - 1));
  maMatrixStackM[++miMatrixStackIndexM] = rMat;
  OnMMatrixDirty();
}

///////////////////////////////////////////////////////////////////////////////

void MatrixStackInterface::PopMMatrix(void) {
  OrkAssert(miMatrixStackIndexM > 0);
  miMatrixStackIndexM--;
  OnMMatrixDirty();
}

///////////////////////////////////////////////////////////////////////////////

void MatrixStackInterface::SetMMatrix(const fmtx4& rMat) {
  OrkAssert(miMatrixStackIndexM >= 0);
  OrkAssert(miMatrixStackIndexM < kiMatrixStackMax);
  maMatrixStackM[miMatrixStackIndexM] = rMat;
  OnMMatrixDirty();
}

///////////////////////////////////////////////////////////////////////////////

const fmtx4& MatrixStackInterface::RefMMatrix(void) const {
  OrkAssert(miMatrixStackIndexM >= 0);
  OrkAssert(miMatrixStackIndexM < kiMatrixStackMax);
  return maMatrixStackM[miMatrixStackIndexM];
}

///////////////////////////////////////////////////////

const fmtx3& MatrixStackInterface::RefR3Matrix(void) const {
  return mmR3Matrix;
}

///////////////////////////////////////////////////////

void MatrixStackInterface::PushVMatrix(const fmtx4& rMat) {
  OrkAssert(miMatrixStackIndexV < (kiMatrixStackMax - 1));
  maMatrixStackV[++miMatrixStackIndexV] = rMat;
  OnVMatrixDirty();
}

///////////////////////////////////////////////////////////////////////////////

void MatrixStackInterface::PopVMatrix(void) {
  OrkAssert(miMatrixStackIndexV > 0);
  fmtx4& rMat = maMatrixStackV[--miMatrixStackIndexV];
  OnVMatrixDirty();
}

///////////////////////////////////////////////////////////////////////////////

const fmtx4& MatrixStackInterface::RefVMatrix(void) const {
  OrkAssert(miMatrixStackIndexV >= 0);
  OrkAssert(miMatrixStackIndexV < kiMatrixStackMax);
  return maMatrixStackV[miMatrixStackIndexV];
}

///////////////////////////////////////////////////////////////////////////////

const fmtx4& MatrixStackInterface::RefVITMatrix(void) const {
  return mMatrixVIT;
}

///////////////////////////////////////////////////////////////////////////////

const fmtx4& MatrixStackInterface::RefVITIYMatrix(void) const {
  return mMatrixVITIY;
}

///////////////////////////////////////////////////////////////////////////////

const fmtx4& MatrixStackInterface::RefVITGMatrix(void) const {
  return mMatrixVITG;
}

///////////////////////////////////////////////////////////////////////////////

const fmtx4& MatrixStackInterface::RefMVMatrix(void) const {
  return mmMVMatrix;
}

///////////////////////////////////////////////////////////////////////////////

const fmtx4& MatrixStackInterface::RefMVPMatrix(void) const {
  return mmMVPMatrix;
}

///////////////////////////////////////////////////////////////////////////////

const fmtx4& MatrixStackInterface::RefVPMatrix(void) const {
  return mmVPMatrix;
}

///////////////////////////////////////////////////////

void MatrixStackInterface::PushPMatrix(const fmtx4& rMat) {
  if (miMatrixStackIndexP == 5) {
    // orkprintf( "yo\n" );
  }
  // orkprintf( "miMatrixStackIndexP<%d>\n", miMatrixStackIndexP );
  OrkAssert(miMatrixStackIndexP < (kiMatrixStackMax - 1));
  maMatrixStackP[++miMatrixStackIndexP] = rMat;
  OnPMatrixDirty();
}

///////////////////////////////////////////////////////////////////////////////

void MatrixStackInterface::PopPMatrix(void) {
  OrkAssert(miMatrixStackIndexP > 0);
  fmtx4& rMat = maMatrixStackP[--miMatrixStackIndexP];
  OnPMatrixDirty();
}

///////////////////////////////////////////////////////////////////////////////

const fmtx4& MatrixStackInterface::RefPMatrix(void) const {
  return maMatrixStackP[miMatrixStackIndexP];
}

///////////////////////////////////////////////////////////////////////////////

fmtx4 MatrixStackInterface::Persp(float fovy, float aspect, float fnear, float ffar) {
  fmtx4 mtx;
  OrkAssert(fnear >= 0.0f);

  if (ffar <= fnear) {
    ffar = fnear + 1;
  }

  float xmin, xmax, ymin, ymax;
  ymax = fnear * tanf(fovy * DTOR * 0.5f);
  ymin = -ymax;
  xmin = ymin * aspect;
  xmax = ymax * aspect;

  mtx = Frustum(xmin, xmax, ymax, ymin, fnear, ffar);

  // mtx.Transpose();
  // mtx.dump("ORK");
  return mtx;
}

///////////////////////////////////////////////////////////////////////////////
// Context::Frustum virtual virtual
///////////////////////////////////////////////////////////////////////////////

fmtx4 MatrixStackInterface::Frustum(float left, float right, float top, float bottom, float zn, float zf) // virtual
{
  fmtx4 rval;

  rval.SetToIdentity();

  float width  = right - left;
  float height = top - bottom;
  float depth  = (zf - zn);
  float two(2.0f);

  /////////////////////////////////////////////

  rval.SetElemYX(0, 0, float((two * zn) / width));
  rval.SetElemYX(1, 1, float((two * zn) / height));
  rval.SetElemYX(2, 2, float(-(zf + zn) / depth));
  rval.SetElemYX(3, 3, float(0.0f));

  rval.SetElemYX(0, 2, float((right + left) / width));
  rval.SetElemYX(1, 2, float((top + bottom) / height));
  rval.SetElemYX(2, 3, float(-(two * zf * zn) / depth));
  rval.SetElemYX(3, 2, float(-1.0f));

  return rval;
}

///////////////////////////////////////////////////////////////////////////////

fmtx4 MatrixStackInterface::LookAt(const fvec3& eye, const fvec3& tgt, const fvec3& up) const {
  fmtx4 rval;

  fvec3 zaxis = (eye - tgt).Normal();
  fvec3 xaxis = (up.Cross(zaxis)).Normal();
  fvec3 yaxis = zaxis.Cross(xaxis);

  rval.SetElemYX(0, 0, xaxis.x);
  rval.SetElemYX(1, 0, yaxis.x);
  rval.SetElemYX(2, 0, zaxis.x);

  rval.SetElemYX(0, 1, xaxis.y);
  rval.SetElemYX(1, 1, yaxis.y);
  rval.SetElemYX(2, 1, zaxis.y);

  rval.SetElemYX(0, 2, xaxis.z);
  rval.SetElemYX(1, 2, yaxis.z);
  rval.SetElemYX(2, 2, zaxis.z);

  rval.SetElemYX(0, 3, -xaxis.Dot(eye));
  rval.SetElemYX(1, 3, -yaxis.Dot(eye));
  rval.SetElemYX(2, 3, -zaxis.Dot(eye));

  return rval;
}

///////////////////////////////////////////////////////////////////////////////

}} // namespace ork::lev2
