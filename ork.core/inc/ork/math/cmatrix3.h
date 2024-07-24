////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once
///////////////////////////////////////////////////////////////////////////////

#include <memory>
#include <ork/math/quaternion.h>
#include <ork/orktypes.h>

#include <ork/config/config.h>
#include <ork/math/math_types.h>

#include <glm/glm.hpp>

///////////////////////////////////////////////////////////////////////////////

namespace ork {

template <typename T> struct Vector4;
template <typename T> struct Matrix44;
template <typename T> struct Vector3;
template <typename T> struct Quaternion;

template <typename T> struct Matrix33 final 
  : public glm::mat<3, 3, T, glm::defaultp> {

  using base_t = glm::mat<3, 3, T, glm::defaultp>;
  using base44_t = glm::mat<4, 4, T, glm::defaultp>;

  constexpr static int knumelements = 9;
  using element_t = T;
  using value_type = T;

  //////////////////////////////////////////////////////
  // constructors/destructors
  //////////////////////////////////////////////////////

  Matrix33(const base_t& m) : base_t(m) {}
  Matrix33(const Matrix33<T>& m) : base_t(m) {}
  Matrix33(const Quaternion<T>& m);

  Matrix33() {
    setToIdentity();
  }

  ~Matrix33() {
  }

  //////////////////////////////////////////////////////
  // converters
  //////////////////////////////////////////////////////

  base_t asGlmMat3() const {
    return base_t(*this);
  }
  base44_t asGlmMat4() const {
    return base44_t(*this);
  }

  void fromQuaternion(Quaternion<T> quat);

  //////////////////////////////////////////////////////
  // operators
  //////////////////////////////////////////////////////

  inline bool operator==(const Matrix33<T>& b) const {
    const base_t& as_base = *this;
    const base_t& oth_as_base = b;
    return (as_base==oth_as_base);
  }
  inline bool operator!=(const Matrix33<T>& b) const {
    const base_t& as_base = *this;
    const base_t& oth_as_base = b;
    return (as_base!=oth_as_base);
  }

  //////////////////////////////////////////////////////
  // dumps
  //////////////////////////////////////////////////////

  void dump(STRING name);

  //////////////////////////////////////////////////////
  // math operations
  //////////////////////////////////////////////////////

  void setToIdentity();

  /////////

  void rotateOnX(T rad);
  void rotateOnY(T rad);
  void rotateOnZ(T rad);
  void setRotateX(T rad);
  void setRotateY(T rad);
  void setRotateZ(T rad);
  void setRotation(const Matrix33<T>& from);

  /////////

  void setScale(const Matrix33<T>& from);
  void setScale(const Vector4<T>& vec);
  void setScale(T x, T y, T z);
  void setScale(T s);
  void scale(const Vector4<T>& vec);
  void scale(T xscl, T yscl, T zscl);

  /////////

  Matrix33<T> multiply(T scalar) const;
  Matrix33<T> multiply(const Matrix33<T>& mat1) const;

  inline Matrix33<T> operator*(const Matrix33<T>& mat) const {
    return multiply(mat);
  }

  void transpose();
  void inverseTranspose();
  void inverse();
  void normalizeInPlace();

  void correctionMatrix(const Matrix33<T>& from, const Matrix33<T>& to);

  void lerp(const Matrix33<T>& from, const Matrix33<T>& to, T par); // par 0.0f .. 1.0f

  void decompose(Quaternion<T>& rot, T& Scale) const;
  void compose(const Quaternion<T>& rot, const T& Scale);

  ////////////////

  void setElemYX(int ix, int iy, T val);
  T elemYX(int ix, int iy) const;
  void setElemXY(int ix, int iy, T val);
  T elemXY(int ix, int iy) const;

  ////////////////


  ///////////////////////////////////////////////////////////////////////////////
  // Column/Row Accessors
  ///////////////////////////////////////////////////////////////////////////////

  Vector3<T> row(int irow) const;
  Vector3<T> column(int icol) const;
  void setRow(int irow, const Vector3<T>& v);
  void setColumn(int icol, const Vector3<T>& v);

  Vector3<T> xNormal() const {
    return column(0);
  }
  Vector3<T> yNormal() const {
    return column(1);
  }
  Vector3<T> zNormal() const {
    return column(2);
  }

  void fromNormalVectors(const Vector3<T>& xv, const Vector3<T>& yv, const Vector3<T>& zv);
  void toNormalVectors(Vector3<T>& xv, Vector3<T>& yv, Vector3<T>& zv) const;

  ///////////////////////////////////////////////////////////////////////////////

  static const Matrix33<T> Identity;
  std::string dumpcn() const;

  T* asArray() {
    const base_t& as_base = *this;
    return (T*)&as_base[0][0];
  }
  const T* asArray() const {
    const base_t& as_base = *this;
    return (T*)&as_base[0][0];
  }

  ///////////////////////////////////////////////////////////////////////////////
};

using fmtx3       = Matrix33<float>;
using dmtx3       = Matrix33<double>;
using fmtx3_ptr_t = std::shared_ptr<fmtx3>;
using dmtx3_ptr_t = std::shared_ptr<dmtx3>;

template <>                       //
struct use_custom_serdes<fmtx3> { //
  static constexpr bool enable = true;
};

///////////////////////////////////////////////////////////////////////////////

fmtx3 dmtx3_to_fmtx3(const dmtx3& dvec);
dmtx3 fmtx3_to_dmtx3(const fmtx3& dvec);

///////////////////////////////////////////////////////////////////////////////

} // namespace ork
