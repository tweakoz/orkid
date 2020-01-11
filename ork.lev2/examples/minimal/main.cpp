#include <QWindow>
#include <ork/kernel/string/deco.inl>
#include <ork/lev2/ezapp.h>
#include <ork/lev2/gfx/renderer/drawable.h>

using namespace std::string_literals;
using namespace ork;
using namespace ork::lev2;
int main(int argc, char** argv) {
  auto qtapp = OrkEzQtApp::create(argc, argv);
  //////////////////////////////////////////////////////////
  qtapp->onGpuInit([&](Context* gfxctx) { deco::printf(fvec3::White(), "gpuINIT - context<%p>", gfxctx); });
  //////////////////////////////////////////////////////////
  qtapp->onDraw([=](const ui::DrawEvent& drwev) {
    auto target = drwev.GetTarget();
    float r     = float(rand() % 255) / 255.0f;
    float g     = float(rand() % 255) / 255.0f;
    float b     = float(rand() % 255) / 255.0f;
    target->FBI()->SetClearColor(fvec4(r, g, b, 1));
    int TARGW           = target->mainSurfaceWidth();
    int TARGH           = target->mainSurfaceHeight();
    const SRect tgtrect = SRect(0, 0, TARGW, TARGH);
    target->beginFrame();
    target->endFrame();
  });
  //////////////////////////////////////////////////////////
  qtapp->onResize([=](int w, int h) { printf("GOTRESIZE<%d %d>\n", w, h); });
  //////////////////////////////////////////////////////////
  qtapp->onUiEvent([=](const ui::Event& ev) -> ui::HandlerResult {
    switch (ev.mEventCode) {
      case ui::UIEV_DOUBLECLICK:
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
