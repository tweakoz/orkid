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
#include <ork/lev2/gfx/util/movie.inl>
#include <ork/profiling.inl>
#include <iostream>
#include <ork/lev2/aud/audiodevice.h>
#include <ork/lev2/aud/singularity/synth.h>
#include <ork/lev2/ez_secondary_win.h>
#include <pybind11/embed.h>  // if using embedded interpreter

///////////////////////////////////////////////////////////////////////////////
namespace ork {
  void initModule(appinitdata_ptr_t init_data);
}
namespace ork::lev2 {
  void initModule(appinitdata_ptr_t init_data);
}
///////////////////////////////////////////////////////////////////////////////

namespace ork::lev2 {
static logchannel_ptr_t logchan_EZAPP = logger()->getChannel("EZAPP");

void ezapp_python_traceback(py::error_already_set& e) {
    try {
        // Import traceback module
        py::object traceback = py::module::import("traceback");
        py::object sys = py::module::import("sys");

        // Get exception info
        py::object exc_type = py::reinterpret_borrow<py::object>(e.type());
        py::object exc_value = py::reinterpret_borrow<py::object>(e.value());
        py::object exc_tb = py::reinterpret_borrow<py::object>(e.trace());

        // Format the traceback
        py::object format_exception = traceback.attr("format_exception");
        py::list tb_lines = format_exception(exc_type, exc_value, exc_tb);

        // Print each line
        for (auto line : tb_lines) {
            auto decoed = deco::string(py::str(line).cast<std::string>(), 255, 100, 0);
            std::cout << decoed;
        }
    } catch (std::exception& e2) {
        std::cerr << "ezapp_python_traceback failed: " << e2.what() << std::endl;
        std::cerr << "Original error: " << e.what() << std::endl;
    } catch (...) {
        std::cerr << "ezapp_python_traceback failed (unknown exception)" << std::endl;
        std::cerr << "Original error: " << e.what() << std::endl;
    }
}

void pyinit_gfx_qtez(py::module& module_lev2) {
  auto type_codec = python::pb11_typecodec_t::instance();
  /////////////////////////////////////////////////////////////////////////////////
  using evinterceptor_ptr_t = std::shared_ptr<EzUiEventInterceptor>;
  auto ezintercept_type = //
      py::class_<EzUiEventInterceptor, ui::Widget, evinterceptor_ptr_t>(module_lev2, "EzUiEventInterceptor")
      .def(py::init<>())
      .def_property("onUiEvent", //
        [](evinterceptor_ptr_t evi) -> py::object { //
          return py::int_(7);
        },
        [](evinterceptor_ptr_t evi, py::object val) { //
          auto as_callable = py::cast<py::function>(val);
          evi->_vars->makeValueForKey<py::function>("_onuieventhandler") = as_callable;
          ui::event_lambda_t handler = [=](ui::event_constptr_t ev) -> ui::HandlerResult {
            py::gil_scoped_acquire acquire;
            auto pyfn = evi->_vars->typedValueForKey<py::function>("_onuieventhandler");
            auto invoked = pyfn.value()(ev);
            return py::cast<ui::HandlerResult>(invoked);
          };
          evi->_onUiEventLambda = handler;
        });
  type_codec->registerStdCodec<evinterceptor_ptr_t>(ezintercept_type);
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
  py::class_<OrkEzApp, ork::Application, orkezapp_ptr_t>(module_lev2, "OrkEzApp") //
      .def_static(
          "create",
          [type_codec](py::object appinstance,py::kwargs kwargs) { //
            ork::genviron.init_from_global_env();
            auto appinit = appinitdata(); // Use the singleton
            rcfd_ptr_t override_rcfd = nullptr;
            std::vector<appinitfn_t> pre_finalize_fns;

            if (kwargs) {
              for (auto item : kwargs) {
                auto key = py::cast<std::string>(item.first);
                if (key == "_pre_init_fns") {
                  auto fns_list = py::cast<py::list>(item.second);
                  for (auto fn_item : fns_list) {
                    auto callable = py::cast<py::function>(fn_item);
                    pre_finalize_fns.push_back([callable](appinitdata_ptr_t appinit) {
                      callable(appinit);
                    });
                  }
                } else if (key == "name") {
                  auto app_name = py::cast<std::string>(item.second);
                  appinit->_application_name = app_name;
                } else if (key == "left") {
                  appinit->_left = py::cast<int>(item.second);
                } else if (key == "top") {
                  appinit->_top = py::cast<int>(item.second);
                } else if (key == "width") {
                  appinit->_width = py::cast<int>(item.second);
                } else if (key == "height") {
                  appinit->_height = py::cast<int>(item.second);
                } else if (key == "fullscreen") {
                  appinit->_fullscreen = py::cast<bool>(item.second);
                } else if (key == "fullscreen_monitor") {
                  appinit->_fullscreen_monitor = py::cast<std::string>(item.second);
                } else if (key == "enable_always_on_top") {
                  appinit->_canalwaysontop =  py::cast<bool>(item.second);
                } else if (key == "enable_graphics") {
                  appinit->_enable_graphics = py::cast<bool>(item.second);
                  //printf("enable_graphics<%d>\n", appinit->_enable_graphics);
                } else if (key == "enable_audio") {
                  appinit->_enable_audio = py::cast<bool>(item.second);
                } else if (key == "enable_audio_input") {
                  appinit->_enable_audio_input = py::cast<bool>(item.second);
                } else if (key == "enable_audio_output") {
                  appinit->_enable_audio_output = py::cast<bool>(item.second);
                } else if (key == "enable_audio_synth") {
                  appinit->_enable_audio_synth = py::cast<bool>(item.second);
                } else if (key == "audio_input_devname") {
                  appinit->_audio_input_devname = py::cast<std::string>(item.second);; // cant have synth without an audio dev output !
                } else if (key == "audio_output_devname") {
                  appinit->_audio_output_devname = py::cast<std::string>(item.second);; // cant have synth without an audio dev output !
                } else if (key == "audio_input_numchannels") {
                  appinit->_audio_input_numchannels = py::cast<int>(item.second);; // cant have synth without an audio dev output !
                } else if (key == "audio_output_numchannels") {
                  appinit->_audio_output_numchannels = py::cast<int>(item.second);; // cant have synth without an audio dev output !
                } else if (key == "audio_stream_sync") {
                  appinit->_audio_stream_sync = py::cast<bool>(item.second);; // cant have synth without an audio dev output !
                } else if (key == "freerun") {
                  appinit->_freerunning = py::cast<bool>(item.second);
                } else if (key == "target_ups") {
                  appinit->_target_ups = py::cast<float>(item.second);
                } else if (key == "target_fps") {
                  appinit->_target_fps = py::cast<float>(item.second);
                } else if (key == "offscreen") {
                  appinit->_offscreen = py::cast<bool>(item.second);
                } else if (key == "ssaa") {
                  appinit->_ssaa_samples = py::cast<int>(item.second);
                } else if (key == "disable_mouse_cursor") {
                  appinit->_disableMouseCursor = py::cast<bool>(item.second);
                } else if (key == "fsmouse") {
                  appinit->_fsMouseMode = py::cast<bool>(item.second);
                } else if (key == "msaa") {
                  appinit->_msaa_samples = py::cast<int>(item.second);
                } else if( key == "rcfd" ) {
                  override_rcfd = py::cast<rcfd_ptr_t>(item.second);
                } else if( key == "movie_output_path" ) {
                  if( py::isinstance<py::str>( item.second ) ) {
                    std::string mpath = py::cast<std::string>(item.second);
                    appinit->_movie_output_path = file::Path(mpath);
                  }
                } else if (key == "enable_freerun_ups") {
                  appinit->_log_freerun_ups = py::cast<bool>(item.second);
                } else if (key == "enable_freerun_fps") {
                  appinit->_log_freerun_fps = py::cast<bool>(item.second);
                } else if (key == "enable_lockstep_ups") {
                  appinit->_log_lockstep_ups = py::cast<bool>(item.second);
                } else if (key == "enable_lockstep_fps") {
                  appinit->_log_lockstep_fps = py::cast<bool>(item.second);
                } else if (key == "drm_mode_id") {
                  if (py::isinstance<py::str>(item.second)) {
                    appinit->_drm_mode = py::cast<std::string>(item.second);
                    appinit->_use_drm = true;
                    printf("USING DRM: mode=%s\n", appinit->_drm_mode.c_str());
                  } else if (!item.second.is_none()) {
                    OrkAssert(false);
                  }
                } else if (key == "use_subsystems") {
                  // Support both list-based API and legacy boolean API
                  if (py::isinstance<py::list>(item.second)) {
                    // New list-based API: use_subsystems=['gpu', 'audioO', 'lev2', pyworker_subsystem]
                    auto subsystem_list = py::cast<py::list>(item.second);
                    appinit->_use_subsystems = true;
                    appinit->_defer_gpu_init = true;
                    for (auto entry : subsystem_list) {
                      if (py::isinstance<py::str>(entry)) {
                        // String name of built-in subsystem
                        appinit->_enabled_subsystems.insert(py::cast<std::string>(entry));
                      } else {
                        // Custom subsystem object from Python
                        auto subsystem = py::cast<subsystem_ptr_t>(entry);
                        appinit->_custom_subsystems.push_back(subsystem);
                        // Also add to enabled set by name for dependency resolution
                        appinit->_enabled_subsystems.insert(subsystem->_name);
                      }
                    }
                  }
                }
              } // for (auto item : kwargs) {
              //////////////////////////////////////
              // ensure flags make sense
              //////////////////////////////////////
              if(not appinit->_freerunning){
                if( not appinit->_offscreen ){
                  // if we are lockstep, force offscreen mode
                  appinit->_offscreen = true;
                  logchan_EZAPP->log("forcing offscreen mode for lockstep operation");
                }
                ork::genviron.set("ORKID_AUDIO_IOCLASS", "STREAM");
                appinit->_audio_stream_sync = true;
                logchan_EZAPP->log("forcing ORKID_AUDIO_IOCLASS to STREAM for lockstep operation");
              }
              if(appinit->_audio_stream_sync ){
                ork::genviron.set("ORKID_AUDIO_IOCLASS", "STREAM");
                appinit->_audio_ioclass = "STREAM";
              }
              if(appinit->_enable_audio_synth){
                // if we have synth enabled, we need audio output
                appinit->_enable_audio_output = true;
              }
              if(appinit->_enable_audio_output){
                // if we have audio output, we need audio enabled
                appinit->_enable_audio = true;
              }
              if(appinit->_enable_audio_input){
                // if we have audio input, we need audio enabled
                appinit->_enable_audio = true;
              }
              //////////////////////////////////////

            } // if (kwargs) {
            /////////////////////////////
            ::ork::lev2::initModule(appinit);
            for (auto& fn : pre_finalize_fns) {
              fn(appinit);
            }
            appinit->finalizeInitialization();
            //logchan_EZAPP->log("finalizeInitialization done..");
            fflush(stdout);
            /////////////////////////////
            auto rval                                                 = OrkEzApp::create(appinit);
            auto d_ev                                                 = std::make_shared<ui::DrawEvent>(nullptr);
            //logchan_EZAPP->log("ezapp<%p>",(void*) rval.get() );
            rval->_vars->makeValueForKey<uidrawevent_ptr_t>("drawev") = d_ev;
            rval->_vars->makeValueForKey<py::object>("appinstance")   = appinstance;
            rval->_overrideRCFD = override_rcfd;
            ////////////////////////////////////////////////////////////////////
            if (py::hasattr(appinstance, "onAppInit")) {
              //logchan_EZAPP->log("REG onAppInit");
              auto appinitfn //
                  = py::cast<py::function>(appinstance.attr("onAppInit"));
              rval->_vars->makeValueForKey<py::function>("appinitfn") = appinitfn;
              rval->onEzAppInit([=]() { //
                //logchan_EZAPP->log("EXE onAppInit");
                py::gil_scoped_acquire acquire;
                auto pyfn = rval->_vars->typedValueForKey<py::function>("appinitfn");
                auto initdata = appinitdata();
                try {
                  pyfn.value()(initdata);
                } catch (py::error_already_set& e) {
                  ezapp_python_traceback(e);
                  printf( "\n\npython exception in onAppInit\n\n");
                  e.restore();
                  PyErr_Print();
                  OrkAssert(false);
                } catch (std::exception& e) {
                  std::cerr << e.what();
                  OrkAssert(false);
                }
              });
            }
            else{
              //logchan_EZAPP->log("NO onAppInit");
            }
            ////////////////////////////////////////////////////////////////////
            if (py::hasattr(appinstance, "onAppExit")) {
              //logchan_EZAPP->log("REG onAppExit");
              auto appexitfn //
                  = py::cast<py::function>(appinstance.attr("onAppExit"));
              rval->_vars->makeValueForKey<py::function>("appexitfn") = appexitfn;
              rval->onEzAppExit([=]() { //
                //logchan_EZAPP->log("EXE onAppExit");
                py::gil_scoped_acquire acquire;
                auto pyfn = rval->_vars->typedValueForKey<py::function>("appexitfn");
                try {
                  pyfn.value()();
                } catch (py::error_already_set& e) {
                  ezapp_python_traceback(e);
                  printf( "\n\npython exception in onAppExit\n\n");
                  e.restore();
                  PyErr_Print();
                  OrkAssert(false);
                } catch (std::exception& e) {
                  std::cerr << e.what();
                  OrkAssert(false);
                }
              });
            }
            else{
              //logchan_EZAPP->log("NO onAppExit");
            }
            ////////////////////////////////////////////////////////////////////
            if (py::hasattr(appinstance, "onAudioInit")) {
              //logchan_EZAPP->log("REG onAudioInit");
              auto audinitfn //
                  = py::cast<py::function>(appinstance.attr("onAudioInit"));
              rval->_vars->makeValueForKey<py::function>("audinitfn") = audinitfn;
              rval->onAudioInit([=](audiodevice_ptr_t adev) { //
                //logchan_EZAPP->log("EXE onAudioInit");
                py::gil_scoped_acquire acquire;
                auto pyfn = rval->_vars->typedValueForKey<py::function>("audinitfn");
                try {
                  pyfn.value()(adev);
                } catch (py::error_already_set& e) {
                  ezapp_python_traceback(e);
                  printf( "\n\npython exception in onAudioInit\n\n");
                  e.restore();
                  PyErr_Print();
                  OrkAssert(false);
                } catch (std::exception& e) {
                  std::cerr << e.what();
                  OrkAssert(false);
                }
              });
            }
            else{
              //logchan_EZAPP->log("NO onAudioInit");
            }
            ////////////////////////////////////////////////////////////////////
            if (py::hasattr(appinstance, "onAudioExit")) {
              auto audexitfn //
                  = py::cast<py::function>(appinstance.attr("onAudioExit"));
              rval->_vars->makeValueForKey<py::function>("audexitfn") = audexitfn;
              rval->onAudioExit([=](audiodevice_ptr_t adev) { //
                py::gil_scoped_acquire acquire;
                auto pyfn = rval->_vars->typedValueForKey<py::function>("audexitfn");
                try {
                  pyfn.value()(adev);
                } catch (py::error_already_set& e) {
                  ezapp_python_traceback(e);
                  printf( "\n\npython exception in onAudioExit\n\n");
                  e.restore();
                  PyErr_Print();
                } catch (std::exception& e) {
                  std::cerr << "onAudioExit: " << e.what() << std::endl;
                }
              });
            }
            ////////////////////////////////////////////////////////////////////
            if (py::hasattr(appinstance, "onSynthInit")) {
              //logchan_EZAPP->log("REG onSynthInit");
              auto syninitfn //
                  = py::cast<py::function>(appinstance.attr("onSynthInit"));
              rval->_vars->makeValueForKey<py::function>("syninitfn") = syninitfn;
              rval->onSynthInit([=](audio::singularity::synth_ptr_t syn) { //
                logchan_EZAPP->log("EXE onSynthInit");
                py::gil_scoped_acquire acquire;
                auto pyfn = rval->_vars->typedValueForKey<py::function>("syninitfn");
                try {
                  pyfn.value()(syn);
                } catch (py::error_already_set& e) {
                  ezapp_python_traceback(e);
                  printf( "\n\npython exception in onSynthInit\n\n");
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
            if (py::hasattr(appinstance, "onSynthExit")) {
              auto synexitfn //
                  = py::cast<py::function>(appinstance.attr("onSynthExit"));
              rval->_vars->makeValueForKey<py::function>("synexitfn") = synexitfn;
              rval->onSynthExit([=](audio::singularity::synth_ptr_t syn) { //
                py::gil_scoped_acquire acquire;
                auto pyfn = rval->_vars->typedValueForKey<py::function>("synexitfn");
                try {
                  pyfn.value()(syn);
                } catch (py::error_already_set& e) {
                  ezapp_python_traceback(e);
                  printf( "\n\npython exception in onSynthExit\n\n");
                  e.restore();
                  PyErr_Print();
                } catch (std::exception& e) {
                  std::cerr << "onSynthExit: " << e.what() << std::endl;
                }
              });
            }
            ////////////////////////////////////////////////////////////////////
            if (py::hasattr(appinstance, "onGpuInit")) {
              auto gpuinitfn //
                  = py::cast<py::function>(appinstance.attr("onGpuInit"));
              rval->_vars->makeValueForKey<py::function>("gpuinitfn") = gpuinitfn;
              rval->onGpuInit([=](Context* ctx) { //
                ctx->makeCurrentContext();
                py::gil_scoped_acquire acquire;
                try {
                  auto pyfn = rval->_vars->typedValueForKey<py::function>("gpuinitfn");
                  pyfn.value()(ctx_t(ctx));
                } catch (py::error_already_set& e) {
                  ezapp_python_traceback(e);
                  printf( "\n\npython exception in onGpuInit\n\n");
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
              rval->onGpuExit([=](Context* ctx) { //
                ctx->makeCurrentContext();
                py::gil_scoped_acquire acquire;
                auto pyfn = rval->_vars->typedValueForKey<py::function>("gpuexitfn");
                try {
                  pyfn.value()(ctx_t(ctx));
                } catch (py::error_already_set& e) {
                  ezapp_python_traceback(e);
                  printf( "\n\npython exception in onGpuExit\n\n");
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
            if (py::hasattr(appinstance, "onGpuUpdate")) {
              auto gpuupdatefn //
                  = py::cast<py::function>(appinstance.attr("onGpuUpdate"));
              rval->_vars->makeValueForKey<py::function>("gpuupdatefn") = gpuupdatefn;
              rval->onGpuUpdate([=](Context* ctx) { //
                ctx->makeCurrentContext();
                py::gil_scoped_acquire acquire;
                auto pyfn = rval->_vars->typedValueForKey<py::function>("gpuupdatefn");
                try {
                  pyfn.value()(ctx_t(ctx));
                } catch (py::error_already_set& e) {
                  ezapp_python_traceback(e);
                  printf( "\n\npython exception in onGpuUpdate\n\n");
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
            if (py::hasattr(appinstance, "onGpuPreFrame")) {
              auto gpupreframefn //
                  = py::cast<py::function>(appinstance.attr("onGpuPreFrame"));
              rval->_vars->makeValueForKey<py::function>("gpupreframefn") = gpupreframefn;
              rval->onGpuPreFrame([=](Context* ctx) { //
                ctx->makeCurrentContext();
                py::gil_scoped_acquire acquire;
                auto pyfn = rval->_vars->typedValueForKey<py::function>("gpupreframefn");
                try {
                  pyfn.value()(ctx_t(ctx));
                } catch (py::error_already_set& e) {
                  ezapp_python_traceback(e);
                  printf( "\n\npython exception in onGpuPreFrame\n\n");
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
            if (py::hasattr(appinstance, "onGpuPostFrame")) {
              auto gpupostframefn //
                  = py::cast<py::function>(appinstance.attr("onGpuPostFrame"));
              rval->_vars->makeValueForKey<py::function>("gpupostframefn") = gpupostframefn;
              rval->onGpuPostFrame([=](Context* ctx) { //
                ctx->makeCurrentContext();
                py::gil_scoped_acquire acquire;
                auto pyfn = rval->_vars->typedValueForKey<py::function>("gpupostframefn");
                try {
                  pyfn.value()(ctx_t(ctx));
                } catch (py::error_already_set& e) {
                  ezapp_python_traceback(e);
                  printf( "\n\npython exception in onGpuPostFrame\n\n");
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
                } catch (py::error_already_set& e) {
                  ezapp_python_traceback(e);
                  printf( "\n\npython exception in onDraw\n\n");
                  e.restore();
                  PyErr_Print();
                  OrkAssert(false);
                } catch (std::exception& e) {
                  std::cerr << e.what();
                  OrkAssert(false);
                }
              });
            } else {
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
                } catch (py::error_already_set& e) {
                  ezapp_python_traceback(e);
                  printf( "\n\npython exception in onUpdateInit\n\n");
                  e.restore();
                  PyErr_Print();
                  OrkAssert(false);
                } catch (std::exception& e) {
                  std::cerr << e.what();
                  abort();
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
                } catch (py::error_already_set& e) {
                  ezapp_python_traceback(e);
                  printf( "\n\npython exception in onUpdateExit\n\n");
                  e.restore();
                  PyErr_Print();
                  OrkAssert(false);
                } catch (std::exception& e) {
                  std::cerr << e.what();
                  abort();
                }
              });
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
                } catch (py::error_already_set& e) {
                  ezapp_python_traceback(e);
                  printf( "\n\npython exception in onUpdate\n\n");
                  e.restore();
                  PyErr_Print();
                  OrkAssert(false);
                } catch (std::exception& e) {
                  std::cerr << e.what();
                  abort();
                }
                catch (...) {
                  printf( "onUpdate unknown exception\n" );
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
                } catch (py::error_already_set& e) {
                  ezapp_python_traceback(e);
                  printf( "\n\npython exception in onGpuInit\n\n");
                  e.restore();
                  PyErr_Print();
                  OrkAssert(false);
                } catch (std::exception& e) {
                  printf( "onUiEvent exception (probably HandlerResult)\n" );
                  std::cerr << e.what() << std::endl;
                  //OrkAssert(false);
                }
                return ui::HandlerResult();
              });
            }
            ////////////////////////////////////////////////////////////////////
            logchan_EZAPP->log("app creation complete app: %p", (void*) rval.get());
            return rval;
          })
      ///////////////////////////////////////////////////////
      .def_static(
          "createEx",
          [](py::object appinstance, py::list init_fns, py::kwargs kwargs) -> orkezapp_ptr_t { //
            // Inject _pre_init_fns into kwargs and delegate to create
            py::dict kw_dict;
            for (auto item : kwargs) {
              kw_dict[item.first] = item.second;
            }
            kw_dict[py::str("_pre_init_fns")] = init_fns;
            auto lev2_mod = py::module::import("orkengine.lev2");
            auto create_fn = lev2_mod.attr("OrkEzApp").attr("create");
            auto result = create_fn(*py::make_tuple(appinstance), **kw_dict);
            return result.cast<orkezapp_ptr_t>();
          })
      ///////////////////////////////////////////////////////
      .def_property(
          "timescale",
          [](orkezapp_ptr_t app) -> float { return app->_timescale; },
          [](orkezapp_ptr_t app, float val) { app->_timescale = val; })
      ///////////////////////////////////////////////////////
      .def_property_readonly("perf_frame_duration", [](orkezapp_ptr_t app) -> double {
        return app->_perf_frame_duration;
      })
      .def_property_readonly("perf_gpu_update_duration", [](orkezapp_ptr_t app) -> double {
        return app->_perf_gpu_update_duration;
      })
      .def_property_readonly("perf_update_duration", [](orkezapp_ptr_t app) -> double {
        return app->_perf_update_duration;
      })
      ///////////////////////////////////////////////////////
      .def_property_readonly("vars", [](orkezapp_ptr_t ezapp) -> varmap::varmap_ptr_t { //
        return ezapp->_vars;
      })
      ///////////////////////////////////////////////////////
      .def_property_readonly("audio_device", [](orkezapp_ptr_t ezapp) -> audiodevice_ptr_t { //
        return ezapp->_audiodevice;  
      })
      ///////////////////////////////////////////////////////
      .def_property_readonly("audio_synth", [](orkezapp_ptr_t ezapp) -> audio::singularity::synth_ptr_t { //
        return ezapp->_synth;
      })
      ///////////////////////////////////////////////////////
      .def_property_readonly("_appinit", [](orkezapp_ptr_t ezapp) -> appinitdata_ptr_t { //
        return ezapp->_initdata;
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
      .def_property("topLayoutGroup", [](orkezapp_ptr_t ezapp) -> uilayoutgroup_ptr_t { //
        return ezapp->_topLayoutGroup;
      },
      [](orkezapp_ptr_t ezapp, uilayoutgroup_ptr_t lg) { //
        ezapp->_topLayoutGroup = lg;
      })
      ///////////////////////////////////////////////////////
      .def_property("uicontext", [](orkezapp_ptr_t ezapp) -> ui::context_ptr_t { //
        return ezapp->_uicontext;
      },
      [](orkezapp_ptr_t ezapp, ui::context_ptr_t ctx) { //
        ezapp->_uicontext = ctx;
      })
      ///////////////////////////////////////////////////////
      .def_property_readonly("total_samples_rendered", [](orkezapp_ptr_t ezapp) -> size_t { //
        return ezapp->_total_samples_rendered;
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
          [](orkezapp_ptr_t self, py::kwargs args ) -> moviecapcontext_ptr_t { //
            auto settings = std::make_shared<MovieCaptureSettings>();
            if (args) {
              for (auto item : args) {
                auto key = py::cast<std::string>(item.first);
                if (key == "output_path") {
                  if( py::isinstance<py::str>( item.second ) ) {
                    std::string mpath = py::cast<std::string>(item.second);
                    settings->_filename = mpath;
                  }
                  else{
                    py::str pystr = py::cast<py::str>(item.second);
                    std::string mpath = py::cast<std::string>(pystr);
                    settings->_filename = mpath;
                  }
                }
                else if (key == "override_rtb") {
                  settings->_rtbuffer = py::cast<rtbuffer_ptr_t>(item.second);
                }
                else if(key=="fps") {
                  if( py::isinstance<py::int_>( item.second ) ) {
                    settings->_fps = py::cast<int>(item.second);;
                  }
                  else {
                    py::float_ pyf = py::cast<py::float_>(item.second);
                    float ffps = py::cast<float>(pyf);
                    settings->_fps = int(ffps);
                  }
                }
                else if(key=="max_queue_size") {
                  settings->_max_queue_size = py::cast<int>(item.second);;
                }
                else if(key=="preset"){
                  std::string preset = py::cast<std::string>(item.second);
                  if( preset != "fast" &&
                      preset != "medium" &&
                      preset != "default" &&
                      preset != "high" &&
                      preset != "ultra") {
                    fprintf( stderr, "Invalid preset name<%s>\n", preset.c_str());
                    fprintf( stderr, "Valid presets are: fast, medium, default, high, ultra\n");
                    OrkAssert(false);
                  }
                  settings->_preset_name = preset;
                }
                else if(key=="audio_test_tone"){
                  settings->_audio_test_tone = py::cast<bool>(item.second);
                }
              }
            }
            self->enableMovieRecording(settings);
            return self->_moviecapcontext;
          })
      ///////////////////////////////////////////////////////
      .def(
          "finishMovieRecording",
          [](orkezapp_ptr_t app) { //
              app->finishMovieRecording();
          })
      ///////////////////////////////////////////////////////
      .def(
          "enqueueMovieCaptureFrame",
          [](orkezapp_ptr_t app,captureasync_ptr_t future) -> size_t { //
            size_t num_enq = 0;
            if(app->_moviecapcontext){
              auto capbuf = future->_captureBuffer;
              static int frame_index = 0;
              int num_samples = 48000.0/60.0;
              num_enq = app->_moviecapcontext->enqueueFrame(future,capbuf,frame_index++,num_samples);
            }
            return num_enq;
          })
       .def_property_readonly(
          "movie_capture_context",
          [](orkezapp_ptr_t app) -> moviecapcontext_ptr_t { //
              return app->_moviecapcontext;
          })
      ///////////////////////////////////////////////////////
      .def(
          "signalExit",
          [](orkezapp_ptr_t app) { //
              app->signalExit();
              app->_onRunLoopIteration = nullptr;
          })
      .def(
          "shutdown",
          [](orkezapp_ptr_t app) { //
              // Trigger HFSM-driven subsystem shutdown in reverse dependency order
              // This ensures audio/GPU cleanup happens on the correct threads
              // IMPORTANT: Release GIL because shutdown threads may call Python callbacks
              py::gil_scoped_release release_gil;
              app->shutdown();
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
              if( app->_onEzAppInit ){
                app->_onEzAppInit();
              }
              auto RES = app->mainThreadLoop();
              if( app->_onEzAppExit ){
                app->_onEzAppExit();
              }
              return RES;
            };
            int rval = wrapped();
            // GIL reacquired
            {
              py::gil_scoped_release release_gil;
              app->joinUpdate();
            }
            return rval;
          }).
          def("mainThreadBegin", [](orkezapp_ptr_t app) { //
            app->_mainThreadLoopBegin();
          })
      .def("mainThreadEnd", [](orkezapp_ptr_t app) { //
            app->_mainThreadLoopEnd();
          })
      .def("mainThreadIter", [](orkezapp_ptr_t app) { //
            app->_mainThreadLoopIter();
          })
      .def("mainThreadIterCommandLine", [](orkezapp_ptr_t app) { //
            app->_mainThreadLoopIter();
            ork::opq::mainSerialQueue()->Process();
          })
      ///////////////////////////////////////////////////////
      // Phase 6: Secondary window methods
      ///////////////////////////////////////////////////////
      .def("createSecondaryWindow",
          [](orkezapp_ptr_t app, py::kwargs kwargs) -> ezsecondarywin_ptr_t {
            EzSecondaryWinConfig config;
            // Apply kwargs
            if (kwargs) {
              for (auto item : kwargs) {
                auto key = py::cast<std::string>(item.first);
                if (key == "width") config._width = py::cast<int>(item.second);
                else if (key == "height") config._height = py::cast<int>(item.second);
                else if (key == "x") config._x = py::cast<int>(item.second);
                else if (key == "y") config._y = py::cast<int>(item.second);
                else if (key == "title") config._title = py::cast<std::string>(item.second);
                else if (key == "decorated") config._decorated = py::cast<bool>(item.second);
                else if (key == "resizable") config._resizable = py::cast<bool>(item.second);
                else if (key == "floating") config._floating = py::cast<bool>(item.second);
                else if (key == "transparent") config._transparent = py::cast<bool>(item.second);
                else if (key == "focus_on_show") config._focusOnShow = py::cast<bool>(item.second);
                else if (key == "focus_follows_mouse") config._focusFollowsMouse = py::cast<bool>(item.second);
                else if (key == "focus_to_front") config._focusToFront = py::cast<bool>(item.second);
                else if (key == "fullscreen_monitor") config._fullscreenMonitor = py::cast<std::string>(item.second);
              }
            }
            return app->createSecondaryWindow(config);
          })
      .def("createSecondaryWindowFromConfig",
          [](orkezapp_ptr_t app, const EzSecondaryWinConfig& config) -> ezsecondarywin_ptr_t {
            return app->createSecondaryWindow(config);
          })
      .def("createPopupWindow",
          [](orkezapp_ptr_t app, int x, int y, int w, int h, bool transparent) -> ezsecondarywin_ptr_t {
            auto config = EzSecondaryWinConfig::popup(x, y, w, h, transparent);
            return app->createSecondaryWindow(config);
          }, py::arg("x"), py::arg("y"), py::arg("w"), py::arg("h"), py::arg("transparent") = false)
      .def("closeSecondaryWindow", [](orkezapp_ptr_t app, ezsecondarywin_ptr_t win) {
            app->closeSecondaryWindow(win);
          })
      .def("closeAllSecondaryWindows", [](orkezapp_ptr_t app) {
            app->closeAllSecondaryWindows();
          })
      .def_property_readonly("secondaryWindows", [](orkezapp_ptr_t app) -> py::list {
            py::list result;
            for (auto& win : app->_secondaryWindows) {
              result.append(win);
            }
            return result;
          });
  /////////////////////////////////////////////////////////////////////////////////
  auto ezmainwin_type = //
      py::class_<EzMainWin, ezmainwin_ptr_t>(module_lev2, "EzMainWin")
      .def_property_readonly("appwin",[](ezmainwin_ptr_t mwin) -> appwindow_ptr_t {
        return mwin->_appwin;
      })
      .def_property_readonly("perf_render_duration", [](ezmainwin_ptr_t mwin) -> double {
        return mwin->_perf_render_duration;
      })
      .def_property_readonly("perf_enqueue_duration", [](ezmainwin_ptr_t mwin) -> double {
        return mwin->_perf_enqueue_duration;
      })
      .def_property_readonly("perf_present_duration", [](ezmainwin_ptr_t mwin) -> double {
        return mwin->_perf_present_duration;
      })
      .def_property_readonly("perf_acquire_duration", [](ezmainwin_ptr_t mwin) -> double {
        return mwin->_perf_acquire_duration;
      })
      .def_property_readonly("perf_fence_wait_duration", [](ezmainwin_ptr_t mwin) -> double {
        return mwin->_perf_fence_wait_duration;
      })
      .def_property_readonly("perf_beginFrame_duration", [](ezmainwin_ptr_t mwin) -> double {
        return mwin->_perf_beginFrame_duration;
      })
      .def_property_readonly("perf_endFrame_duration", [](ezmainwin_ptr_t mwin) -> double {
        return mwin->_perf_endFrame_duration;
      })
      .def_property_readonly("perf_submit_duration", [](ezmainwin_ptr_t mwin) -> double {
        return mwin->_perf_submit_duration;
      })
      .def_property_readonly("perf_present_vk_duration", [](ezmainwin_ptr_t mwin) -> double {
        return mwin->_perf_present_vk_duration;
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
  // GLFW Monitor enumeration
  /////////////////////////////////////////////////////////////////////////////////
  auto glfwmoninfo_type = //
      py::class_<GlfwMonitorInfo, glfwmonitorinfo_ptr_t>(module_lev2, "GlfwMonitorInfo")
      .def_readonly("name", &GlfwMonitorInfo::_name)
      .def_readonly("x", &GlfwMonitorInfo::_x)
      .def_readonly("y", &GlfwMonitorInfo::_y)
      .def_readonly("width", &GlfwMonitorInfo::_width)
      .def_readonly("height", &GlfwMonitorInfo::_height)
      .def_readonly("refresh_rate", &GlfwMonitorInfo::_refreshRate)
      .def_readonly("physical_width_mm", &GlfwMonitorInfo::_physicalWidthMM)
      .def_readonly("physical_height_mm", &GlfwMonitorInfo::_physicalHeightMM)
      .def_readonly("content_scale_x", &GlfwMonitorInfo::_contentScaleX)
      .def_readonly("content_scale_y", &GlfwMonitorInfo::_contentScaleY)
      .def_readonly("primary", &GlfwMonitorInfo::_primary)
      .def("__repr__", [](glfwmonitorinfo_ptr_t info) -> std::string {
        return FormatString("GlfwMonitorInfo(name='%s', %dx%d@%dHz, pos=%d,%d%s)",
                            info->_name.c_str(), info->_width, info->_height,
                            info->_refreshRate, info->_x, info->_y,
                            info->_primary ? ", primary" : "");
      });
  type_codec->registerStdCodec<glfwmonitorinfo_ptr_t>(glfwmoninfo_type);

  module_lev2.def("enumerateGlfwMonitors", &enumerateGlfwMonitors,
      "Enumerate all GLFW monitors on the system");
  /////////////////////////////////////////////////////////////////////////////////
  // Phase 6: Secondary Window Bindings
  /////////////////////////////////////////////////////////////////////////////////
  using ezsecwinconfig_ptr_t = std::shared_ptr<EzSecondaryWinConfig>;
  auto ezsecwinconfig_type = //
      py::class_<EzSecondaryWinConfig, ezsecwinconfig_ptr_t>(module_lev2, "EzSecondaryWinConfig")
      .def(py::init<>())
      .def_readwrite("width", &EzSecondaryWinConfig::_width)
      .def_readwrite("height", &EzSecondaryWinConfig::_height)
      .def_readwrite("x", &EzSecondaryWinConfig::_x)
      .def_readwrite("y", &EzSecondaryWinConfig::_y)
      .def_readwrite("title", &EzSecondaryWinConfig::_title)
      .def_readwrite("decorated", &EzSecondaryWinConfig::_decorated)
      .def_readwrite("resizable", &EzSecondaryWinConfig::_resizable)
      .def_readwrite("floating", &EzSecondaryWinConfig::_floating)
      .def_readwrite("transparent", &EzSecondaryWinConfig::_transparent)
      .def_readwrite("focus_on_show", &EzSecondaryWinConfig::_focusOnShow)
      .def_readwrite("focus_follows_mouse", &EzSecondaryWinConfig::_focusFollowsMouse)
      .def_readwrite("focus_to_front", &EzSecondaryWinConfig::_focusToFront)
      .def_readwrite("fullscreen_monitor", &EzSecondaryWinConfig::_fullscreenMonitor)
      .def_static("popup", [](int x, int y, int w, int h, bool transparent) {
        return std::make_shared<EzSecondaryWinConfig>(
            EzSecondaryWinConfig::popup(x, y, w, h, transparent));
      }, py::arg("x"), py::arg("y"), py::arg("w"), py::arg("h"), py::arg("transparent") = false);
  type_codec->registerStdCodec<ezsecwinconfig_ptr_t>(ezsecwinconfig_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto ezsecwin_type = //
      py::class_<EzSecondaryWin, ezsecondarywin_ptr_t>(module_lev2, "EzSecondaryWin")
      .def_property_readonly("width", &EzSecondaryWin::width)
      .def_property_readonly("height", &EzSecondaryWin::height)
      .def_property_readonly("perf_render_duration", [](ezsecondarywin_ptr_t win) -> double {
        return win->_perf_render_duration;
      })
      .def_property_readonly("perf_enqueue_duration", [](ezsecondarywin_ptr_t win) -> double {
        return win->_perf_enqueue_duration;
      })
      .def_property_readonly("perf_present_duration", [](ezsecondarywin_ptr_t win) -> double {
        return win->_perf_present_duration;
      })
      .def_property_readonly("perf_acquire_duration", [](ezsecondarywin_ptr_t win) -> double {
        return win->_perf_acquire_duration;
      })
      .def_property_readonly("perf_fence_wait_duration", [](ezsecondarywin_ptr_t win) -> double {
        return win->_perf_fence_wait_duration;
      })
      .def_property_readonly("perf_beginFrame_duration", [](ezsecondarywin_ptr_t win) -> double {
        return win->_perf_beginFrame_duration;
      })
      .def_property_readonly("perf_endFrame_duration", [](ezsecondarywin_ptr_t win) -> double {
        return win->_perf_endFrame_duration;
      })
      .def_property_readonly("perf_submit_duration", [](ezsecondarywin_ptr_t win) -> double {
        return win->_perf_submit_duration;
      })
      .def_property_readonly("perf_present_vk_duration", [](ezsecondarywin_ptr_t win) -> double {
        return win->_perf_present_vk_duration;
      })
      .def_property_readonly("should_close", &EzSecondaryWin::shouldClose)
      .def_property_readonly("ui_context", [](ezsecondarywin_ptr_t win) -> ui::context_ptr_t {
        return win->uiContextPtr();
      })
      .def_property_readonly("gfx_context", [](ezsecondarywin_ptr_t win) -> ctx_t {
        return ctx_t(win->gfxContext());
      })
      .def("requestClose", &EzSecondaryWin::requestClose)
      .def("markDirty", &EzSecondaryWin::markDirty)
      .def_readwrite("maxStalenessSeconds", &EzSecondaryWin::_maxStalenessSeconds)
      .def_property("onDraw",
          [](ezsecondarywin_ptr_t win) -> py::object { return py::none(); },
          [](ezsecondarywin_ptr_t win, py::object callback) {
            if (callback.is_none()) {
              win->_onDraw = nullptr;
            } else {
              auto pyfn = py::cast<py::function>(callback);
              win->_onDraw = [pyfn](ui::drawevent_constptr_t drwev) {
                py::gil_scoped_acquire acquire;
                try {
                  pyfn(drwev);
                } catch (py::error_already_set& e) {
                  ezapp_python_traceback(e);
                  e.restore();
                  PyErr_Print();
                }
              };
            }
          })
      .def_property("onResize",
          [](ezsecondarywin_ptr_t win) -> py::object { return py::none(); },
          [](ezsecondarywin_ptr_t win, py::object callback) {
            if (callback.is_none()) {
              win->_onResize = nullptr;
            } else {
              auto pyfn = py::cast<py::function>(callback);
              win->_onResize = [pyfn](int w, int h) {
                py::gil_scoped_acquire acquire;
                try {
                  pyfn(w, h);
                } catch (py::error_already_set& e) {
                  ezapp_python_traceback(e);
                  e.restore();
                  PyErr_Print();
                }
              };
            }
          })
      .def_property("onUiEvent",
          [](ezsecondarywin_ptr_t win) -> py::object { return py::none(); },
          [](ezsecondarywin_ptr_t win, py::object callback) {
            if (callback.is_none()) {
              win->_onUiEvent = nullptr;
            } else {
              auto pyfn = py::cast<py::function>(callback);
              win->_onUiEvent = [pyfn](ui::event_constptr_t ev) -> ui::HandlerResult {
                py::gil_scoped_acquire acquire;
                try {
                  return pyfn(ev).cast<ui::HandlerResult>();
                } catch (py::error_already_set& e) {
                  ezapp_python_traceback(e);
                  e.restore();
                  PyErr_Print();
                  return ui::HandlerResult();
                }
              };
            }
          })
      .def_property("onGpuInit",
          [](ezsecondarywin_ptr_t win) -> py::object { return py::none(); },
          [](ezsecondarywin_ptr_t win, py::object callback) {
            if (callback.is_none()) {
              win->_onGpuInit = nullptr;
            } else {
              auto pyfn = py::cast<py::function>(callback);
              win->_onGpuInit = [pyfn](Context* ctx) {
                py::gil_scoped_acquire acquire;
                try {
                  pyfn(ctx_t(ctx));
                } catch (py::error_already_set& e) {
                  ezapp_python_traceback(e);
                  e.restore();
                  PyErr_Print();
                }
              };
            }
          })
      .def_property("onClosed",
          [](ezsecondarywin_ptr_t win) -> py::object { return py::none(); },
          [](ezsecondarywin_ptr_t win, py::object callback) {
            if (callback.is_none()) {
              win->_onClosed = nullptr;
            } else {
              auto pyfn = py::cast<py::function>(callback);
              win->_onClosed = [pyfn]() {
                py::gil_scoped_acquire acquire;
                try {
                  pyfn();
                } catch (py::error_already_set& e) {
                  ezapp_python_traceback(e);
                  e.restore();
                  PyErr_Print();
                }
              };
            }
          });
  type_codec->registerStdCodec<ezsecondarywin_ptr_t>(ezsecwin_type);
  /////////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2

} // namespace ork::lev2
