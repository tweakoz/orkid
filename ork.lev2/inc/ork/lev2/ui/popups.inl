#pragma once

#include <ork/lev2/ui/context.h>
#include <ork/lev2/ui/box.h>
#include <ork/lev2/ui/lineedit.h>
#include <ork/lev2/ui/coloredit.h>
#include <ork/lev2/ui/choicelist.h>
#include <ork/lev2/ui/layoutgroup.inl>
#include <ork/kernel/core_interface.h>
#include <ork/lev2/gfx/dbgfontman.h>
#include <ork/lev2/glfw/ctx_glfw.h>

extern "C" {
char* tinyfd_openFileDialog(
    char const* aTitle,                   /* NULL or "" */
    char const* aDefaultPathAndFile,      /* NULL or "" */
    int aNumOfFilterPatterns,             /* 0 (2 in the following example) */
    char const* const* aFilterPatterns,   /* NULL or char const * lFilterPatterns[2]={"*.png","*.jpg"}; */
    char const* aSingleFilterDescription, /* NULL or "image files" */
    int aAllowMultipleSelects);           /* 0 or 1 */
/* in case of multiple files, the separator is | */
/* returns NULL on cancel */

char* tinyfd_saveFileDialog(
    char const* aTitle,                    /* NULL or "" */
    char const* aDefaultPathAndFile,       /* NULL or "" */
    int aNumOfFilterPatterns,              /* 0  (1 in the following example) */
    char const* const* aFilterPatterns,    /* NULL or char const * lFilterPatterns[1]={"*.txt"} */
    char const* aSingleFilterDescription); /* NULL or "text files" */
/* returns NULL on cancel */

char* tinyfd_selectFolderDialog(
    char const* aTitle,        /* NULL or "" */
    char const* aDefaultPath); /* NULL or "" */
/* returns NULL on cancel */
}

namespace ork::ui {

///////////////////////////////////////////////////////////////////////////////

inline std::string popupLineEdit(       //
    lev2::Context* ctx,                 //
    int x,                              //
    int y,                              //
    int w,                              //
    int h,                              //
    const std::string& initial_value) { //

  lev2::PopupWindow popwin(ctx, x, y, w, h);
  auto uic             = popwin._uicontext;
  auto root            = uic->makeTop<ui::LayoutGroup>("lg", 0, 0, w, h);
  auto lineedit_item   = root->makeChild<ui::LineEdit>("LineEdit", fvec4(1, 1, 0, 1), 0, 0, 0, 0);
  auto lineedit_layout = lineedit_item._layout;
  auto lineedit        = std::dynamic_pointer_cast<ui::LineEdit>(lineedit_item._widget);
  auto root_layout     = root->_layout;
  lineedit_layout->top()->anchorTo(root_layout->top());
  lineedit_layout->left()->anchorTo(root_layout->left());
  lineedit_layout->right()->anchorTo(root_layout->right());
  lineedit_layout->bottom()->anchorTo(root_layout->bottom());
  root_layout->updateAll();
  lineedit->setValue(initial_value);
  popwin.mainThreadLoop();
  return lineedit->_value;
}

///////////////////////////////////////////////////////////////////////////////

inline std::string popupChoiceList(            //
    lev2::Context* ctx,                        //
    int x,                                     //
    int y,                                     //
    const std::vector<std::string>& choices,
    fvec2 dimensions) { //

  int w            = int(dimensions.x);
  int h            = int(dimensions.y);
  lev2::PopupWindow popwin(ctx, x, y, w, h);
  auto uic               = popwin._uicontext;
  auto root              = uic->makeTop<ui::LayoutGroup>("lg", 0, 0, w, h);
  auto choicelist_item   = root->makeChild<ui::ChoiceList>("ChoiceList", fvec4(1, 1, 0, 1), 0, 0, 0, 0, dimensions);
  auto choicelist_layout = choicelist_item._layout;
  auto choicelist        = std::dynamic_pointer_cast<ui::ChoiceList>(choicelist_item._widget);
  choicelist->_choices   = choices;
  auto root_layout       = root->_layout;
  choicelist_layout->top()->anchorTo(root_layout->top());
  choicelist_layout->left()->anchorTo(root_layout->left());
  choicelist_layout->right()->anchorTo(root_layout->right());
  choicelist_layout->bottom()->anchorTo(root_layout->bottom());
  root_layout->updateAll();
  popwin.mainThreadLoop();
  root->onPreDestroy();
  return choicelist->_value;
}

///////////////////////////////////////////////////////////////////////////////

inline fvec4 popupColorEdit(      //
    lev2::Context* ctx,           //
    int x,                        //
    int y,                        //
    int w,                        //
    int h,                        //
    const fvec4& initial_value) { //

  lev2::PopupWindow popwin(ctx, x, y, w, h, true); // transparent
  auto uic               = popwin._uicontext;
  uic->_id               = "popupColorEdit";
  auto root              = uic->makeTop<ui::LayoutGroup>("lg", 0, 0, w, h);
  auto coloredit_item    = root->makeChild<ui::ColorEdit>("ColorEdit", initial_value, 0, 0, 0, 0);
  auto coloredit_layout  = coloredit_item._layout;
  auto coloredit         = std::dynamic_pointer_cast<ui::ColorEdit>(coloredit_item._widget);
  auto root_layout       = root->_layout;
  uic->_mousefocuswidget = coloredit.get();
  coloredit_layout->top()->anchorTo(root_layout->top());
  coloredit_layout->left()->anchorTo(root_layout->left());
  coloredit_layout->right()->anchorTo(root_layout->right());
  coloredit_layout->bottom()->anchorTo(root_layout->bottom());
  root_layout->updateAll();
  popwin.mainThreadLoop();
  return coloredit->_currentColor;
}

///////////////////////////////////////////////////////////////////////////////

inline std::string popupOpenDialog(           //
    std::string title,                        //
    std::string default_path_and_file,        //
    std::vector<std::string> filter_patterns, //
    bool allow_multiple_selects) {            //

  int num_patterns          = filter_patterns.size();
  auto filter_patterns_cstr = new const char*[num_patterns];
  for (int i = 0; i < num_patterns; i++)
    filter_patterns_cstr[i] = filter_patterns[i].c_str();

  char const* filter_description = nullptr;
  if (num_patterns)
    filter_description = filter_patterns[0].c_str();

  char const* default_path_and_file_cstr = nullptr;
  if (default_path_and_file.length())
    default_path_and_file_cstr = default_path_and_file.c_str();

  char const* title_cstr = nullptr;
  if (title.length())
    title_cstr = title.c_str();

  char* result = tinyfd_openFileDialog(
      title_cstr, default_path_and_file_cstr, num_patterns, filter_patterns_cstr, filter_description, allow_multiple_selects);

  delete[] filter_patterns_cstr;

  std::string rval;
  if (result)
    rval = result;

  return rval;
}

///////////////////////////////////////////////////////////////////////////////

inline std::string popupSaveDialog(             //
    std::string title,                          //
    std::string default_path_and_file,          //
    std::vector<std::string> filter_patterns) { //

  int num_patterns          = filter_patterns.size();
  auto filter_patterns_cstr = new const char*[num_patterns];
  for (int i = 0; i < num_patterns; i++)
    filter_patterns_cstr[i] = filter_patterns[i].c_str();

  char const* filter_description = nullptr;
  if (num_patterns)
    filter_description = filter_patterns[0].c_str();

  char const* default_path_and_file_cstr = nullptr;
  if (default_path_and_file.length())
    default_path_and_file_cstr = default_path_and_file.c_str();

  char const* title_cstr = nullptr;
  if (title.length())
    title_cstr = title.c_str();

  char* result =
      tinyfd_saveFileDialog(title_cstr, default_path_and_file_cstr, num_patterns, filter_patterns_cstr, filter_description);

  delete[] filter_patterns_cstr;

  std::string rval;
  if (result)
    rval = result;

  return rval;
}

///////////////////////////////////////////////////////////////////////////////

inline std::string popupFolderDialog( //
    std::string title,                      //
    std::string default_path) {             //

  char const* default_path_cstr = nullptr;
  if (default_path.length())
    default_path_cstr = default_path.c_str();

  char const* title_cstr = nullptr;
  if (title.length())
    title_cstr = title.c_str();

  char* result = tinyfd_selectFolderDialog(title_cstr, default_path_cstr);

  std::string rval;
  if (result)
    rval = result;

  return rval;
}

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::ui