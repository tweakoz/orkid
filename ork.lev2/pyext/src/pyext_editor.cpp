////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "pyext.h"
#include <ork/lev2/input/inputdevice.h>
#include <ork/lev2/editor/editor.h>
#include <ork/lev2/editor/selection.h>
#include <ork/lev2/editor/manip.h>

///////////////////////////////////////////////////////////////////////////////

namespace ork::lev2 {

void pyinit_editor(py::module& module_lev2) {
  using namespace editor;
  auto type_codec                  = python::pb11_typecodec_t::instance();
  /////////////////////////////////////////////////////////////////////////////////
  auto ed_type_t = py::class_<Editor, editor_ptr_t>(module_lev2, "Editor") //
                         .def(py::init([] -> editor_ptr_t {
                           return std::make_shared<Editor>();
                         }))
                         .def_property_readonly(
                             "selectionManager",                //
                             [](editor_ptr_t editor) -> selmgr_ptr_t { //
                               return editor->_selection_manager;
                             })
                         .def_property_readonly(
                             "currentManipInterface",                //
                             [](editor_ptr_t editor) -> manipinterface_ptr_t { //
                               return editor->_current_manipulator_interface;
                             });
  type_codec->registerStdCodec<editor_ptr_t>(ed_type_t);
  /////////////////////////////////////////////////////////////////////////////////
  auto sm_type_t = py::class_<SelectionManager, selmgr_ptr_t>(module_lev2, "SelectionManager")
    .def_property_readonly(
        "currentObject",                //
        [](selmgr_ptr_t selmgr) -> object_ptr_t { //
        return selmgr->_selectedObject;
        });
  type_codec->registerStdCodec<selmgr_ptr_t>(sm_type_t);
  /////////////////////////////////////////////////////////////////////////////////
  auto mif_type_t = py::class_<ManipulatorInterface, manipinterface_ptr_t>(module_lev2, "ManipulatorInterface");
  type_codec->registerStdCodec<manipinterface_ptr_t>(mif_type_t);
  /////////////////////////////////////////////////////////////////////////////////
}

} //namespace ork::lev2::editor {
