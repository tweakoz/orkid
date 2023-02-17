////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include "pyext.h"
#include <ork/kernel/string/deco.inl>
#include <iostream>

///////////////////////////////////////////////////////////////////////////////

namespace ork::lev2 {

void pyinit_gfx_qtez(py::module& module_lev2) {
  auto type_codec = python::TypeCodec::instance();

  auto base_init_qtapp = []() {

  };

  /////////////////////////////////////////////////////////////////////////////////
  auto bind_scene = [](orkezapp_ptr_t app, scenegraph::scene_ptr_t scene){
    app->onDraw([=](ui::drawevent_constptr_t drwev) { //
      ork::opq::mainSerialQueue()->Process();
      auto context = drwev->GetTarget();
      scene->renderOnContext(context);
    });
    app->onResize([=](int w, int h) {
      scene->_compositorImpl->compositingContext().Resize(w, h);
    });
    app->_mainWindow->_execsceneparams = scene->_params;
    app->_mainWindow->_execscene = scene;
  };
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<OrkEzApp, orkezapp_ptr_t>(module_lev2, "OrkEzApp") //
      .def_static(
          "create",
          [type_codec](py::object appinstance) { //

            auto appinitdata = std::make_shared<AppInitData>();
            auto rval                                              = OrkEzApp::create(appinitdata);
            auto d_ev                                              = std::make_shared<ui::DrawEvent>(nullptr);
            rval->_vars.makeValueForKey<uidrawevent_ptr_t>("drawev") = d_ev;
            ////////////////////////////////////////////////////////////////////
            if (py::hasattr(appinstance, "onGpuInit")) {
              auto gpuinitfn //
                  = py::cast<py::function>(appinstance.attr("onGpuInit"));
              rval->_vars.makeValueForKey<py::function>("gpuinitfn") = gpuinitfn;
              rval->onGpuInit([=](Context* ctx) { //
                ctx->makeCurrentContext();
                py::gil_scoped_acquire acquire;
                auto pyfn = rval->_vars.typedValueForKey<py::function>("gpuinitfn");
                pyfn.value()(ctx_t(ctx));
                // try {
                // pyfn.value()(ctx_t(ctx));
                //} catch (std::exception& e) {
                // OrkAssert(false);
                //}
              });
            }
            ////////////////////////////////////////////////////////////////////
            if (py::hasattr(appinstance, "onDraw")) {
              auto drawfn //
                  = py::cast<py::function>(appinstance.attr("onDraw"));
              rval->_vars.makeValueForKey<py::function>("drawfn") = drawfn;
              rval->onDraw([=](ui::drawevent_constptr_t drwev) { //
                ork::opq::mainSerialQueue()->Process();
                py::gil_scoped_acquire acquire;
                auto pyfn       = rval->_vars.typedValueForKey<py::function>("drawfn");
                auto mydrev     = rval->_vars.typedValueForKey<uidrawevent_ptr_t>("drawev");
                *mydrev.value() = *drwev;
                try {
                  pyfn.value()(drwev);
                } catch (std::exception& e) {
                  std::cerr << e.what();
                  OrkAssert(false);
                }
              });
            } else {
            }
            ////////////////////////////////////////////////////////////////////
            if (py::hasattr(appinstance, "onUpdate")) {
              auto updfn //
                  = py::cast<py::function>(appinstance.attr("onUpdate"));
              rval->_vars.makeValueForKey<py::function>("updatefn") = updfn;
              rval->onUpdate([=](ui::updatedata_ptr_t updata) { //
                py::gil_scoped_acquire acquire;
                auto pyfn = rval->_vars.typedValueForKey<py::function>("updatefn");
                try {
                  pyfn.value()(updata);
                } catch (std::exception& e) {
                  std::cerr << e.what();
                  OrkAssert(false);
                }
              });
            }
            ////////////////////////////////////////////////////////////////////
            if (py::hasattr(appinstance, "onUiEvent")) {
              bool using_scene = py::hasattr(appinstance, "sceneparams");
              auto uievfn //
                  = py::cast<py::function>(appinstance.attr("onUiEvent"));
              rval->_vars.makeValueForKey<py::function>("uievfn") = uievfn;
              rval->onUiEvent([=](ui::event_constptr_t ev) -> ui::HandlerResult { //
                py::gil_scoped_acquire acquire;
                auto pyfn = rval->_vars.typedValueForKey<py::function>("uievfn");
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
            return rval;
          })
      ///////////////////////////////////////////////////////
      .def(
          "createScene",
          [bind_scene](orkezapp_ptr_t app, varmap::varmap_ptr_t params) -> scenegraph::scene_ptr_t { //
              auto scene = std::make_shared<scenegraph::Scene>(params);
              bind_scene(app,scene);
              return scene;
          })
      ///////////////////////////////////////////////////////
      .def(
          "setRefreshPolicy",
          [](orkezapp_ptr_t app, ERefreshPolicy policy, int fps) { //
            app->setRefreshPolicy(RefreshPolicyItem{policy, fps});
          })
      .def(
          "mainThreadLoop",
          [](orkezapp_ptr_t app) -> int { //
            auto wrapped = [&]() -> int {
              py::gil_scoped_release release_gil;
              // The main thread is now owned by C++
              //  therefore the main thread has to let go of the GIL
              // it will be reacquired post-runloop()
              return app->mainThreadLoop();
            };
            int rval = wrapped();
            // GIL reacquired
            {
              py::gil_scoped_release release_gil;
              app->joinUpdate();
            }
            return rval;
          });
} // namespace ork::lev2

} // namespace ork::lev2
