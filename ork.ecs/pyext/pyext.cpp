////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include "pyext.h"
#include <ork/lev2/ui/event.h>
#include <ork/lev2/init.h>
#include <ork/ecs/ecs.h>
#include <ork/kernel/profiler.h>
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
void pyinit_transformcurve(py::module& module_ecs);
void pyinit_boids(py::module& module_ecs);
void pyinit_stochwav(py::module& module_ecs);
void pyinit_simplesound(py::module& module_ecs);
void pyinit_globalsynth(py::module& module_ecs);
void pyinit_probe(py::module& module_ecs);
void pyinit_soundfield(py::module& module_ecs);
void pyinit_asset_system(py::module& module_ecs);

} // namespace ork::ecs

// Forward declaration of the lev2 process-wide loader context. Lives
// at ork::lev2::gloadercontext (defined in ork.lev2/src/lev2_init.cpp);
// pyecs_headless_init binds it to the calling thread's TLS so headless
// scripts get a valid GfxEnv.loadingContext().
namespace ork::lev2 { extern context_ptr_t gloadercontext; }

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

// Headless ECS init — runs the same lev2 + ecs reflection-registration
// + finalize-initialization sequence as ecsappcreate, but does NOT
// construct an EzApp / spin up a window. The Vulkan loader context
// (gloadercontext) IS created and bound to the calling thread's TLS
// so GfxEnv.loadingContext() / lev2::contextForCurrentThread() return
// it — that lets headless scripts (e.g. ork.scene.tojson.py) run
// Scene.__init__ code paths that eager-build asset wrappers needing
// a GPU context (PbrMaterial.build, FloatGrid uploads, etc.).
//
// Idempotent: safe to call multiple times in a single process.
//
// Pair with pyecs_headless_exit() at script end for clean teardown
// (drains operation queues, runs Context::shutdown).
void pyecs_headless_init() {
  static bool s_done = false;
  if (s_done) return;
  s_done = true;
  auto stringpoolctx = std::make_shared<StringPoolContext>();
  StringPoolStack::push(stringpoolctx);
  auto init_data = std::make_shared<ork::AppInitData>();
  lev2::initModule(init_data);
  ecs::initModule(init_data);
  init_data->finalizeInitialization();
  ::ork::python::GlobalState::instance();

  // Bind gloadercontext to this thread's TLS so loadingContext()
  // returns it. ThreadGfxContext is RAII (ctor pushes, dtor pops);
  // we hold it in a process-lifetime static so the push survives
  // until exit. The extern needs to be fully namespaced — we're in
  // ork::ecs here, but gloadercontext lives in ork::lev2.
  if (ork::lev2::gloadercontext) {
    static auto s_tls = std::make_shared<lev2::ThreadGfxContext>(
        ork::lev2::gloadercontext.get());
    (void)s_tls;
  }
}

void pyecs_headless_exit() {
  // lev2::initModule auto-spawns g_loader_thread as soon as
  // gloadercontext exists. The static-destruction path on Python exit
  // tries to destroy a still-joinable std::thread → terminate(). Join
  // it explicitly before draining the op queues.
  ork::lev2::stopLoaderThread();
  ork::opq::exit();
  // Latch GPU-shutdown, exactly as OrkEzApp::_onGpuExit does for the windowed
  // funnel: the loader context is torn down and released above, so every GPU
  // resource destructor that still fires (python object graph teardown, atexit)
  // must no-op rather than call into a driver that is on its way out. Set LAST —
  // the real vkDestroy* work above has to run with the latch still clear.
  lev2::GfxEnv::setGpuShutdownComplete(true);
}

ork::lev2::orkezapp_ptr_t ecsappcreate(py::object appinstance, py::kwargs kwargs) {
  auto stringpoolctx = std::make_shared<StringPoolContext>();
  StringPoolStack::push(stringpoolctx);
  auto init_data = std::make_shared<ork::AppInitData>();
  lev2::initModule(init_data);
  ecs::initModule(init_data);
  init_data->finalizeInitialization();
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
      try {
        auto pyfn = rval->_vars->typedValueForKey<py::function>("gpuinitfn");
        pyfn.value()(ctx_t(ctx));
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
      py::gil_scoped_acquire acquire;
      try {
        auto pyfn = rval->_vars->typedValueForKey<py::function>("updatefn");
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
      OrkProfilerSampleScope(CHANNEL_MAIN, "ecsapp::onUiEvent");
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

// mod_gil_not_used: see the note over PYBIND11_MODULE(_core) — an undeclared
// extension makes CPython 3.14t silently re-enable the GIL at import time.
PYBIND11_MODULE(_ecs, module_ecs, py::mod_gil_not_used()) {
  // module_ecs.attr("__name__") = "ecs";
  //////////////////////////////////////////////////////////////////////////////
  // Force orkengine.core and orkengine.lev2 (in that order) to load before
  // any of ecs's bindings register, so the pybind11 type registry already
  // contains fvec4, drawables, scenegraph, etc. when ecs's def() statements
  // convert their default arg values to Python objects. Same pattern as
  // orkengine.lev2 — see ork.lev2/pyext/src/pyext.cpp.
  py::module_::import("orkengine.core");
  py::module_::import("orkengine.lev2");
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
  pyinit_transformcurve(module_ecs);
  pyinit_boids(module_ecs);
  pyinit_stochwav(module_ecs);
  pyinit_simplesound(module_ecs);
  pyinit_globalsynth(module_ecs);
  pyinit_probe(module_ecs);
  pyinit_soundfield(module_ecs);
  pyinit_asset_system(module_ecs);
  //////////////////////////////////////////////////////////////////////////////
  module_ecs.def("createApp", &ecsappcreate);
  module_ecs.def("headless_init", &pyecs_headless_init,
                 "Headless lev2+ecs reflection init (no EzApp/GPU). "
                 "Call at the top of a data-only pyext test.");
  module_ecs.def("headless_exit", &pyecs_headless_exit,
                 "Drain operation queues — pair with headless_init.");
  // headless_appinit — like headless_init but ALSO creates an
  // offscreen ezapp. Use when the headless tool needs to drive GPU
  // work inline (HdriToXir bake, etc.) — pair with
  // ezapp.mainThreadBegin() + ezapp.bindGfxToCurrentThread().
  // kwargs are forwarded to lev2.lev2appinit (use_subsystems=, etc.).
  module_ecs.def("headless_appinit", [](py::kwargs kwargs) -> ork::lev2::orkezapp_ptr_t {
        // ECS reflection registration (idempotent via static guard).
        pyecs_headless_init();
        // Delegate ezapp creation to lev2's lev2appinit so the kwargs
        // surface stays single-sourced.
        auto lev2_mod = py::module_::import("orkengine.lev2");
        auto fn       = lev2_mod.attr("lev2appinit");
        py::object rv = fn(**kwargs);
        return rv.cast<ork::lev2::orkezapp_ptr_t>();
      },
      "Headless lev2+ecs init + offscreen ezapp creation. kwargs "
      "forwarded to lev2.lev2appinit (use_subsystems=, width=, etc.).");
  //////////////////////////////////////////////////////////////////////////////
  module_ecs.def("ecsInitCallback", [](ork::appinitdata_ptr_t appinit) {
    auto stringpoolctx = std::make_shared<StringPoolContext>();
    StringPoolStack::push(stringpoolctx);
    ecs::initModule(appinit);
  });
}
