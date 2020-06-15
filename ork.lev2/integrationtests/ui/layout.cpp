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
  auto w1 = std::make_shared<Panel>("w1", 0, 0, 0, 0);
  auto w2 = std::make_shared<Panel>("w2", 0, 0, 0, 0);
  auto vp = std::make_shared<TestViewport>();
  vp->addChild(w1);
  vp->addChild(w2);
  //////////////////////////////////////
  auto vplayout = std::make_shared<anchor::Layout>(vp);
  auto l1       = std::make_shared<anchor::Layout>(w1);
  auto l2       = std::make_shared<anchor::Layout>(w2);
  //////////////////////////////////////
  l1->top()->setMargin(4);
  l1->left()->setMargin(4);
  l1->bottom()->setMargin(4);
  l1->right()->setMargin(4);
  //////////////////////////////////////
  l2->top()->setMargin(4);
  l2->left()->setMargin(4);
  l2->bottom()->setMargin(4);
  l2->right()->setMargin(4);
  //////////////////////////////////////
  l1->top()->anchorTo(vplayout->top());
  l1->left()->anchorTo(vplayout->left());
  l1->bottom()->anchorTo(vplayout->bottom());
  l1->right()->anchorTo(vplayout->centerH());
  //////////////////////////////////////
  l2->top()->anchorTo(vplayout->top());
  l2->left()->anchorTo(vplayout->centerH());
  l2->bottom()->anchorTo(vplayout->bottom());
  l2->right()->anchorTo(vplayout->right());
  //////////////////////////////////////
  vplayout->dump();
  //////////////////////////////////////
  vplayout->updateAll();
  //////////////////////////////////////
  auto vpg = vp->geometry();
  auto w1g = w1->geometry();
  auto w2g = w2->geometry();
  printf("vp x<%d> y<%d> w<%d> h<%d>\n", vpg._x, vpg._y, vpg._w, vpg._h);
  printf("w1 x<%d> y<%d> w<%d> h<%d>\n", w1g._x, w1g._y, w1g._w, w1g._h);
  printf("w2 x<%d> y<%d> w<%d> h<%d>\n", w2g._x, w2g._y, w2g._w, w2g._h);
  //////////////////////////////////////
  auto app   = createEZapp(argc, argv);
  app->_uivp = vp;
  return app->exec();
}
