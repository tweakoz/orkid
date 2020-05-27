#include <utpp/UnitTest++.h>
#include <cmath>
#include <limits>
#include <ork/math/cvector3.h>
#include <ork/math/cvector4.h>
#include <ork/math/cmatrix4.h>
#include <ork/math/misc_math.h>

using namespace ork;
static const float MyEPSILON = 5.0e-07f; // std::numeric_limits<float>::epsilon();

TEST(Vector3DefaultConstructor) {
  fvec3 v;
  CHECK_CLOSE(0.0f, v.x, MyEPSILON);
  CHECK_CLOSE(0.0f, v.y, MyEPSILON);
  CHECK_CLOSE(0.0f, v.z, MyEPSILON);
}

TEST(Vector3Constructor) {
  fvec3 v(1.0f, 2.0f, 3.0f);
  CHECK_CLOSE(1.0f, v.x, MyEPSILON);
  CHECK_CLOSE(2.0f, v.y, MyEPSILON);
  CHECK_CLOSE(3.0f, v.z, MyEPSILON);
}

TEST(Vector3CopyConstructor) {
  fvec3 v(1.0f, 2.0f, 3.0f);
  fvec3 copy(v);
  CHECK_CLOSE(1.0f, copy.x, MyEPSILON);
  CHECK_CLOSE(2.0f, copy.y, MyEPSILON);
  CHECK_CLOSE(3.0f, copy.z, MyEPSILON);
}

TEST(Vector3RGBAConstructor) {
  fvec3 v(0xFF00FFFF);
  CHECK_CLOSE(1.0f, v.x, MyEPSILON);
  CHECK_CLOSE(0.0f, v.y, MyEPSILON);
  CHECK_CLOSE(1.0f, v.z, MyEPSILON);
}

TEST(Vector3RotateX) {
  fvec3 v(0.0f, 1.0f, 0.0f);
  v.RotateX(PI / 2.0f);
  CHECK_CLOSE(0.0f, v.x, MyEPSILON);
  CHECK_CLOSE(0.0f, v.y, MyEPSILON);
  CHECK_CLOSE(1.0f, v.z, MyEPSILON);
}

TEST(Vector3RotateY) {
  fvec3 v(1.0f, 0.0f, 0.0f);
  v.RotateY(PI / float(2.0f));
  CHECK_CLOSE(0.0f, v.x, MyEPSILON);
  CHECK_CLOSE(0.0f, v.y, MyEPSILON);
  CHECK_CLOSE(1.0f, v.z, MyEPSILON);
}

TEST(Vector3RotateZ) {
  fvec3 v(1.0f, 0.0f, 0.0f);
  v.RotateZ(PI / float(2.0f));
  CHECK_CLOSE(0.0f, v.x, MyEPSILON);
  CHECK_CLOSE(1.0f, v.y, MyEPSILON);
  CHECK_CLOSE(0.0f, v.z, MyEPSILON);
}

TEST(Vector3Add) {
  fvec3 v(1.0f, 0.0f, 0.0f);
  Vector3 res = v + Vector3(0.0f, 1.0f, 0.0f);
  CHECK_CLOSE(1.0f, res.x, MyEPSILON);
  CHECK_CLOSE(1.0f, res.y, MyEPSILON);
  CHECK_CLOSE(0.0f, res.z, MyEPSILON);
}

TEST(Vector3AddTo) {
  fvec3 v(1.0f, 0.0f, 0.0f);
  v += Vector3(0.0f, 1.0f, 0.0f);
  CHECK_CLOSE(1.0f, v.x, MyEPSILON);
  CHECK_CLOSE(1.0f, v.y, MyEPSILON);
  CHECK_CLOSE(0.0f, v.z, MyEPSILON);
}

TEST(Vector3Subtract) {
  fvec3 v(1.0f, 0.0f, 0.0f);
  Vector3 res = v - Vector3(0.0f, 1.0f, 0.0f);
  CHECK_CLOSE(1.0f, res.x, MyEPSILON);
  CHECK_CLOSE(-1.0f, res.y, MyEPSILON);
  CHECK_CLOSE(0.0f, res.z, MyEPSILON);
}

TEST(Vector3SubtractFrom) {
  fvec3 v(1.0f, 0.0f, 0.0f);
  v -= fvec3(0.0f, 1.0f, 0.0f);
  CHECK_CLOSE(1.0f, v.x, MyEPSILON);
  CHECK_CLOSE(-1.0f, v.y, MyEPSILON);
  CHECK_CLOSE(0.0f, v.z, MyEPSILON);
}

TEST(Vector3Multiply) {
  fvec3 v(1.0f, 2.0f, 3.0f);
  fvec3 res = v * fvec3(3.0f, 2.0f, 1.0f);
  CHECK_CLOSE(3.0f, res.x, MyEPSILON);
  CHECK_CLOSE(4.0f, res.y, MyEPSILON);
  CHECK_CLOSE(3.0f, res.z, MyEPSILON);
}

TEST(Vector3MultiplyTo) {
  fvec3 v(1.0f, 2.0f, 3.0f);
  v *= fvec3(3.0f, 2.0f, 1.0f);
  CHECK_CLOSE(3.0f, v.x, MyEPSILON);
  CHECK_CLOSE(4.0f, v.y, MyEPSILON);
  CHECK_CLOSE(3.0f, v.z, MyEPSILON);
}

TEST(Vector3Divide) {
  fvec3 v(1.0f, 2.0f, 3.0f);
  fvec3 res = v / Vector3(3.0f, 2.0f, 1.0f);
  CHECK_CLOSE(.333333f, res.x, MyEPSILON);
  CHECK_CLOSE(1.0f, res.y, MyEPSILON);
  CHECK_CLOSE(3.0f, res.z, MyEPSILON);
}

TEST(Vector3DivideTo) {
  fvec3 v(1.0f, 2.0f, 3.0f);
  v /= fvec3(3.0f, 2.0f, 1.0f);
  CHECK_CLOSE(0.333333f, v.x, MyEPSILON);
  CHECK_CLOSE(1.0f, v.y, MyEPSILON);
  CHECK_CLOSE(3.0f, v.z, MyEPSILON);
}

TEST(Vector3Scale) {
  fvec3 v(1.0f, 2.0f, 3.0f);
  fvec3 res = v * float(3.0f);
  CHECK_CLOSE(3.0f, res.x, MyEPSILON);
  CHECK_CLOSE(6.0f, res.y, MyEPSILON);
  CHECK_CLOSE(9.0f, res.z, MyEPSILON);
}

TEST(Vector3ScaleTo) {
  fvec3 v(1.0f, 2.0f, 3.0f);
  v *= float(3.0f);
  CHECK_CLOSE(3.0f, v.x, MyEPSILON);
  CHECK_CLOSE(6.0f, v.y, MyEPSILON);
  CHECK_CLOSE(9.0f, v.z, MyEPSILON);
}

TEST(Vector3ScalePre) {
  fvec3 v(1.0f, 2.0f, 3.0f);
  fvec3 res = float(3.0f) * v;
  CHECK_CLOSE(3.0f, res.x, MyEPSILON);
  CHECK_CLOSE(6.0f, res.y, MyEPSILON);
  CHECK_CLOSE(9.0f, res.z, MyEPSILON);
}

TEST(Vector3InvScale) {
  fvec3 v(1.0f, 2.0f, 3.0f);
  fvec3 res = v / float(3.0f);
  CHECK_CLOSE(0.333333f, res.x, MyEPSILON);
  CHECK_CLOSE(0.666667f, res.y, MyEPSILON);
  CHECK_CLOSE(1.0f, res.z, MyEPSILON);
}

TEST(Vector3InvScaleTo) {
  fvec3 v(1.0f, 2.0f, 3.0f);
  v /= float(3.0f);
  CHECK_CLOSE(0.333333f, v.x, MyEPSILON);
  CHECK_CLOSE(0.666667f, v.y, MyEPSILON);
  CHECK_CLOSE(1.0f, v.z, MyEPSILON);
}

TEST(Vector3EqualCompare) {
  fvec3 v1(1.0f, 2.0f, 3.0f);
  fvec3 v2(1.0f, 2.0f, 3.0f);
  fvec3 v3(4.0f, 3.0f, 2.0f);
  CHECK_EQUAL(true, v1 == v2);
  CHECK_EQUAL(false, v1 == v3);
}

TEST(Vector3NotEqualCompare) {
  fvec3 v1(1.0f, 2.0f, 3.0f);
  fvec3 v2(1.0f, 2.0f, 3.0f);
  fvec3 v3(4.0f, 3.0f, 2.0f);
  CHECK_EQUAL(false, v1 != v2);
  CHECK_EQUAL(true, v1 != v3);
}

TEST(Vector3Dot) {
  fvec3 v1(1.0f, 2.0f, 3.0f);
  fvec3 v2(4.0f, 3.0f, 2.0f);
  float res = v1.Dot(v2);
  CHECK_CLOSE(16.0f, res, MyEPSILON);
}

TEST(Vector3Cross) {
  fvec3 v1(1.0f, 2.0f, 3.0f);
  fvec3 v2(4.0f, 3.0f, 2.0f);
  fvec3 res = v1.Cross(v2);
  CHECK_CLOSE(-5.0f, res.x, MyEPSILON);
  CHECK_CLOSE(10.0f, res.y, MyEPSILON);
  CHECK_CLOSE(-5.0f, res.z, MyEPSILON);
}

TEST(Vector3Normalize) {
  fvec3 v(1.0f, 2.0f, 3.0f);
  v.Normalize();
  CHECK_CLOSE(0.267261f, v.x, MyEPSILON);
  CHECK_CLOSE(0.534522f, v.y, MyEPSILON);
  CHECK_CLOSE(0.801784f, v.z, MyEPSILON);
}

TEST(Vector3Normal) {
  fvec3 v(1.0f, 2.0f, 3.0f);
  Vector3 res = v.Normal();
  CHECK_CLOSE(0.267261f, res.x, MyEPSILON);
  CHECK_CLOSE(0.534522f, res.y, MyEPSILON);
  CHECK_CLOSE(0.801784f, res.z, MyEPSILON);
}

TEST(Vector3Mag) {
  fvec3 v(3.0f, 4.0f, 0.0f);
  float res = v.Mag();
  CHECK_CLOSE(5.0f, res, MyEPSILON);
}

TEST(Vector3MagSquared) {
  fvec3 v(3.0f, 4.0f, 0.0f);
  float res = v.MagSquared();
  CHECK_CLOSE(25.0f, res, MyEPSILON);
}

TEST(Vector3Transform) {
  fvec3 v(0.0f, 1.0f, 0.0f);
  fmtx4 m;
  m.SetRotateX(PI / float(2.0f));
  fvec4 res = v.Transform(m);
  CHECK_CLOSE(0.0f, res.x, MyEPSILON);
  CHECK_CLOSE(0.0f, res.y, MyEPSILON);
  CHECK_CLOSE(1.0f, res.z, MyEPSILON);
  CHECK_CLOSE(1.0f, res.w, MyEPSILON);
}
