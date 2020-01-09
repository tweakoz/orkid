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

  auto white   = fvec3(1, 1, 1);
  auto yellow  = fvec3(1, 1, 0);
  auto orange  = fvec3(1, 0.5, 0);
  auto cyan    = fvec3(0, 1, 1);
  auto magenta = fvec3(1, 0, 1);
  auto blugrn  = fvec3(0, 1, .5);

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
    animinst->SetCurrentFrame(15);
    animinst->SetWeight(1);
    animinst->RefMask().EnableAll();

    auto modl_asset = asset::AssetManager<XgmModelAsset>::Load("data://test/rigtest");
    printf("modl_asset<%p>\n", modl_asset);
    CHECK(modl_asset != nullptr);
    auto model = modl_asset->GetModel();
    auto& skel = model->skeleton();
    printf("model<%p> isskinned<%d>\n", model, int(model->isSkinned()));

    auto modelinst = new XgmModelInst(model);
    printf("modelinst<%p>\n", modelinst);

    modelinst->SetBlenderZup(true);
    modelinst->EnableSkinning();
    modelinst->EnableAllMeshes();

    deco::prints(skel.dump(cyan), true);

    deco::printe(blugrn, "SkelInvBind (post-concat)", true);
    deco::prints(skel.dumpInvBind(blugrn), true);

    auto& localpose = modelinst->RefLocalPose();
    localpose.BindPose();
    deco::printe(white, "BindPose (pre-concat)", true);
    deco::prints(localpose.dumpc(white), true);

    localpose.Concatenate();
    deco::printe(orange, "BindPose (post-concat)", true);
    deco::prints(localpose.dumpc(orange), true);

    localpose.BindPose();
    localpose.BindAnimInst(*animinst);
    localpose.BuildPose();
    deco::printe(yellow, "AnimPose (pre-concat)", true);
    deco::prints(localpose.dumpc(yellow), true);

    localpose.Concatenate();
    deco::printe(orange, "AnimPose (post-concat)", true);
    deco::prints(localpose.dumpc(orange), true);

    XgmWorldPose worldpose(skel);
    worldpose.apply(ork::fmtx4(), localpose);
    deco::printe(magenta, "WorldPose (post-concat)", true);
    deco::prints(worldpose.dumpc(magenta), true);

    delete animinst;
    delete modelinst;
  });
  opq::mainSerialQueue().drain();
}
