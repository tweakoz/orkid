////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include "pyext.h"
#include <ork/lev2/ui/event.h>
#include <ork/ecs/ecs.h>
#include <ork/profiling.inl>
#include <iostream>

///////////////////////////////////////////////////////////////////////////////

namespace ork::ecs {

void pyinit_scene(py::module& module_ecs);
void pyinit_archetype(py::module& module_ecs);
void pyinit_entity(py::module& module_ecs);
void pyinit_component(py::module& module_ecs);
void pyinit_system(py::module& module_ecs);
void pyinit_simulation(py::module& module_ecs);
void pyinit_scenegraph(py::module& module_ecs);
void pyinit_controller(py::module& module_ecs);
void pyinit_datatable(py::module& module_ecs);
void pyinit_physics(py::module& module_ecs);
void pyinit_pysys(py::module& module_ecs);

} // namespace ork::ecs

////////////////////////////////////////////////////////////////////////////////

using drawevent_ptr_t = std::shared_ptr<ui::DrawEvent>;
using ctx_t           = ork::python::unmanaged_ptr<lev2::Context>;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wwritable-strings"

struct PseudoArgs {
  int _argc      = 1;
  char* _argv[1] = {"wtf"};
};

#pragma GCC diagnostic pop

using pseudoargs_ptr_t = std::shared_ptr<PseudoArgs>;

ork::lev2::orkezapp_ptr_t ecsappcreate(py::object appinstance, py::kwargs kwargs) {
  auto init_data = std::make_shared<ork::AppInitData>();
  lev2::initModule(init_data);
  ecs::initModule(init_data);
  ////////////////////////////////////////////////////////////////////
  ::ork::python::GlobalState::instance();
  ////////////////////////////////////////////////////////////////////
  if (kwargs) {
    for (auto item : kwargs) {
      auto key = py::cast<std::string>(item.first);
      if (key == "ssaa") {
        init_data->_ssaa_samples = py::cast<int>(item.second);
      } else if (key == "left") {
        init_data->_left = py::cast<int>(item.second);
      } else if (key == "top") {
        init_data->_top = py::cast<int>(item.second);
      } else if (key == "width") {
        init_data->_width = py::cast<int>(item.second);
      } else if (key == "height") {
        init_data->_height = py::cast<int>(item.second);
      } else if (key == "fullscreen") {
        init_data->_fullscreen = py::cast<bool>(item.second);
      } else if (key == "disableMouseCursor") {
        init_data->_disableMouseCursor = py::cast<bool>(item.second);
      }
    }
  }
  ////////////////////////////////////////////////////////////////////
  auto args                                               = std::make_shared<PseudoArgs>();
  auto rval                                               = ork::lev2::OrkEzApp::create(init_data);
  auto d_ev                                               = std::make_shared<ork::ui::DrawEvent>(nullptr);
  rval->_vars->makeValueForKey<drawevent_ptr_t>("drawev") = d_ev;
  rval->_vars->makeValueForKey<pseudoargs_ptr_t>("args")  = args;
  ////////////////////////////////////////////////////////////////////
  if (py::hasattr(appinstance, "onGpuInit")) {
    auto gpuinitfn //
        = py::cast<py::function>(appinstance.attr("onGpuInit"));
    rval->_vars->makeValueForKey<py::function>("gpuinitfn") = gpuinitfn;
    rval->onGpuInit([=](lev2::Context* ctx) { //
      ctx->makeCurrentContext();
      py::gil_scoped_acquire acquire;
      auto pyfn = rval->_vars->typedValueForKey<py::function>("gpuinitfn");
      pyfn.value()(ctx_t(ctx));
    });
  }
  ////////////////////////////////////////////////////////////////////
  if (py::hasattr(appinstance, "onGpuExit")) {
    auto gpuexitfn //
        = py::cast<py::function>(appinstance.attr("onGpuExit"));
    rval->_vars->makeValueForKey<py::function>("gpuexitfn") = gpuexitfn;
    rval->onGpuExit([=](lev2::Context* ctx) { //
      ctx->makeCurrentContext();
      py::gil_scoped_acquire acquire;
      auto pyfn = rval->_vars->typedValueForKey<py::function>("gpuexitfn");
      pyfn.value()(ctx_t(ctx));
    });
  }
  ////////////////////////////////////////////////////////////////////
  if (py::hasattr(appinstance, "onUpdate")) {
    auto updfn //
        = py::cast<py::function>(appinstance.attr("onUpdate"));
    rval->_vars->makeValueForKey<py::function>("updatefn") = updfn;
    rval->onUpdate([=](ork::ui::updatedata_ptr_t updata) { //
      auto pyfn = rval->_vars->typedValueForKey<py::function>("updatefn");
      try {
        py::gil_scoped_acquire acquire;
        pyfn.value()(updata);
      } catch (py::error_already_set& e) {
        printf( "\n\npython exception in onUpdate\n\n");
        e.restore();
        PyErr_Print();
        OrkAssert(false);
      } catch (std::exception& e) {
        std::cerr << e.what();
        OrkAssert(false);
      }
    });
  }
  ////////////////////////////////////////////////////////////////////
  if (py::hasattr(appinstance, "onUpdateInit")) {
    auto updfn //
        = py::cast<py::function>(appinstance.attr("onUpdateInit"));
    rval->_vars->makeValueForKey<py::function>("updateinitfn") = updfn;
    rval->onUpdateInit([=]() { //
      py::gil_scoped_acquire acquire;
      auto pyfn = rval->_vars->typedValueForKey<py::function>("updateinitfn");
      try {
        pyfn.value()();
      } catch (std::exception& e) {
        std::cerr << e.what();
        OrkAssert(false);
      }
    });
  }
  ////////////////////////////////////////////////////////////////////
  if (py::hasattr(appinstance, "onUpdateExit")) {
    auto updfn //
        = py::cast<py::function>(appinstance.attr("onUpdateExit"));
    rval->_vars->makeValueForKey<py::function>("updateexitfn") = updfn;
    rval->onUpdateExit([=]() { //
      py::gil_scoped_acquire acquire;
      auto pyfn = rval->_vars->typedValueForKey<py::function>("updateexitfn");
      try {
        pyfn.value()();
        auto ctrl = rval->_vars->typedValueForKey<ork::ecs::controller_ptr_t>("controller");
        if(ctrl) {
          if(ctrl.value()->_onSimulationExit){
            py::gil_scoped_release rel;
            ctrl.value()->_onSimulationExit();
          }
        }
      } catch (std::exception& e) {
        std::cerr << e.what();
        OrkAssert(false);
      }
    });
  }
  ////////////////////////////////////////////////////////////////////
  if (py::hasattr(appinstance, "onUiEvent")) {
    auto uievfn //
        = py::cast<py::function>(appinstance.attr("onUiEvent"));
    rval->_vars->makeValueForKey<py::function>("uievfn") = uievfn;
    rval->onUiEvent([=](ork::ui::event_constptr_t ev) -> ui::HandlerResult { //
      EASY_BLOCK("ecsapp::onUiEvent");
      py::gil_scoped_acquire acquire;
      auto pyfn = rval->_vars->typedValueForKey<py::function>("uievfn");
      try {
        pyfn.value()(ev);
      } catch (std::exception& e) {
        std::cerr << e.what();
        OrkAssert(false);
      }
      return ui::HandlerResult();
    });
  }
  ////////////////////////////////////////////////////////////////////
  if (py::hasattr(appinstance, "onDraw")) {
    auto drawfn //
        = py::cast<py::function>(appinstance.attr("onDraw"));
    rval->_vars->makeValueForKey<py::function>("drawfn") = drawfn;
    rval->onDraw([=](ui::drawevent_constptr_t drwev) { //
      ork::opq::mainSerialQueue()->Process();
      py::gil_scoped_acquire acquire;
      auto pyfn       = rval->_vars->typedValueForKey<py::function>("drawfn");
      auto mydrev     = rval->_vars->typedValueForKey<drawevent_ptr_t>("drawev");
      *mydrev.value() = *drwev;
      try {
        pyfn.value()(drwev);
      } catch (std::exception& e) {
        std::cerr << e.what();
        OrkAssert(false);
      }
    });
  } /* else {
     auto scene = py::cast<scenegraph::scene_ptr_t>(appinstance.attr("scene"));
     rval->onDraw([=](ui::drawevent_constptr_t drwev) { //
       ork::opq::mainSerialQueue()->Process();
       auto context = drwev->GetTarget();
       scene->renderOnContext(context);
     });
   }*/
  return rval;
}

PYBIND11_MODULE(_ecs, module_ecs) {
  // module_ecs.attr("__name__") = "ecs";
  //////////////////////////////////////////////////////////////////////////////
  module_ecs.doc() = "Orkid Ecs Library (scene/actor composition, simulation)";
  //////////////////////////////////////////////////////////////////////////////
  pyinit_scene(module_ecs);
  pyinit_archetype(module_ecs);
  pyinit_entity(module_ecs);
  pyinit_component(module_ecs);
  pyinit_system(module_ecs);
  pyinit_simulation(module_ecs);
  pyinit_scenegraph(module_ecs);
  pyinit_controller(module_ecs);
  pyinit_datatable(module_ecs);
  pyinit_physics(module_ecs);
  pyinit_pysys(module_ecs);
  //////////////////////////////////////////////////////////////////////////////
  module_ecs.def("createApp", &ecsappcreate);
}
