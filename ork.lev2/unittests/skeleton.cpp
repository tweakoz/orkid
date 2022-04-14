////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

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
  mxa.setTranslation(ta);
  fmtx4 mxb;
  mxb.setTranslation(tb);
  fmtx4 mxc;
  mxc.setTranslation(tc);
  fmtx4 mxd;
  mxd.setTranslation(td);

  fmtx4 a2b;
  a2b.correctionMatrix(mxa, mxb);
  fmtx4 b2c;
  b2c.correctionMatrix(mxb, mxc);
  fmtx4 c2d;
  c2d.correctionMatrix(mxc, mxd);

  a2b.dump("a2b");
  b2c.dump("b2c");
  c2d.dump("c2d");
}
