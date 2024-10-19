////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "pyext.h"
#include <ork/kernel/string/deco.inl>
#include <ork/kernel/environment.h>
#include <ork/lev2/ui/layoutgroup.inl>
#include <ork/profiling.inl>
#include <iostream>

///////////////////////////////////////////////////////////////////////////////

namespace ork::lev2 {

void pyinit_gfx_qtez(py::module& module_lev2) {
  auto type_codec = python::pb11_typecodec_t::instance();
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
        if(app->_overrideRCFD)
          scene->renderOnContext(context,app->_overrideRCFD);
        else
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
            rcfd_ptr_t override_rcfd = nullptr;
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
                } else if (key == "use_audio") {
                  appinitdata->_audio = py::cast<bool>(item.second);
                } else if (key == "offscreen") {
                  appinitdata->_offscreen = py::cast<bool>(item.second);
                } else if (key == "ssaa") {
                  appinitdata->_ssaa_samples = py::cast<int>(item.second);
                } else if (key == "disableMouseCursor") {
                  appinitdata->_disableMouseCursor = py::cast<bool>(item.second);
                } else if (key == "msaa") {
                  appinitdata->_msaa_samples = py::cast<int>(item.second);
                } else if( key == "rcfd" ) {
                  override_rcfd = py::cast<rcfd_ptr_t>(item.second);
                }
              }
            }

            auto rval                                              = OrkEzApp::create(appinitdata);
            
            auto d_ev                                              = std::make_shared<ui::DrawEvent>(nullptr);
            rval->_vars->makeValueForKey<uidrawevent_ptr_t>("drawev") = d_ev;
            rval->_vars->makeValueForKey<py::object>("appinstance") = appinstance;
            rval->_overrideRCFD = override_rcfd;
            ////////////////////////////////////////////////////////////////////
            if (py::hasattr(appinstance, "onGpuInit")) {
              auto gpuinitfn //
                  = py::cast<py::function>(appinstance.attr("onGpuInit"));
              rval->_vars->makeValueForKey<py::function>("gpuinitfn") = gpuinitfn;
              rval->onGpuInit([=](Context* ctx) { //
                ctx->makeCurrentContext();
                py::gil_scoped_acquire acquire;
                auto pyfn = rval->_vars->typedValueForKey<py::function>("gpuinitfn");
                pyfn.value()(ctx_t(ctx));
              });
            }
            ////////////////////////////////////////////////////////////////////
            if (py::hasattr(appinstance, "onGpuUpdate")) {
              auto gpuupdatefn //
                  = py::cast<py::function>(appinstance.attr("onGpuUpdate"));
              rval->_vars->makeValueForKey<py::function>("gpuupdatefn") = gpuupdatefn;
              rval->onGpuUpdate([=](Context* ctx) { //
                ctx->makeCurrentContext();
                py::gil_scoped_acquire acquire;
                auto pyfn = rval->_vars->typedValueForKey<py::function>("gpuupdatefn");
                pyfn.value()(ctx_t(ctx));
              });
            }
            ////////////////////////////////////////////////////////////////////
            if (py::hasattr(appinstance, "onGpuPreFrame")) {
              auto gpupreframefn //
                  = py::cast<py::function>(appinstance.attr("onGpuPreFrame"));
              rval->_vars->makeValueForKey<py::function>("gpupreframefn") = gpupreframefn;
              rval->onGpuPreFrame([=](Context* ctx) { //
                ctx->makeCurrentContext();
                py::gil_scoped_acquire acquire;
                auto pyfn = rval->_vars->typedValueForKey<py::function>("gpupreframefn");
                pyfn.value()(ctx_t(ctx));
              });
            }
            ////////////////////////////////////////////////////////////////////
            if (py::hasattr(appinstance, "onGpuPostFrame")) {
              auto gpupostframefn //
                  = py::cast<py::function>(appinstance.attr("onGpuPostFrame"));
              rval->_vars->makeValueForKey<py::function>("gpupostframefn") = gpupostframefn;
              rval->onGpuPostFrame([=](Context* ctx) { //
                ctx->makeCurrentContext();
                py::gil_scoped_acquire acquire;
                auto pyfn = rval->_vars->typedValueForKey<py::function>("gpupostframefn");
                pyfn.value()(ctx_t(ctx));
              });
            }
            ////////////////////////////////////////////////////////////////////
            if (py::hasattr(appinstance, "onDraw")) {
              auto drawfn //
                  = py::cast<py::function>(appinstance.attr("onDraw"));
              rval->_vars->makeValueForKey<py::function>("drawfn") = drawfn;
              rval->_userSpecifiedOnDraw = true;
              rval->onDraw([=](ui::drawevent_constptr_t drwev) { //
                ork::opq::mainSerialQueue()->Process();
                py::gil_scoped_acquire acquire;
                auto pyfn       = rval->_vars->typedValueForKey<py::function>("drawfn");
                auto mydrev     = rval->_vars->typedValueForKey<uidrawevent_ptr_t>("drawev");
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
              rval->_vars->makeValueForKey<py::function>("updatefn") = updfn;
              rval->onUpdate([=](ui::updatedata_ptr_t updata) { //
                py::gil_scoped_acquire acquire;
                auto pyfn = rval->_vars->typedValueForKey<py::function>("updatefn");
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
              rval->_vars->makeValueForKey<py::function>("uievfn") = uievfn;
              rval->onUiEvent([=](ui::event_constptr_t ev) -> ui::HandlerResult { //
                EASY_BLOCK("pyezapp::evh1", profiler::colors::Red);
                py::gil_scoped_acquire acquire;
                EASY_END_BLOCK;
                EASY_BLOCK("pyezapp::evh2", profiler::colors::Red);
                auto pyfn = rval->_vars->typedValueForKey<py::function>("uievfn");
                try {
                  auto res = pyfn.value()(ev).cast<ui::HandlerResult>();
                  if(res.mHandler==nullptr){
                    res = rval->_topLayoutGroup->OnUiEvent(ev);
                  }
                  return res;
                } catch (std::exception& e) {
                  printf( "onUiEvent exception (probably HandlerResult)\n" );
                  std::cerr << e.what() << std::endl;
                  //OrkAssert(false);
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
      .def_property_readonly("vars", [](orkezapp_ptr_t ezapp) -> varmap::varmap_ptr_t { //
        return ezapp->_vars;
      })
      ///////////////////////////////////////////////////////
      .def_property_readonly(
          "mainwin",
          [](orkezapp_ptr_t app) -> ezmainwin_ptr_t { return app->_mainWindow; })
      ///////////////////////////////////////////////////////
      .def_property_readonly(
          "shouldUpdateThrottleOnGPU",
          [](orkezapp_ptr_t app) -> bool { //
          return app->shouldUpdateThrottleOnGPU();
      })
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
          [=](orkezapp_ptr_t app,py::kwargs kwargs) -> int { //
            if (kwargs) {
              for (auto item : kwargs) {
                auto key = py::cast<std::string>(item.first);
                if (key == "on_iter") {
                 auto py_val = py::cast<py::object>(item.second);
                 OrkAssert(py::hasattr(py_val, "__call__"));
                  app->_onRunLoopIteration = [py_val](){
                      py::gil_scoped_acquire acquire_gil;
                      py_val();
                  };
                }
              }
            }



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
              auto mwin = ezw->_mainwin;
              OrkAssert(mwin->_app._userSpecifiedOnDraw == false );
              ezw->enableUiDraw();
          });
  type_codec->registerStdCodec<eztopwidget_ptr_t>(eztopwidget_type);
  /////////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2

} // namespace ork::lev2
