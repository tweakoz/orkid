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

TEST(pbr1) {
  // we must load shaders on the main thread!
  ork::opq::mainSerialQueue()->enqueue([&]() {
    auto targ = ork::lev2::contextForCurrentThread();
    printf("targ<%p>\n", (void*) targ);
    CHECK(targ != nullptr);

    auto fxi = targ->FXI();
    printf("fxi<%p>\n", (void*) fxi);
    CHECK(fxi != nullptr);

    auto mtl = new ork::lev2::PBRMaterial();
    mtl->setTextureBaseName("yo");
    mtl->gpuInit(targ);
    printf("mtl<%p>\n", (void*) mtl);
    CHECK(mtl != nullptr);
  });

  ork::opq::mainSerialQueue()->drain();
}
