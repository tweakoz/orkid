#include <utpp/UnitTest++.h>
#include <cmath>
#include <limits>
#include <ork/math/cvector3.h>
#include <ork/math/cvector4.h>
#include <ork/math/cmatrix4.h>
#include <ork/math/quaternion.h>
#include <ork/math/misc_math.h>

using namespace ork;
static const float MyEPSILON = 5.0e-07f; // std::numeric_limits<float>::epsilon();

TEST(QuatStraightDown) {
  fvec4 v(1, 0, 0, PI * 0.5);
  fquat q;
  q.fromAxisAngle(v);
  printf("axis<%g %g %g> angle<%g>\n", v.x, v.y, v.z, v.w);
  printf("q<%g %g %g %g>\n", q.x, q.y, q.z, q.w);
}

TEST(Vector4DefaultConstructor) {
  fvec4 v;
  CHECK_CLOSE(0.0f, v.x, MyEPSILON);
  CHECK_CLOSE(0.0f, v.y, MyEPSILON);
  CHECK_CLOSE(0.0f, v.z, MyEPSILON);
  CHECK_CLOSE(1.0f, v.w, MyEPSILON);
}

TEST(Vector4Constructor) {
  fvec4 v(1.0f, 2.0f, 3.0f, 4.0f);
  CHECK_CLOSE(1.0f, v.x, MyEPSILON);
  CHECK_CLOSE(2.0f, v.y, MyEPSILON);
  CHECK_CLOSE(3.0f, v.z, MyEPSILON);
  CHECK_CLOSE(4.0f, v.w, MyEPSILON);
}

TEST(Vector4CopyConstructor) {
  fvec4 v(1.0f, 2.0f, 3.0f, 4.0f);
  Vector4 copy(v);
  CHECK_CLOSE(1.0f, copy.x, MyEPSILON);
  CHECK_CLOSE(2.0f, copy.y, MyEPSILON);
  CHECK_CLOSE(3.0f, copy.z, MyEPSILON);
  CHECK_CLOSE(4.0f, copy.w, MyEPSILON);
}

TEST(Vector4RGBAConstructor) {
  fvec4 v(0xFF00FFFF);
  CHECK_CLOSE(1.0f, v.x, MyEPSILON);
  CHECK_CLOSE(0.0f, v.y, MyEPSILON);
  CHECK_CLOSE(1.0f, v.z, MyEPSILON);
  CHECK_CLOSE(1.0f, v.w, MyEPSILON);
}

TEST(Vector4RotateX) {
  fvec4 v(0.0f, 1.0f, 0.0f);
  v.RotateX(PI / float(2.0f));
  CHECK_CLOSE(0.0f, v.x, MyEPSILON);
  CHECK_CLOSE(0.0f, v.y, MyEPSILON);
  CHECK_CLOSE(1.0f, v.z, MyEPSILON);
  CHECK_CLOSE(1.0f, v.w, MyEPSILON);
}

TEST(Vector4RotateY) {
  fvec4 v(1.0f, 0.0f, 0.0f);
  v.RotateY(PI / float(2.0f));
  CHECK_CLOSE(0.0f, v.x, MyEPSILON);
  CHECK_CLOSE(0.0f, v.y, MyEPSILON);
  CHECK_CLOSE(1.0f, v.z, MyEPSILON);
  CHECK_CLOSE(1.0f, v.w, MyEPSILON);
}

TEST(Vector4RotateZ) {
  fvec4 v(1.0f, 0.0f, 0.0f);
  v.RotateZ(PI / float(2.0f));
  CHECK_CLOSE(0.0f, v.x, MyEPSILON);
  CHECK_CLOSE(1.0f, v.y, MyEPSILON);
  CHECK_CLOSE(0.0f, v.z, MyEPSILON);
  CHECK_CLOSE(1.0f, v.w, MyEPSILON);
}

TEST(Vector4Add) {
  fvec4 v(1.0f, 0.0f, 0.0f);
  fvec4 res = v + Vector4(0.0f, 1.0f, 0.0f);
  CHECK_CLOSE(1.0f, res.x, MyEPSILON);
  CHECK_CLOSE(1.0f, res.y, MyEPSILON);
  CHECK_CLOSE(0.0f, res.z, MyEPSILON);
  CHECK_CLOSE(2.0f, res.w, MyEPSILON);
}

TEST(Vector4AddTo) {
  fvec4 v(1.0f, 0.0f, 0.0f);
  v += Vector4(0.0f, 1.0f, 0.0f);
  CHECK_CLOSE(1.0f, v.x, MyEPSILON);
  CHECK_CLOSE(1.0f, v.y, MyEPSILON);
  CHECK_CLOSE(0.0f, v.z, MyEPSILON);
  CHECK_CLOSE(2.0f, v.w, MyEPSILON);
}

TEST(Vector4Subtract) {
  fvec4 v(1.0f, 0.0f, 0.0f);
  Vector4 res = v - Vector4(0.0f, 1.0f, 0.0f);
  CHECK_CLOSE(1.0f, res.x, MyEPSILON);
  CHECK_CLOSE(-1.0f, res.y, MyEPSILON);
  CHECK_CLOSE(0.0f, res.z, MyEPSILON);
  CHECK_CLOSE(0.0f, res.w, MyEPSILON);
}

TEST(Vector4SubtractFrom) {
  fvec4 v(1.0f, 0.0f, 0.0f);
  v -= Vector4(0.0f, 1.0f, 0.0f);
  CHECK_CLOSE(1.0f, v.x, MyEPSILON);
  CHECK_CLOSE(-1.0f, v.y, MyEPSILON);
  CHECK_CLOSE(0.0f, v.z, MyEPSILON);
  CHECK_CLOSE(0.0f, v.w, MyEPSILON);
}

TEST(Vector4Multiply) {
  fvec4 v(1.0f, 2.0f, 3.0f);
  Vector4 res = v * Vector4(3.0f, 2.0f, 1.0f);
  CHECK_CLOSE(3.0f, res.x, MyEPSILON);
  CHECK_CLOSE(4.0f, res.y, MyEPSILON);
  CHECK_CLOSE(3.0f, res.z, MyEPSILON);
  CHECK_CLOSE(1.0f, res.w, MyEPSILON);
}

TEST(Vector4MultiplyTo) {
  fvec4 v(1.0f, 2.0f, 3.0f);
  v *= Vector4(3.0f, 2.0f, 1.0f);
  CHECK_CLOSE(3.0f, v.x, MyEPSILON);
  CHECK_CLOSE(4.0f, v.y, MyEPSILON);
  CHECK_CLOSE(3.0f, v.z, MyEPSILON);
  CHECK_CLOSE(1.0f, v.w, MyEPSILON);
}

TEST(Vector4Divide) {
  fvec4 v(1.0f, 2.0f, 3.0f);
  Vector4 res = v / Vector4(3.0f, 2.0f, 1.0f);
  CHECK_CLOSE(.333333f, res.x, MyEPSILON);
  CHECK_CLOSE(1.0f, res.y, MyEPSILON);
  CHECK_CLOSE(3.0f, res.z, MyEPSILON);
  CHECK_CLOSE(1.0f, res.w, MyEPSILON);
}

TEST(Vector4DivideTo) {
  fvec4 v(1.0f, 2.0f, 3.0f);
  v /= Vector4(3.0f, 2.0f, 1.0f);
  CHECK_CLOSE(0.333333f, v.x, MyEPSILON);
  CHECK_CLOSE(1.0f, v.y, MyEPSILON);
  CHECK_CLOSE(3.0f, v.z, MyEPSILON);
  CHECK_CLOSE(1.0f, v.w, MyEPSILON);
}

TEST(Vector4Scale) {
  fvec4 v(1.0f, 2.0f, 3.0f);
  fvec4 res = v * float(3.0f);
  CHECK_CLOSE(3.0f, res.x, MyEPSILON);
  CHECK_CLOSE(6.0f, res.y, MyEPSILON);
  CHECK_CLOSE(9.0f, res.z, MyEPSILON);
  CHECK_CLOSE(3.0f, res.w, MyEPSILON);
}

TEST(Vector4ScaleTo) {
  fvec4 v(1.0f, 2.0f, 3.0f);
  v *= float(3.0f);
  CHECK_CLOSE(3.0f, v.x, MyEPSILON);
  CHECK_CLOSE(6.0f, v.y, MyEPSILON);
  CHECK_CLOSE(9.0f, v.z, MyEPSILON);
  CHECK_CLOSE(3.0f, v.w, MyEPSILON);
}

TEST(Vector4ScalePre) {
  fvec4 v(1.0f, 2.0f, 3.0f);
  fvec4 res = float(3.0f) * v;
  CHECK_CLOSE(3.0f, res.x, MyEPSILON);
  CHECK_CLOSE(6.0f, res.y, MyEPSILON);
  CHECK_CLOSE(9.0f, res.z, MyEPSILON);
  CHECK_CLOSE(1.0f, res.w, MyEPSILON);
}

TEST(Vector4InvScale) {
  fvec4 v(1.0f, 2.0f, 3.0f);
  fvec4 res = v / float(3.0f);
  CHECK_CLOSE(0.333333f, res.x, MyEPSILON);
  CHECK_CLOSE(0.666667f, res.y, MyEPSILON);
  CHECK_CLOSE(1.0f, res.z, MyEPSILON);
  CHECK_CLOSE(0.333333f, res.w, MyEPSILON);
}

TEST(Vector4InvScaleTo) {
  fvec4 v(1.0f, 2.0f, 3.0f);
  v /= float(3.0f);
  CHECK_CLOSE(0.333333f, v.x, MyEPSILON);
  CHECK_CLOSE(0.666667f, v.y, MyEPSILON);
  CHECK_CLOSE(1.0f, v.z, MyEPSILON);
  CHECK_CLOSE(0.333333f, v.w, MyEPSILON);
}

TEST(Vector4EqualCompare) {
  fvec4 v1(1.0f, 2.0f, 3.0f, 4.0f);
  fvec4 v2(1.0f, 2.0f, 3.0f, 4.0f);
  fvec4 v3(4.0f, 3.0f, 2.0f, 1.0f);
  CHECK_EQUAL(true, v1 == v2);
  CHECK_EQUAL(false, v1 == v3);
}

TEST(Vector4NotEqualCompare) {
  fvec4 v1(1.0f, 2.0f, 3.0f, 4.0f);
  fvec4 v2(1.0f, 2.0f, 3.0f, 4.0f);
  fvec4 v3(4.0f, 3.0f, 2.0f, 1.0f);
  CHECK_EQUAL(false, v1 != v2);
  CHECK_EQUAL(true, v1 != v3);
}

TEST(Vector4Dot) {
  fvec4 v1(1.0f, 2.0f, 3.0f);
  fvec4 v2(4.0f, 3.0f, 2.0f);
  float res = v1.Dot(v2);
  CHECK_CLOSE(16.0f, res, MyEPSILON);
}

TEST(Vector4Cross) {
  fvec4 v1(1.0f, 2.0f, 3.0f);
  fvec4 v2(4.0f, 3.0f, 2.0f);
  fvec4 res = v1.Cross(v2);
  CHECK_CLOSE(-5.0f, res.x, MyEPSILON);
  CHECK_CLOSE(10.0f, res.y, MyEPSILON);
  CHECK_CLOSE(-5.0f, res.z, MyEPSILON);
  CHECK_CLOSE(1.0f, res.w, MyEPSILON);
}

TEST(Vector4Normalize) {
  fvec4 v(1.0f, 2.0f, 3.0f);
  v.Normalize();
  CHECK_CLOSE(0.267261f, v.x, MyEPSILON);
  CHECK_CLOSE(0.534522f, v.y, MyEPSILON);
  CHECK_CLOSE(0.801784f, v.z, MyEPSILON);
  CHECK_CLOSE(1.0f, v.w, MyEPSILON);
}

TEST(Vector4Normal) {
  fvec4 v(1.0f, 2.0f, 3.0f);
  fvec4 res = v.Normal();
  CHECK_CLOSE(0.267261f, res.x, MyEPSILON);
  CHECK_CLOSE(0.534522f, res.y, MyEPSILON);
  CHECK_CLOSE(0.801784f, res.z, MyEPSILON);
  CHECK_CLOSE(1.0f, res.w, MyEPSILON);
}

TEST(Vector4Mag) {
  fvec4 v(3.0f, 4.0f, 0.0f);
  float res = v.Mag();
  CHECK_CLOSE(5.0f, res, MyEPSILON);
}

TEST(Vector4MagSquared) {
  fvec4 v(3.0f, 4.0f, 0.0f);
  float res = v.MagSquared();
  CHECK_CLOSE(25.0f, res, MyEPSILON);
}

TEST(Vector4Transform) {
  fvec4 v(0.0f, 1.0f, 0.0f);
  fmtx4 m;
  m.SetRotateX(PI / float(2.0f));
  fvec4 res = v.Transform(m);
  CHECK_CLOSE(0.0f, res.x, MyEPSILON);
  CHECK_CLOSE(0.0f, res.y, MyEPSILON);
  CHECK_CLOSE(1.0f, res.z, MyEPSILON);
  CHECK_CLOSE(1.0f, res.w, MyEPSILON);
}

TEST(Vector4PerspectiveDivide) {
  fvec4 v(4.0f, 8.0f, 12.0f, 4.0f);
  v.PerspectiveDivide();
  CHECK_CLOSE(1.0f, v.x, MyEPSILON);
  CHECK_CLOSE(2.0f, v.y, MyEPSILON);
  CHECK_CLOSE(3.0f, v.z, MyEPSILON);
  CHECK_CLOSE(1.0f, v.w, MyEPSILON);
}

TEST(Vector4_YO) {
  float fx0 = 100.0f;
  float fy0 = 110.0f;
  float fx1 = 300.0f;
  float fy1 = 320.0f;

  float fu0 = 0.0f;
  float fv0 = 0.0f;
  float fu1 = 1.0f;
  float fv1 = 1.0f;

  float fw_W = (fx1 - fx0);
  float fw_H = (fy1 - fy0);
  float fu_W = (fu1 - fu0);
  float fu_H = (fv1 - fv0);

  float fw2u_hsca = fu_W / fw_W;
  float fw2u_vsca = fu_H / fw_H;
  float fw2u_hoff = -fx0;
  float fw2u_voff = -fy0;

  fmtx4 w2u_mtx, w2u_Smtx, w2u_Tmtx;
  w2u_Smtx.SetScale(fw2u_hsca, fw2u_vsca, 1.0f);
  w2u_Tmtx.SetTranslation(fw2u_hoff, fw2u_voff, 0.0f);
  w2u_mtx = w2u_Tmtx * w2u_Smtx;

  fvec4 v0 = fvec4(fx0, fy0, 0.0f).Transform(w2u_mtx);
  fvec4 v1 = fvec4(fx1, fy1, 0.0f).Transform(w2u_mtx);

  printf("aa::V0<%f %f> V1<%f %f>\n", v0.x, v0.y, v1.x, v1.y);

  // CHECK_CLOSE(1.0f, v.w, MyEPSILON);
}
