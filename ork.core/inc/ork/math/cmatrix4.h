////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <memory>
#include <ork/math/quaternion.h>
#include <ork/orktypes.h>
#include <ork/math/math_types.h>

#include <ork/config/config.h>

#include <glm/glm.hpp>

///////////////////////////////////////////////////////////////////////////////

namespace ork {

template <typename T> struct Vector4;
template <typename T> struct Vector3;
template <typename T> struct Quaternion;
template <typename T> struct Matrix33;

template <typename T> struct Matrix44 final 
  : public glm::mat<4, 4, T, glm::defaultp> {

  using base_t = glm::mat<4, 4, T, glm::defaultp>;
  using base33_t = glm::mat<3, 3, T, glm::defaultp>;

  constexpr static int knumelements = 16;
  using element_t = T;
  typedef T value_type;

  //////////////////////////////////////////////////////
  // constructors/destructors
  //////////////////////////////////////////////////////

  Matrix44(const base_t& m) : base_t(m) {}
  Matrix44(const Matrix44<T>& m) : base_t(m) {}
  Matrix44(const Quaternion<T>& q);
  Matrix44(const kln::translator& t);
  Matrix44(const kln::rotor& r);
  Matrix44(const kln::motor& m);

  Matrix44() {
    setToIdentity();
  }

  ~Matrix44() {
  }

  //////////////////////////////////////////////////////
  // converters
  //////////////////////////////////////////////////////

  const base_t& asGlmMat4() const {
    return *this;
  }

  void fromQuaternion(Quaternion<T> quat);
  void createBillboard(Vector3<T> objectPos, Vector3<T> viewPos, Vector3<T> upVec);
  void createBillboard2(Vector3<T> objectPos, Vector3<T> viewPos, Vector3<T> upVec);

  //////////////////////////////////////////////////////
  // operators
  //////////////////////////////////////////////////////

  inline bool operator==(const Matrix44<T>& b) const {
    const base_t& as_base = *this;
    const base_t& oth_as_base = b;
    return (as_base==oth_as_base);
  }
  inline bool operator!=(const Matrix44<T>& b) const {
    const base_t& as_base = *this;
    const base_t& oth_as_base = b;
    return (as_base!=oth_as_base);
  }

  //////////////////////////////////////////////////////
  // dumps
  //////////////////////////////////////////////////////

  void dump(std::string name) const;
  std::string dump(Vector3<T> color) const;
  std::string dump4x3(Vector3<T> color) const;
  std::string dump4x3cn(bool do_axis_angle=false) const;
  std::string dump4x4cn() const;
  std::string dump() const;
  std::string dump4x3() const;

  //////////////////////////////////////////////////////
  // math operations
  //////////////////////////////////////////////////////

  void setToIdentity();

  ///////////////////////////////////////////////////////////////////////////////

  using array44_t = T[4][4];

  void setXY(const array44_t& val);
  void setYX(const array44_t& val);

  /////////

  void setTranslation(const Matrix44<T>& from);
  void setTranslation(const Vector3<T>& vec);
  void setTranslation(T x, T y, T z);
  Vector3<T> translation(void) const;
  void translate(const Vector4<T>& vec);
  void translate(T vx, T vy, T vz);

  /////////

  void rotateOnX(T rad);
  void rotateOnY(T rad);
  void rotateOnZ(T rad);
  void setRotateX(T rad);
  void setRotateY(T rad);
  void setRotateZ(T rad);
  void setRotation(const Matrix44<T>& from);

  /////////

  void setScale(const Matrix44<T>& from);
  void setScale(const Vector4<T>& vec);
  void setScale(T x, T y, T z);
  void setScale(T s);
  void scale(const Vector4<T>& vec);
  void scale(T xscl, T yscl, T zscl);

  /////////

  void toEulerXYZ(T& ex, T& ey, T& ez) const;
  void fromEulerXYZ(T ex, T ey, T ez);

  Matrix33<T> rotMatrix33() const;
  Matrix44<T> rotMatrix44() const;

  Matrix44<T> multiply(T scalar) const;

  ////////////////////////////////////
  // traditional matrix multiplication (rtol)
  ////////////////////////////////////

  Matrix44<T> multiply_rtol(const Matrix44<T>& mat1) const;
  
  inline Matrix44<T> operator*(const Matrix44<T>& mat) const {
    return multiply_rtol(mat);
  }

  ////////////////////////////////////
  // ltor matrix multiplication
  ////////////////////////////////////

  static Matrix44<T> multiply_ltor(const Matrix44<T>& a,const Matrix44<T>& b);
  static Matrix44<T> multiply_ltor(const Matrix44<T>& a,const Matrix44<T>& b, const Matrix44<T>& c);
  static Matrix44<T> multiply_ltor(const Matrix44<T>& a,const Matrix44<T>& b, const Matrix44<T>& c, const Matrix44<T>& d);
  static Matrix44<T> multiply_ltor(const Matrix44<T>& a,const Matrix44<T>& b, const Matrix44<T>& c,const Matrix44<T>& d, const Matrix44<T>& e );
  static Matrix44<T> fromOuterProduct(const Vector4<T>& c, const Vector4<T>& r);

  ////////////////////////////////////

  Matrix44<T> translationOnly() const;
  Matrix44<T> rotScaleOnly() const;

  void transpose();
  Matrix44<T> transposed() const;
  void normalizeInPlace();
  void inverseOf(const Matrix44<T>& in);
  Matrix44<T> inverse() const;

  void correctionMatrix(const Matrix44<T>& from, const Matrix44<T>& to);

  void lerp(const Matrix44<T>& from, const Matrix44<T>& to, T par); // par 0.0f .. 1.0f

  ////////////////

  void setElemYX(int ix, int iy, T val);
  T elemYX(int ix, int iy) const;
  void setElemXY(int ix, int iy, T val);
  T elemXY(int ix, int iy) const;

  T operator[](int i, int j) const;
  T& operator[](int i, int j);

  ///////////////////////////////////////////////////////////////////////////////
  // Column/Row Accessors
  ///////////////////////////////////////////////////////////////////////////////

  Vector4<T> row(int irow) const;
  Vector4<T> column(int icol) const;

  void setRow(int irow, const Vector4<T>& v);
  void setColumn(int icol, const Vector4<T>& v);
  void setRow(int irow, float a, float b, float c, float d);
  void setColumn(int icol, float a, float b, float c, float d);

  ///////////////////////////////////////////////////////////////////////////////
  // normal Accessors
  ///////////////////////////////////////////////////////////////////////////////

  Vector3<T> xNormal(void) const {
    return column(0).xyz();
  }
  Vector3<T> yNormal(void) const {
    return column(1).xyz();
  }
  Vector3<T> zNormal(void) const {
    return column(2).xyz();
  }

  void fromNormalVectors(const Vector3<T>& xv, const Vector3<T>& yv, const Vector3<T>& zv);
  void toNormalVectors(Vector3<T>& xv, Vector3<T>& yv, Vector3<T>& zv) const;

  ///////////////////////////////////////////////////////////////////////////////
  // composition/decomposition
  ///////////////////////////////////////////////////////////////////////////////

  bool decompose(Vector3<T>& pos, Quaternion<T>& rot, T& Scale) const;
  void compose(const Vector3<T>& pos, const Quaternion<T>& rot, const T& Scale);
  void compose(const Vector3<T>& pos, const Quaternion<T>& rot, const T& ScaleX, const T& ScaleY, const T& ScaleZ);
  void compose(const Vector3<T>& pos, const Quaternion<T>& rot, const Vector3<T>& scale);
  void compose2(const Vector3<T>& pos, const Quaternion<T>& rot, const T& Scale);
  void compose2(const Vector3<T>& pos, const Quaternion<T>& rot, const T& ScaleX, const T& ScaleY, const T& ScaleZ);

  void compose(const Vector3<T>& pos, const Quaternion<T>& rot);

  static Matrix44<T> composeFrom(const Vector3<T>& pos, const Quaternion<T>& rot, const Vector3<T>& scale);

  ///////////////////////////////////////////////////////////////////////////////
  // view/projection operations
  ///////////////////////////////////////////////////////////////////////////////

  void perspective(T fovy /*degrees*/, T aspect, T near, T far);
  void frustum(T left, T right, T top, T bottom, T nearval, T farval);
  void lookAt(T eyex, T eyey, T eyez, T centerx, T centery, T centerz, T upx, T upy, T upz);
  void lookAt(const Vector3<T>& eye, const Vector3<T>& ctr, const Vector3<T>& up);
  void ortho(T left, T right, T top, T bottom, T fnear, T ffar);

  static bool unProject(const Matrix44<T>& rIMVP, const Vector3<T>& ClipCoord, Vector3<T>& rVObj);
  static bool unProject(const Vector4<T>& rVWin, const Matrix44<T>& rIMVP, const SRect& rVP, Vector3<T>& rVObj);

  static Matrix44<T> createPerspectiveMatrix(T fovy /*degrees*/, T aspect, T near, T far) {
    Matrix44<T> rval;
    rval.perspective(fovy, aspect, near, far);
    return rval;
  }

  ///////////////////////////////////////////////////////////////////////////////

  static const Matrix44<T> Identity();

  T* asArray() {
    const base_t& as_base = *this;
    return (T*)&as_base[0][0];
  }
  const T* asArray() const {
    const base_t& as_base = *this;
    return (T*)&as_base[0][0];
  }

  ///////////////////////////////////////////////////////////////////////////////

  T determinant() const;
  T determinant3x3() const;
  Vector4<T> eigenvalues() const;
  Matrix44<T> eigenvectors() const;

  ///////////////////////////////////////////////////////////////////////////////
};

using fmtx4       = Matrix44<float>;
using fmtx4_ptr_t = std::shared_ptr<fmtx4>;
using dmtx4       = Matrix44<double>;
using dmtx4_ptr_t = std::shared_ptr<dmtx4>;

template <>                       //
struct use_custom_serdes<fmtx4> { //
  static constexpr bool enable = true;
};

///////////////////////////////////////////////////////////////////////////////

fmtx4 dmtx4_to_fmtx4(const dmtx4& dvec);
dmtx4 fmtx4_to_dmtx4(const fmtx4& dvec);

///////////////////////////////////////////////////////////////////////////////

} // namespace ork
