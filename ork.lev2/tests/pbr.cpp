#include <ork/pch.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/material_pbr.inl>
#include <ork/lev2/lev2_asset.h>
#include <ork/kernel/opq.h>
#include <utpp/UnitTest++.h>

TEST(pbr1) {
  // we must load shaders on the main thread!
  ork::opq::mainSerialQueue()->enqueue([&]() {
    auto targ = ork::lev2::GfxEnv::GetRef().loadingContext();
    printf("targ<%p>\n", targ);
    CHECK(targ != nullptr);

    auto fxi = targ->FXI();
    printf("fxi<%p>\n", fxi);
    CHECK(fxi != nullptr);

    auto mtl = new ork::lev2::PBRMaterial();
    mtl->setTextureBaseName("yo");
    mtl->Init(targ);
    printf("mtl<%p>\n", mtl);
    CHECK(mtl != nullptr);
  });

  ork::opq::mainSerialQueue()->drain();
}
