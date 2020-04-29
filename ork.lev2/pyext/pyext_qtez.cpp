#include "pyext.h"
#include <ork/kernel/string/deco.inl>

///////////////////////////////////////////////////////////////////////////////

namespace ork::lev2 {

void pyinit_gfx_qtez(py::module& module_lev2) {
  auto type_codec = python::TypeCodec::instance();

  auto base_init_qtapp = []() {

  };
  /////////////////////////////////////////////////////////////////////////////////
  auto updata_type =                                                      //
      py::class_<UpdateData, updatedata_ptr_t>(module_lev2, "UpdateData") //
          .def_property_readonly(
              "absolutetime",                         //
              [](updatedata_ptr_t updata) -> double { //
                return updata->_abstime;
              })
          .def_property_readonly(
              "deltatime",                            //
              [](updatedata_ptr_t updata) -> double { //
                return updata->_dt;
              });
  type_codec->registerStdCodec<updatedata_ptr_t>(updata_type);
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<OrkEzQtApp, qtezapp_ptr_t>(module_lev2, "OrkEzQtApp") //
      .def_static(
          "create",
          [type_codec](py::object appinstance) { //
            auto rval                                      = OrkEzQtApp::create();
            drwev_t d_ev                                   = drwev_t(new ui::DrawEvent(nullptr));
            rval->_vars.makeValueForKey<drwev_t>("drawev") = d_ev;
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
              });
            }
            ////////////////////////////////////////////////////////////////////
            if (py::hasattr(appinstance, "onDraw")) {
              auto drawfn //
                  = py::cast<py::function>(appinstance.attr("onDraw"));
              rval->_vars.makeValueForKey<py::function>("drawfn") = drawfn;
              rval->onDraw([=](const ui::DrawEvent& drwev) { //
                ork::opq::mainSerialQueue()->Process();
                py::gil_scoped_acquire acquire;
                auto pyfn                = rval->_vars.typedValueForKey<py::function>("drawfn");
                auto mydrev              = rval->_vars.typedValueForKey<drwev_t>("drawev");
                mydrev.value()->mpTarget = drwev.GetTarget();
                pyfn.value()(drwev_t(mydrev.value()));
              });
            } else if (py::hasattr(appinstance, "sceneparams")) {
              auto sceneparams //
                  = py::cast<varmap::varmap_ptr_t>(appinstance.attr("sceneparams"));
              auto scene = std::make_shared<scenegraph::Scene>(sceneparams);
              varmap::VarMap::value_type scenevar;
              scenevar.Set<scenegraph::scene_ptr_t>(scene);
              auto pyscene = type_codec->encode(scenevar);
              py::setattr(appinstance, "scene", pyscene);
              rval->onDraw([=](const ui::DrawEvent& drwev) { //
                ork::opq::mainSerialQueue()->Process();
                auto context = drwev.GetTarget();
                scene->renderOnContext(context);
              });
            }
            ////////////////////////////////////////////////////////////////////
            if (py::hasattr(appinstance, "onUpdate")) {
              auto updfn //
                  = py::cast<py::function>(appinstance.attr("onUpdate"));
              rval->_vars.makeValueForKey<py::function>("updatefn") = updfn;
              rval->onUpdate([=](updatedata_ptr_t updata) { //
                py::gil_scoped_acquire acquire;
                auto pyfn = rval->_vars.typedValueForKey<py::function>("updatefn");
                pyfn.value()(updata);
              });
            }
            ////////////////////////////////////////////////////////////////////
            return rval;
          })
      ///////////////////////////////////////////////////////
      .def(
          "setRefreshPolicy",
          [](qtezapp_ptr_t app, ERefreshPolicy policy, int fps) { //
            app->setRefreshPolicy(RefreshPolicyItem{policy, fps});
          })
      .def(
          "exec",
          [](qtezapp_ptr_t app) -> int { //
            auto wrapped = [&]() -> int {
              py::gil_scoped_release release_gil;
              // The main thread is now owned by C++
              //  therefore the main thread has to let go of the GIL
              // it will be reacquired post-runloop()
              return app->runloop();
            };
            int rval = wrapped();
            // GIL reacquired
            app->joinUpdate();
            return rval;
          });
}

} // namespace ork::lev2
