////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "pyext.h"
#include <boost/filesystem.hpp>
#include <ork/kernel/environment.h>
#include <ork/event/Event.h>
#include <ork/kernel/datablock.h>
#include <ork/kernel/datacache.h>

///////////////////////////////////////////////////////////////////////////////
struct CorePythonApplication {

  CorePythonApplication() {
    _stringpoolctx = std::make_shared<ork::StringPoolContext>();
    StringPoolStack::push(_stringpoolctx);
  }
  ~CorePythonApplication() {
    StringPoolStack::pop();
  }
  stringpoolctx_ptr_t _stringpoolctx;
};
///////////////////////////////////////////////////////////////////////////////
namespace ork {
void pyinit_math(py::module& module_core);
void pyinit_dataflow(py::module& module_core);
void pyinit_datablock(py::module& module_core);
void pyinit_asset(py::module& module_core);

static void _coreappinit() {
  SetCurrentThreadName("main");
  ork::genviron.init_from_global_env();

  static std::vector<std::string> _dynaargs_storage;
  static std::vector<char*> _dynaargs_refs;

  py::object python_exec = py::module_::import("sys").attr("executable");
  py::object argv_list   = py::module_::import("sys").attr("argv");

  auto exec_as_str = py::cast<std::string>(python_exec);
  // printf( "exec_as_str<%s>\n", exec_as_str.c_str() );

  _dynaargs_storage.push_back(exec_as_str);

  for (auto item : argv_list) {
    auto as_str = py::cast<std::string>(item);
    _dynaargs_storage.push_back(as_str);
    // printf( "as_str<%s>\n", as_str.c_str() );
  }
  // OrkAssert(false);

  for (std::string& item : _dynaargs_storage) {
    char* ref = item.data();
    _dynaargs_refs.push_back(ref);
  }

  int argc    = _dynaargs_refs.size();
  char** argv = _dynaargs_refs.data();

  for (int i = 0; i < argc; i++) {
    printf("dynarg<%d:%s>\n", i, argv[i]);
  }

  static auto init_data = std::make_shared<AppInitData>(argc, argv);

  static CorePythonApplication the_app;

  static auto WorkingDirContext = std::make_shared<FileDevContext>();
  OldSchool::SetGlobalPathVariable("data://", file::Path::orkroot_dir());

  rtti::Class::InitializeClasses();
}

static file::Path _thispath() {

  auto inspect    = py::module::import("inspect");
  auto os         = py::module::import("os");
  auto path       = os.attr("path");
  auto fn_abspath = path.attr("abspath");
  auto fn_stack   = py::cast<py::function>(inspect.attr("stack"));

  auto the_stack    = py::cast<py::list>(fn_stack());
  auto i0           = the_stack[0];
  auto i1           = py::cast<py::list>(i0)[1];
  auto abs_path     = fn_abspath(i1);
  auto abs_path_str = py::cast<std::string>(abs_path);

  return file::Path(abs_path_str);
}

static file::Path _thisdir() {

  auto inspect    = py::module::import("inspect");
  auto os         = py::module::import("os");
  auto path       = os.attr("path");
  auto fn_dirname = path.attr("dirname");
  auto fn_abspath = path.attr("abspath");
  auto fn_stack   = py::cast<py::function>(inspect.attr("stack"));

  auto the_stack    = py::cast<py::list>(fn_stack());
  auto i0           = the_stack[0];
  auto i1           = py::cast<py::list>(i0)[1];
  auto abs_path     = fn_abspath(i1);
  auto directory    = fn_dirname(abs_path);
  auto abs_path_str = py::cast<std::string>(directory);
  return file::Path(abs_path_str);
}
static file::Path _orkdir() {
  std::string ork_dir;
  genviron.get("ORKID_WORKSPACE_DIR",ork_dir);
  return file::Path(ork_dir);
}
static file::Path _lev2dir() {
  return _orkdir()/"ork.lev2";
}
static file::Path _lev2pyexdir() {
  static file::Path rval = _lev2dir()/"examples"/"python";
  return rval;
}

static PoolString _addpoolstring(std::string str) {
  return AddPooledString(str.c_str());
}

///////////////////////////////////////////////////////////////////////////////

void pyinit_reflection(py::module& module_core);

PYBIND11_MODULE(_core, module_core) {
  module_core.doc() = "Orkid Core Library (math,kernel,reflection,ect..)";
  /////////////////////////////////////////////////////////////////////////////////
  module_core.def("coreappinit", &_coreappinit);
  module_core.def("thispath", &_thispath);
  module_core.def("thisdir", &_thisdir);
  module_core.def("orkdir", &_orkdir);
  module_core.def("lev2dir", &_lev2dir);
  module_core.def("lev2pyexdir", &_lev2pyexdir);
  module_core.def("addpoolstring", &_addpoolstring);

  /////////////////////////////////////////////////////////////////////////////////
  // core decoder tyoes
  /////////////////////////////////////////////////////////////////////////////////
  auto type_codec = python::TypeCodec::instance();
  /////////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////////
  using coreapp_ptr_t   = std::shared_ptr<CorePythonApplication>;
  auto application_type = py::class_<CorePythonApplication, coreapp_ptr_t>(module_core, "Application")
                              .def("__repr__", [](coreapp_ptr_t app) -> std::string {
                                fxstring<256> fxs;
                                fxs.format("OrkPyCoreApp(%p)", (void*)app.get());
                                return fxs.c_str();
                              });
  type_codec->registerStdCodec<coreapp_ptr_t>(application_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto crcstr_type =                                                   //
      py::class_<CrcString, crcstring_ptr_t>(module_core, "CrcString") //
          .def(py::init<>([](std::string str) -> crcstring_ptr_t { return std::make_shared<CrcString>(str.c_str()); }))
          .def_property_readonly(
              "hashed",
              [](crcstring_ptr_t s) -> uint64_t { //
                return s->hashed();
              })
          .def_property_readonly(
              "hashedi",
              [](crcstring_ptr_t s) -> int { //
                return int(s->hashed());
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
  using crc64_ctx_t     = boost::Crc64;
  using crc64_ctx_ptr_t = std::shared_ptr<crc64_ctx_t>;
  auto crc64_type       =                                                   //
      py::class_<crc64_ctx_t, crc64_ctx_ptr_t>(module_core, "Crc64Context") //
          .def(py::init<>())
          .def("begin", [](crc64_ctx_ptr_t ctx) { ctx->init(); })
          .def("finish", [](crc64_ctx_ptr_t ctx) { ctx->finish(); })
          .def(
              "accum",
              [](crc64_ctx_ptr_t ctx, py::object value) {
                if (py::isinstance<py::str>(value)) {
                  ctx->accumulateString(py::cast<std::string>(value));
                } else if (py::isinstance<py::int_>(value)) {
                  ctx->accumulateItem<int>(py::cast<int>(value));
                } else if (py::isinstance<py::float_>(value)) {
                  ctx->accumulateItem<float>(py::cast<float>(value));
                } else if (py::isinstance<fvec2>(value)) {
                  ctx->accumulateItem<fvec2>(py::cast<fvec2>(value));
                } else if (py::isinstance<fvec3>(value)) {
                  ctx->accumulateItem<fvec3>(py::cast<fvec3>(value));
                } else if (py::isinstance<fvec4>(value)) {
                  ctx->accumulateItem<fvec4>(py::cast<fvec4>(value));
                } else {
                  OrkAssert(false);
                }
              })
          .def_property_readonly("result", [](crc64_ctx_ptr_t ctx) -> uint64_t { return ctx->result(); });
  type_codec->registerStdCodec<crc64_ctx_ptr_t>(crc64_type);
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
      .def_property_readonly(
          "normalized",
          [](const file::Path& a) -> file::Path {
            auto as_bfs = a.toBFS();
            file::Path b;
            b.fromBFS(as_bfs.normalize());
            return b;
          })
      .def_property_readonly("as_string", [](const file::Path& a) -> std::string { return a.c_str(); })
      .def("isAbsolute", &file::Path::isAbsolute)
      .def("IsRelative", &file::Path::isRelative)
      .def("addToSysPath", [](const file::Path& self) {
        auto syspath = py::module::import("sys").attr("path");
        auto str     = self.c_str();
        syspath.attr("append")(str);
      })
      .def("__truediv__", [](const file::Path& a, std::string b) -> file::Path { return (a / b); })
      .def("__str__", [](const file::Path& s) -> std::string { return s.c_str(); })
      .def("__repr__", [](const file::Path& s) -> std::string {
        fxstring<512> fxs;
        fxs.format("Path(%s)", s.c_str());
        return fxs.c_str();
      });
  /////////////////////////////////////////////////////////////////////////////////
  struct VarMapKeyIterator {

    using map_t            = typename varmap::VarMap::map_t;
    using map_const_iter_t = map_t::const_iterator;

    VarMapKeyIterator(varmap::varmap_ptr_t vmap)
        : _vmap(vmap) {
    }

    std::string operator*() const {
      return _it->first;
    }

    VarMapKeyIterator operator++() {
      ++_it;
      return *this;
    }

    bool operator==(const VarMapKeyIterator& other) const {
      return _vmap == other._vmap;
    }

    static VarMapKeyIterator _begin(varmap::varmap_ptr_t vmap) {
      auto it = VarMapKeyIterator(vmap);
      it._it  = vmap->_themap.begin();
      return it;
    }

    static VarMapKeyIterator _end(varmap::varmap_ptr_t vmap) {
      auto it = VarMapKeyIterator(vmap);
      it._it  = vmap->_themap.end();
      return it;
    }

    varmap::varmap_ptr_t _vmap;
    map_const_iter_t _it;
  };
  /////////////////////////////////////////////////////////////////////////////////
  auto varmaptype_t =                                                         //
      py::class_<varmap::VarMap, varmap::varmap_ptr_t>(module_core, "VarMap") //
          .def(py::init<>())
          .def(py::init([](py::dict dict) -> varmap::varmap_ptr_t {
            auto vmap = std::make_shared<varmap::VarMap>();
            for (auto item : dict) {
              auto key = py::cast<std::string>(item.first);
              auto val = item.second;
              vmap->setValueForKey(key, val);
            }
            return vmap;
          }))
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
              })
          .def("__len__", [](varmap::varmap_ptr_t vmap) -> size_t { return vmap->_themap.size(); })
          .def(
              "__iter__",
              [](varmap::varmap_ptr_t vmap) { //
                OrkAssert(false);
                return py::make_iterator( //
                  VarMapKeyIterator::_begin(vmap), //
                  VarMapKeyIterator::_end(vmap));
              },
              py::keep_alive<0, 1>())
          .def("__contains__", [](varmap::varmap_ptr_t vmap, std::string key) { //
            return vmap->_themap.contains(key);
          })
          .def("__getitem__", [type_codec](varmap::varmap_ptr_t vmap, std::string key) -> py::object { //
            auto it = vmap->_themap.find(key);
            if( it == vmap->_themap.end() )
              throw py::key_error("key not found");
            else {
              auto varmap_val = it->second;
              auto python_val = type_codec->encode(varmap_val);
              return python_val;
              }
          })
          .def("keys", [](varmap::varmap_ptr_t vmap) -> py::list {
            py::list rval;
            for( auto item : vmap->_themap ){
              rval.append(item.first);
            }
            return rval;           
          })
          .def("__repr__", [](varmap::varmap_ptr_t vmap) -> std::string {
            std::string rval;
            size_t numkeys = vmap->_themap.size();
            rval = FormatString("VarMap(nkeys:%zu)", numkeys );
            return rval;           
          })
          .def("dumpToString", [](varmap::varmap_ptr_t vmap) -> std::string {
            std::string rval = "varmap: {\n";
            for (const auto& item : vmap->_themap) {
              const auto& key = item.first;
              auto val = vmap->encodeAsString(key);
              //printf( "key<%s> val<%s>\n", key.c_str(), val.dumpToString().c_str() );
              rval += FormatString("  %s: %s\n", key.c_str(), val.c_str());
            }
            rval += "}\n";
            return rval;
          })
          .def("clone", [](varmap::varmap_ptr_t vmap) -> varmap::varmap_ptr_t {
            auto vmap_out = std::make_shared<varmap::VarMap>();
            (*vmap_out) = (*vmap);
            return vmap_out;           
          });
  //.def("__reversed__", [](varmap::varmap_ptr_t vmap) -> Sequence { return s.reversed(); })
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
            return FormatString("updata[abs:%g dt:%g]", updata->_abstime, updata->_dt);
          });
  type_codec->registerStdCodec<ui::updatedata_ptr_t>(updata_type);
  /////////////////////////////////////////////////////////////////////////////////
  pyinit_reflection(module_core);
  pyinit_math(module_core);
  pyinit_dataflow(module_core);
  pyinit_datablock(module_core);
  pyinit_asset(module_core);
  /////////////////////////////////////////////////////////////////////////////////
  auto l2pedir = py::cast(_lev2pyexdir());
  module_core.attr("lev2_pyexdir") = l2pedir;
  /////////////////////////////////////////////////////////////////////////////////
}; // PYBIND11_MODULE(_core, module_core) {

} // namespace ork
