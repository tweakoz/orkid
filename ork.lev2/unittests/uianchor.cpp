////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/lev2/ui/anchor.h>
#include <ork/lev2/ui/widget.h>
#include <ork/lev2/ui/panel.h>
#include <ork/lev2/ui/viewport.h>
#include <ork/lev2/ui/layoutgroup.inl>
#include <utpp/UnitTest++.h>

using namespace ork::ui;

///////////////////////////////////////////////////////////////////////////////
TEST(uianchor1) {
  //////////////////////////////////////
  auto w1 = std::make_shared<Panel>("w1", 0, 0, 0, 0);
  auto w2 = std::make_shared<Panel>("w2", 0, 0, 0, 0);
  auto vp = std::make_shared<Viewport>("vp", 20, 20, 100, 100, ork::fvec3(), 1.0f);
  //////////////////////////////////////
  auto vplayout = std::make_shared<anchor::Layout>(vp.get());
  auto l1       = std::make_shared<anchor::Layout>(w1.get());
  auto l2       = std::make_shared<anchor::Layout>(w2.get());
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
}
