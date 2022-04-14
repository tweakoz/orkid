////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/lev2_asset.h>
#include <ork/kernel/opq.h>
#include <utpp/UnitTest++.h>

TEST(glfx1) {
  // we must load shaders on the main thread!
  ork::opq::mainSerialQueue()->enqueue([&]() {
    auto targ = ork::lev2::contextForCurrentThread();
    printf("targ<%p>\n", (void*)targ);
    CHECK(targ != nullptr);

    auto fxi = targ->FXI();
    printf("fxi<%p>\n", (void*)fxi);
    CHECK(fxi != nullptr);

    auto asset = ork::asset::AssetManager<ork::lev2::FxShaderAsset>::load("orkshader://deferrednvms");
    printf("asset<%p>\n", (void*)asset.get());
    CHECK(asset != nullptr);

    auto shader = asset->GetFxShader();
    printf("shader<%s:%p>\n", shader->mName.c_str(), (void*)shader);
    CHECK(shader != nullptr);

    for (auto item : shader->_techniques) {
      printf("tek<%s:%p>\n", item.first.c_str(), (void*)item.second);
    }
    for (auto item : shader->_parameterByName) {
      printf("param<%s:%p>\n", item.first.c_str(), (void*)item.second);
    }
    for (auto item : shader->_parameterBlockByName) {
      printf("paramblock<%s:%p>\n", item.first.c_str(), (void*)item.second);
    }
    for (auto item : shader->_computeShaderByName) {
      printf("computeshader<%s:%p>\n", item.first.c_str(), (void*)item.second);
    }
  });

  ork::opq::mainSerialQueue()->drain();
}
