////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/math/quaternion.h>
#include <ork/orktypes.h>

#include <ork/config/config.h>

///////////////////////////////////////////////////////////////////////////////

namespace ork {

template <typename T> class Vector4;
template <typename T> class Vector3;
template <typename T> class Quaternion;
template <typename T> class Matrix33;

template <typename T> class Matrix44 {
  friend class Vector4<T>;

public:
  typedef T value_type;

  ////////////////

  T elements[4][4];

  Matrix44(const Matrix44<T>& m) {
    for (int i = 0; i < 4; i++) {
      for (int j = 0; j < 4; j++) {
        elements[i][j] = m.elements[i][j];
      }
    }
  }

  Matrix44(const Quaternion<T>& q);

  ////////////////

  Matrix44(void) {
    SetToIdentity();
  }

  ~Matrix44() {
  }

  /////////

  void SetToIdentity(void);

  /////////

  void SetTranslation(const Vector3<T>& vec);
  void SetTranslation(T x, T y, T z);
  Vector3<T> GetTranslation(void) const;
  void Translate(const Vector4<T>& vec);
  void Translate(T vx, T vy, T vz);

  /////////

  void RotateX(T rad);
  void RotateY(T rad);
  void RotateZ(T rad);
  void SetRotateX(T rad);
  void SetRotateY(T rad);
  void SetRotateZ(T rad);

  /////////

  void SetScale(const Vector4<T>& vec);
  void SetScale(T x, T y, T z);
  void SetScale(T s);
  void Scale(const Vector4<T>& vec);
  void Scale(T xscl, T yscl, T zscl);

  /////////

  void FromQuaternion(Quaternion<T> quat);
  void CreateBillboard(Vector3<T> objectPos, Vector3<T> viewPos, Vector3<T> upVec);

  /////////

  Matrix33<T> rotMatrix33() const;
  Matrix44<T> rotMatrix44() const;

  Matrix44<T> Mult(T scalar) const;
  Matrix44<T> MatrixMult(const Matrix44<T>& mat1) const;

  inline Matrix44<T> operator*(const Matrix44<T>& mat) const {
    return MatrixMult(mat);
  }

  Matrix44<T> Concat43(const Matrix44<T>& mat) const;
  Matrix44<T> Concat43Transpose(const Matrix44<T>& mat) const;

  void Transpose(void);
  void Normalize(void);
  void inverseOf(const Matrix44<T>& in);
  Matrix44<T> inverse() const;

  void CorrectionMatrix(const Matrix44<T>& from, const Matrix44<T>& to);
  void SetRotation(const Matrix44<T>& from);
  void SetTranslation(const Matrix44<T>& from);
  void SetScale(const Matrix44<T>& from);

  void Lerp(const Matrix44<T>& from, const Matrix44<T>& to, T par); // par 0.0f .. 1.0f

  void decompose(Vector3<T>& pos, Quaternion<T>& rot, T& Scale) const;
  void compose(const Vector3<T>& pos, const Quaternion<T>& rot, const T& Scale);

  ////////////////

  void SetElemYX(int ix, int iy, T val);
  T GetElemYX(int ix, int iy) const;
  void SetElemXY(int ix, int iy, T val);
  T GetElemXY(int ix, int iy) const;

  ////////////////

  void dump(std::string name) const;
  std::string dump(Vector3<T> color) const;
  std::string dump4x3(Vector3<T> color) const;
  std::string dump4x3cn() const;
  std::string dump() const;
  std::string dump4x3() const;

  inline bool operator==(const Matrix44<T>& b) const {
    bool beq = true;
    for (int i = 0; i < 4; i++) {
      for (int j = 0; j < 4; j++) {
        if (elements[i][j] != b.elements[i][j]) {
          beq = false;
        }
      }
    }
    return beq;
  }
  inline bool operator!=(const Matrix44<T>& b) const {
    bool beq = true;
    for (int i = 0; i < 4; i++) {
      for (int j = 0; j < 4; j++) {
        if (elements[i][j] != b.elements[i][j]) {
          beq = false;
        }
      }
    }
    return (false == beq);
  }

  ///////////////////////////////////////////////////////////////////////////////
  // Column/Row Accessors
  ///////////////////////////////////////////////////////////////////////////////

  Vector4<T> GetRow(int irow) const;
  Vector4<T> GetColumn(int icol) const;
  void SetRow(int irow, const Vector4<T>& v);
  void SetColumn(int icol, const Vector4<T>& v);

  Vector3<T> GetXNormal(void) const {
    return GetColumn(0).xyz();
  }
  Vector3<T> GetYNormal(void) const {
    return GetColumn(1).xyz();
  }
  Vector3<T> GetZNormal(void) const {
    return GetColumn(2).xyz();
  }

  void fromNormalVectors(const Vector3<T>& xv, const Vector3<T>& yv, const Vector3<T>& zv);
  void toNormalVectors(Vector3<T>& xv, Vector3<T>& yv, Vector3<T>& zv) const;

  ///////////////////////////////////////////////////////////////////////////////

  void Perspective(T fovy, T aspect, T near, T far);
  void Frustum(T left, T right, T top, T bottom, T nearval, T farval);
  void LookAt(T eyex, T eyey, T eyez, T centerx, T centery, T centerz, T upx, T upy, T upz);
  void LookAt(const Vector3<T>& eye, const Vector3<T>& ctr, const Vector3<T>& up);
  void Ortho(T left, T right, T top, T bottom, T fnear, T ffar);

  static bool UnProject(const Matrix44<T>& rIMVP, const Vector3<T>& ClipCoord, Vector3<T>& rVObj);
  static bool UnProject(const Vector4<T>& rVWin, const Matrix44<T>& rIMVP, const SRect& rVP, Vector3<T>& rVObj);

  static Matrix44<T> perspective(T fovy, T aspect, T near, T far) {
    Matrix44<T> rval;
    rval.Perspective(fovy, aspect, near, far);
    return rval;
  }

  static const Matrix44<T> Identity;

  T* GetArray(void) const {
    return (T*)&elements[0][0];
  }

  ///////////////////////////////////////////////////////////////////////////////
};

typedef Matrix44<float> fmtx4;
extern template class Matrix44<float>;
extern template class Matrix44<double>;

///////////////////////////////////////////////////////////////////////////////

} // namespace ork
