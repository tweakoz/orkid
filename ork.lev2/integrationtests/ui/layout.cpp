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
  auto uic = app->_eztopwidget->_uicontext;
  //////////////////////////////////////
  auto vp                  = app->_topLayoutGroup;
  auto root_layout         = vp->_layout;
  root_layout->setMargin(margin);
  vp->_margin = margin;
  //////////////////////////////////////
  auto i_top               = vp->makeChild<EvTestBox>("top", fvec4(0.5, 0.3, 0.2, 1));
  auto i_mid               = vp->makeChild<LayoutGroup>("mid", 0, 0, 0, 0, margin); 
  auto i_bot               = vp->makeChild<EvTestBox>("bot", fvec4(0, 1, 0, 1));
  auto w_top               = i_top.typedWidget();
  auto w_mid               = i_mid.typedWidget();
  auto w_bot               = i_bot.typedWidget();
  //////////////////////////////////////
  auto i_mid_left          = w_mid->makeChild<LayoutGroup>("mid-left", 0, 0, 0, 0, margin);
  auto i_4x4               = w_mid->makeChild<LayoutGroup>("4x4", 0, 0, 0, 0, margin);
  auto w_mid_left          = i_mid_left.typedWidget();
  auto w_4x4               = i_4x4.typedWidget();
  //vp->removeChild(w_mid._layout);
  //vp->removeChild(w_4x4._layout);
  //////////////////////////////////////
  auto i_lbox              = w_mid_left->makeChild<EvTestBox>("lbox", fvec4(0, 1, 1, 1));
  auto i_8x8               = w_mid_left->makeChild<LayoutGroup>("8x8", 0, 0, 0, 0, margin);
  auto w_lbox              = i_lbox.typedWidget();
  auto w_8x8               = i_8x8.typedWidget();
  //////////////////////////////////////
  w_4x4->makeGridOfWidgets<EvTestBox>(4,4,"yo",fvec4(0, 0, .3, 1));
  w_8x8->makeGridOfWidgets<EvTestBox>(8,8,"yo",fvec4(0.25, 0, 0.4, 1));
  //////////////////////////////////////
  auto color = fvec4(0.4,0.4,0.5,1);
  vp->_clearColorGuide = color;
  w_mid->_clearColorGuide = color;
  w_mid_left->_clearColorGuide = color;
  w_4x4->_clearColorGuide = color;
  //w_lbox->_clearColorGuide = fvec4(.5,.2,1, 1);
  w_8x8->_clearColorGuide = color;
  //////////////////////////////////////
  auto l_top          = i_top._layout;
  auto l_mid          = i_mid._layout;
  auto l_bot          = i_bot._layout;
  //////////////////////////////////////
  auto l_mid_left     = i_mid_left._layout;
  auto l_4x4          = i_4x4._layout;
  //////////////////////////////////////
  auto l_lbox         = i_lbox._layout;
  auto l_8x8          = i_8x8._layout;
  //////////////////////////////////////
  // Set margin at root - it will propagate to all children
  //////////////////////////////////////
  auto cg_topmid = root_layout->proportionalHorizontalGuide(0.25);
  auto cg_midbot = root_layout->fixedHorizontalGuide(-32);
  cg_midbot->_locked = true;
  //////////////////////////////////////
  l_top->top()->anchorTo(root_layout->top());     
  l_top->left()->anchorTo(root_layout->left());   
  l_top->bottom()->anchorTo(cg_topmid);           
  l_top->right()->anchorTo(root_layout->right()); 
  //////////////////////////////////////
  l_mid->top()->anchorTo(cg_topmid);                     
  l_mid->left()->anchorTo(root_layout->left());     
  l_mid->bottom()->anchorTo(cg_midbot); 
  l_mid->right()->anchorTo(root_layout->right());   
  //////////////////////////////////////
  l_bot->top()->anchorTo(cg_midbot);                     
  l_bot->left()->anchorTo(root_layout->left());     
  l_bot->bottom()->anchorTo(root_layout->bottom()); 
  l_bot->right()->anchorTo(root_layout->right());   
  //////////////////////////////////////
  auto cg_vsplit = l_mid->proportionalVerticalGuide(0.5);
  cg_vsplit->_margin = margin;
  // Margin inherited from l_mid
  l_mid_left->top()->anchorTo(l_mid->top());
  l_mid_left->left()->anchorTo(l_mid->left());
  l_mid_left->bottom()->anchorTo(l_mid->bottom());
  l_mid_left->right()->anchorTo(cg_vsplit);
  //////////////////////////////////////
  l_4x4->top()->anchorTo(l_mid->top());
  l_4x4->left()->anchorTo(cg_vsplit);
  l_4x4->bottom()->anchorTo(l_mid->bottom());
  l_4x4->right()->anchorTo(l_mid->right());  
  //////////////////////////////////////
  // Create child widgets for w_mid_left (which is now a LayoutGroup)
  //////////////////////////////////////
  //////////////////////////////////////
  // Create a horizontal guide in w1 to split it (like SplitPanel would)
  auto splitH = l_mid_left->proportionalHorizontalGuide(0.5); // Split at 50%
  // Layouts for the panels inside w1
  l_lbox->bottom()->anchorTo(splitH);
  l_lbox->top()->anchorTo(l_mid_left->top());
  l_lbox->left()->anchorTo(l_mid_left->left());
  l_lbox->right()->anchorTo(l_mid_left->right());
  // Anchor panel_w1 to bottom half
  l_8x8->top()->anchorTo(splitH);
  l_8x8->left()->anchorTo(l_mid_left->left());
  l_8x8->bottom()->anchorTo(l_mid_left->bottom());
  l_8x8->right()->anchorTo(l_mid_left->right());
  //////////////////////////////////////
  //root_layout->dump();
  // exit(0);
  app->_eztopwidget->enableUiDraw();
  //////////////////////////////////////
  app->setRefreshPolicy({EREFRESH_FIXEDFPS, 60});
  return app->mainThreadLoop();
}
