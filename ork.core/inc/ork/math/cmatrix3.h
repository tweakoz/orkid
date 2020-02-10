////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once
///////////////////////////////////////////////////////////////////////////////

#include <ork/math/quaternion.h>
#include <ork/orktypes.h>

#include <ork/config/config.h>

///////////////////////////////////////////////////////////////////////////////

namespace ork {

template <typename T> class Vector4;
template <typename T> class Vector3;
template <typename T> class Quaternion;

template <typename T> class Matrix33 {
  friend class Vector4<T>;

public:
  typedef T value_type;

  ////////////////

  T elements[3][3];

  Matrix33(const Matrix33<T>& m) {
    for (int i = 0; i < 3; i++) {
      for (int j = 0; j < 3; j++) {
        elements[i][j] = m.elements[i][j];
      }
    }
  }
  Matrix33(const Quaternion<T>& m);
  ////////////////

  Matrix33(void) { SetToIdentity(); }

  ~Matrix33() {}

  /////////

  void SetToIdentity(void);

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

  /////////

  Matrix33<T> Mult(T scalar) const;
  Matrix33<T> MatrixMult(const Matrix33<T>& mat1) const;

  inline Matrix33<T> operator*(const Matrix33<T>& mat) const { return MatrixMult(mat); }

  void Transpose(void);
  void InverseTranspose();
  void Inverse(void);
  void Normalize(void);

  void CorrectionMatrix(const Matrix33<T>& from, const Matrix33<T>& to);
  void SetRotation(const Matrix33<T>& from);
  void SetScale(const Matrix33<T>& from);

  void Lerp(const Matrix33<T>& from, const Matrix33<T>& to, T par); // par 0.0f .. 1.0f

  void decompose(Quaternion<T>& rot, T& Scale) const;
  void compose(const Quaternion<T>& rot, const T& Scale);

  ////////////////

  void SetElemYX(int ix, int iy, T val);
  T GetElemYX(int ix, int iy) const;
  void SetElemXY(int ix, int iy, T val);
  T GetElemXY(int ix, int iy) const;

  ////////////////

  void dump(STRING name);

  inline bool operator==(const Matrix33<T>& b) const {
    bool beq = true;
    for (int i = 0; i < 3; i++) {
      for (int j = 0; j < 3; j++) {
        if (elements[i][j] != b.elements[i][j]) {
          beq = false;
        }
      }
    }
    return beq;
  }
  inline bool operator!=(const Matrix33<T>& b) const {
    bool beq = true;
    for (int i = 0; i < 3; i++) {
      for (int j = 0; j < 3; j++) {
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

  Vector3<T> GetRow(int irow) const;
  Vector3<T> GetColumn(int icol) const;
  void SetRow(int irow, const Vector3<T>& v);
  void SetColumn(int icol, const Vector3<T>& v);

  Vector3<T> GetXNormal(void) const { return GetColumn(0); }
  Vector3<T> GetYNormal(void) const { return GetColumn(1); }
  Vector3<T> GetZNormal(void) const { return GetColumn(2); }

  void fromNormalVectors(const Vector3<T>& xv, const Vector3<T>& yv, const Vector3<T>& zv);
  void toNormalVectors(Vector3<T>& xv, Vector3<T>& yv, Vector3<T>& zv) const;

  ///////////////////////////////////////////////////////////////////////////////

  static const Matrix33<T> Identity;
  std::string dumpcn() const;

  T* GetArray(void) const { return (T*)&elements[0][0]; }

  ///////////////////////////////////////////////////////////////////////////////
};

typedef Matrix33<float> fmtx3;

///////////////////////////////////////////////////////////////////////////////

} // namespace ork
