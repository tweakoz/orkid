////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/math/cvector3.h>
#include <ork/lev2/aud/singularity/dspblocks.h>
#include <ork/lev2/aud/singularity/hud.h>
#include <ork/lev2/gfx/material_freestyle.h>
#include <ork/lev2/ezapp.h> // todo move updatedata_ptr_t out..
#include <ork/util/logger.h>
#include <ork/lev2/gfx/gfxvtxbuf.inl>

using namespace ork;
using namespace ork::lev2;

///////////////////////////////////////////////////////////////////////////////
namespace ork::audio::singularity {
static logchannel_ptr_t logchan_hud = logger()->createChannel("singul.hud", fvec3(1, 0.6, .8), true);
float hud_contentscale() {
  return _HIDPI() ? 2.0 : 1.0;
}
int hud_lineheight() {
  return FontMan::currentFont()->description().miCharHeight;
}
///////////////////////////////////////////////////////////////////////////////
static vtxbuf_ptr_t create_vertexbuffer(Context* context) {
  auto vb = std::make_shared<vtxbuf_t>(16 << 20, 0); // ~800 MB
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
    : ui::LayoutGroup("HUD", 0, 0, 1280, 720) { // //here (name==1)

  /////////////////////////////////
  // route musical kb events to here
  //  all other events route as normal
  /////////////////////////////////

  _notemap = {
      {'z', 36},
      {'s', 37},
      {'x', 38},
      {'d', 39},
      {'c', 40},
      {'v', 41},
      {'g', 42},
      {'b', 43},
      {'h', 44},
      {'n', 45},
      {'j', 46},
      {'m', 47},
      {',', 48},
      {'l', 49},
      {'.', 50},
      {';', 51},
      {'/', 52},
  };
  _handledkeymap = _notemap;
  for (int i = 0; i <= 9; i++){
    _handledkeymap['0' + i] = 0;
  }
  _handledkeymap['['] = 0;
  _handledkeymap[']'] = 0;
  _handledkeymap[0] = 0;

  _evrouter = [this](ui::event_constptr_t ev) -> ui::Widget* { //
    switch (ev->_eventcode) {
      case ui::EventCode::KEY_DOWN:
      case ui::EventCode::KEY_UP: {
        int key = ev->miKeyCode;
        auto it = _handledkeymap.find(key);
        if (it != _handledkeymap.end()) {
          printf("hudevroute<%p>\n", (void*) this);
          return this;
        }
        break;
      }
      default:
        break;
    }
    return doRouteUiEvent(ev);
  };

  _evhandler = [this](ui::event_constptr_t ev) -> ui::HandlerResult { //
    ui::HandlerResult result;

    bool was_handled = false;
    //printf("hudlg evh\n");
    switch (ev->_eventcode) {
      case ui::EventCode::KEY_DOWN: {
        printf( "ev->miKeyCode<%d>\n", ev->miKeyCode );
        switch (ev->miKeyCode) {

          case 256: { // escape 
            _ezapp->signalExit();
            break;
          }
          case '[': {
            synth::instance()->_masterGain *= 0.95;
            logchan_hud->log("MasterGain<%g>", synth::instance()->_masterGain );
            was_handled = true;
            break;
          }
          case ']': {
            synth::instance()->_masterGain *= 1.0/0.95;
            logchan_hud->log("MasterGain<%g>", synth::instance()->_masterGain );
            was_handled = true;
            break;
          }
          case '5': {
            auto bus = synth::instance()->_curprogrambus;
            synth::instance()->nextEffect(bus);
            was_handled = true;
            break;
          }
          case '9': {
            _velocity += 16;
            _velocity   = std::clamp(_velocity, 0, 127);
            was_handled = true;
            break;
          }
          case '7': {
            _velocity -= 16;
            _velocity   = std::clamp(_velocity, 0, 127);
            was_handled = true;
            break;
          }
          case '6': {
            synth::instance()->nextProgram();
            was_handled = true;
            break;
          }
          case '4': {
            synth::instance()->prevProgram();
            was_handled = true;
            break;
          }
          case '3': {
            _octaveshift++;
            _octaveshift = std::clamp(_octaveshift, -1, 4);
            was_handled  = true;
            break;
          }
          case '1': {
            _octaveshift--;
            _octaveshift = std::clamp(_octaveshift, -1, 4);
            was_handled  = true;
            break;
          }
          case ' ': {
            for (auto item : _activenotes) {
              auto pi = item.second;
              //synth::instance()->liveKeyOff(pi);
            }
            _activenotes.clear();
            was_handled = true;
            break;
          }
          default: {
            auto prog = synth::instance()->_globalprog;
            auto it   = _notemap.find(ev->miKeyCode);
            if (it != _notemap.end()) {
              int note = it->second;
              note += (_octaveshift * 12);
              printf("note<%d>\n", note);
              auto pi            = synth::instance()->liveKeyOn(note, _velocity, prog);
              _activenotes[note] = pi;
              was_handled        = true;
            }
            break;
          }
        } // switch (ev->miKeyCode) {
        break;
        default:
          break;
      }
      case ui::EventCode::KEY_UP: {
        switch (ev->miKeyCode) {
          default: {
            auto it = _notemap.find(ev->miKeyCode);
            if (it != _notemap.end()) {
              was_handled = true;
              int note    = it->second;
              note += (_octaveshift * 12);
              auto it2 = _activenotes.find(note);
              if (it2 != _activenotes.end()) {
                auto pi = it2->second;
                if (pi){
                  //synth::instance()->liveKeyOff(pi);
                }
                _activenotes.erase(it2);
              }
            }
            break;
          }
        }
        break;
      } // case ui::EventCode::KEYUP: {
      /////////////////////////////
      // handle layout guide drag events
      /////////////////////////////
      case ui::EventCode::PUSH: 
      case ui::EventCode::RELEASE: 
      case ui::EventCode::BEGIN_DRAG: 
      case ui::EventCode::END_DRAG: 
      case ui::EventCode::DRAG: {
        result = LayoutGroup::OnUiEvent(ev);
        was_handled = (result.mHandler!=nullptr);
        break;
      }
    }   // switch (ev->_eventcode) {
    if(was_handled)
      result.setHandled(this);
    return result;
  };

  /////////////////////////////////
} // namespace ork::audio::singularity
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
    if (hdata.isA<HudFrameControl>()) {
      syn->_curhud_kframe = hdata.get<HudFrameControl>();
    }
  }
  if ((_updcount & 0xff) == 0) {
    logchan_hud->log("Synth CPULOAD<%0.1f%%>", syn->_cpuload * 100.0f);
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
  auto RCFD = std::make_shared<RenderContextFrameData>(context);
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
  mtl->_rasterstate.SetBlending(Blending::OFF);
  gbi->DrawPrimitiveEML(vw, PrimitiveType::LINES);
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

///////////////////////////////////////////////////////////////////////////////

void HudEventRouter::registerSinkForHudEvent(uint32_t eventID, hudeventsink_ptr_t sink){
  _routing_map[eventID].push_back(sink);
}

///////////////////////////////////////////////////////////////////////////////

void HudEventRouter::routeEvent(hudevent_ptr_t hev){
  uint32_t eventID = hev->_eventID;
  auto it = _routing_map.find(eventID);
  if(it!=_routing_map.end()){
    auto& sinks = it->second;
    for(auto sink : sinks){
      sink->_onEvent(hev);
    }
  }
}

///////////////////////////////////////////////////////////////////////////////

void HudEventRouter::processEvents(){
  hudevent_ptr_t hev;
  while(_hudevents.try_pop(hev)){
    routeEvent(hev);
  }
}

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::audio::singularity
