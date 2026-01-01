////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "pyext.h"
#include <ork/lev2/ui/widget.h>
#include <ork/lev2/ui/group.h>
#include <ork/lev2/ui/layoutgroup.inl>
#include <ork/lev2/ui/outliner.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2 {
void pyinit_ui_outliner(py::module& uimodule) {
  auto type_codec = python::pb11_typecodec_t::instance();

  /////////////////////////////////////////////////////////////////////////////////
  auto outliner_type = //
      py::class_<ui::Outliner, ui::Widget, ui::outliner_ptr_t>(uimodule, "Outliner")
          .def_static(
              "wfactory",
              [type_codec](py::list py_args) -> ui::outliner_ptr_t { //
                auto decoded_args = type_codec->decodeList(py_args);
                auto name         = decoded_args[0].get<std::string>();
                auto outliner     = std::make_shared<ui::Outliner>(name);
                return outliner;
              })
          .def_static(
              "uifactory",
              [type_codec](uilayoutgroup_ptr_t lg, py::list py_args) -> uilayoutitem_ptr_t { //
                auto decoded_args = type_codec->decodeList(py_args);
                auto name         = decoded_args[0].get<std::string>();
                auto layoutitem   = lg->makeChild<ui::Outliner>(name);
                return layoutitem.as_shared();
              })
          .def_property(
              "data",
              [](ui::outliner_ptr_t outliner) -> varmap::varmap_ptr_t { //
                return outliner->getData();
              },
              [](ui::outliner_ptr_t outliner, varmap::varmap_ptr_t data) { //
                outliner->setData(data);
              })
          .def_property(
              "selected_key",
              [](ui::outliner_ptr_t outliner) -> std::string { //
                return outliner->getSelectedKey();
              },
              [](ui::outliner_ptr_t outliner, const std::string& key) { //
                outliner->setSelectedKey(key);
              })
          .def(
              "setExpanded",
              [](ui::outliner_ptr_t outliner, const std::string& key, bool expanded) { //
                outliner->setExpanded(key, expanded);
              })
          .def(
              "isExpanded",
              [](ui::outliner_ptr_t outliner, const std::string& key) -> bool { //
                return outliner->isExpanded(key);
              })
          .def(
              "expandAll",
              [](ui::outliner_ptr_t outliner) { //
                outliner->expandAll();
              })
          .def(
              "collapseAll",
              [](ui::outliner_ptr_t outliner) { //
                outliner->collapseAll();
              })
          .def(
              "onSelect",
              [](ui::outliner_ptr_t outliner, py::object callback) { //
                outliner->_onSelect = [callback](const std::string& key) {
                  py::gil_scoped_acquire acquire;
                  callback(key);
                };
              })
          .def_property(
              "item_height",
              [](ui::outliner_ptr_t outliner) -> int { //
                return outliner->_item_height;
              },
              [](ui::outliner_ptr_t outliner, int h) { //
                outliner->_item_height = h;
              })
          .def_property(
              "indent_width",
              [](ui::outliner_ptr_t outliner) -> int { //
                return outliner->_indent_width;
              },
              [](ui::outliner_ptr_t outliner, int w) { //
                outliner->_indent_width = w;
              })
          .def_property(
              "bgcolor",
              [](ui::outliner_ptr_t outliner) -> fvec4 { //
                return outliner->_bgcolor;
              },
              [](ui::outliner_ptr_t outliner, fvec4 c) { //
                outliner->_bgcolor = c;
              })
          .def_property(
              "text_color",
              [](ui::outliner_ptr_t outliner) -> fvec4 { //
                return outliner->_text_color;
              },
              [](ui::outliner_ptr_t outliner, fvec4 c) { //
                outliner->_text_color = c;
              })
          .def_property(
              "selected_color",
              [](ui::outliner_ptr_t outliner) -> fvec4 { //
                return outliner->_selected_color;
              },
              [](ui::outliner_ptr_t outliner, fvec4 c) { //
                outliner->_selected_color = c;
              })
          .def_property(
              "hover_color",
              [](ui::outliner_ptr_t outliner) -> fvec4 { //
                return outliner->_hover_color;
              },
              [](ui::outliner_ptr_t outliner, fvec4 c) { //
                outliner->_hover_color = c;
              })
          .def_property(
              "font",
              [](ui::outliner_ptr_t outliner) -> font_ptr_t { //
                return outliner->_font;
              },
              [](ui::outliner_ptr_t outliner, font_ptr_t font) { //
                outliner->_font = font;
              })
          .def_readwrite("draw_background", &ui::Outliner::_draw_background)
          .def("__repr__", [](ui::outliner_ptr_t outliner) {
            return FormatString("<Outliner name<%s> widget<%p>>", outliner->GetName().c_str(), (void*)outliner.get());
          });

  type_codec->registerStdCodec<ui::outliner_ptr_t>(outliner_type);
}
///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
