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
  auto w_top               = vp->makeChild<EvTestBox>("w0", fvec4(1, 1, 0, 1));
  auto w_mid_left          = vp->makeChild<LayoutGroup>("w1", 0, 0, 0, 0,margin); 
  auto w_4x4               = vp->makeChild<LayoutGroup>("w2", 0, 0, 0, 0,margin);
  auto w_bot               = vp->makeChild<EvTestBox>("w3", fvec4(0, 1, 0, 1));
  //////////////////////////////////////
  auto root_layout = vp->_layout;
  auto l_top          = w_top._layout;
  auto l_mid_left     = w_mid_left._layout;
  auto l_4x4          = w_4x4._layout;
  auto l_bot          = w_bot._layout;

  //root_layout->_locked = true;
  //////////////////////////////////////
  l_top->setMargin(margin);
  l_mid_left->setMargin(margin);
  l_4x4->setMargin(margin);
  l_bot->setMargin(margin);
  //////////////////////////////////////
  auto cg_topmid = root_layout->proportionalHorizontalGuide(0.25); 
  auto cg_midbot = root_layout->fixedHorizontalGuide(-32);         
  cg_midbot->_locked = true;
  auto cg_vsplit = root_layout->proportionalVerticalGuide(0.5);         
  //////////////////////////////////////
  l_top->top()->anchorTo(root_layout->top());     
  l_top->left()->anchorTo(root_layout->left());   
  l_top->bottom()->anchorTo(cg_topmid);           
  l_top->right()->anchorTo(root_layout->right()); 
  //////////////////////////////////////
  l_mid_left->top()->anchorTo(cg_topmid);                      
  l_mid_left->left()->anchorTo(root_layout->left());     
  l_mid_left->bottom()->anchorTo(cg_midbot);                   
  l_mid_left->right()->anchorTo(cg_vsplit);
  //////////////////////////////////////
  l_4x4->top()->anchorTo(cg_topmid);                     
  l_4x4->left()->anchorTo(cg_vsplit); 
  l_4x4->bottom()->anchorTo(cg_midbot);                  
  l_4x4->right()->anchorTo(root_layout->right());  
  //////////////////////////////////////
  l_bot->top()->anchorTo(cg_midbot);                     
  l_bot->left()->anchorTo(root_layout->left());     
  l_bot->bottom()->anchorTo(root_layout->bottom()); 
  l_bot->right()->anchorTo(root_layout->right());   
  //////////////////////////////////////
  // Create child widgets for w_mid_left (which is now a LayoutGroup)
  //////////////////////////////////////
  auto w_lbox = w_mid_left.typedWidget()->makeChild<EvTestBox>("panel-w0", fvec4(0, 1, 1, 1));
  auto w_8x8 = w_mid_left.typedWidget()->makeChild<LayoutGroup>("panel-w1", 0, 0, 0, 0,margin);
  //////////////////////////////////////
  // Create a horizontal guide in w1 to split it (like SplitPanel would)
  auto splitH = l_mid_left->proportionalHorizontalGuide(0.5); // Split at 50%

  // Layouts for the panels inside w1
  auto l_lbox         = w_lbox._layout;
  auto l_8x8         = w_8x8._layout;

  l_lbox->setMargin(margin);
  l_lbox->bottom()->anchorTo(splitH);
  l_lbox->top()->anchorTo(l_mid_left->top());
  l_lbox->left()->anchorTo(l_mid_left->left());
  l_lbox->right()->anchorTo(l_mid_left->right());

  // Anchor panel_w1 to bottom half
  l_8x8->setMargin(margin);
  l_8x8->top()->anchorTo(splitH);
  l_8x8->left()->anchorTo(l_mid_left->left());
  l_8x8->bottom()->anchorTo(l_mid_left->bottom());
  l_8x8->right()->anchorTo(l_mid_left->right());
  //////////////////////////////////////
  w_4x4.typedWidget()->makeGridOfWidgets<EvTestBox>(4,4,"yo",fvec4(1, 1, 1, 1));
  w_8x8.typedWidget()->makeGridOfWidgets<EvTestBox>(8,8,"yo",fvec4(0.25, 0, 0.4, 1));
  //////////////////////////////////////
  root_layout->dump();
  // exit(0);
  app->_eztopwidget->enableUiDraw();
  //////////////////////////////////////
  app->setRefreshPolicy({EREFRESH_FIXEDFPS, 60});
  return app->mainThreadLoop();
}
