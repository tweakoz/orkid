#include <ork/pch.h>
#include <ork/lev2/ui/anchor.h>
#include <ork/lev2/ui/box.h>
#include <ork/lev2/ui/viewport.h>
#include <ork/lev2/ui/layoutgroup.inl>
#include <ork/lev2/ui/context.h>
#include "harness.h"

using namespace ork::ui;

void TestViewport::DoDraw(ui::drawevent_constptr_t drwev) {
  drawChildren(drwev);
}
void TestViewport::onUpdateThreadTick(ui::updatedata_ptr_t updata) {
}

int main(int argc, char** argv, char** envp) {
  int margin = 3;
  auto initdata = std::make_shared<ork::AppInitData>(argc,argv,envp);
  auto app = createEZapp(initdata);
  //////////////////////////////////////
  auto vp                  = app->_topLayoutGroup;
  auto w0                  = vp->makeChild<EvTestBox>("w0", fvec4(1, 1, 0, 1));
  auto w1                  = vp->makeChild<LayoutGroup>("w1", 0, 0, 0, 0,margin);  // Changed to LayoutGroup
  auto w2                  = vp->makeChild<LayoutGroup>("w2", 0, 0, 0, 0,margin);
  auto w3                  = vp->makeChild<EvTestBox>("w3", fvec4(0, 1, 0, 1));
  //////////////////////////////////////
  // Create child widgets for w1 (which is now a LayoutGroup)
  auto panel_w0 = w1.typedWidget()->makeChild<EvTestBox>("panel-w0", fvec4(0, 1, 1, 1));
  auto panel_w1 = w1.typedWidget()->makeChild<LayoutGroup>("panel-w1", 0, 0, 0, 0,margin);
  //////////////////////////////////////
  auto root_layout = vp->_layout;
  auto l0          = w0._layout;
  auto l1          = w1._layout;
  auto l2          = w2._layout;
  auto l3          = w3._layout;

  // Layouts for the panels inside w1
  auto lp0         = panel_w0._layout;
  auto lp1         = panel_w1._layout;
  //root_layout->_locked = true;
  //////////////////////////////////////
  l0->setMargin(margin);
  l1->setMargin(margin);
  l2->setMargin(margin);
  l3->setMargin(margin);
  //////////////////////////////////////
  auto cg0 = root_layout->proportionalHorizontalGuide(0.25); // 0
  auto cg1 = root_layout->fixedHorizontalGuide(-32);         // 1
  cg1->_locked = true;
  auto cgH = root_layout->proportionalVerticalGuide(0.5);         // 1
  //////////////////////////////////////
  l0->top()->anchorTo(root_layout->top());     // 2,3
  l0->left()->anchorTo(root_layout->left());   // 4,5
  l0->bottom()->anchorTo(cg0);                 // 6
  l0->right()->anchorTo(root_layout->right()); // 7,8
  //////////////////////////////////////
  l1->top()->anchorTo(cg0);                      // 9
  l1->left()->anchorTo(root_layout->left());     // 10
  l1->bottom()->anchorTo(cg1);                   // 11
  l1->right()->anchorTo(cgH); // 12,13
  //////////////////////////////////////
  l2->top()->anchorTo(cg0);                     // 14
  l2->left()->anchorTo(cgH); // 15
  l2->bottom()->anchorTo(cg1);                  // 16
  l2->right()->anchorTo(root_layout->right());  // 17
  //////////////////////////////////////
  l3->top()->anchorTo(cg1);                      // 18
  l3->left()->anchorTo(root_layout->left());     // 19
  l3->bottom()->anchorTo(root_layout->bottom()); // 20,21
  l3->right()->anchorTo(root_layout->right());   // 22
  //////////////////////////////////////
  // Create a horizontal guide in w1 to split it (like SplitPanel would)
  auto splitH = l1->proportionalHorizontalGuide(0.5); // Split at 50%

  // Anchor panel_w0 to top half
  lp0->setMargin(2);
  lp0->top()->anchorTo(l1->top());
  lp0->left()->anchorTo(l1->left());
  lp0->bottom()->anchorTo(splitH);
  lp0->right()->anchorTo(l1->right());

  // Anchor panel_w1 to bottom half
  lp1->setMargin(2);
  lp1->top()->anchorTo(splitH);
  lp1->left()->anchorTo(l1->left());
  lp1->bottom()->anchorTo(l1->bottom());
  lp1->right()->anchorTo(l1->right());
  //////////////////////////////////////
  w2.typedWidget()->makeGridOfWidgets<EvTestBox>(4,4,"yo",fvec4(1, 1, 1, 1));
  panel_w1.typedWidget()->makeGridOfWidgets<EvTestBox>(8,8,"yo",fvec4(0.25, 0, 0.4, 1));
  //////////////////////////////////////
  root_layout->dump();
  // exit(0);
  app->_eztopwidget->enableUiDraw();
  //////////////////////////////////////
  app->setRefreshPolicy({EREFRESH_FIXEDFPS, 60});
  return app->mainThreadLoop();
}
