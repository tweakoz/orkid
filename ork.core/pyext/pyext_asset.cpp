///////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "pyext.h"
#include <ork/asset/Asset.h>
#include <ork/asset/AssetLoader.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork {
using namespace asset;
///////////////////////////////////////////////////////////////////////////////
void pyinit_asset(py::module& module_core) {
  auto amodule  = module_core.def_submodule("asset", "core asset operations");
  auto type_codec = python::pb11_typecodec_t::instance();
  /////////////////////////////////////////////////////////////////////////////////
  amodule.def("enqueueLoad", [type_codec](py::kwargs _kwargs) -> loadrequest_ptr_t {

    //const std::string& apath,py::object vars,py::function py_on_event
    varmap::varmap_ptr_t as_varmap = nullptr;

    py::dict vars;
    varmap::VarMap varmap;
    py::function py_on_event;
    std::string apath;
    for (auto item : _kwargs) {
      auto key = item.first.cast<std::string>();
      if (key == "path") {
        apath = item.second.cast<std::string>();
      }
      else if (key == "vars") {
        if(py::isinstance<py::dict>(item.second)){
          vars = item.second.cast<py::dict>();
        }
        else if(isinstance<varmap::VarMap>(item.second)){
        }
      }
      else if (key == "onEvent") {
        py_on_event = item.second.cast<py::function>();
      }
    }
    OrkAssert(apath.size() > 0);

    // check if vars is a dictionary
    if (py::isinstance<py::dict>(vars)) {
      as_varmap = std::make_shared<varmap::VarMap>();
      auto py_dict = vars.cast<py::dict>();
      for (auto item : py_dict) {
        auto key = item.first.cast<std::string>();
        printf( "key<%s>\n", key.c_str());
        auto as_pyobj = py::cast<py::object>(item.second);
        auto value = type_codec->decode(as_pyobj);
        as_varmap->setValueForKey(key,value);
      }
    }
    else if (py::isinstance<varmap::VarMap>(vars)) {
      as_varmap = vars.cast<varmap::varmap_ptr_t>();
    }
    auto loadreq = std::make_shared<LoadRequest>(apath,as_varmap);
    if(py_on_event){
      loadreq->_asset_vars->makeValueForKey<py::function>("_event_handler") = py_on_event;
      auto cpp_on_event = ([=](uint32_t event,varmap::var_t value) { //
        py::gil_scoped_acquire acquire;
        auto pyfn = loadreq->_asset_vars->typedValueForKey<py::function>("_event_handler");
        auto encoded = type_codec->encode(value);
        pyfn.value()(loadreq,event,encoded);
      });
      loadreq->_on_event = cpp_on_event;
    }

    assetloader_ptr_t loader;
    AssetLoader::_loaders_by_ext.atomicOp([&](AssetLoader::loader_by_ext_map_t& unlocked) {
      //unlocked[extension] = loader;
      file::Path as_path(apath);
      auto ext = as_path.getExtension();
      auto it = unlocked.find(ext.c_str());
      OrkAssert(it != unlocked.end());
      loader = it->second;
    });
    OrkAssert(loader);
    loader->load(loadreq);
    return loadreq;
  });
  /////////////////////////////////////////////////////////////////////////////////
  auto aset_type = py::class_<Asset,asset_ptr_t>(amodule, "Asset");
  type_codec->registerStdCodec<asset_ptr_t>(aset_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto lreq_type = py::class_<LoadRequest,loadrequest_ptr_t>(amodule, "LoadRequest")
  .def("waitForCompletion", &LoadRequest::waitForCompletion)
  .def_property_readonly("assetPath", [](loadrequest_ptr_t lreq) -> std::string { return lreq->_asset_path.c_str(); });
  type_codec->registerStdCodec<loadrequest_ptr_t>(lreq_type);
}
}
///////////////////////////////////////////////////////////////////////////////
