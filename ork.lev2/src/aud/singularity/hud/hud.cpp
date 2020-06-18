#include <ork/math/cvector3.h>
#include <ork/lev2/aud/singularity/dspblocks.h>
#include <ork/lev2/aud/singularity/hud.h>

using namespace ork;
using namespace ork::lev2;

///////////////////////////////////////////////////////////////////////////////
namespace ork::audio::singularity {
float hud_contentscale() {
  return _HIDPI() ? 2.0 : 1.0;
}
int hud_lineheight() {
  return FontMan::currentFont()->description().miCharHeight;
}
///////////////////////////////////////////////////////////////////////////////
static vtxbuf_ptr_t create_vertexbuffer(Context* context) {
  auto vb = std::make_shared<vtxbuf_t>(16 << 20, 0, EPrimitiveType::NONE); // ~800 MB
  vb->SetRingLock(true);
  return vb;
}
vtxbuf_ptr_t get_vertexbuffer(Context* context) {
  static auto vb = create_vertexbuffer(context);
  return vb;
}
///////////////////////////////////////////////////////////////////////////////
static freestyle_mtl_ptr_t create_hud_material(Context* context) {
  auto mtl = std::make_shared<FreestyleMaterial>();
  mtl->gpuInit(context, "orkshader://solid");
  return mtl;
}
freestyle_mtl_ptr_t hud_material(Context* context) {
  static auto mtl = create_hud_material(context);
  return mtl;
}
///////////////////////////////////////////////////////////////////////////////
void Rect::PushOrtho(Context* context) const {
  int w = context->mainSurfaceWidth();
  int h = context->mainSurfaceHeight();
  context->MTXI()->PushUIMatrix(w, h);
}
///////////////////////////////////////////////////////////////////////////////
void Rect::PopOrtho(Context* context) const {
  context->MTXI()->PopUIMatrix();
}
///////////////////////////////////////////////////////////////////////////////
HudLayoutGroup::HudLayoutGroup() //
    : ui::LayoutGroup("HUD", 0, 0, 1280, 720) {
}
///////////////////////////////////////////////////////////////////////////////
void HudLayoutGroup::onUpdateThreadTick(ui::updatedata_ptr_t updata) {
  ///////////////////////////////////////////
  // pull hud frame data
  ///////////////////////////////////////////
  auto syn = synth::instance();
  if (syn->_clearhuddata) {
    syn->_hudsample_map.clear();
    syn->_clearhuddata = false;
  }
  svar_t hdata;
  while (syn->_hudbuf.try_pop(hdata)) {
    if (hdata.IsA<HudFrameControl>()) {
      syn->_curhud_kframe = hdata.Get<HudFrameControl>();
    }
  }
  if ((_updcount & 0xff) == 0) {
    printf("Synth CPULOAD<%0.1f%%>\n", syn->_cpuload * 100.0f);
    fflush(stdout);
  }
  _updcount++;
}
///////////////////////////////////////////////////////////////////////////////
void drawtext(
    ui::Surface* surface, //
    Context* context,     //
    const std::string& str,
    float x,
    float y,
    float scale,
    float r,
    float g,
    float b) {

  int w = surface->width();
  int h = surface->height();
  context->MTXI()->PushUIMatrix(w, h);

  auto fontman = FontMan::instance();

  if (ork::lev2::_HIDPI()) {
    fontman->SetCurrentFont("i32");
  } else {
    fontman->SetCurrentFont("i16");
  }
  context->PushModColor(fcolor4(r, g, b, 1));

  fontman->beginTextBlock(context, str.length());
  fontman->DrawText(context, x, y, "%s", str.c_str());
  fontman->endTextBlock(context);
  context->PopModColor();

  context->MTXI()->PopUIMatrix();
}

///////////////////////////////////////////////////////////////////////////////

void drawHudLines(
    ui::Surface* surface, //
    Context* context,
    const hudlines_t& lines) {

  if (lines.size() == 0)
    return;

  auto mtl = hud_material(context);
  auto tek = mtl->technique("vtxcolor");
  RenderContextFrameData RCFD(context);
  auto vbuf = get_vertexbuffer(context);
  auto mtxi = context->MTXI();
  auto gbi  = context->GBI();

  auto par_mvp = mtl->param("MatMVP");

  VtxWriter<SVtxV16T16C16> vw;
  vw.Lock(context, vbuf.get(), lines.size() * 2);
  for (auto& l : lines) {
    const auto& p1 = l._from;
    const auto& p2 = l._to;
    const auto& c  = l._color;
    vw.AddVertex(SVtxV16T16C16(fvec3(p1), fvec4(), c));
    vw.AddVertex(SVtxV16T16C16(fvec3(p2), fvec4(), c));
  }
  vw.UnLock(context);

  int w = surface->width();
  int h = surface->height();
  mtxi->PushUIMatrix(w, h);
  mtl->begin(tek, RCFD);
  mtl->bindParamMatrix(par_mvp, mtxi->RefMVPMatrix());
  mtl->_rasterstate.SetBlending(EBLENDING_OFF);
  gbi->DrawPrimitiveEML(vw, EPrimitiveType::LINES);
  mtl->end(RCFD);
  mtxi->PopUIMatrix();
}

///////////////////////////////////////////////////////////////////////////////

float FUNH(float vpw, float vph) {
  return (vph / 6);
}
float FUNW(float vpw, float vph) {
  float fh   = FUNH(vpw, vph);
  float funw = fh * 1.5;
  return funw;
}
float FUNX(float vpw, float vph) {
  return vpw - FUNW(vpw, vph);
}
float ENVW(float vpw, float vph) {
  return FUNW(vpw, vph) * 1.5;
}
float ENVH(float vpw, float vph) {
  return (vph / 8);
}
float ENVX(float vpw, float vph) {
  return FUNX(vpw, vph) - ENVW(vpw, vph) - 16;
}
float DSPW(float vpw, float vph) {
  return FUNW(vpw, vph);
}
float DSPX(float vpw, float vph) {
  return ENVX(vpw, vph) - (FUNW(vpw, vph) + 16);
}

float SCOPEX() {
  return 32 * hud_contentscale();
}
float SCOPEW() {
  return 960 * hud_contentscale();
}
float SCOPEH() {
  return 400 * hud_contentscale();
}
///////////////////////////////////////////////////////////////////////////////
void DrawBorder(Context* context, int X1, int Y1, int X2, int Y2, int color) {
  hudlines_t lines;
  fvec3 vcolor(1, 1, 1);
  switch (color) {
    case 0:
      vcolor = fvec3(0.6, 0.3, 0.6);
      break;
    case 1:
      vcolor = fvec3(0.0, 0.0, 0.0);
      break;
    case 2:
      vcolor = fvec3(0.9, 0.0, 0.0);
      break;
  }
  auto addline = [&lines, &vcolor](float xa, float ya, float xb, float yb) { //
    lines.push_back(HudLine{fvec2{xa, ya}, fvec2{xb, yb}, vcolor});
  };
  addline(X1, Y1, X2, Y1);
  addline(X2, Y1, X2, Y2);
  addline(X2, Y2, X1, Y2);
  addline(X1, Y2, X1, Y1);
  // drawHudLines(context, lines);
}
} // namespace ork::audio::singularity
