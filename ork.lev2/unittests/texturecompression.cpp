////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/lev2/config.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/lev2_asset.h>
#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/kernel/opq.h>
#include <utpp/UnitTest++.h>
#include <ork/kernel/string/deco.inl>
#include <ork/lev2/gfx/texman.h>

#if defined(ENABLE_ISPC)
using namespace ork;
namespace ork::lev2 {
void bc7testcomp();
void astctestcomp();
} // namespace ork::lev2

TEST(bc7test) {
  ork::lev2::bc7testcomp();
}
TEST(astctest) {
  ork::lev2::astctestcomp();
}
#endif
