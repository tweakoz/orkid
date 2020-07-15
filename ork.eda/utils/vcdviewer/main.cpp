#include <ork/pch.h>
#include <ork/lev2/ui/anchor.h>
#include <ork/lev2/ui/box.h>
#include <ork/lev2/ui/split_panel.h>
#include <ork/lev2/ui/viewport.h>
#include <ork/lev2/ui/layoutgroup.inl>
#include <ork/lev2/ui/context.h>
#include <ork/lev2/ui/label.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/lev2/gfx/gfxmaterial_ui.h>
#include <ork/util/hotkey.h>
#include <ork/lev2/gfx/dbgfontman.h>
#include <ork/lev2/gfx/gfxprimitives.h>
#include <queue>
#include "harness.h"

using namespace std::string_literals;
using namespace ork;
using namespace ork::hdl;
using namespace ork::hdl::vcd;
using namespace ork::lev2;
using namespace ork::ui;

void TestViewport::DoDraw(ui::drawevent_constptr_t drwev) {
  drawChildren(drwev);
}
void TestViewport::onUpdateThreadTick(ui::updatedata_ptr_t updata) {
}
struct ViewParams {
  int _min_timestamp = 0;
  int _max_timestamp = 0;
};
using viewparams_ptr_t = std::shared_ptr<ViewParams>;

struct SignalTrack {
  std::string _name;
  signal_ptr_t _signal;
};
struct ScopeTrack {
  std::string _name;
  scope_ptr_t _scope;
  std::vector<SignalTrack> _sigtracks;
};

struct SignalTrackWidget final : public Widget {

  using vtx_t = SVtxV12C4T16;

  SignalTrackWidget(
      signal_ptr_t sig, //
      fvec4 color)
      : Widget("SignalTrackWidget")
      , _color(color)
      , _signal(sig)
      , _vtxbuf(1 << 20, 0, PrimitiveType::NONE) {
    _textcolor = fvec4(1, 1, 1, 1);
  }
  fvec4 _color;
  fvec4 _textcolor;
  signal_ptr_t _signal;
  viewparams_ptr_t _viewparams;
  std::string _label;
  size_t _numsamples;
  std::string _font = "i14";
  bool _vbdirty     = true;
  DynamicVertexBuffer<vtx_t> _vtxbuf;
  void DoDraw(ui::drawevent_constptr_t drwev) override {
    {

      _label = FormatString(
          "%s[%d] numpts<%zu>", //
          _signal->_longname.c_str(),
          _signal->_bit_width,
          _signal->_samples.size());

      auto tgt    = drwev->GetTarget();
      auto fbi    = tgt->FBI();
      auto gbi    = tgt->GBI();
      auto mtxi   = tgt->MTXI();
      auto& primi = lev2::GfxPrimitives::GetRef();
      auto defmtl = lev2::defaultUIMaterial();

      ////////////////////////////////
      if (_vbdirty) {
        _numsamples = _signal->_samples.size();
        _vbdirty    = false;
        VtxWriter<vtx_t> vw;
        vw.Lock(
            tgt, //
            &_vtxbuf,
            _numsamples * 2);

        for (auto sitem : _signal->_samples) {
          int timestamp = sitem.first;
          float fx      = float(timestamp);
          vtx_t vtx;
          vtx._position = fvec3(fx, 0.0, 0.0f);
          vtx._color    = 0x00ff00ff;
          vw.AddVertex(vtx);
          vtx._position = fvec3(fx, 1.0, 0.0f);
          vw.AddVertex(vtx);
        }
        vw.UnLock(tgt);
      }
      ////////////////////////////////
      float ftimemin   = float(_viewparams->_min_timestamp);
      float ftimemax   = float(_viewparams->_max_timestamp);
      float ftimerange = ftimemax - ftimemin;
      float mainsurfw  = tgt->mainSurfaceWidth();
      float mainsurfh  = tgt->mainSurfaceHeight();
      // float viewproportionw = float(_geometry._w) / mainsurfw;
      // float viewproportionh = float(_geometry._h) / mainsurfh;
      // float viewproportiony = float(_geometry._y) / mainsurfh;
      /*printf(
          "msurf<%g %g> vp<%g %g>\n", //
          mainsurfw,
          mainsurfh,
          viewproportionw,
          viewproportionh);*/
      float matrix_xscale  = float(_geometry._w) / ftimerange;
      float matrix_xoffset = float(_geometry._x);
      float matrix_yscale  = float(_geometry._h - 4);
      float matrix_yoffset = float(_geometry._y) + 5;
      ////////////////////////////////
      int ix1, iy1, ix2, iy2, ixc, iyc;
      LocalToRoot(0, 0, ix1, iy1);
      ix2 = ix1 + _geometry._w;
      iy2 = iy1 + _geometry._h;
      ixc = ix1 + (_geometry._w >> 1);
      iyc = iy1 + (_geometry._h >> 1);

      defmtl->_rasterstate.SetBlending(lev2::Blending::ALPHA);
      defmtl->_rasterstate.SetDepthTest(lev2::EDEPTHTEST_OFF);
      tgt->PushModColor(_color);
      mtxi->PushUIMatrix();
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

      fmtx4 scale_matrix, trans_matrix;
      scale_matrix.SetScale(matrix_xscale, matrix_yscale, 1.0f);
      trans_matrix.Translate(matrix_xoffset, matrix_yoffset, 0.0f);
      mtxi->PushMMatrix(scale_matrix * trans_matrix);
      tgt->PushModColor(fvec4(0, 1, 0, 1));

      gbi->DrawPrimitive(
          defmtl.get(), //
          _vtxbuf,
          PrimitiveType::LINES,
          0,
          _numsamples * 2);

      tgt->PopModColor();
      mtxi->PopMMatrix();

      mtxi->PopUIMatrix();
      /*int lablen = _label.length();
      if (lablen) {
        tgt->PushModColor(_textcolor);
        ork::lev2::FontMan::PushFont(_font);
        lev2::FontMan::beginTextBlock(tgt, lablen);
        int sw = lev2::FontMan::stringWidth(lablen);
        lev2::FontMan::DrawText(
            tgt, //
            ixc - (sw >> 1),
            iyc - 6,
            _label.c_str());
        //
        lev2::FontMan::endTextBlock(tgt);
        ork::lev2::FontMan::PopFont();
        tgt->PopModColor();
      }*/
    }
  }
};

int main(int argc, char** argv) {
  //////////////////////////////////////
  std::string orkdir = getenv("ORKID_WORKSPACE_DIR");
  //////////////////////////////////////
  auto inppath = ork::file::Path(orkdir) //
                 / "ork.data"            //
                 / "tests"               //
                 / "test.vcd";
  //////////////////////////////////////
  vcd::File vcdfile;
  vcdfile.parse(inppath);
  //////////////////////////////////////
  auto app = createEZapp(argc, argv);
  //////////////////////////////////////
  auto vp       = app->_topLayoutGroup;
  auto w_tracks = vp->makeChild<LayoutGroup>("w_tracks");
  auto w_status = vp->makeChild<EvTestBox>("status", fvec4(0, 1, 0, 1));
  //////////////////////////////////////
  auto root_layout = vp->_layout;
  auto l_tracks    = w_tracks._layout;
  auto l_status    = w_status._layout;
  //////////////////////////////////////
  l_tracks->setMargin(4);
  l_status->setMargin(2);
  //////////////////////////////////////
  auto cg1 = root_layout->fixedHorizontalGuide(-32); // 1
  //////////////////////////////////////
  l_tracks->top()->anchorTo(root_layout->top());       // 2,3
  l_tracks->left()->anchorTo(root_layout->left());     // 4,5
  l_tracks->bottom()->anchorTo(cg1);                   // 6
  l_tracks->right()->anchorTo(root_layout->right());   // 7,8
  auto selx_guide = l_tracks->fixedVerticalGuide(192); // 1
  //////////////////////////////////////
  l_status->top()->anchorTo(cg1);                      // 18
  l_status->left()->anchorTo(root_layout->left());     // 19
  l_status->bottom()->anchorTo(root_layout->bottom()); // 20,21
  l_status->right()->anchorTo(root_layout->right());   // 22
  //////////////////////////////////////
  // recursive descent into scoped-signals
  //////////////////////////////////////
  std::stack<scope_ptr_t> scope_stack;
  std::stack<std::string> scopename_stack;
  std::vector<ScopeTrack> scopetracks;
  std::vector<SignalTrack> flat_sigtracks;
  scope_stack.push(vcdfile._root);
  scopename_stack.push("");
  while (not scope_stack.empty()) {
    auto front   = scope_stack.top();
    auto topname = scopename_stack.top();

    printf("visiting scope<%s>\n", topname.c_str());

    ScopeTrack scopetrk;
    scopetrk._name  = topname;
    scopetrk._scope = front;

    for (auto sigitem : front->_signals) {
      auto signal = sigitem.second;
      if (signal->_samples.size() != 0) {
        SignalTrack sigtrk;
        sigtrk._name   = signal->_longname;
        sigtrk._signal = signal;
        scopetrk._sigtracks.push_back(sigtrk);
        flat_sigtracks.push_back(sigtrk);
      }
    }
    scopetracks.push_back(scopetrk);
    scope_stack.pop();
    scopename_stack.pop();
    for (auto ch : front->_child_scopes) {
      scope_stack.push(ch.second);
      auto chname = topname + "/" + ch.second->_name;
      scopename_stack.push(chname);
    }
  }
  //////////////////////////////////////
  auto vparams            = std::make_shared<ViewParams>();
  vparams->_min_timestamp = *vcdfile._timestamps.begin();
  vparams->_max_timestamp = *vcdfile._timestamps.rbegin();
  printf("min_timestamp<%d>\n", vparams->_min_timestamp);
  printf("max_timestamp<%d>\n", vparams->_max_timestamp);
  //////////////////////////////////////
  int numsigtracks = flat_sigtracks.size();
  std::vector<anchor::guide_ptr_t> track_guides;
  track_guides.resize(numsigtracks + 1);
  track_guides[0] = l_tracks->top();
  for (int i = 0; i < numsigtracks; i++) {
    float fitop         = float(i) / float(numsigtracks);
    float fibot         = float(i + 1) / float(numsigtracks);
    auto gtop           = l_tracks->proportionalHorizontalGuide(fitop);
    auto gbot           = l_tracks->proportionalHorizontalGuide(fibot);
    track_guides[i + 1] = gbot;
    auto sigtrack       = flat_sigtracks[i];

    auto label = FormatString("%s[%d]", sigtrack._name.c_str(), sigtrack._signal->_bit_width);

    auto labclr = (i & 1) ? fvec4(0.1, 0.1, 0.1, 1.0) : fvec4(0.2, 0.2, 0.2, 1.0);

    auto w  = w_tracks._widget->makeChild<Label>(sigtrack._name, labclr, label);
    auto wl = w._layout;
    wl->top()->anchorTo(gtop);
    wl->bottom()->anchorTo(gbot);
    wl->left()->anchorTo(l_tracks->left());
    wl->right()->anchorTo(selx_guide);

    auto trkclr = (i & 1) ? fvec4(0.15, 0.15, 0.15, 1.0) : fvec4(0.25, 0.25, 0.25, 1.0);

    auto t                 = w_tracks._widget->makeChild<SignalTrackWidget>(sigtrack._signal, trkclr);
    auto tl                = t._layout;
    t._widget->_viewparams = vparams;
    tl->top()->anchorTo(gtop);
    tl->bottom()->anchorTo(gbot);
    tl->left()->anchorTo(selx_guide);
    tl->right()->anchorTo(l_tracks->right());
  }
  //////////////////////////////////////
  // root_layout->dump();
  // exit(0);
  //////////////////////////////////////
  app->setRefreshPolicy({EREFRESH_FIXEDFPS, 60});
  return app->exec();
}
