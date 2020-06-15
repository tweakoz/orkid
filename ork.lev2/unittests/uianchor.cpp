#include <ork/pch.h>
#include <ork/lev2/ui/anchor.h>
#include <ork/lev2/ui/widget.h>
#include <ork/lev2/ui/panel.h>
#include <ork/lev2/ui/viewport.h>
#include <utpp/UnitTest++.h>

using namespace ork::ui;

///////////////////////////////////////////////////////////////////////////////
TEST(uianchor1) {

  auto w1 = std::make_shared<Panel>("w1", 0, 0, 0, 0);
  // auto w2 = std::make_shared<Panel>("w2", 0, 0, 0, 0);
  auto vp = std::make_shared<Viewport>("vp", 20, 20, 100, 100, ork::fvec3(), 1.0f);

  auto vplayout = std::make_shared<anchor::Layout>(vp);
  auto l1       = std::make_shared<anchor::Layout>(w1);

  l1->top()->anchorTo(vplayout->top());
  l1->top()->setMargin(10);
  l1->left()->anchorTo(vplayout->left());
  l1->left()->setMargin(10);
  l1->bottom()->anchorTo(vplayout->bottom());
  l1->bottom()->setMargin(10);
  l1->right()->anchorTo(vplayout->right());
  l1->right()->setMargin(10);
}
