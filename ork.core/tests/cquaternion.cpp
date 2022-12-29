////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <utpp/UnitTest++.h>
#include <cmath>
#include <limits>
#include <ork/math/cvector3.h>
#include <ork/math/cvector4.h>
#include <ork/math/cmatrix4.h>
#include <ork/math/quaternion.hpp>
#include <ork/math/misc_math.h>
#include <ork/math/math_types.inl>

using namespace ork;
static const float MyEPSILON = 5.0e-07f; // std::numeric_limits<float>::epsilon();

TEST(QuatStraightDown) {
  fvec4 v(1, 0, 0, PI * 0.5);
  fquat q;
  q.fromAxisAngle(v);
  printf("axis<%g %g %g> angle<%g>\n", v.x, v.y, v.z, v.w);
  printf("q<%g %g %g %g>\n", q.x, q.y, q.z, q.w);
}

TEST(QuatConjugate) {
  fvec4 v(1, 0, 0, PI * 0.5);
  fquat q;
  q.fromAxisAngle(v);

  fquat cq = q.conjugate();

  printf("axis<%g %g %g> angle<%g>\n", v.x, v.y, v.z, v.w);
  printf("%s\n", q.formatcn("q").c_str());
  printf("%s\n", cq.formatcn("cq").c_str());
}

TEST(QuatInverse) {
  fvec4 v(1, 0, 0, PI * 0.5);
  fquat q;
  q.fromAxisAngle(v);

  fquat iq  = q.inverse();
  fquat iiq = iq.inverse();

  fquat chk = iq * iiq;
  fquat identity;

  CHECK(identity == chk);

  printf("axis<%g %g %g> angle<%g>\n", v.x, v.y, v.z, v.w);
  printf("%s\n", q.formatcn("q").c_str());
  printf("%s\n", iq.formatcn("iq").c_str());
  printf("%s\n", identity.formatcn("identity").c_str());
  printf("%s\n", chk.formatcn("chk").c_str());
}

TEST(QuatKlnRotorConversion) {

  float this_EPSILON = 0.0001;

  math::FRANDOMGEN RG(10);

  for (int i = 0; i < 100; i++) {
    float fx = RG.rangedf(-1, 1);
    float fy = RG.rangedf(-1, 1);
    float fz = RG.rangedf(-1, 1);
    float fa = RG.rangedf(-PI2, PI2);

    auto axis = fvec3(fx, fy, fz).normalized();
    fvec4 aa(axis.x, axis.y, axis.z, fa);

    fquat q;
    q.fromAxisAngle(aa);

    auto r  = q.asKleinRotor();
    auto q2 = fquat(r);
    CHECK_CLOSE(q.x, q2.x, this_EPSILON);
    CHECK_CLOSE(q.y, q2.y, this_EPSILON);
    CHECK_CLOSE(q.z, q2.z, this_EPSILON);
    CHECK_CLOSE(q.w, q2.w, this_EPSILON);

    auto P  = fvec3(1, 1, 1);
    auto P2 = q.transform(P);

    auto KP  = P.asKleinPoint();
    auto KP2 = r(KP);

    bool XOK = fabs(P2.x - KP2.x()) < this_EPSILON;
    bool YOK = fabs(P2.y - KP2.y()) < this_EPSILON;
    bool ZOK = fabs(P2.z - KP2.z()) < this_EPSILON;

    if (XOK and YOK and ZOK) {

    } else {
      printf("AA<%g %g %g> <%g>\n", aa.x, aa.y, aa.z, aa.w);
      printf("P<%g %g %g>\n", P.x, P.y, P.z);
      printf("P2<%g %g %g>\n", P2.x, P2.y, P2.z);
      printf("KP2<%g %g %g>\n", KP2.x(), KP2.y(), KP2.z());
    }

    CHECK_CLOSE(P2.x, KP2.x(), this_EPSILON);
    CHECK_CLOSE(P2.y, KP2.y(), this_EPSILON);
    CHECK_CLOSE(P2.z, KP2.z(), this_EPSILON);
  }
}

TEST(DualQuatKnlMotor) {

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

    auto MR = fdualquat(KM);
    auto P  = fvec3(KP);
    //auto P2 = P*MR;
    auto P2 = P*MR;

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
      OrkAssert(false);
    }
  }
}

