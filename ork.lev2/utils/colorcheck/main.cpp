////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/kernel/string/deco.inl>
#include <ork/kernel/spawner.h>
#include <ork/math/colormath.inl>
#include <ork/lev2/ezapp.h>
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/gfx/material_freestyle.h>
#include <iomanip>
#include <iostream>

using namespace std::string_literals;
using namespace ork;
using namespace ork::lev2;
using namespace ork::color;

//////////////////////////////////////////////////////////
// read colorimeter
//////////////////////////////////////////////////////////

fvec3 spotread() {

  // todo : keep spotread open and stream
  //   repeated measurements
  //   to amortize colorimeter startup time costs

  // todo: Im pretty sure spotread in -u mode
  //   spits out CIELUV - so ensure YUV conversion
  //   is correct..

  fvec3 rval;
  const int MAX_BUFFER = 255;
  std::string stdout;
  char buffer[MAX_BUFFER];
  FILE* stream = popen("spotread -e -O -T -x", "r");
  while (fgets(buffer, MAX_BUFFER, stream) != NULL)
    stdout.append(buffer);
  pclose(stream);
  auto lines = SplitString(stdout, '\n');

  for (auto l : lines) {
    auto it = l.find("Yxy");
    if (it != std::string::npos) {
      auto lsub = l.substr(it);
      // printf("lsub<%s>\n", lsub.c_str());
      float Y, x, y;
      sscanf(lsub.c_str(), "Yxy: %f %f %f", &Y, &x, &y);
      rval = fvec3(Y, x, y);
    }
  }
  return rval;
}

//////////////////////////////////////////////////////////

int main(int argc, char** argv) {
  auto qtapp = OrkEzApp::create(argc, argv);
  //////////////////////////////////////////////////////////
  deco::printf(fvec3::White(), "reading colorimeter, please wait...\n");
  fvec3 yxy = spotread();
  deco::printf(fvec3::Yellow(), "CIE 1931 Y<%g nits> xy<%g %g>\n", yxy.x, yxy.y, yxy.z);
  float yy = yxy.x;
  float xx = yxy.y * yxy.x / yxy.z;
  float zz = (1.0 - yxy.y - yxy.z) * yxy.x / yxy.z;
  deco::printf(fvec3::Yellow(), "CIE XYZ <%g %g %g> absolute\n", xx, yy, zz);
  // printf("XYZ<%g %g %g>\n", xx, yy, zz);
  const float STDLUM = 300.0f; // nits
  fvec3 XYZ(xx, yy, zz);
  XYZ = XYZ * 1.0f / STDLUM;
  deco::printf(fvec3::Yellow(), "CIE XYZ<%g %g %g> std\n", XYZ.x, XYZ.y, XYZ.z);
  //////////////////////////////////////////////////////////
  fvec3 adobergb50 = cieXyz_to_D50AdobeRGB(XYZ);
  fvec3 adobergb65 = cieXyz_to_D65AdobeRGB(XYZ);
  deco::printf(fvec3::Yellow(), "AdobeRGB-D50<%g %g %g>\n", adobergb50.x, adobergb50.y, adobergb50.z);
  deco::printf(fvec3::Yellow(), "AdobeRGB-D65<%g %g %g>\n", adobergb65.x, adobergb65.y, adobergb65.z);
  //////////////////////////////////////////////////////////
  fvec3 srgb50 = cieXyz_to_D50sRGB(XYZ);
  fvec3 srgb65 = cieXyz_to_D65sRGB(XYZ);
  deco::printf(fvec3::Yellow(), "sRGB-D50<%g %g %g>\n", srgb50.x, srgb50.y, srgb50.z);
  deco::printf(fvec3::Yellow(), "sRGB-D65<%g %g %g>\n", srgb65.x, srgb65.y, srgb65.z);
  //////////////////////////////////////////////////////////
  fvec3 clamped = srgb65.saturated();
  fvec3 rgb     = linear_to_sRGB(clamped, 2.2);
  deco::printf(fvec3::Yellow(), "LinearRGB <%g %g %g>\n", rgb.x, rgb.y, rgb.z);
  //////////////////////////////////////////////////////////
  // project private asset filedevctx
  // so we can load our private shader
  //////////////////////////////////////////////////////////
  auto this_dir  = file::Path::orkroot_dir() / "ork.lev2" / "utils" / "colorcheck";
  auto ccheckctx = qtapp->newFileDevContext("colorcheck://", this_dir);
  ccheckctx->SetPrependFilesystemBase(true);
  printf("ccheckpath<%s>\n", this_dir.c_str());
  //////////////////////////////////////////////////////////
  auto qtwin  = qtapp->_mainWindow;
  auto gfxwin = qtwin->_gfxwin;
  FreestyleMaterial material;
  const FxShaderTechnique* fxtechnique = nullptr;
  const FxShaderParam* fxparameterMVP  = nullptr;
  const FxShaderParam* fxparameterT    = nullptr;
  float t                              = 0.0f;
  //////////////////////////////////////////////////////////
  //////////////////////////////////////////////////////////
  qtapp->onGpuInit([&](Context* ctx) {
    material.gpuInit(ctx, "colorcheck://shader");
    fxtechnique    = material.technique("yuv2rgb");
    fxparameterMVP = material.param("mvp");
    fxparameterT   = material.param("t");
    deco::printf(fvec3::White(), "gpuINIT - context<%p>\n", ctx, fxtechnique);
    deco::printf(fvec3::Yellow(), "  fxtechnique<%p>\n", fxtechnique);
    deco::printf(fvec3::Yellow(), "  fxparameterMVP<%p>\n", fxparameterMVP);
    deco::printf(fvec3::Yellow(), "  fxparameterT<%p>\n", fxparameterT);
    qtwin->_ctqt->pushRefreshPolicy(RefreshPolicyItem{EREFRESH_FIXEDFPS, 60});
  });
  //////////////////////////////////////////////////////////
  qtapp->onDraw([&](ui::drawevent_constptr_t drwev) {
    auto context        = drwev->GetTarget();
    auto fbi            = context->FBI(); // FrameBufferInterface
    auto fxi            = context->FXI(); // FX Interface
    int TARGW           = context->mainSurfaceWidth();
    int TARGH           = context->mainSurfaceHeight();
    const SRect tgtrect = SRect(0, 0, TARGW, TARGH);

    fbi->SetClearColor(fvec4::Black());
    context->beginFrame();
    RenderContextFrameData RCFD(context);
    material.bindTechnique(fxtechnique);
    material.begin(RCFD);
    material.bindParamMatrix(fxparameterMVP, fmtx4::Identity());
    material.bindParamFloat(fxparameterT, fmod(t, 1));
    gfxwin->Render2dQuadEML(fvec4(-1, -1, 2, 2), fvec4(0), fvec4(0));
    material.end(RCFD);
    context->endFrame();
    t += 0.001f;
  });
  //////////////////////////////////////////////////////////
  qtapp->onResize([&](int w, int h) { printf("GOTRESIZE<%d %d>\n", w, h); });
  //////////////////////////////////////////////////////////
  qtapp->onUiEvent([&](ui::event_constptr_t ev) -> ui::HandlerResult {
    switch (ev._eventcode) {
      case ui::EventCode::DOUBLECLICK:
        OrkAssert(false);
        break;
      default:
        break;
    }
    ui::HandlerResult rval;
    return rval;
  });
  //////////////////////////////////////////////////////////
  return qtapp->exec();
}
