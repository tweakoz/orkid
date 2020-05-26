#include "pyext.h"
#include <ork/kernel/string/deco.inl>

///////////////////////////////////////////////////////////////////////////////

namespace ork::lev2 {

void pyinit_gfx_qtez(py::module& module_lev2) {
  auto type_codec = python::TypeCodec::instance();

  auto base_init_qtapp = []() {

  };
  /////////////////////////////////////////////////////////////////////////////////
  using drawevent_ptr_t = std::shared_ptr<ui::DrawEvent>;
  py::class_<ui::DrawEvent, drawevent_ptr_t>(module_lev2, "DrawEvent")       //
      .def_property_readonly("context", [](drawevent_ptr_t event) -> ctx_t { //
        return ctx_t(event->GetTarget());
      });
  /////////////////////////////////////////////////////////////////////////////////
  auto uievent_type = //
      py::class_<ui::Event, ui::event_ptr_t>(module_lev2, "UiEvent")
          .def_property_readonly(
              "x",                            //
              [](ui::event_ptr_t ev) -> int { //
                return ev->miX;
              })
          .def_property_readonly(
              "y",                            //
              [](ui::event_ptr_t ev) -> int { //
                return ev->miY;
              })
          .def_property_readonly(
              "code",                         //
              [](ui::event_ptr_t ev) -> int { //
                return ev->miEventCode;
              })
          .def_property_readonly(
              "shift",                        //
              [](ui::event_ptr_t ev) -> int { //
                return int(ev->mbSHIFT);
              })
          .def_property_readonly(
              "alt",                          //
              [](ui::event_ptr_t ev) -> int { //
                return int(ev->mbALT);
              })
          .def_property_readonly(
              "ctrl",                         //
              [](ui::event_ptr_t ev) -> int { //
                return int(ev->mbCTRL);
              })
          .def_property_readonly(
              "left",                         //
              [](ui::event_ptr_t ev) -> int { //
                return int(ev->mbLeftButton);
              })
          .def_property_readonly(
              "middle",                       //
              [](ui::event_ptr_t ev) -> int { //
                return int(ev->mbMiddleButton);
              })
          .def_property_readonly(
              "right",                        //
              [](ui::event_ptr_t ev) -> int { //
                return int(ev->mbRightButton);
              });
  type_codec->registerStdCodec<ui::event_ptr_t>(uievent_type);
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
            auto rval                                              = OrkEzQtApp::create();
            auto d_ev                                              = std::make_shared<ui::DrawEvent>(nullptr);
            rval->_vars.makeValueForKey<drawevent_ptr_t>("drawev") = d_ev;
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
            if (py::hasattr(appinstance, "sceneparams")) {
              auto sceneparams //
                  = py::cast<varmap::varmap_ptr_t>(appinstance.attr("sceneparams"));
              auto scene = std::make_shared<scenegraph::Scene>(sceneparams);
              varmap::VarMap::value_type scenevar;
              scenevar.Set<scenegraph::scene_ptr_t>(scene);
              auto pyscene = type_codec->encode(scenevar);
              py::setattr(appinstance, "scene", pyscene);
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
                auto mydrev     = rval->_vars.typedValueForKey<drawevent_ptr_t>("drawev");
                *mydrev.value() = *drwev;
                pyfn.value()(mydrev);
              });
            } else {
              auto scene = py::cast<scenegraph::scene_ptr_t>(appinstance.attr("scene"));
              rval->onDraw([=](ui::drawevent_constptr_t drwev) { //
                ork::opq::mainSerialQueue()->Process();
                auto context = drwev->GetTarget();
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
            if (py::hasattr(appinstance, "onUiEvent")) {
              bool using_scene = py::hasattr(appinstance, "sceneparams");
              auto uievfn //
                  = py::cast<py::function>(appinstance.attr("onUiEvent"));
              rval->_vars.makeValueForKey<py::function>("uievfn") = uievfn;
              rval->onUiEvent([=](ui::event_constptr_t ev) -> ui::HandlerResult { //
                py::gil_scoped_acquire acquire;
                auto pyfn = rval->_vars.typedValueForKey<py::function>("uievfn");
                pyfn.value()(ev);
                return ui::HandlerResult();
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
            {
              py::gil_scoped_release release_gil;
              app->joinUpdate();
            }
            return rval;
          });
} // namespace ork::lev2

} // namespace ork::lev2
