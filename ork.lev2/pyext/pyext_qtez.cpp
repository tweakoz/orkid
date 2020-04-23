#include "pyext.h"
#include <ork/kernel/string/deco.inl>

///////////////////////////////////////////////////////////////////////////////

namespace ork::lev2 {

void pyinit_gfx_qtez(py::module& module_lev2) {
  auto type_codec = python::TypeCodec::instance();

  auto base_init_qtapp = []() {

  };

  py::class_<OrkEzQtApp, qtezapp_ptr_t>(module_lev2, "OrkEzQtApp") //
      .def_static(
          "createWithScene",
          [](py::function gpuinitfn, py::function updfn) { //
            auto rval = OrkEzQtApp::createWithScene();

            rval->_vars.makeValueForKey<py::function>("gpuinitfn") = gpuinitfn;
            rval->_vars.makeValueForKey<py::function>("updatefn")  = updfn;
            drwev_t d_ev                                           = drwev_t(new ui::DrawEvent(nullptr));
            rval->_vars.makeValueForKey<drwev_t>("drawev")         = d_ev;

            rval->onGpuInitWithScene([=](Context* ctx, scenegraph::scene_ptr_t scene) { //
              ctx->makeCurrentContext();
              auto pyfn = rval->_vars.typedValueForKey<py::function>("gpuinitfn");
              pyfn.value()(ctx_t(ctx), scene);
              // The main thread is now owned by C++
              //  therefore the main thread has to let go of the GIL
              rval->_vars.makeValueForKey<py::gil_scoped_release>("permaletgoGIL");
              // it will be released post-exec()
            });
            rval->onUpdateWithScene([=](UpdateData updata, scenegraph::scene_ptr_t scene) { //
              py::gil_scoped_acquire acquire;
              auto pyfn = rval->_vars.typedValueForKey<py::function>("updatefn");
              pyfn.value()(updata, scene);
            });
            return rval;
          })
      .def_static(
          "create",
          [](py::function gpuinitfn, py::function updfn, py::function drawfn) { //
            auto rval = OrkEzQtApp::create();

            rval->_vars.makeValueForKey<py::function>("gpuinitfn") = gpuinitfn;
            rval->_vars.makeValueForKey<py::function>("drawfn")    = drawfn;
            rval->_vars.makeValueForKey<py::function>("updatefn")  = updfn;
            drwev_t d_ev                                           = drwev_t(new ui::DrawEvent(nullptr));
            rval->_vars.makeValueForKey<drwev_t>("drawev")         = d_ev;

            rval->onGpuInit([=](Context* ctx) { //
              ctx->makeCurrentContext();
              auto pyfn = rval->_vars.typedValueForKey<py::function>("gpuinitfn");
              pyfn.value()(ctx_t(ctx));
            });
            rval->onUpdate([=](UpdateData updata) { //
              py::gil_scoped_acquire acquire;
              auto pyfn = rval->_vars.typedValueForKey<py::function>("updatefn");
              pyfn.value()();
            });

            rval->onDraw([=](const ui::DrawEvent& drwev) { //
              auto pyfn                = rval->_vars.typedValueForKey<py::function>("drawfn");
              auto mydrev              = rval->_vars.typedValueForKey<drwev_t>("drawev");
              mydrev.value()->mpTarget = drwev.GetTarget();
              pyfn.value()(drwev_t(mydrev.value()));
            });

            return rval;
          })
      .def(
          "setRefreshPolicy",
          [](qtezapp_ptr_t app, ERefreshPolicy policy, int fps) { //
            app->setRefreshPolicy(RefreshPolicyItem{policy, fps});
          })
      .def(
          "exec",
          [](qtezapp_ptr_t app) -> int { //
            int rval = app->runloop();
            /////////////////////////////////////////
            // unpermarelease the GIL
            //  (if it was previously permareleased)
            /////////////////////////////////////////
            app->_vars.makeValueForKey<void*>(nullptr);
            // may call ~py::gil_scoped_release()
            /////////////////////////////////////////
            return rval;
          });
}

} // namespace ork::lev2
