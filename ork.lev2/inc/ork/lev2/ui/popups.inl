#pragma once

#include <ork/lev2/ui/context.h>
#include <ork/lev2/ui/box.h>
#include <ork/lev2/ui/lineedit.h>
#include <ork/lev2/ui/layoutgroup.inl>
#include <ork/kernel/core_interface.h>
#include <ork/lev2/gfx/dbgfontman.h>
#include <ork/lev2/glfw/ctx_glfw.h>

namespace ork::ui {

    std::string popupLineEdit(lev2::Context* ctx,
                              int x, int y, int w, int h, //
                              const std::string& initial_value){

        lev2::PopupWindow popwin(ctx, x,y,w,h);
        auto uic = popwin._uicontext;
        auto root = uic->makeTop<ui::LayoutGroup>("lg",0,0,w,h);
        auto lineedit_item = root->makeChild<ui::LineEdit>("LineEdit",fvec4(1,1,1,1),0,0,0,0);
        auto lineedit_layout = lineedit_item._layout;
        auto lineedit = std::dynamic_pointer_cast<ui::LineEdit>(lineedit_item._widget);
        //auto lineedit_item = root->makeChild<ui::Box>("HI",fvec4(1,1,1,1),0,0,0,0);
        auto root_layout = root->_layout;
        lineedit_layout->top()->anchorTo(root_layout->top());
        lineedit_layout->left()->anchorTo(root_layout->left());
        lineedit_layout->right()->anchorTo(root_layout->right());
        lineedit_layout->bottom()->anchorTo(root_layout->bottom());
        root_layout->updateAll();
        lineedit->_value = initial_value;
        popwin.mainThreadLoop();        
        return lineedit->_value;
    }
}