#include <ork/pch.h>
#include <ork/lev2/ui/anchor.h>
#include <ork/lev2/ui/widget.h>
#include <ork/lev2/ui/split_panel.h>
#include <ork/lev2/ui/viewport.h>
#include <ork/lev2/ui/layoutgroup.inl>
#include "harness.h"

using namespace ork::ui;

void TestViewport::DoDraw(ui::drawevent_constptr_t drwev) {
  drawChildren(drwev);
}
void TestViewport::onUpdateThreadTick(ui::updatedata_ptr_t updata) {
}

int main(int argc, char** argv) {
  //////////////////////////////////////
  auto vp                  = std::make_shared<LayoutGroup>("layoutgroup", 0, 0, 1280, 720);
  auto w0                  = vp->makeChild<EvTestBox>("w0", fvec4(1, 1, 0, 1));
  auto w1                  = vp->makeChild<SplitPanel>("w1");
  auto w2                  = vp->makeChild<LayoutGroup>("w2", 0, 0, 0, 0);
  auto w3                  = vp->makeChild<EvTestBox>("w3", fvec4(0, 1, 0, 1));
  w1._widget->_moveEnabled = false;
  //////////////////////////////////////
  auto panel_w0 = std::make_shared<EvTestBox>("panel-w0", fvec4(0, 1, 1, 1));
  auto panel_w1 = std::make_shared<LayoutGroup>("panel-w1");
  w1._widget->setChild1(panel_w0);
  w1._widget->setChild2(panel_w1);
  //////////////////////////////////////
  auto root_layout = vp->_layout;
  auto l0          = w0._layout;
  auto l1          = w1._layout;
  auto l2          = w2._layout;
  auto l3          = w3._layout;
  //////////////////////////////////////
  l0->setMargin(4);
  l1->setMargin(4);
  l2->setMargin(16);
  l3->setMargin(2);
  //////////////////////////////////////
  auto cg0 = root_layout->proportionalHorizontalGuide(0.25); // 0
  auto cg1 = root_layout->fixedHorizontalGuide(-32);         // 1
  //////////////////////////////////////
  l0->top()->anchorTo(root_layout->top());     // 2,3
  l0->left()->anchorTo(root_layout->left());   // 4,5
  l0->bottom()->anchorTo(cg0);                 // 6
  l0->right()->anchorTo(root_layout->right()); // 7,8
  //////////////////////////////////////
  l1->top()->anchorTo(cg0);                      // 9
  l1->left()->anchorTo(root_layout->left());     // 10
  l1->bottom()->anchorTo(cg1);                   // 11
  l1->right()->anchorTo(root_layout->centerH()); // 12,13
  //////////////////////////////////////
  l2->top()->anchorTo(cg0);                     // 14
  l2->left()->anchorTo(root_layout->centerH()); // 15
  l2->bottom()->anchorTo(cg1);                  // 16
  l2->right()->anchorTo(root_layout->right());  // 17
  //////////////////////////////////////
  l3->top()->anchorTo(cg1);                      // 18
  l3->left()->anchorTo(root_layout->left());     // 19
  l3->bottom()->anchorTo(root_layout->bottom()); // 20,21
  l3->right()->anchorTo(root_layout->right());   // 22
  //////////////////////////////////////
  auto makegrid = [](layoutgroup_ptr_t layout_widget, fvec4 color) {
    auto lname  = layout_widget->GetName();
    auto layout = layout_widget->_layout;
    for (int x = 0; x < 4; x++) {
      float fxa = float(x) / 4.0f;
      float fxb = float(x + 1) / 4.0f;
      auto gxa  = layout->proportionalVerticalGuide(fxa); // 23,27,31,35
      auto gxb  = layout->proportionalVerticalGuide(fxb); // 24,28,32,36
      for (int y = 0; y < 4; y++) {
        float fya   = float(y) / 4.0f;
        float fyb   = float(y + 1) / 4.0f;
        auto gya    = layout->proportionalHorizontalGuide(fya); // 25,29,33,37
        auto gyb    = layout->proportionalHorizontalGuide(fyb); // 26,30,34,38
        auto name   = lname + FormatString("-ch-%d", (y * 4 + x));
        auto chitem = layout_widget->makeChild<EvTestBox>(name, color);
        chitem._layout->setMargin(2);
        chitem._layout->top()->anchorTo(gya);
        chitem._layout->left()->anchorTo(gxa);
        chitem._layout->bottom()->anchorTo(gyb);
        chitem._layout->right()->anchorTo(gxb);
      }
    }
  };
  makegrid(w2._widget, fvec4(1, 1, 1, 1));
  makegrid(panel_w1, fvec4(0.25, 0, 0.4, 1));
  //////////////////////////////////////
  root_layout->dump();
  // exit(0);
  //////////////////////////////////////
  auto app   = createEZapp(argc, argv);
  app->_uivp = vp;
  return app->exec();
}
