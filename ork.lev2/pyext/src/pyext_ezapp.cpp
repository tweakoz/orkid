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
                if (key == "name") {
                  appinitdata->_application_name = py::cast<std::string>(item.second);
                } else if (key == "left") {
                  appinitdata->_left = py::cast<int>(item.second);
                } else if (key == "top") {
                  appinitdata->_top = py::cast<int>(item.second);
                } else if (key == "width") {
                  appinitdata->_width = py::cast<int>(item.second);
                } else if (key == "height") {
                  appinitdata->_height = py::cast<int>(item.second);
                } else if (key == "fullscreen") {
                  appinitdata->_fullscreen = py::cast<bool>(item.second);
                } else if (key == "fullscreen_monitor") {
                  appinitdata->_fullscreen_monitor = py::cast<std::string>(item.second);
                } else if (key == "enable_always_on_top") {
                  appinitdata->_canalwaysontop =  py::cast<bool>(item.second);
                } else if (key == "enable_graphics") {
                  appinitdata->_enable_graphics = py::cast<bool>(item.second);
                  //printf("enable_graphics<%d>\n", appinitdata->_enable_graphics);
                } else if (key == "enable_audio") {
                  appinitdata->_enable_audio = py::cast<bool>(item.second);
                } else if (key == "enable_audio_input") {
                  appinitdata->_enable_audio_input = py::cast<bool>(item.second);
                } else if (key == "enable_audio_output") {
                  appinitdata->_enable_audio_output = py::cast<bool>(item.second);
                } else if (key == "enable_audio_synth") {
                  appinitdata->_enable_audio_synth = py::cast<bool>(item.second);
                } else if (key == "audio_input_devname") {
                  appinitdata->_audio_input_devname = py::cast<std::string>(item.second);; // cant have synth without an audio dev output !
                } else if (key == "audio_output_devname") {
                  appinitdata->_audio_output_devname = py::cast<std::string>(item.second);; // cant have synth without an audio dev output !
                } else if (key == "audio_input_numchannels") {
                  appinitdata->_audio_input_numchannels = py::cast<int>(item.second);; // cant have synth without an audio dev output !
                } else if (key == "audio_output_numchannels") {
                  appinitdata->_audio_output_numchannels = py::cast<int>(item.second);; // cant have synth without an audio dev output !
                } else if (key == "audio_stream_sync") {
                  appinitdata->_audio_stream_sync = py::cast<bool>(item.second);; // cant have synth without an audio dev output !
                } else if (key == "freerun") {
                  appinitdata->_freerunning = py::cast<bool>(item.second);
                } else if (key == "target_ups") {
                  appinitdata->_target_ups = py::cast<float>(item.second);
                } else if (key == "target_fps") {
                  appinitdata->_target_fps = py::cast<float>(item.second);
                } else if (key == "offscreen") {
                  appinitdata->_offscreen = py::cast<bool>(item.second);
                } else if (key == "ssaa") {
                  appinitdata->_ssaa_samples = py::cast<int>(item.second);
                } else if (key == "disable_mouse_cursor") {
                  appinitdata->_disableMouseCursor = py::cast<bool>(item.second);
                } else if (key == "msaa") {
                  appinitdata->_msaa_samples = py::cast<int>(item.second);
                } else if( key == "rcfd" ) {
                  override_rcfd = py::cast<rcfd_ptr_t>(item.second);
                } else if( key == "movie_output_path" ) {
                  if( py::isinstance<py::str>( item.second ) ) {
                    std::string mpath = py::cast<std::string>(item.second);
                    appinitdata->_movie_output_path = file::Path(mpath);
                  }
                } else if (key == "enable_freerun_ups") {
                  appinitdata->_log_freerun_ups = py::cast<bool>(item.second);
                } else if (key == "enable_freerun_fps") {
                  appinitdata->_log_freerun_fps = py::cast<bool>(item.second);
                } else if (key == "enable_lockstep_ups") {
                  appinitdata->_log_lockstep_ups = py::cast<bool>(item.second);
                } else if (key == "enable_lockstep_fps") {
                  appinitdata->_log_lockstep_fps = py::cast<bool>(item.second);
                }
              } // for (auto item : kwargs) {
              //////////////////////////////////////
              // ensure flags make sense
              //////////////////////////////////////
              if(not appinitdata->_freerunning){
                if( not appinitdata->_offscreen ){
                  // if we are lockstep, force offscreen mode
                  appinitdata->_offscreen = true;
                  logchan_EZAPP->log("forcing offscreen mode for lockstep operation");
                }
                ork::genviron.set("ORKID_AUDIO_IOCLASS", "STREAM");
                appinitdata->_audio_stream_sync = true;
                logchan_EZAPP->log("forcing ORKID_AUDIO_IOCLASS to STREAM for lockstep operation");
              }
              if(appinitdata->_audio_stream_sync ){
                ork::genviron.set("ORKID_AUDIO_IOCLASS", "STREAM");
                appinitdata->_audio_ioclass = "STREAM";
              }
              if(appinitdata->_enable_audio_synth){
                // if we have synth enabled, we need audio output
                appinitdata->_enable_audio_output = true;
              }
              if(appinitdata->_enable_audio_output){
                // if we have audio output, we need audio enabled
                appinitdata->_enable_audio = true;
              }
              if(appinitdata->_enable_audio_input){
                // if we have audio input, we need audio enabled
                appinitdata->_enable_audio = true;
              }
              //////////////////////////////////////

            } // if (kwargs) {
            /////////////////////////////
            ::ork::lev2::initModule(appinitdata);
            logchan_EZAPP->log("finalizeInitialization begin..");
            fflush(stdout);
            appinitdata->finalizeInitialization();
            logchan_EZAPP->log("finalizeInitialization done..");
            fflush(stdout);
            /////////////////////////////
            auto rval                                                 = OrkEzApp::create(appinitdata);
            auto d_ev                                                 = std::make_shared<ui::DrawEvent>(nullptr);
            logchan_EZAPP->log("ezapp<%p>",(void*) rval.get() );
            rval->_vars->makeValueForKey<uidrawevent_ptr_t>("drawev") = d_ev;
            rval->_vars->makeValueForKey<py::object>("appinstance")   = appinstance;
            rval->_overrideRCFD = override_rcfd;
            ////////////////////////////////////////////////////////////////////
            if (py::hasattr(appinstance, "onAppInit")) {
              logchan_EZAPP->log("REG onAppInit");
              auto appinitfn //
                  = py::cast<py::function>(appinstance.attr("onAppInit"));
              rval->_vars->makeValueForKey<py::function>("appinitfn") = appinitfn;
              rval->onAppInit([=]() { //
                logchan_EZAPP->log("EXE onAppInit");
                py::gil_scoped_acquire acquire;
                auto pyfn = rval->_vars->typedValueForKey<py::function>("appinitfn");
                auto initdata = appinitdata;
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
              logchan_EZAPP->log("NO onAppInit");
            }
            ////////////////////////////////////////////////////////////////////
            if (py::hasattr(appinstance, "onAppExit")) {
              logchan_EZAPP->log("REG onAppExit");
              auto appexitfn //
                  = py::cast<py::function>(appinstance.attr("onAppExit"));
              rval->_vars->makeValueForKey<py::function>("appexitfn") = appexitfn;
              rval->onAppExit([=]() { //
                logchan_EZAPP->log("EXE onAppExit");
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
              logchan_EZAPP->log("NO onAppExit");
            }
            ////////////////////////////////////////////////////////////////////
            if (py::hasattr(appinstance, "onAudioInit")) {
              logchan_EZAPP->log("REG onAudioInit");
              auto audinitfn //
                  = py::cast<py::function>(appinstance.attr("onAudioInit"));
              rval->_vars->makeValueForKey<py::function>("audinitfn") = audinitfn;
              rval->onAudioInit([=](audiodevice_ptr_t adev) { //
                logchan_EZAPP->log("EXE onAudioInit");
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
              logchan_EZAPP->log("NO onAudioInit");
            }
            ////////////////////////////////////////////////////////////////////
            if (py::hasattr(appinstance, "onAudioExit")) {
              auto audexitfn //
                  = py::cast<py::function>(appinstance.attr("onAudioExit"));
              rval->_vars->makeValueForKey<py::function>("audexitfn") = audexitfn;
              rval->onAudioExit([=](audiodevice_ptr_t adev) { //
                py::gil_scoped_acquire acquire;
                auto pyfn = rval->_vars->typedValueForKey<py::function>("audinitfn");
                try {
                  pyfn.value()(adev);
                } catch (py::error_already_set& e) {
                  ezapp_python_traceback(e);
                  printf( "\n\npython exception in onAudioExit\n\n");
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
            if (py::hasattr(appinstance, "onSynthInit")) {
              logchan_EZAPP->log("REG onSynthInit");
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
                  OrkAssert(false);
                } catch (std::exception& e) {
                  std::cerr << e.what();
                  OrkAssert(false);
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
      .def_property(
          "timescale",
          [](orkezapp_ptr_t app) -> float { return app->_timescale; },
          [](orkezapp_ptr_t app, float val) { app->_timescale = val; })
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
              if( app->_onAppInit ){
                app->_onAppInit();
              }
              auto RES = app->mainThreadLoop();
              if( app->_onAppExit ){
                app->_onAppExit();
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
