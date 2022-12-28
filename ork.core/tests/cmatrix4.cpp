////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
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

TEST(Matrix44CompDecomp) {
  fmtx4 mr, ms, mt, mm, mm2;

  mr.setRotateX(90 * DTOR);
  ms.setScale(0.5);
  mt.setTranslation(1, 2, 3);

  mm = fmtx4::multiply_ltor(ms, mr, mt);

  fvec3 t;
  fquat r;
  float s;

  mm.decompose(t, r, s);

  printf("t<%g %g %g>\n", t.x, t.y, t.z);
  printf("r(x,y,x,w) <%g %g %g %g>\n", r.x, r.y, r.z, r.w);
  printf("s<%g>\n", s);

  mm2.compose(t, r, s * 2.0f);
  mm2.decompose(t, r, s);

  printf("t2<%g %g %g>\n", t.x, t.y, t.z);
  printf("r2(x,y,x,w) <%g %g %g %g>\n", r.x, r.y, r.z, r.w);
  printf("s2<%g>\n", s);

  CHECK_CLOSE(t.x, 1.0, MyEPSILON);
  CHECK_CLOSE(t.y, 2.0, MyEPSILON);
  CHECK_CLOSE(t.z, 3.0, MyEPSILON);
  CHECK_CLOSE(s, 1.0, MyEPSILON);
}

TEST(Matrix44CompDecomp2) {
  fmtx4 mm, mm2;

  mm.setColumn(0, 0.529338, 0.030632, -0.847858, 0.000000);
  mm.setColumn(1, -0.068524, 0.997627, -0.006738, 0.000000);
  mm.setColumn(2, -0.845639, -0.061665, -0.530181, 0.000000);
  mm.setColumn(3, -0.049359, 0.002166, -0.019145, 1.000000);

  fvec3 t, t2;
  fquat r, r2;
  float s, s2;

  mm.decompose(t, r, s);

  printf("t<%g %g %g>\n", t.x, t.y, t.z);
  printf("r(x,y,x,w) <%g %g %g %g>\n", r.x, r.y, r.z, r.w);
  printf("s<%g>\n", s);

  mm2.compose(t, r, s);
  mm2.decompose(t2, r2, s2);

  printf("t2<%g %g %g>\n", t2.x, t2.y, t2.z);
  printf("r2(x,y,x,w) <%g %g %g %g>\n", r2.x, r2.y, r2.z, r2.w);
  printf("s2<%g>\n", s2);

  CHECK_CLOSE(t.x, t2.x, MyEPSILON);
  CHECK_CLOSE(t.y, t2.y, MyEPSILON);
  CHECK_CLOSE(t.z, t2.z, MyEPSILON);
  CHECK_CLOSE(s, s2, MyEPSILON);
}

TEST(Matrix44CompDecomp3) {
  fmtx4 mm, mm2;

  mm.setColumn(0, 0.661251, 0.149310, -0.735156, 0.000000);
  mm.setColumn(1, -0.132303, 0.987842, 0.081628, 0.000000);
  mm.setColumn(2, -0.738406, -0.043287, -0.672966, 0.000000);
  mm.setColumn(3, -0.063490, -0.004191, -0.013718, 1.000000);

  fvec3 t, t2;
  fquat r, r2;
  float s, s2;

  mm.decompose(t, r, s);
  auto d1 = mm.dump4x3cn();

  printf("t1<%g %g %g>\n", t.x, t.y, t.z);
  printf("r1(x,y,x,w) <%g %g %g %g>\n", r.x, r.y, r.z, r.w);
  printf("s1<%g>\n", s);
  printf("d1<%s>\n", d1.c_str());
  mm2.compose(t, r, s);
  mm2.decompose(t2, r2, s2);
  auto d2 = mm2.dump4x3cn();

  printf("t2<%g %g %g>\n", t2.x, t2.y, t2.z);
  printf("r2(x,y,z,w) <%g %g %g %g>\n", r2.x, r2.y, r2.z, r2.w);
  printf("s2<%g>\n", s2);
  printf("d2<%s>\n", d2.c_str());

  CHECK_CLOSE(t.x, t2.x, MyEPSILON);
  CHECK_CLOSE(t.y, t2.y, MyEPSILON);
  CHECK_CLOSE(t.z, t2.z, MyEPSILON);
  CHECK_CLOSE(mm.elemXY(0, 0), mm2.elemXY(0, 0), MyEPSILON);
  CHECK_CLOSE(mm.elemXY(1, 1), mm2.elemXY(1, 1), MyEPSILON);
  CHECK_CLOSE(mm.elemXY(2, 2), mm2.elemXY(2, 2), MyEPSILON);
  CHECK_CLOSE(s, s2, MyEPSILON);
}

TEST(Matrix44KlnTranslator) {

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
