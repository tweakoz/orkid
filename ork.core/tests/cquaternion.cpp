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
#include <ork/math/quaternion.h>
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
  printf("%s\n", q.formatcn("q").c_str() );
  printf("%s\n", cq.formatcn("cq").c_str() );
}

TEST(QuatInverse) {
  fvec4 v(1, 0, 0, PI * 0.5);
  fquat q;
  q.fromAxisAngle(v);

  fquat iq = q.inverse();
  fquat iiq = iq.inverse();

  fquat chk = iq*iiq;
  fquat identity;

  CHECK(identity==chk);

  printf("axis<%g %g %g> angle<%g>\n", v.x, v.y, v.z, v.w);
  printf("%s\n", q.formatcn("q").c_str() );
  printf("%s\n", iq.formatcn("iq").c_str() );
  printf("%s\n", identity.formatcn("identity").c_str() );
  printf("%s\n", chk.formatcn("chk").c_str() );
}

TEST(QuatKlnRotorConversion) {

  fvec4 aa(0, 1, 0, PI * 0.5);
  printf("AA<%g %g %g> <%g>\n", aa.x,aa.y,aa.z, aa.w);

  fquat q;
  q.fromAxisAngle(aa);

  auto r = q.asKleinRotor();
  auto q2 = fquat(r);
  CHECK_CLOSE(q.x, q2.x, MyEPSILON);
  CHECK_CLOSE(q.y, q2.y, MyEPSILON);
  CHECK_CLOSE(q.z, q2.z, MyEPSILON);
  CHECK_CLOSE(q.w, q2.w, MyEPSILON);

  auto P = fvec3(1,1,1);
  auto P2 = q.transform(P);

  auto KP = P.asKleinPoint();
  auto KP2 = r(KP);  

  printf("P<%g %g %g>\n", P.x,P.y,P.z);
  printf("P2<%g %g %g>\n", P2.x,P2.y,P2.z);
  printf("KP2<%g %g %g>\n", KP2.x(),KP2.y(),KP2.z());

  CHECK_CLOSE(P2.x, KP2.x(), MyEPSILON);
  CHECK_CLOSE(P2.y, KP2.y(), MyEPSILON);
  CHECK_CLOSE(P2.z, KP2.z(), MyEPSILON);

}
