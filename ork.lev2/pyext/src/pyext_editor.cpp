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
#include <ork/lev2/editor/manip_controller.h>

///////////////////////////////////////////////////////////////////////////////

namespace ork::lev2 {

///////////////////////////////////////////////////////////////////////////////
// Python trampoline for ManipulatorInterface
///////////////////////////////////////////////////////////////////////////////

struct PyManipulatorInterface : public editor::ManipulatorInterface {
  using editor::ManipulatorInterface::ManipulatorInterface;

  bool supportsTranslation() const override {
    PYBIND11_OVERRIDE(bool, editor::ManipulatorInterface, supportsTranslation);
  }
  bool supportsRotation() const override {
    PYBIND11_OVERRIDE(bool, editor::ManipulatorInterface, supportsRotation);
  }
  bool supportsScaling() const override {
    PYBIND11_OVERRIDE(bool, editor::ManipulatorInterface, supportsScaling);
  }

  fmtx4 getWorldMatrix() const override {
    PYBIND11_OVERRIDE_PURE(fmtx4, editor::ManipulatorInterface, getWorldMatrix);
  }
  fvec3 getWorldPosition() const override {
    PYBIND11_OVERRIDE_PURE(fvec3, editor::ManipulatorInterface, getWorldPosition);
  }
  fquat getWorldRotation() const override {
    PYBIND11_OVERRIDE_PURE(fquat, editor::ManipulatorInterface, getWorldRotation);
  }

  void applyTranslationDelta(const fvec3& delta) override {
    PYBIND11_OVERRIDE_PURE(void, editor::ManipulatorInterface, applyTranslationDelta, delta);
  }
  void applyRotationDelta(const fquat& delta) override {
    PYBIND11_OVERRIDE_PURE(void, editor::ManipulatorInterface, applyRotationDelta, delta);
  }
  void applyScaleDelta(float uniformDelta) override {
    PYBIND11_OVERRIDE_PURE(void, editor::ManipulatorInterface, applyScaleDelta, uniformDelta);
  }

  void onBeginManipulation(editor::ManipMode mode) override {
    PYBIND11_OVERRIDE(void, editor::ManipulatorInterface, onBeginManipulation, mode);
  }
  void onEndManipulation(editor::ManipMode mode) override {
    PYBIND11_OVERRIDE(void, editor::ManipulatorInterface, onEndManipulation, mode);
  }
};

///////////////////////////////////////////////////////////////////////////////

void pyinit_editor(py::module& module_lev2) {
  using namespace editor;
  auto type_codec = python::pb11_typecodec_t::instance();

  /////////////////////////////////////////////////////////////////////////////////
  // Enums
  /////////////////////////////////////////////////////////////////////////////////

  py::enum_<ManipMode>(module_lev2, "ManipMode")
      .value("TRANSLATE", ManipMode::TRANSLATE)
      .value("ROTATE", ManipMode::ROTATE)
      .value("SCALE", ManipMode::SCALE)
      .export_values();

  py::enum_<ManipAxis>(module_lev2, "ManipAxis")
      .value("NONE", ManipAxis::NONE)
      .value("X", ManipAxis::X)
      .value("Y", ManipAxis::Y)
      .value("Z", ManipAxis::Z)
      .value("XY", ManipAxis::XY)
      .value("XZ", ManipAxis::XZ)
      .value("YZ", ManipAxis::YZ)
      .value("VIEW", ManipAxis::VIEW)
      .value("FREE", ManipAxis::FREE)
      .export_values();

  /////////////////////////////////////////////////////////////////////////////////
  // ManipulatorInterface (with trampoline for Python subclassing)
  /////////////////////////////////////////////////////////////////////////////////

  auto mif_type_t = py::class_<ManipulatorInterface, PyManipulatorInterface, manipinterface_ptr_t>(module_lev2, "ManipulatorInterface")
      .def(py::init<>())
      .def("supportsTranslation", &ManipulatorInterface::supportsTranslation)
      .def("supportsRotation", &ManipulatorInterface::supportsRotation)
      .def("supportsScaling", &ManipulatorInterface::supportsScaling)
      .def("getWorldMatrix", &ManipulatorInterface::getWorldMatrix)
      .def("getWorldPosition", &ManipulatorInterface::getWorldPosition)
      .def("getWorldRotation", &ManipulatorInterface::getWorldRotation)
      .def("applyTranslationDelta", &ManipulatorInterface::applyTranslationDelta)
      .def("applyRotationDelta", &ManipulatorInterface::applyRotationDelta)
      .def("applyScaleDelta", &ManipulatorInterface::applyScaleDelta)
      .def("onBeginManipulation", &ManipulatorInterface::onBeginManipulation)
      .def("onEndManipulation", &ManipulatorInterface::onEndManipulation);
  type_codec->registerStdCodec<manipinterface_ptr_t>(mif_type_t);

  /////////////////////////////////////////////////////////////////////////////////
  // DecompTransformManipulator
  /////////////////////////////////////////////////////////////////////////////////

  auto dxm_type_t = py::class_<DecompTransformManipulator, ManipulatorInterface, decompxfmanip_ptr_t>(module_lev2, "DecompTransformManipulator")
      .def(py::init<>())
      .def(py::init<decompxf_ptr_t>())
      .def_property("target",
          &DecompTransformManipulator::target,
          &DecompTransformManipulator::setTarget);
  type_codec->registerStdCodec<decompxfmanip_ptr_t>(dxm_type_t);

  /////////////////////////////////////////////////////////////////////////////////
  // Editor
  /////////////////////////////////////////////////////////////////////////////////

  auto ed_type_t = py::class_<Editor, editor_ptr_t>(module_lev2, "Editor")
      .def(py::init([]() -> editor_ptr_t {
        return std::make_shared<Editor>();
      }))
      .def_property_readonly("selectionManager",
          [](editor_ptr_t editor) -> selmgr_ptr_t {
            return editor->_selection_manager;
          })
      .def_property("currentManipInterface",
          [](editor_ptr_t editor) -> manipinterface_ptr_t {
            return editor->_current_manipulator_interface;
          },
          [](editor_ptr_t editor, manipinterface_ptr_t manip) {
            editor->_current_manipulator_interface = manip;
          });
  type_codec->registerStdCodec<editor_ptr_t>(ed_type_t);

  /////////////////////////////////////////////////////////////////////////////////
  // ManipController
  /////////////////////////////////////////////////////////////////////////////////

  auto mc_type_t = py::class_<ManipController, manipcontroller_ptr_t>(module_lev2, "ManipController")
      .def(py::init<>())
      .def_property("mode",
          &ManipController::mode,
          &ManipController::setMode)
      .def_property("target",
          &ManipController::target,
          &ManipController::setTarget)
      .def("updateCamera", &ManipController::updateCamera)
      .def("handleEvent", &ManipController::handleEvent)
      .def("draw", &ManipController::draw)
      .def_property_readonly("hoveredAxis", &ManipController::hoveredAxis)
      .def_property_readonly("activeAxis", &ManipController::activeAxis)
      .def_property_readonly("isDragging", &ManipController::isDragging)
      .def_readwrite("gizmoScale", &ManipController::_gizmoScale)
      .def_readwrite("hitThreshold", &ManipController::_hitThreshold);
  type_codec->registerStdCodec<manipcontroller_ptr_t>(mc_type_t);

  /////////////////////////////////////////////////////////////////////////////////
  // SelectionManager
  /////////////////////////////////////////////////////////////////////////////////

  auto sm_type_t = py::class_<SelectionManager, selmgr_ptr_t>(module_lev2, "SelectionManager")
      .def_property_readonly("currentObject",
          [](selmgr_ptr_t selmgr) -> object_ptr_t {
            return selmgr->_selectedObject;
          });
  type_codec->registerStdCodec<selmgr_ptr_t>(sm_type_t);
}

} // namespace ork::lev2
