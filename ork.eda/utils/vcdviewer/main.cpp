#include <ork/pch.h>
#include <ork/lev2/ui/anchor.h>
#include <ork/lev2/ui/box.h>
#include <ork/lev2/ui/split_panel.h>
#include <ork/lev2/ui/viewport.h>
#include <ork/lev2/ui/layoutgroup.inl>
#include <ork/lev2/ui/context.h>
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
  auto w_header = vp->makeChild<EvTestBox>("w_header", fvec4(1, 1, 0, 1));
  auto w_status = vp->makeChild<EvTestBox>("status", fvec4(0, 1, 0, 1));
  //////////////////////////////////////
  auto root_layout = vp->_layout;
  auto l_header    = w_header._layout;
  auto l_status    = w_status._layout;
  //////////////////////////////////////
  l_header->setMargin(4);
  l_status->setMargin(2);
  //////////////////////////////////////
  // recursive descent into scoped-signals
  //////////////////////////////////////
  std::stack<scope_ptr_t> scope_stack;
  std::stack<std::string> scopename_stack;
  scope_stack.push(vcdfile._root);
  scopename_stack.push("");
  while (not scope_stack.empty()) {
    auto front   = scope_stack.top();
    auto topname = scopename_stack.top();

    printf("visiting scope<%s>\n", topname.c_str());

    scope_stack.pop();
    scopename_stack.pop();
    for (auto ch : front->_child_scopes) {
      scope_stack.push(ch.second);
      auto chname = topname + "/" + ch.second->_name;
      scopename_stack.push(chname);
    }
  }
  //////////////////////////////////////
  auto cg1 = root_layout->fixedHorizontalGuide(-32); // 1
  //////////////////////////////////////
  l_header->top()->anchorTo(root_layout->top());     // 2,3
  l_header->left()->anchorTo(root_layout->left());   // 4,5
  l_header->bottom()->anchorTo(cg1);                 // 6
  l_header->right()->anchorTo(root_layout->right()); // 7,8
  //////////////////////////////////////
  l_status->top()->anchorTo(cg1);                      // 18
  l_status->left()->anchorTo(root_layout->left());     // 19
  l_status->bottom()->anchorTo(root_layout->bottom()); // 20,21
  l_status->right()->anchorTo(root_layout->right());   // 22
  //////////////////////////////////////
  // root_layout->dump();
  // exit(0);
  //////////////////////////////////////
  app->setRefreshPolicy({EREFRESH_FIXEDFPS, 60});
  return app->exec();
}
