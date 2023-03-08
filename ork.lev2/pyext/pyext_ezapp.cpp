////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include "pyext.h"
#include <ork/kernel/string/deco.inl>
#include <ork/kernel/environment.h>
#include <ork/lev2/ui/layoutgroup.inl>
#include <iostream>

///////////////////////////////////////////////////////////////////////////////

namespace ork::lev2 {

void pyinit_gfx_qtez(py::module& module_lev2) {
  auto type_codec = python::TypeCodec::instance();
  /////////////////////////////////////////////////////////////////////////////////
  auto ezappcontext_type = //
      py::class_<EzAppContext, ezappctx_ptr_t>(module_lev2, "EzAppContext");
  type_codec->registerStdCodec<ezappctx_ptr_t>(ezappcontext_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto bind_scene = [](orkezapp_ptr_t app, scenegraph::scene_ptr_t scene){
    if(not app->_userSpecifiedOnDraw){
      app->onDraw([=](ui::drawevent_constptr_t drwev) { //
        ork::opq::mainSerialQueue()->Process();
        auto context = drwev->GetTarget();
        scene->renderOnContext(context);
      });
    }
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
          [type_codec](py::object appinstance,py::kwargs kwargs) { //

            ork::genviron.init_from_global_env();

            auto appinitdata = std::make_shared<AppInitData>();

            if (kwargs) {
              for (auto item : kwargs) {
                auto key = py::cast<std::string>(item.first);
                if (key == "left") {
                  appinitdata->_left = py::cast<int>(item.second);
                } else if (key == "top") {
                  appinitdata->_top = py::cast<int>(item.second);
                } else if (key == "width") {
                  appinitdata->_width = py::cast<int>(item.second);
                } else if (key == "height") {
                  appinitdata->_height = py::cast<int>(item.second);
                } else if (key == "fullscreen") {
                  appinitdata->_fullscreen = py::cast<bool>(item.second);
                } else if (key == "ssaa") {
                  appinitdata->_ssaa_samples = py::cast<int>(item.second);
                }
              }
            }


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
              });
            }
            ////////////////////////////////////////////////////////////////////
            if (py::hasattr(appinstance, "onDraw")) {
              auto drawfn //
                  = py::cast<py::function>(appinstance.attr("onDraw"));
              rval->_vars.makeValueForKey<py::function>("drawfn") = drawfn;
              rval->_userSpecifiedOnDraw = true;
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
      .def_property(
          "timescale",
          [](orkezapp_ptr_t app) -> float { return app->_timescale; },
          [](orkezapp_ptr_t app, float val) { app->_timescale = val; })
      ///////////////////////////////////////////////////////
      .def_property_readonly(
          "mainwin",
          [](orkezapp_ptr_t app) -> ezmainwin_ptr_t { return app->_mainWindow; })
      ///////////////////////////////////////////////////////
      .def_property_readonly("topWidget", [](orkezapp_ptr_t ezapp) -> eztopwidget_ptr_t { //
        return ezapp->_eztopwidget;
      })
      ///////////////////////////////////////////////////////
      .def_property_readonly("topLayoutGroup", [](orkezapp_ptr_t ezapp) -> uilayoutgroup_ptr_t { //
        return ezapp->_topLayoutGroup;
      })
      ///////////////////////////////////////////////////////
      .def_property_readonly("uicontext", [](orkezapp_ptr_t ezapp) -> ui::context_ptr_t { //
        return ezapp->_uicontext;
      })
      ///////////////////////////////////////////////////////
      .def(
          "createScene",
          [bind_scene](orkezapp_ptr_t app, varmap::varmap_ptr_t params) -> scenegraph::scene_ptr_t { //

              if(app->_movie_record_frame_lambda){
                params->makeValueForKey<gfxcontext_lambda_t>("onRenderComplete",app->_movie_record_frame_lambda);
              }

              auto scene = std::make_shared<scenegraph::Scene>(params);
              bind_scene(app,scene);
              return scene;
          })
      ///////////////////////////////////////////////////////
      .def(
          "processMainSerialQueue",
          [](orkezapp_ptr_t app) { //
            ork::opq::mainSerialQueue()->Process();
          })
      ///////////////////////////////////////////////////////
      .def(
          "renderSceneGraph",
          [](orkezapp_ptr_t app) { //
            ork::opq::mainSerialQueue()->Process();
          })
      ///////////////////////////////////////////////////////
      .def(
          "setRefreshPolicy",
          [](orkezapp_ptr_t app, ERefreshPolicy policy, int fps) { //
            app->setRefreshPolicy(RefreshPolicyItem{policy, fps});
          })
      ///////////////////////////////////////////////////////
      .def(
          "enableMovieRecording",
          [](orkezapp_ptr_t app, std::string path) { //
              app->enableMovieRecording(path);
          })
      ///////////////////////////////////////////////////////
      .def(
          "finishMovieRecording",
          [](orkezapp_ptr_t app) { //
              app->finishMovieRecording();
          })
      ///////////////////////////////////////////////////////
      .def(
          "signalExit",
          [](orkezapp_ptr_t app) { //
              app->signalExit();
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
  /////////////////////////////////////////////////////////////////////////////////
  auto ezmainwin_type = //
      py::class_<EzMainWin, ezmainwin_ptr_t>(module_lev2, "EzMainWin")
      .def_property_readonly("appwin",[](ezmainwin_ptr_t mwin) -> appwindow_ptr_t {
        return mwin->_appwin;
      });
  type_codec->registerStdCodec<ezmainwin_ptr_t>(ezmainwin_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto eztopwidget_type = //
      py::class_<EzTopWidget, ui::Group, eztopwidget_ptr_t>(module_lev2, "EzTopWidget")
      .def(
          "enableUiDraw",
          [](eztopwidget_ptr_t ezw) { //
              ezw->enableUiDraw();
          });
  type_codec->registerStdCodec<eztopwidget_ptr_t>(eztopwidget_type);
  /////////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2

} // namespace ork::lev2
