#include <ork/pch.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/lev2/gfx/gfxmaterial_ui.h>
#include <ork/util/hotkey.h>
#include <ork/lev2/gfx/dbgfontman.h>
#include <ork/lev2/gfx/gfxprimitives.h>
#include <ork/lev2/ui/choicelist.h>
#include <ork/lev2/ui/context.h>

namespace ork::ui {
///////////////////////////////////////////////////////////////////////////////
static constexpr int CELL_H = 32;
static constexpr int MAX_H = CELL_H*8;
static constexpr const char* FONTNAME = "i14";
///////////////////////////////////////////////////////////////////////////////
ChoiceList::ChoiceList(
    const std::string& name, //
    fvec4 color,
    int x,
    int y,
    int w,
    int h,
    fvec2 dimensions)
    : Widget(name, x, y, w, h)
    , _fg_color(color)
    , _dimensions(dimensions) {
    
    if(height()>MAX_H)
      SetH(MAX_H);
    
  _value = "";
}
///////////////////////////////////////////////////////////////////////////////
ChoiceList::~ChoiceList(){
  if(_uicontext){
    _uicontext->unsubscribeFromTicks(this);
  }
}
///////////////////////////////////////////////////////////////////////////////
void ChoiceList::_doOnParentChanged(Group* parent) {
  if(_uicontext){
    _uicontext->subscribeToTicks(this,[this](updatedata_ptr_t updata){
      float abstime = updata->_abstime;
      _hl_color.x = 0.5f + (0.5f * sinf(abstime * 3.0f));
      _hl_color.y = 0.5f + (0.5f * sinf(abstime * 3.1f));
      _hl_color.z = 0.5f + (0.5f * sinf(abstime * 3.2f));
      _hoverTime += updata->_dt;

      if( _hoverTime > 0.5 ){
        if( _mouse_hover_y < (CELL_H>>1) ){
          incScroll(8);
        }
        else if( _mouse_hover_y < CELL_H ){
          incScroll(4);
        }
        else if( _mouse_hover_y > (height()-(CELL_H>>1)) ){
          incScroll(-8);
        }
        else if( _mouse_hover_y > (height()-CELL_H) ){
          incScroll(-4);
        }
      }
    });
  }
}
///////////////////////////////////////////////////////////////////////////////
void ChoiceList::setValue(const std::string& val) {
  _value = val;
}
///////////////////////////////////////////////////////////////////////////////
int ChoiceList::selection_index() const{
    int actual_y = _mouse_hover_y - _scroll_y;
    int selidx = std::clamp(actual_y / CELL_H,0,int(_choices.size()-1));
  return selidx;
}
///////////////////////////////////////////////////////////////////////////////
void ChoiceList::incScroll(int amt){
    int invisible_h = _choices.size() * CELL_H - MAX_H;
    int new_scroll = _scroll_y + amt;
    if(new_scroll<(-invisible_h>>1))
      new_scroll = -invisible_h>>1;
    if(new_scroll>(invisible_h>>1))
      new_scroll = invisible_h>>1;
    _scroll_y = new_scroll;
}
///////////////////////////////////////////////////////////////////////////////
HandlerResult ChoiceList::DoOnUiEvent(event_constptr_t cev) {
  HandlerResult rval;

  auto print_item = [this](){
      int selidx = selection_index();
      std::string selstr = _choices[selidx];
      printf( "_scroll_y<%d> selidx<%d> selstr<%s>\n", _scroll_y, selidx, selstr.c_str() );
  };

  switch (cev->_eventcode) {
    case EventCode::KEY_DOWN: {
      int key = cev->miKeyCode;
      printf("key<%d>\n", key);
      switch (key) {
        case 256: // esc
          //_value = _original_value;
          rval._widget_finished = true;
          break;
        case 257: { // enter
          rval._widget_finished = true;
          print_item();
          int selidx = selection_index();
          _value = _choices[selidx];
          break;
        }
        default:
          break;
      }
      rval.setHandled(this);
      _hoverTime = 0.0;

    }
    case ui::EventCode::MOUSEWHEEL: {

      #if defined(__APPLE__) // TODO: technically we want to select on trackpad
      incScroll(cev->miMWY);
      #else
      incScroll(cev->miMWY*16);
      #endif

      print_item();
      break;
    }
    case EventCode::DOUBLECLICK: {
        int selidx = selection_index();
        _value = _choices[selidx];
        rval.setHandled(this);
        rval._widget_finished = true;
        _hoverTime = 0.0;
        break;
    }
    case EventCode::MOUSE_ENTER: {
      OrkAssert(false);
      break;
    }
    case EventCode::MOUSE_LEAVE: {
      OrkAssert(false);
      break;
    }
    case EventCode::MOVE: {
      int x = cev->miX;
      int y = cev->miY;
      _mouse_hover_y = y;

      //printf("ChoiceList::DoOnUiEvent<MOVE> x<%d> y<%d>\n", x, y);
      print_item();
      rval.setHandled(this);
      _hoverTime = 0.0;

      break;
    }
    default:
      break;
  }
  return rval;
}
///////////////////////////////////////////////////////////////////////////////
fvec2 ChoiceList::computeDimensions(const std::vector<std::string>& choices) {
  int numchoices = choices.size();
  int w          = 0;
  int h          = numchoices * CELL_H;
  for (int i = 0; i < numchoices; i++) {
    const std::string& choice = choices[i];
    int sw                    = lev2::FontMan::stringWidth(choice.length());
    w                         = std::max(w, sw);
  }
  if(h>MAX_H)
    h = MAX_H;
  return fvec2(w, h);
}
///////////////////////////////////////////////////////////////////////////////
void ChoiceList::DoDraw(drawevent_constptr_t drwev) {

  int num_choices = _choices.size();

  auto tgt    = drwev->GetTarget();
  auto fbi    = tgt->FBI();
  auto mtxi   = tgt->MTXI();
  auto& primi = lev2::GfxPrimitives::GetRef();
  auto defmtl = lev2::defaultUIMaterial();

  mtxi->PushUIMatrix();
  {
    int ix1, iy1, ix2, iy2, ixc, iyc;
    LocalToRoot(0, 0, ix1, iy1);
    ix2 = ix1 + _geometry._w;
    iy2 = iy1 + _geometry._h;
    ixc = ix1 + (_geometry._w >> 1);
    iyc = iy1 + (_geometry._h >> 1);

    if (0)
      printf(
          "drawChoiceList<%s> xy1<%d,%d> xy2<%d,%d>\n", //
          _name.c_str(),
          ix1,
          iy1,
          ix2,
          iy2);

    ///////////////////////////////////////////////////////////////////////
    // draw default background
    ///////////////////////////////////////////////////////////////////////

    defmtl->_rasterstate.SetBlending(lev2::Blending::ALPHA);
    defmtl->_rasterstate.SetDepthTest(lev2::EDepthTest::OFF);
    tgt->PushModColor(_bg_color);
    defmtl->SetUIColorMode(lev2::UiColorMode::MOD);
    primi.RenderQuadAtZ(
        defmtl.get(),
        tgt,
        ix1,  // x0
        ix2,  // x1
        iy1,  // y0
        iy2,  // y1
        0.0f, // z
        0.0f,
        1.0f, // u0, u1
        0.0f,
        1.0f // v0, v1
    );
    tgt->PopModColor();

    ///////////////////////////////////////////////////////////////////////
    // draw bg for selected choice
    ///////////////////////////////////////////////////////////////////////

    int selidx = (_mouse_hover_y) / CELL_H;
    if (selidx >= 0 && selidx < num_choices) {
      int iy1 = selidx * CELL_H + (_scroll_y%CELL_H);
      int iy2 = iy1 + CELL_H;
      tgt->PushModColor(_hl_color);
      primi.RenderQuadAtZ(
          defmtl.get(),
          tgt,
          ix1,  // x0
          ix2,  // x1
          iy1,  // y0
          iy2,  // y1
          0.0f, // z
          0.0f,
          1.0f, // u0, u1
          0.0f,
          1.0f // v0, v1
      );
      tgt->PopModColor();
    }

    ///////////////////////////////////////////////////////////////////////
    // draw choice labels
    ///////////////////////////////////////////////////////////////////////

    tgt->PushModColor(_fg_color);
    auto font = ork::lev2::FontMan::PushFont(FONTNAME);
    auto& fontdesc = font->description();
    int font_h = fontdesc.miCharHeight;

    constexpr int KMAXCHARS = 32;

    lev2::FontMan::beginTextBlock(tgt, num_choices * KMAXCHARS);

    int numchtodraw = std::min(num_choices, MAX_H / CELL_H);
    for (int i = 0; i < numchtodraw; i++) {
      int iyy     = i - (_scroll_y/CELL_H);
      iyy = std::clamp(iyy, 0, num_choices-1);
      auto choice = _choices[iyy];
      int sw      = lev2::FontMan::stringWidth(choice.length());
      int iyc     = iy1 + (CELL_H>>1);
      lev2::FontMan::DrawText(
          tgt, //
          ixc - (sw >> 1),
          iyc - (font_h>>1) + (_scroll_y % CELL_H),
          choice.c_str());
      iy1 += CELL_H;
    }
    lev2::FontMan::endTextBlock(tgt);
    ork::lev2::FontMan::PopFont();
    tgt->PopModColor();
  }
  mtxi->PopUIMatrix();
}
///////////////////////////////////////////////////////////////////////////////
} // namespace ork::ui
