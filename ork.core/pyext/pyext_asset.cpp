///////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "pyext.h"
#include <ork/asset/Asset.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork {
using namespace asset;
///////////////////////////////////////////////////////////////////////////////
void pyinit_asset(py::module& module_core) {
  auto amodule  = module_core.def_submodule("asset", "core asset operations");
  auto type_codec = python::TypeCodec::instance();
  /////////////////////////////////////////////////////////////////////////////////
  amodule.def("enqueueLoad", [type_codec](const std::string& apath,py::object vars) -> loadrequest_ptr_t {
    varmap::varmap_ptr_t as_varmap = nullptr;
    // check if vars is a dictionary
    if (py::isinstance<py::dict>(vars)) {
      as_varmap = std::make_shared<varmap::VarMap>();
      auto py_dict = vars.cast<py::dict>();
      for (auto item : py_dict) {
        auto key = item.first.cast<std::string>();
        auto value = type_codec->encode(item.second);
        as_varmap->setValueForKey(key,value);
      }
    }
    else{
      as_varmap = vars.cast<varmap::varmap_ptr_t>();
    }
    auto loadreq = std::make_shared<LoadRequest>(apath,as_varmap);
    return loadreq;
  });
  /////////////////////////////////////////////////////////////////////////////////
  auto aset_type = py::class_<Asset,asset_ptr_t>(amodule, "Asset");
  type_codec->registerStdCodec<asset_ptr_t>(aset_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto lreq_type = py::class_<LoadRequest,loadrequest_ptr_t>(amodule, "LoadRequest")
  .def(py::init([](const std::string& apath) -> loadrequest_ptr_t {
    return std::make_shared<LoadRequest>(apath);
  }))
  .def("waitForCompletion", &LoadRequest::waitForCompletion)
  .def("enqueueAsync", [](loadrequest_ptr_t lreq,py::function py_on_complete) {
    lreq->_asset_vars->makeValueForKey<py::function>("completion_handler") = py_on_complete;
    auto cpp_on_complete = ([=]() { //
      py::gil_scoped_acquire acquire;
      auto pyfn = lreq->_asset_vars->typedValueForKey<py::function>("completion_handler");
      pyfn.value()();
    });
    lreq->enqueueAsync(cpp_on_complete);
  });
  type_codec->registerStdCodec<loadrequest_ptr_t>(lreq_type);
}
}
///////////////////////////////////////////////////////////////////////////////
