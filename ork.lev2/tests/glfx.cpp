#include <ork/pch.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/lev2_asset.h>
#include <ork/kernel/opq.h>
#include <utpp/UnitTest++.h>

TEST(glfx1) {
  // we must load shaders on the main thread!
  ork::opq::mainThreadQueue().enqueue([&]() {
    auto targ = ork::lev2::GfxEnv::GetRef().GetLoaderTarget();
    printf("targ<%p>\n", targ);
    CHECK(targ != nullptr);

    auto fxi = targ->FXI();
    printf("fxi<%p>\n", fxi);
    CHECK(fxi != nullptr);

    auto asset = ork::asset::AssetManager<ork::lev2::FxShaderAsset>::Load("orkshader://deferrednvms");
    printf("asset<%p>\n", asset);
    CHECK(asset != nullptr);

    auto shader = asset->GetFxShader();
    printf("shader<%s:%p>\n", shader->mName.c_str(), shader);
    CHECK(shader != nullptr);

    for (auto item : shader->_techniques) {
      printf("tek<%s:%p>\n", item.first.c_str(), item.second);
    }
    for (auto item : shader->_parameterByName) {
      printf("param<%s:%p>\n", item.first.c_str(), item.second);
    }
    for (auto item : shader->_parameterBlockByName) {
      printf("paramblock<%s:%p>\n", item.first.c_str(), item.second);
    }
    for (auto item : shader->_computeShaderByName) {
      printf("computeshader<%s:%p>\n", item.first.c_str(), item.second);
    }
  });

  ork::opq::mainThreadQueue().drain();
}
