#include <ork/pch.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/material_pbr.inl>
#include <ork/lev2/lev2_asset.h>
#include <ork/kernel/opq.h>
#include <utpp/UnitTest++.h>

TEST(skel1) {

  using namespace ork;

  auto ta = fvec3(0, 0, 1.99);
  auto tb = fvec3(0, 0, 3.41);
  auto tc = fvec3(0, 0, 4.86);
  auto td = fvec3(0, 0, 6.05);

  fmtx4 mxa;
  mxa.SetTranslation(ta);
  fmtx4 mxb;
  mxb.SetTranslation(tb);
  fmtx4 mxc;
  mxc.SetTranslation(tc);
  fmtx4 mxd;
  mxd.SetTranslation(td);

  fmtx4 a2b;
  a2b.CorrectionMatrix(mxa, mxb);
  fmtx4 b2c;
  b2c.CorrectionMatrix(mxb, mxc);
  fmtx4 c2d;
  c2d.CorrectionMatrix(mxc, mxd);

  a2b.dump("a2b");
  b2c.dump("b2c");
  c2d.dump("c2d");
}
