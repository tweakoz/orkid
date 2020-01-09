#include <ork/pch.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/lev2_asset.h>
#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/kernel/opq.h>
#include <utpp/UnitTest++.h>
#include <ork/kernel/string/deco.inl>

using namespace ork;
using namespace ork::lev2;

TEST(gfxanim1) {
  // we must load shaders on the main thread!
  opq::mainSerialQueue().enqueue([&]() {
    auto targ = GfxEnv::GetRef().GetLoaderTarget();
    printf("targ<%p>\n", targ);
    CHECK(targ != nullptr);

    auto fxi = targ->FXI();
    printf("fxi<%p>\n", fxi);
    CHECK(fxi != nullptr);

    auto anim_asset = asset::AssetManager<XgmAnimAsset>::Load("data://tests/rigtest_link");
    printf("anim_asset<%p>\n", anim_asset);
    CHECK(anim_asset != nullptr);
    auto anim     = anim_asset->GetAnim();
    auto animinst = new XgmAnimInst;
    animinst->BindAnim(anim);
    animinst->SetCurrentFrame(0);
    animinst->SetWeight(1);
    animinst->RefMask().EnableAll();

    auto modl_asset = asset::AssetManager<XgmModelAsset>::Load("data://test/rigtest");
    printf("modl_asset<%p>\n", modl_asset);
    CHECK(modl_asset != nullptr);
    auto model = modl_asset->GetModel();
    auto& skel = model->RefSkel();
    printf("model<%p> isskinned<%d>\n", model, int(model->IsSkinned()));

    auto modelinst = new XgmModelInst(model);
    printf("modelinst<%p>\n", modelinst);

    modelinst->SetBlenderZup(true);
    modelinst->EnableSkinning();
    modelinst->EnableAllMeshes();

    deco::printe(fvec3(0, 1, 1), skel.dump(), true);

    auto& localpose = modelinst->RefLocalPose();
    localpose.BindPose();
    deco::printe(fvec3(1, 1, 1), localpose.dump(), true);
    localpose.BindAnimInst(*animinst);
    localpose.BuildPose();
    deco::printe(fvec3(1, 1, 0), localpose.dump(), true);
    XgmWorldPose worldpose(skel, localpose);
    worldpose.apply(ork::fmtx4(), localpose);

    delete animinst;
    delete modelinst;
  });
  opq::mainSerialQueue().drain();
}
