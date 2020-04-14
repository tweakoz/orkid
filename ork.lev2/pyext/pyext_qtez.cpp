#include "pyext.h"
#include <ork/kernel/string/deco.inl>

///////////////////////////////////////////////////////////////////////////////

namespace ork::lev2 {

void pyinit_gfx_qtez(py::module& module_lev2) {
  auto type_codec = python::TypeCodec::instance();

  py::class_<OrkEzQtApp, std::shared_ptr<OrkEzQtApp>>(module_lev2, "OrkEzQtApp") //
      .def_static(
          "create",
          [](py::function gpuinitfn, py::function updfn, py::function drawfn) { //
            int* argc  = new int(1);
            auto argv  = (char**)malloc(sizeof(char**));
            argv[0]    = (char*)malloc(1);
            argv[0][0] = 0;
            auto rval  = OrkEzQtApp::create(*argc, argv);

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
          [](std::shared_ptr<OrkEzQtApp>& app, ERefreshPolicy policy, int fps) { //
            app->setRefreshPolicy(RefreshPolicyItem{policy, fps});
          })
      .def("exec", [](std::shared_ptr<OrkEzQtApp>& app) -> int { //
        return app->exec();
      });
} // namespace ork::lev2

} // namespace ork::lev2
