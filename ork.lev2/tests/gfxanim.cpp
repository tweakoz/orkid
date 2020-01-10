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
  auto indigo  = fvec3(0.7, 0.2, 1);
  auto blugrn  = fvec3(0, 1, .5);

  opq::mainSerialQueue().enqueue([&]() {
    auto targ = GfxEnv::GetRef().GetLoaderTarget();
    printf("targ<%p>\n", targ);
    CHECK(targ != nullptr);

    auto fxi = targ->FXI();
    printf("fxi<%p>\n", fxi);
    CHECK(fxi != nullptr);

    auto anim   = new XgmAnim;
    bool loadOK = XgmAnim::LoadUnManaged(anim, "data://test/rigtest_link");
    OrkAssert(loadOK);
    auto animinst = new XgmAnimInst;

    auto modl_asset = asset::AssetManager<XgmModelAsset>::Load("data://test/rigtest_exp");
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

    animinst->BindAnim(anim);
    animinst->RefMask().EnableAll();

    deco::prints(skel.dump(cyan), true);

    deco::printf(cyan, "//////////////////////////////////////////////\n");
    deco::printf(cyan, "// skeleton pose info\n");
    deco::printf(cyan, "//////////////////////////////////////////////\n");

    deco::printe(blugrn, "SkelInvBind (post-concat)", true);
    deco::prints(skel.dumpInvBind(blugrn), true);

    auto& localpose = modelinst->RefLocalPose();
    localpose.BindPose();
    deco::printe(white, "BindPose (pre-concat)", true);
    deco::prints(localpose.dumpc(white), true);

    localpose.Concatenate();
    deco::printe(orange, "BindPose (post-concat)", true);
    deco::prints(localpose.dumpc(orange), true);

    localpose.BindAnimInst(*animinst);

    int iframe = 0;

    XgmWorldPose worldpose(skel);

    worldpose.apply(ork::fmtx4(), localpose);
    deco::printf(magenta, "WorldPose (bind-post-concat)\n");
    deco::prints(worldpose.dumpc(magenta), true);
    usleep(1 << 20);

    deco::printf(cyan, "//////////////////////////////////////////////\n");
    deco::printf(cyan, "// begin animation\n");
    deco::printf(cyan, "//////////////////////////////////////////////\n");

    while (true) {
      localpose.BindPose();
      animinst->SetCurrentFrame((iframe++) % 20);
      animinst->SetWeight(1);
      localpose.ApplyAnimInst(*animinst);
      localpose.BuildPose();
      deco::printf(white, "fr<%d> AnimPose (pre-concat)\n", iframe);
      deco::prints(localpose.dumpc(white), true);

      localpose.Concatenate();
      deco::printf(orange, "fr<%d> AnimPose (post-concat)\n", iframe);
      deco::prints(localpose.dumpc(orange), true);

      worldpose.apply(ork::fmtx4(), localpose);
      deco::printf(magenta, "fr<%d> WorldPose (post-concat)", iframe);
      deco::prints(worldpose.dumpc(magenta), true);
      usleep(1 << 20);
    }

    delete animinst;
    delete modelinst;
  });
  opq::mainSerialQueue().drain();
}
