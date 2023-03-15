////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include "pyext.h"
#include <boost/filesystem.hpp>
#include <ork/kernel/environment.h>
#include <ork/event/Event.h>
#include <ork/kernel/datablock.h>

///////////////////////////////////////////////////////////////////////////////
struct CorePythonApplication  {

  CorePythonApplication(){
        _stringpoolctx = std::make_shared<ork::StringPoolContext>();
    StringPoolStack::push(_stringpoolctx);
  }
  ~CorePythonApplication(){
    StringPoolStack::pop();
  }
  stringpoolctx_ptr_t _stringpoolctx;
};
///////////////////////////////////////////////////////////////////////////////
namespace ork {
void pyinit_math(py::module& module_core);
void pyinit_dataflow(py::module& module_core);

static void _coreappinit() {
  SetCurrentThreadName("main");
  ork::genviron.init_from_global_env();

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

  for( int i=0; i<argc; i++ ){
    printf( "dynarg<%d:%s>\n", i, argv[i] );
  }

  static auto init_data = std::make_shared<AppInitData>(argc,argv);

  static CorePythonApplication the_app;

  static auto WorkingDirContext = std::make_shared<FileDevContext>();
  OldSchool::SetGlobalPathVariable("data://", file::Path::orkroot_dir());

  rtti::Class::InitializeClasses();
}

static file::Path _thispath() {

  auto inspect   = py::module::import("inspect");
  auto os   = py::module::import("os");
  auto path = os.attr("path");
  auto fn_abspath = path.attr("abspath");
  auto fn_stack = py::cast<py::function>(inspect.attr("stack"));

  auto the_stack = py::cast<py::list>(fn_stack());
  auto i0 = the_stack[0];
  auto i1 = py::cast<py::list>(i0)[1];
  auto abs_path = fn_abspath(i1);
  auto abs_path_str = py::cast<std::string>(abs_path);

  return file::Path(abs_path_str);
}

static file::Path _thisdir() {

  auto inspect   = py::module::import("inspect");
  auto os   = py::module::import("os");
  auto path = os.attr("path");
  auto fn_dirname = path.attr("dirname");
  auto fn_abspath = path.attr("abspath");
  auto fn_stack = py::cast<py::function>(inspect.attr("stack"));

  auto the_stack = py::cast<py::list>(fn_stack());
  auto i0 = the_stack[0];
  auto i1 = py::cast<py::list>(i0)[1];
  auto abs_path = fn_abspath(i1);
  auto directory = fn_dirname(abs_path);
  auto abs_path_str = py::cast<std::string>(directory);
  return file::Path(abs_path_str);
}

static PoolString _addpoolstring(std::string str) {
  return AddPooledString(str.c_str());
}

///////////////////////////////////////////////////////////////////////////////

PYBIND11_MODULE(_core, module_core) {
  module_core.doc() = "Orkid Core Library (math,kernel,reflection,ect..)";
  /////////////////////////////////////////////////////////////////////////////////
  module_core.def("coreappinit", &_coreappinit);
  module_core.def("thispath", &_thispath);
  module_core.def("thisdir", &_thisdir);
  module_core.def("addpoolstring", &_addpoolstring);
  /////////////////////////////////////////////////////////////////////////////////
  // core decoder tyoes
  /////////////////////////////////////////////////////////////////////////////////
  auto type_codec = python::TypeCodec::instance();
  /////////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////////
  using coreapp_ptr_t = std::shared_ptr<CorePythonApplication>;
  auto application_type = py::class_<CorePythonApplication,coreapp_ptr_t>(module_core, "Application")
      .def(
          "__repr__",
          [](coreapp_ptr_t app) -> std::string {
            fxstring<256> fxs;
            fxs.format("OrkPyCoreApp(%p)", (void*) app.get());
            return fxs.c_str();
          });
  type_codec->registerStdCodec<coreapp_ptr_t>(application_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto crcstr_type =                                                   //
      py::class_<CrcString, crcstring_ptr_t>(module_core, "CrcString") //
          .def(py::init<>([](std::string str)->crcstring_ptr_t{
            return std::make_shared<CrcString>(str.c_str());
          }))
          .def_property_readonly(
              "hashed",
              [](crcstring_ptr_t s) -> uint64_t { //
                return s->hashed();
              })
          .def("__repr__", [](crcstring_ptr_t s) -> std::string {
            fxstring<64> fxs;
            fxs.format("CrcString(0x%zx:%zu)", s->hashed(), s->hashed());
            return fxs.c_str();
          });
  type_codec->registerStdCodec<crcstring_ptr_t>(crcstr_type);
  /////////////////////////////////////////////////////////////////////////////////
  struct CrcStringProxy {};
  using crcstrproxy_ptr_t = std::shared_ptr<CrcStringProxy>;
  auto crcstrproxy_type   =                                                        //
      py::class_<CrcStringProxy, crcstrproxy_ptr_t>(module_core, "CrcStringProxy") //
          .def(py::init<>())
          .def(
              "__getattr__",                                                           //
              [](crcstrproxy_ptr_t proxy, const std::string& key) -> crcstring_ptr_t { //
                return std::make_shared<CrcString>(key.c_str());
              });
  type_codec->registerStdCodec<crcstrproxy_ptr_t>(crcstrproxy_type);
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<PoolString>(module_core, "PoolString") //
      .def("__repr__", [](const PoolString& s) -> std::string {
        fxstring<512> fxs;
        fxs.format("PoolString(%s)", s.c_str());
        return fxs.c_str();
      });
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<file::Path>(module_core, "Path")
      .def(py::init<std::string>())
      .def_property_readonly("normalized",[](const file::Path& a) -> file::Path {
        auto as_bfs =  a.toBFS();
        file::Path b;
        b.fromBFS(as_bfs.normalize());
        return b;
      })
      .def_property_readonly("as_string",[](const file::Path& a) -> std::string {
        return a.c_str();
      })
      .def("isAbsolute", &file::Path::isAbsolute)
      .def("IsRelative", &file::Path::isRelative)
      .def("__truediv__", [](const file::Path& a, std::string b) -> file::Path {
        return (a/b);
      })
      .def("__str__", [](const file::Path& s) -> std::string {
        return s.c_str();
      })
      .def("__repr__", [](const file::Path& s) -> std::string {
        fxstring<512> fxs;
        fxs.format("Path(%s)", s.c_str());
        return fxs.c_str();
      });

  /////////////////////////////////////////////////////////////////////////////////
  auto dblock_type = py::class_<DataBlock,datablock_ptr_t>(module_core, "DataBlock")
      .def(py::init<>())
      .def("addData",[](datablock_ptr_t db, py::buffer data) {
        py::buffer_info info = data.request();
        OrkAssert(info.format == py::format_descriptor<uint8_t>::format());
        auto src = (const uint8_t*) info.ptr;

        printf( "ndim<%d>\n", info.ndim );
        switch( info.ndim ){
          case 3:{
            size_t length = info.shape[0];
            size_t width = info.shape[1];
            size_t depth = info.shape[2];
            printf( "length<%zu>\n", length );
            printf( "width<%zu>\n", width );
            printf( "depth<%zu>\n", depth );
            size_t numbytes = length*width*depth;
            db->addData(src,numbytes);
            break;
          }
          default:
            OrkAssert(false);
        }
      })
      .def_property_readonly("size",[](datablock_ptr_t db) -> size_t {
        return db->length();
      })
      .def_property_readonly("hash",[](datablock_ptr_t db) -> uint64_t {
        return db->hash();
      })
      .def("__str__", [](datablock_ptr_t db) -> std::string {
        fxstring<512> fxs;
        fxs.format("DataBlock(%p) len<%d>", (void*) db.get(), (int) db->length() );
        return fxs.c_str();
      })
      .def("__repr__", [](datablock_ptr_t db) -> std::string {
        fxstring<512> fxs;
        fxs.format("DataBlock(%p)", (void*) db.get());
        return fxs.c_str();
      });
  type_codec->registerStdCodec<datablock_ptr_t>(dblock_type);
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<Object>(module_core, "Object") //
      .def("clazz", [](Object* o) -> std::string {
        auto clazz = rtti::downcast<object::ObjectClass*>(o->GetClass());
        auto name  = clazz->Name();
        return name.c_str();
      });
  /////////////////////////////////////////////////////////////////////////////////
  auto varmaptype_t =                                                         //
      py::class_<varmap::VarMap, varmap::varmap_ptr_t>(module_core, "VarMap") //
          .def(py::init<>())
          .def(
              "__setattr__",                                                                    //
              [type_codec](varmap::varmap_ptr_t vmap, const std::string& key, py::object val) { //
                auto varmap_val = type_codec->decode(val);
                vmap->setValueForKey(key, varmap_val);
              })
          .def(
              "__getattr__",                                                                  //
              [type_codec](varmap::varmap_ptr_t vmap, const std::string& key) -> py::object { //
                auto varmap_val = vmap->valueForKey(key);
                auto python_val = type_codec->encode(varmap_val);
                return python_val;
              });
  type_codec->registerStdCodec<varmap::varmap_ptr_t>(varmaptype_t);
  /////////////////////////////////////////////////////////////////////////////////
  auto updata_type =                                                              //
      py::class_<ui::UpdateData, ui::updatedata_ptr_t>(module_core, "UpdateData") //
          .def(py::init<>())
          .def_property(
              "absolutetime",                             //
              [](ui::updatedata_ptr_t updata) -> double { //
                return updata->_abstime;
              },
              [](ui::updatedata_ptr_t updata, double val) { //
                updata->_abstime = val;
              })
          .def_property(
              "deltatime",                                //
              [](ui::updatedata_ptr_t updata) -> double { //
                return updata->_dt;
              },
              [](ui::updatedata_ptr_t updata, double val) { //
                updata->_dt = val;
              })
          .def("__repr__", [](ui::updatedata_ptr_t updata) -> std::string {
            return FormatString("updata[abs:%g dt:%g]", updata->_abstime, updata->_dt );
          });
  type_codec->registerStdCodec<ui::updatedata_ptr_t>(updata_type);
  /////////////////////////////////////////////////////////////////////////////////
  pyinit_math(module_core);
  pyinit_dataflow(module_core);
  /////////////////////////////////////////////////////////////////////////////////
}; // PYBIND11_MODULE(_core, module_core) {

} // namespace ork
