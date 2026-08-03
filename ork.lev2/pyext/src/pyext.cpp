////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/lev2/config.h>

#include "pyext.h"
#include <ork/kernel/environment.h>
#include <ork/lev2/ui/ged/ged_test_objects.h>


///////////////////////////////////////////////////////////////////////////////

namespace ork {
  void initModule(appinitdata_ptr_t init_data);
  void postInitModule(appinitdata_ptr_t init_data);
}

namespace ork::audio::singularity{
  void pyinit_aud_singularity(py::module& module_lev2);
}

namespace ork::lev2 {

void pyinit_aud_device(py::module& lev2_module);
void pyinit_gfx(py::module& module_lev2);
void pyinit_gfx_compositor(py::module& module_lev2);
void pyinit_gfx_material(py::module& module_lev2);
void pyinit_gfx_drawables(py::module& module_lev2);
void pyinit_gfx_drawabledatas(py::module& module_lev2);
void pyinit_gfx_shader(py::module& module_lev2);
void pyinit_gfx_renderer(py::module& module_lev2);
void pyinit_gfx_lighting(py::module& module_lev2);
void pyinit_gfx_qtez(py::module& module_lev2);
void pyinit_gfx_buffers(py::module& module_lev2);
void pyinit_gfx_particles(py::module& module_lev2);
void pyinit_gfx_image(py::module& module_lev2);
void pyinit_gfx_terrain(py::module& module_lev2);
void pyinit_gfx_hypermesh(py::module& module_lev2);
void pyinit_gfx_dflow(py::module& module_lev2);
void pyinit_gfx_image_renderer(py::module& module_lev2);
void pyinit_gfx_font(py::module& module_lev2);
void pyinit_radiance_maps_processor(py::module& module_lev2);
void pyinit_primitives(py::module& module_lev2);
void pyinit_scenegraph(py::module& module_lev2);
void pyinit_meshutil(py::module& module_lev2);
void pyinit_gfx_xgmmodel(py::module& module_lev2);
void pyinit_gfx_xgmanim(py::module& module_lev2);
void pyinit_ui(py::module& module_lev2);
void pyinit_gfx_pbr(py::module& module_lev2);
void pyinit_midi(py::module& module_lev2);
void pyinit_gfx_camera(py::module& module_lev2);
void pyinit_gfx_openvdb(py::module& module_lev2);
void pyinit_gfx_asset_gen(py::module& module_lev2);
void pyinit_vr(py::module& module_lev2);
void pyinit_movie(py::module& module_lev2);
void pyinit_shmtexture(py::module& module_lev2);
void pyinit_editor(py::module& module_lev2);
void pyinit_drm(py::module& module_lev2);
void ClassInit();
void GfxInit(const std::string& gfxlayer);


extern context_ptr_t gloadercontext;

} // namespace ork::lev2

ork::lev2::orkezapp_ptr_t pylev2appinit(py::kwargs kwargs) {
  py::object python_exec = py::module_::import("sys").attr("executable");
  py::object argv_list = py::module_::import("sys").attr("argv");
  auto exec_as_str = py::cast<std::string>(python_exec);
  //printf( "exec_as_str<%s>\n", exec_as_str.c_str() );
  auto init_data  = std::make_shared<AppInitData>();

  init_data->_dynaargs_storage.push_back(exec_as_str);

  for (auto item : argv_list) {
    auto as_str = py::cast<std::string>(item);
    init_data->_dynaargs_storage.push_back(as_str);
    //printf( "as_str<%s>\n", as_str.c_str() );
  }
  //OrkAssert(false);

  for( std::string& item : init_data->_dynaargs_storage ){
    char* ref = item.data();
    init_data->_dynaargs_refs.push_back(ref);
  }

  int argc      = init_data->_dynaargs_refs.size();
  char** argv = init_data->_dynaargs_refs.data();

  // Process keyword arguments to configure AppInitData

  if (kwargs) {
    for (auto item : kwargs) {
      auto key = py::cast<std::string>(item.first);
      if (key == "left") {
        init_data->_left = py::cast<int>(item.second);
      } else if (key == "top") {
        init_data->_top = py::cast<int>(item.second);
      } else if (key == "width") {
        init_data->_width = py::cast<int>(item.second);
      } else if (key == "height") {
        init_data->_height = py::cast<int>(item.second);
      } else if (key == "fullscreen") {
        init_data->_fullscreen = py::cast<bool>(item.second);
      } else if (key == "fullscreen_mode") {
        auto mode_str = py::cast<std::string>(item.second);
        if (mode_str == "windowed") {
          init_data->_fullscreen_mode = AppInitData::EFullScreenMode::Windowed;
        } else if (mode_str == "immersive") {
          init_data->_fullscreen_mode = AppInitData::EFullScreenMode::Immersive;
        } else {
          throw std::runtime_error(
            "createEzApp: fullscreen_mode must be 'windowed' or 'immersive', got: " + mode_str);
        }
      } else if (key == "fullscreen_monitor") {
        init_data->_fullscreen_monitor = py::cast<std::string>(item.second);
      } else if (key == "hidpi") {
        // Opt into a backing-scaled (Retina) framebuffer. Default is LoDPI to
        // save fillrate; HiDPI must be explicitly requested.
        init_data->_allowHIDPI = py::cast<bool>(item.second);
      } else if (key == "enable_always_on_top") {
        init_data->_canalwaysontop = py::cast<bool>(item.second);
      } else if (key == "enable_graphics") {
        init_data->_enable_graphics = py::cast<bool>(item.second);
      } else if (key == "enable_audio") {
        init_data->_enable_audio = py::cast<bool>(item.second);
      } else if (key == "enable_audio_input") {
        init_data->_enable_audio_input = py::cast<bool>(item.second);
      } else if (key == "enable_audio_output") {
        init_data->_enable_audio_output = py::cast<bool>(item.second);
      } else if (key == "enable_audio_synth") {
        init_data->_enable_audio_synth = py::cast<bool>(item.second);
        init_data->_enable_audio_output = true; // can't have synth without audio output
      } else if (key == "audio_input_devname") {
        init_data->_audio_input_devname = py::cast<std::string>(item.second);
      } else if (key == "audio_output_devname") {
        init_data->_audio_output_devname = py::cast<std::string>(item.second);
      } else if (key == "audio_input_numchannels") {
        init_data->_audio_input_numchannels = py::cast<int>(item.second);
      } else if (key == "audio_output_numchannels") {
        init_data->_audio_output_numchannels = py::cast<int>(item.second);
      } else if (key == "offscreen") {
        init_data->_offscreen = py::cast<bool>(item.second);
      } else if (key == "ssaa") {
        init_data->_ssaa_samples = py::cast<int>(item.second);
      } else if (key == "disableMouseCursor") {
        init_data->_disableMouseCursor = py::cast<bool>(item.second);
      } else if (key == "msaa") {
        init_data->_msaa_samples = py::cast<int>(item.second);
      } else if (key == "use_subsystems") {
        // Matches the OrkEzApp.create kwargs surface (see pyext_ezapp.cpp).
        // Accepts a list of subsystem names ['opq','core','gpu','lev2', ...]
        // and optional custom subsystem objects. Flips on the HFSM init
        // path (_initForSubsystems) and defers GPU init to subsystem
        // state-machine transitions instead of the inline ad-hoc path.
        if (py::isinstance<py::list>(item.second)) {
          auto subsystem_list = py::cast<py::list>(item.second);
          init_data->_use_subsystems = true;
          init_data->_defer_gpu_init = true;
          for (auto entry : subsystem_list) {
            if (py::isinstance<py::str>(entry)) {
              init_data->_enabled_subsystems.insert(py::cast<std::string>(entry));
            } else {
              auto subsystem = py::cast<subsystem_ptr_t>(entry);
              init_data->_custom_subsystems.push_back(subsystem);
              init_data->_enabled_subsystems.insert(subsystem->_name);
            }
          }
        }
      }
    }
  }

  return lev2appinit(init_data);
}

////////////////////////////////////////////////////////////////////////////////

void lev2apppoll() {
  while (ork::opq::mainSerialQueue()->Process()) {
  }
}

void lev2appshutdown() { // TODO fixme
  ork::opq::exit();
}

  ////////////////////////////////////////////////////////////////////////////////

static file::Path lev2exdir() {
  std::string base;
  bool OK = genviron.get("ORKID_LEV2_EXAMPLES_DIR",base);
  OrkAssert(OK);
  return file::Path(base);
}

////////////////////////////////////////////////////////////////////////////////

namespace ork {

// mod_gil_not_used: see the note over PYBIND11_MODULE(_core) — an undeclared
// extension makes CPython 3.14t silently re-enable the GIL at import time.
PYBIND11_MODULE(_lev2, module_lev2, py::mod_gil_not_used()) {
  // module_lev2.attr("__name__") = "lev2";

  //////////////////////////////////////////////////////////////////////////////
  // Force orkengine.core to load FIRST so its pybind11 type registry
  // (fvec4, fmtx4, etc.) is populated before any of lev2's bindings below
  // try to convert default-arg values like `fvec4(1,1,1,1)` into Python
  // objects. Pybind11 type sharing across modules requires the dependency
  // module to be initialized before the dependent module's bindings run —
  // doing it here at the C++/PYBIND11_MODULE level removes the need for
  // every Python caller to remember to import orkengine.core first.
  py::module_::import("orkengine.core");
  //////////////////////////////////////////////////////////////////////////////
  module_lev2.doc() = "Orkid Lev2 Library (graphics,audio,vr,input,etc..)";
  //////////////////////////////////////////////////////////////////////////////
  module_lev2.def("lev2appinit", &pylev2appinit);
  module_lev2.def("lev2apppoll", &lev2apppoll);
  module_lev2.def("lev2exdir", &lev2exdir);
  module_lev2.def("shutdownApp", &lev2appshutdown);
  
  //////////////////////////////////////////////////////////////////////////////
  pyinit_aud_device(module_lev2);
  pyinit_ui(module_lev2);
  pyinit_gfx(module_lev2);
  pyinit_meshutil(module_lev2);
  pyinit_midi(module_lev2);
  pyinit_primitives(module_lev2);
  pyinit_scenegraph(module_lev2);
  pyinit_gfx_buffers(module_lev2);
  pyinit_gfx_camera(module_lev2);
  pyinit_gfx_compositor(module_lev2);
  pyinit_gfx_drawabledatas(module_lev2);
  pyinit_gfx_drawables(module_lev2);
  pyinit_gfx_shader(module_lev2);  // shader before material (material uses shader types)
  pyinit_gfx_material(module_lev2);
  pyinit_gfx_renderer(module_lev2);
  pyinit_gfx_lighting(module_lev2);
  pyinit_gfx_qtez(module_lev2);
  pyinit_gfx_particles(module_lev2);
  pyinit_gfx_xgmmodel(module_lev2);
  pyinit_gfx_xgmanim(module_lev2);
  pyinit_gfx_pbr(module_lev2);
  pyinit_gfx_image(module_lev2);
  pyinit_gfx_terrain(module_lev2);
  pyinit_gfx_hypermesh(module_lev2);
  pyinit_gfx_dflow(module_lev2);
  pyinit_gfx_image_renderer(module_lev2);
  pyinit_gfx_font(module_lev2);
  pyinit_radiance_maps_processor(module_lev2);
  pyinit_gfx_openvdb(module_lev2);
  pyinit_gfx_asset_gen(module_lev2);
  pyinit_vr(module_lev2);
  pyinit_movie(module_lev2);
  pyinit_shmtexture(module_lev2);
  pyinit_editor(module_lev2);
  pyinit_drm(module_lev2);
  ::ork::audio::singularity::pyinit_aud_singularity(module_lev2);
  //////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////////
  auto type_codec = python::pb11_typecodec_t::instance();
  /////////////////////////////////////////////////////////////////////////////////
  using namespace lev2::ged;
  auto gedto_type =                                                              //
      py::class_<TestObject,Object,testobject_ptr_t>(module_lev2, "GedTestObject") //
          .def(py::init<>())
          .def("createCurve", [](testobject_ptr_t to, std::string objname) -> multicurve1d_ptr_t {
            auto curve = std::make_shared<MultiCurve1D>();
            to->_curves.AddSorted(objname,curve);
            return curve;
          })
          .def("createGradient", [](testobject_ptr_t to, std::string objname) -> gradient_fvec4_ptr_t {
            auto gradient = std::make_shared<gradient_fvec4_t>();
            to->_gradients.AddSorted(objname,gradient);
            return gradient;
          })
          .def("createParticleSystem", [](testobject_ptr_t to, std::string objname) -> dataflow::graphdata_ptr_t {
            auto graphdata = std::make_shared<dataflow::GraphData>();
            to->_particlesystems.AddSorted(objname,graphdata);
            return graphdata;
          })
          .def("setParticleSystem", [](testobject_ptr_t to, std::string objname, dataflow::graphdata_ptr_t graphdata) {
            to->_particlesystems.AddSorted(objname,graphdata);
          });
  type_codec->registerStdCodec<testobject_ptr_t>(gedto_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto gedtocfg_type =                                                              //
    py::class_<TestObjectConfiguration,Object,testobjectconfiguration_ptr_t>(module_lev2, "GedTestObjectConfiguration") //
        .def(py::init<>())
        .def("createTestObject", [](testobjectconfiguration_ptr_t toc, std::string objname) -> testobject_ptr_t {
          auto to = std::make_shared<TestObject>();
          toc->_testobjects.AddSorted(objname,to);
          return to;
        });
  type_codec->registerStdCodec<testobjectconfiguration_ptr_t>(gedtocfg_type);
  //////////////////////////////////////////////////////////////////////////////

  
};

} // namespace ork
