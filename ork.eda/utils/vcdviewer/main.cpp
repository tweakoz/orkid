////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include "vcdviewer.h"
#include <ork/lev2/ui/simpleuiapp.h>
///////////////////////////////////////////////////////////////////////////////
viewparams_ptr_t ViewParams::instance() {
  static viewparams_ptr_t __vparams = std::make_shared<ViewParams>();
  return __vparams;
}
///////////////////////////////////////////////////////////////////////////////
int main(int argc, char** argv, char** envp) {
  auto init_data = std::make_shared<ork::AppInitData>(argc,argv,envp);
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
  auto app = createSimpleUiApp(init_data);
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
  auto viewparams            = ViewParams::instance();
  viewparams->_min_timestamp = *vcdfile._timestamps.begin();
  viewparams->_max_timestamp = *vcdfile._timestamps.rbegin();
  //printf("min_timestamp<%lu>\n", viewparams->_min_timestamp);
  //printf("max_timestamp<%lu>\n", viewparams->_max_timestamp);
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

    auto w  = w_tracks.typedWidget()->makeChild<Label>(sigtrack._name, labclr, label);
    auto wl = w._layout;
    wl->top()->anchorTo(gtop);
    wl->bottom()->anchorTo(gbot);
    wl->left()->anchorTo(l_tracks->left());
    wl->right()->anchorTo(selx_guide);

    auto trkclr = (i & 1) ? fvec4(0.1, 0.0, 0.1, 1.0) : fvec4(0.0, 0.0, 0.1, 1.0);

    auto t  = w_tracks.typedWidget()->makeChild<SignalTrackWidget>(sigtrack._signal, trkclr);
    auto tl = t._layout;
    tl->top()->anchorTo(gtop);
    tl->bottom()->anchorTo(gbot);
    tl->left()->anchorTo(selx_guide);
    tl->right()->anchorTo(l_tracks->right());

    viewparams->_sigtracks.push_back(t.typedWidget());
  }
  //////////////////////////////////////
  auto overlay = Overlay::instance();
  auto ol      = w_tracks.typedWidget()->layoutAndAddChild(overlay);
  ol->top()->anchorTo(l_tracks->top());
  ol->bottom()->anchorTo(l_tracks->bottom());
  ol->left()->anchorTo(l_tracks->left());
  ol->right()->anchorTo(l_tracks->right());
  // ol->setMargin(4);
  //////////////////////////////////////
  app->setRefreshPolicy({EREFRESH_WHENDIRTY, -1});
  return app->mainThreadLoop();
}
