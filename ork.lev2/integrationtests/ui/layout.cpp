#include <ork/pch.h>
#include <ork/lev2/ui/anchor.h>
#include <ork/lev2/ui/widget.h>
#include <ork/lev2/ui/panel.h>
#include <ork/lev2/ui/viewport.h>
#include "harness.h"

using namespace ork::ui;

void TestViewport::DoDraw(ui::drawevent_constptr_t drwev) {
  drawChildren(drwev);
}
void TestViewport::onUpdateThreadTick(ui::updatedata_ptr_t updata) {
}

int main(int argc, char** argv) {
  //////////////////////////////////////
  auto w0 = std::make_shared<EvTestBox>("w0", fvec4(1, 1, 0, 1));
  auto w1 = std::make_shared<EvTestBox>("w1", fvec4(1, 0, 0, 1));
  auto w2 = std::make_shared<LayoutGroup>("w2", 0, 0, 0, 0);
  auto w3 = std::make_shared<EvTestBox>("w3", fvec4(0, 1, 0, 1));
  auto vp = std::make_shared<LayoutGroup>("layoutgroup", 0, 0, 1280, 720);
  vp->addChild(w0);
  vp->addChild(w1);
  vp->addChild(w2);
  vp->addChild(w3);
  //////////////////////////////////////
  auto root_layout = std::make_shared<anchor::Layout>(vp);
  auto l0          = root_layout->childLayout(w0);
  auto l1          = root_layout->childLayout(w1);
  auto l2          = root_layout->childLayout(w2);
  auto l3          = root_layout->childLayout(w3);
  vp->_layout      = root_layout;
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
  if (1)
    for (int x = 0; x < 4; x++) {
      float fxa = float(x) / 4.0f;
      float fxb = float(x + 1) / 4.0f;
      auto gxa  = l2->proportionalVerticalGuide(fxa); // 23,27,31,35
      auto gxb  = l2->proportionalVerticalGuide(fxb); // 24,28,32,36
      for (int y = 0; y < 4; y++) {
        float fya = float(y) / 4.0f;
        float fyb = float(y + 1) / 4.0f;
        auto gya  = l2->proportionalHorizontalGuide(fya); // 25,29,33,37
        auto gyb  = l2->proportionalHorizontalGuide(fyb); // 26,30,34,38
        auto name = FormatString("ch-%d", (y * 4 + x));
        auto ch   = std::make_shared<EvTestBox>(name, fvec4(1, 1, 1, 1));
        auto lch  = l2->childLayout(ch);
        lch->setMargin(2);
        w2->addChild(ch);
        lch->top()->anchorTo(gya);
        lch->left()->anchorTo(gxa);
        lch->bottom()->anchorTo(gyb);
        lch->right()->anchorTo(gxb);
      }
    }
  //////////////////////////////////////
  root_layout->dump();
  // exit(0);
  //////////////////////////////////////
  auto app   = createEZapp(argc, argv);
  app->_uivp = vp;
  return app->exec();
}
