////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <utpp/UnitTest++.h>
#include <cmath>
#include <limits>
#include <ork/math/misc_math.h>
#include <ork/math/math_types.inl>
#include <ork/math/cmatrix4.h>
#include <ork/math/cvector3.h>
#include <ork/math/cvector4.h>

using namespace ork;
static const float MyEPSILON = 5.0e-07f; // std::numeric_limits<float>::epsilon();

// max abs elementwise difference over all 16 elements (the OLD compose
// transposed the rotation block, so a diagonal-only check was transpose-blind
// — this compares every element).
static float mtxMaxDiff(const fmtx4& a, const fmtx4& b) {
  float md = 0.0f;
  for (int i = 0; i < 4; i++)
    for (int j = 0; j < 4; j++)
      md = std::max(md, std::fabs(a.elemXY(i, j) - b.elemXY(i, j)));
  return md;
}

// |dot(a,b)| for quaternion equality up to double-cover (q and -q are the same
// rotation): equal rotations give |dot| == 1.
static float quatAbsDot(const fquat& a, const fquat& b) {
  return std::fabs(a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w);
}

// L-B (round-trip) + L-C (agreement): given a synthetic rotate+uniform-scale+
// translate, compose(decompose(M)) == M (all 16), compose == compose2 (all 16),
// and the recovered quaternion matches up to double-cover.
TEST(Matrix44CompDecomp) {
  fmtx4 mr, ms, mt, mm;

  mr.setRotateX(90 * DTOR);
  ms.setScale(0.5);
  mt.setTranslation(1, 2, 3);

  mm = fmtx4::multiply_ltor(ms, mr, mt);

  fvec3 t;
  fquat r;
  float s;

  mm.decompose(t, r, s);

  CHECK_CLOSE(t.x, 1.0, MyEPSILON);
  CHECK_CLOSE(t.y, 2.0, MyEPSILON);
  CHECK_CLOSE(t.z, 3.0, MyEPSILON);
  CHECK_CLOSE(s, 0.5, MyEPSILON);

  fmtx4 recomposed, viacompose2;
  recomposed.compose(t, r, s);
  viacompose2.compose2(t, r, s);

  // L-B: recompose reproduces the original matrix exactly (full 16).
  CHECK(mtxMaxDiff(mm, recomposed) < MyEPSILON);
  // L-C: compose agrees with compose2 (full 16).
  CHECK(mtxMaxDiff(recomposed, viacompose2) < MyEPSILON);

  // quat round-trip (double-cover).
  fvec3 t2;
  fquat r2;
  float s2;
  recomposed.decompose(t2, r2, s2);
  CHECK(quatAbsDot(r, r2) >= (1.0f - MyEPSILON));
}

// Captured near-orthonormal bases (6-digit truncated) used as realistic
// orientation sources. compose/compose2 agree only for UNIT quats (compose
// normalizes by 2/|q|^2, compose2's glm::mat4_cast assumes |q|==1), and a
// real orientation IS a unit quat — so normalize the decomposed quat, then:
//   L-C: compose(t,r,s) == compose2(t,r,s) over the full 16 elements;
//   L-B: decompose(compose(t,r,s)) round-trips (t,s) and r up to double-cover.
#define COMPDECOMP_FULL_ORACLE(captured, E)                             \
  do {                                                                  \
    fvec3 t, t2;                                                        \
    fquat r, r2;                                                        \
    float s, s2;                                                        \
    (captured).decompose(t, r, s);                                      \
    r.normalizeInPlace();                                               \
    fmtx4 recomposed, viacompose2;                                      \
    recomposed.compose(t, r, s);                                        \
    viacompose2.compose2(t, r, s);                                      \
    CHECK(mtxMaxDiff(recomposed, viacompose2) < (E)); /* L-C full 16 */ \
    recomposed.decompose(t2, r2, s2);                                   \
    CHECK(quatAbsDot(r, r2) >= (1.0f - (E)));         /* L-B quat rt */  \
    CHECK_CLOSE(t.x, t2.x, (E));                                        \
    CHECK_CLOSE(t.y, t2.y, (E));                                        \
    CHECK_CLOSE(t.z, t2.z, (E));                                        \
    CHECK_CLOSE(s, s2, (E));                                            \
  } while (0)

TEST(Matrix44CompDecomp2) {
  fmtx4 mm;

  mm.setColumn(0, 0.529338, 0.030632, -0.847858, 0.000000);
  mm.setColumn(1, -0.068524, 0.997627, -0.006738, 0.000000);
  mm.setColumn(2, -0.845639, -0.061665, -0.530181, 0.000000);
  mm.setColumn(3, -0.049359, 0.002166, -0.019145, 1.000000);

  COMPDECOMP_FULL_ORACLE(mm, 1.0e-5f);
}

TEST(Matrix44CompDecomp3) {
  fmtx4 mm;

  mm.setColumn(0, 0.661251, 0.149310, -0.735156, 0.000000);
  mm.setColumn(1, -0.132303, 0.987842, 0.081628, 0.000000);
  mm.setColumn(2, -0.738406, -0.043287, -0.672966, 0.000000);
  mm.setColumn(3, -0.063490, -0.004191, -0.013718, 1.000000);

  COMPDECOMP_FULL_ORACLE(mm, 1.0e-5f);
}

// L-A (basis/intent): compose(0,q,1).zNormal() == the active rotation of local
// +Z by q. Reference is the DirectionalLight::lookAt basis build
// (gfx_lighting.cpp:254-266), which puts the travel direction in column 2 ==
// zNormal(). Cases mirror the DSL elevation/azimuth sun aims.
TEST(Matrix44ComposeIntent_LA) {
  const float E = 1.0e-5f;
  struct Case {
    float el_deg, az_deg;
  };
  Case cases[] = {{45, 0}, {45, 30}, {30, 285}, {60, 120}, {0, 90}, {75, 200}};

  for (auto c : cases) {
    float el = c.el_deg * DTOR;
    float az = c.az_deg * DTOR;
    // sun-aim quat: azimuth(+Y) ∘ elevation(+X), same as the DSL helper.
    fquat q_el(fvec3(1, 0, 0), el);
    fquat q_az(fvec3(0, 1, 0), az);
    fquat q = q_az * q_el;

    fmtx4 m;
    m.compose(fvec3(0, 0, 0), q, 1.0f);
    fvec3 zn = m.zNormal();

    // analytic active rotation of +Z: R_az * R_el * [0,0,1].
    fvec3 travel(cosf(el) * sinf(az), -sinf(el), cosf(el) * cosf(az));
    CHECK_CLOSE(zn.x, travel.x, E);
    CHECK_CLOSE(zn.y, travel.y, E);
    CHECK_CLOSE(zn.z, travel.z, E);

    // mirror DirectionalLight::lookAt basis construction; its zNormal must match.
    fvec3 zdir = travel.normalized();
    fvec3 up(0, 1, 0);
    fvec3 xdir = up.crossWith(zdir);
    if (xdir.magnitude() < 1e-4f)
      xdir = fvec3(1, 0, 0).crossWith(zdir);
    xdir.normalizeInPlace();
    fvec3 ydir = zdir.crossWith(xdir).normalized();
    fmtx4 look;
    look.setColumn(0, fvec4(xdir, 0));
    look.setColumn(1, fvec4(ydir, 0));
    look.setColumn(2, fvec4(zdir, 0));
    look.setColumn(3, fvec4(0, 0, 0, 1));
    fvec3 lzn = look.zNormal();
    CHECK_CLOSE(zn.x, lzn.x, E);
    CHECK_CLOSE(zn.y, lzn.y, E);
    CHECK_CLOSE(zn.z, lzn.z, E);
  }
}

// L-C (agreement) over random rotation + non-uniform scale + translation:
// compose == compose2 == multiply_ltor(toMatrix(q), scale, translation), full 16.
TEST(Matrix44ComposeAgreesWithCompose2_LC) {
  const float E = 1.0e-5f; // transpose bug is O(1); this only excludes FP rounding (~1e-6)
  math::FRANDOMGEN RG(1234);

  for (int i = 0; i < 64; i++) {
    fvec3 axis(RG.rangedf(-1, 1), RG.rangedf(-1, 1), RG.rangedf(-1, 1));
    if (axis.magnitude() < 1e-3f)
      axis = fvec3(0, 1, 0);
    axis.normalizeInPlace();
    float angle = RG.rangedf(-3.14159f, 3.14159f);
    fquat q(axis, angle);

    fvec3 t(RG.rangedf(-10, 10), RG.rangedf(-10, 10), RG.rangedf(-10, 10));
    float sx = RG.rangedf(0.1f, 3.0f);
    float sy = RG.rangedf(0.1f, 3.0f);
    float sz = RG.rangedf(0.1f, 3.0f);

    fmtx4 mc, mc2;
    mc.compose(t, q, sx, sy, sz);
    mc2.compose2(t, q, sx, sy, sz);
    CHECK(mtxMaxDiff(mc, mc2) < E);
  }
}

TEST(Matrix44KlnTranslator) {

  // TODO fixme

  float this_EPSILON = 0.0001;

  math::FRANDOMGEN RG(10);

  for (int i = 0; i < 100; i++) {

    float fx = RG.rangedf(-1, 1);
    float fy = RG.rangedf(-1, 1);
    float fz = RG.rangedf(-1, 1);
    float fd = RG.rangedf(-1000, 1000);

    kln::translator KT(fd, fx, fy, fz);
    kln::point KP(0, 0, 0);
    kln::point KP2 = KT(KP);

    auto MT = fmtx4(KT);
    auto P  = fvec3(0, 0, 0);
    auto P2 = P.transform(MT);

    CHECK_CLOSE(P2.x, KP2.x(), this_EPSILON);
    CHECK_CLOSE(P2.y, KP2.y(), this_EPSILON);
    CHECK_CLOSE(P2.z, KP2.z(), this_EPSILON);

    bool XOK = fabs(P2.x - KP2.x()) < this_EPSILON;
    bool YOK = fabs(P2.y - KP2.y()) < this_EPSILON;
    bool ZOK = fabs(P2.z - KP2.z()) < this_EPSILON;

    if (XOK and YOK and ZOK) {
    } else {
      printf("KP(x,y,z) <%g %g %g>\n", KP.x(), KP.y(), KP.z());
      printf("KP2(x,y,z) <%g %g %g>\n", KP2.x(), KP2.y(), KP2.z());

      printf("P(x,y,z) <%g %g %g>\n", P.x, P.y, P.z);
      printf("P2(x,y,z) <%g %g %g>\n", P2.x, P2.y, P2.z);
    }
  }
}

TEST(Matrix44KlnRotor) {

  float this_EPSILON = 0.0001;

  math::FRANDOMGEN RG(10);

  for (int i = 0; i < 100; i++) {

    float fx = RG.rangedf(-1, 1);
    float fy = RG.rangedf(-1, 1);
    float fz = RG.rangedf(-1, 1);
    float fd = RG.rangedf(-1000, 1000);

    kln::rotor KR(fd, fx, fy, fz);
    kln::point KP(0, 0, 0);
    kln::point KP2 = KR(KP);

    auto MR = fmtx4(KR);
    auto P  = fvec3(0, 0, 0);
    auto P2 = P.transform(MR);

    CHECK_CLOSE(P2.x, KP2.x(), this_EPSILON);
    CHECK_CLOSE(P2.y, KP2.y(), this_EPSILON);
    CHECK_CLOSE(P2.z, KP2.z(), this_EPSILON);

    bool XOK = fabs(P2.x - KP2.x()) < this_EPSILON;
    bool YOK = fabs(P2.y - KP2.y()) < this_EPSILON;
    bool ZOK = fabs(P2.z - KP2.z()) < this_EPSILON;

    if (XOK and YOK and ZOK) {
    } else {
      printf("KP(x,y,z) <%g %g %g>\n", KP.x(), KP.y(), KP.z());
      printf("KP2(x,y,z) <%g %g %g>\n", KP2.x(), KP2.y(), KP2.z());

      printf("P(x,y,z) <%g %g %g>\n", P.x, P.y, P.z);
      printf("P2(x,y,z) <%g %g %g>\n", P2.x, P2.y, P2.z);
    }
  }
}

TEST(Matrix44KlnMotor) {

  float this_EPSILON = 0.0001;

  math::FRANDOMGEN RG(10);

  for (int i = 0; i < 100; i++) {

    float fx = RG.rangedf(-1, 1);
    float fy = RG.rangedf(-1, 1);
    float fz = RG.rangedf(-1, 1);
    float fd = RG.rangedf(-1000, 1000);
    float fx2 = RG.rangedf(-1, 1);
    float fy2 = RG.rangedf(-1, 1);
    float fz2 = RG.rangedf(-1, 1);
    float fd2 = RG.rangedf(-1000, 1000);


    kln::rotor KR(fd, fx, fy, fz);
    kln::translator KT(fd2, fx2, fy2, fz2);

    kln::motor KM = KR*KT;
    kln::point KP(0, 0, 0);
    kln::point KP2 = KM(KP);

    auto MR = fmtx4(KM);
    auto P  = fvec3(0, 0, 0);
    auto P2 = P.transform(MR);

    CHECK_CLOSE(P2.x, KP2.x(), this_EPSILON);
    CHECK_CLOSE(P2.y, KP2.y(), this_EPSILON);
    CHECK_CLOSE(P2.z, KP2.z(), this_EPSILON);

    bool XOK = fabs(P2.x - KP2.x()) < this_EPSILON;
    bool YOK = fabs(P2.y - KP2.y()) < this_EPSILON;
    bool ZOK = fabs(P2.z - KP2.z()) < this_EPSILON;

    if (XOK and YOK and ZOK) {
    } else {
      printf("KP(x,y,z) <%g %g %g>\n", KP.x(), KP.y(), KP.z());
      printf("KP2(x,y,z) <%g %g %g>\n", KP2.x(), KP2.y(), KP2.z());

      printf("P(x,y,z) <%g %g %g>\n", P.x, P.y, P.z);
      printf("P2(x,y,z) <%g %g %g>\n", P2.x, P2.y, P2.z);
    }
  }
}
