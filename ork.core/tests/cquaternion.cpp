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
