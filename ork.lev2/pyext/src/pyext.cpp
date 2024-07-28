////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "pyext.h"
#include <ork/kernel/environment.h>
#include <ork/lev2/ui/ged/ged_test_objects.h>

///////////////////////////////////////////////////////////////////////////////

namespace ork::audio::singularity{
  void pyinit_aud_singularity(py::module& module_lev2);
}

namespace ork::lev2 {

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
void pyinit_primitives(py::module& module_lev2);
void pyinit_scenegraph(py::module& module_lev2);
void pyinit_meshutil(py::module& module_lev2);
void pyinit_gfx_xgmmodel(py::module& module_lev2);
void pyinit_gfx_xgmanim(py::module& module_lev2);
void pyinit_ui(py::module& module_lev2);
void pyinit_gfx_pbr(py::module& module_lev2);
void pyinit_midi(py::module& module_lev2);
void pyinit_gfx_camera(py::module& module_lev2);
void pyinit_vr(py::module& module_lev2);
void pyinit_editor(py::module& module_lev2);
void ClassInit();
void GfxInit(const std::string& gfxlayer);


extern context_ptr_t gloadercontext;

} // namespace ork::lev2


////////////////////////////////////////////////////////////////////////////////


orkezapp_ptr_t lev2appinit() {

  ork::SetCurrentThreadName("main");

  
  ork::genviron.init_from_global_env();

  static std::shared_ptr<lev2::ThreadGfxContext> _gthreadgfxctx;
  static std::vector<std::string> _dynaargs_storage;
  static std::vector<char*> _dynaargs_refs;

  py::object python_exec = py::module_::import("sys").attr("executable");
  py::object argv_list = py::module_::import("sys").attr("argv");

  auto exec_as_str = py::cast<std::string>(python_exec);
  //printf( "exec_as_str<%s>\n", exec_as_str.c_str() );

  _dynaargs_storage.push_back(exec_as_str);

  for (auto item : argv_list) {
    auto as_str = py::cast<std::string>(item);
    _dynaargs_storage.push_back(as_str);
    //printf( "as_str<%s>\n", as_str.c_str() );
  }
  //OrkAssert(false);

  for( std::string& item : _dynaargs_storage ){
    char* ref = item.data();
    _dynaargs_refs.push_back(ref);
  }

  int argc      = _dynaargs_refs.size();
  char** argv = _dynaargs_refs.data();

  //for( int i=0; i<argc; i++ ){
    //printf( "dynarg<%d:%s>\n", i, argv[i] );
  //}

  static auto init_data = std::make_shared<AppInitData>(argc,argv);

  auto po_opts = OrkEzApp::createDefaultOptions( init_data, "python-ork-app");
  init_data->_offscreen = true;
  auto vars = *init_data->parse();
  auto ezapp = OrkEzApp::create(init_data);


  lev2::ClassInit();
  rtti::Class::InitializeClasses();
  lev2::GfxInit("");
  lev2::FontMan::GetRef();

  _gthreadgfxctx = std::make_shared<lev2::ThreadGfxContext>(gloadercontext.get());

  lev2::gloadercontext->makeCurrentContext();

  return ezapp;
}

////////////////////////////////////////////////////////////////////////////////

void lev2apppoll() {
  while (ork::opq::mainSerialQueue()->Process()) {
  }
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

PYBIND11_MODULE(_lev2, module_lev2) {
  // module_lev2.attr("__name__") = "lev2";

  //////////////////////////////////////////////////////////////////////////////
  module_lev2.doc() = "Orkid Lev2 Library (graphics,audio,vr,input,etc..)";
  //////////////////////////////////////////////////////////////////////////////
  module_lev2.def("lev2appinit", &lev2appinit);
  module_lev2.def("lev2apppoll", &lev2apppoll);
  module_lev2.def("lev2exdir", &lev2exdir);
  //////////////////////////////////////////////////////////////////////////////
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
  pyinit_gfx_material(module_lev2);
  pyinit_gfx_shader(module_lev2);
  pyinit_gfx_renderer(module_lev2);
  pyinit_gfx_lighting(module_lev2);
  pyinit_gfx_qtez(module_lev2);
  pyinit_gfx_particles(module_lev2);
  pyinit_gfx_xgmmodel(module_lev2);
  pyinit_gfx_xgmanim(module_lev2);
  pyinit_gfx_pbr(module_lev2);
  pyinit_gfx_image(module_lev2);
  pyinit_vr(module_lev2);
  pyinit_editor(module_lev2);
  ::ork::audio::singularity::pyinit_aud_singularity(module_lev2);
  //////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////////
  auto type_codec = python::pb11_typecodec_t::instance();
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
