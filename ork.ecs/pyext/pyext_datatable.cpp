////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include "pyext.h"
#include <ork/ecs/datatable.h>

///////////////////////////////////////////////////////////////////////////////

namespace ork::ecs {




void pyinit_datatable(py::module& module_ecs) {
  auto type_codec = python::TypeCodec::instance();
  /////////////////////////////////////////////////////////////////////////////////
  // datatable_param_proxy
  /////////////////////////////////////////////////////////////////////////////////
  /*struct datatable_param_proxy {
    DataTable& _table;
  };
  using datatable_param_proxy_ptr_t = std::shared_ptr<datatable_param_proxy>;
  auto datatable_params_type   =                                                               //
      py::class_<datatable_param_proxy, datatable_param_proxy_ptr_t>(module_ecs, "DataTableParams") //
          .def(
              "__repr__",
              [](datatable_param_proxy_ptr_t proxy) -> std::string {
                std::string output;
                output += FormatString("datatable_param_proxy<%p>{\n", proxy.get());
                for (auto item : proxy->_table._items) {
                  const auto& k = item.first;
                  const auto& v = item.second;
                  auto vstr     = v.typestr();
                  output += FormatString("  param(%s): valtype(%s),\n", k->_name.c_str(), vstr.c_str());
                }
                output += "}\n";
                return output.c_str();
              })
          .def(
              "__setitem__",                                                                  //
              [type_codec](datatable_param_proxy_ptr_t proxy, py::object key, py::object val) { //
                auto var_key = type_codec->decode(key);
                auto var_val = type_codec->decode(val);
                if (auto as_param = var_key.tryAs<fxparam_constptr_t>()) {
                  proxy->_table._items[as_param.value()] = var_val;
                } else {
                  OrkAssert(false);
                }
              })
          .def(
              "__getitem__",                                                                //
              [type_codec](datatable_param_proxy_ptr_t proxy, py::object key) -> py::object { //
                auto var_key = type_codec->decode(key);
                if (auto as_param = var_key.tryAs<fxparam_constptr_t>()) {
                  auto it = proxy->_table._items.find(as_param.value());
                  if (it != proxy->_table._items.end()) {
                    auto var_val = it->second;
                    return type_codec->encode(var_val);
                  }
                } else {
                  OrkAssert(false);
                }
                return py::none();
              });

  type_codec->registerStdCodec<datatable_param_proxy_ptr_t>(datatable_params_type);*/
  /////////////////////////////////////////////////////////////////////////////////
  auto d_type = py::class_<DataTable>(module_ecs, "DataTable")
      .def(py::init<>())
      .def(py::init<>([type_codec](py::dict dict) -> DataTable {
        DataTable dtab;
        DataKey dkey;
        for (auto item : dict) {
          auto key = py::cast<crcstring_ptr_t>(item.first);
          auto val = py::reinterpret_borrow<py::object>(item.second);
          auto var_val = type_codec->decode64(val);
          dkey._encoded = *key;
          dtab[dkey] = var_val;
        }
        return dtab;
      }))
      .def("set",[type_codec](DataTable& dtab, crcstring_ptr_t key, py::object val) {
        auto var_val = type_codec->decode64(val);
        DataKey dkey;
        dkey._encoded = (*key);           
        dtab[dkey] = var_val;
      })
      .def("__getitem__", [type_codec](DataTable& dtab, crcstring_ptr_t key) -> py::object {
        DataKey dkey;
        dkey._encoded = (*key);
        auto var_val = dtab[dkey];
        return py::none();//type_codec->encode(var_val);
      })
      .def(
          "__setitem__",
          [type_codec](DataTable& dtab, py::object key, py::object val) {
            auto var_key = type_codec->decode64(key);
            auto var_val = type_codec->decode64(val);
            DataKey dkey;
            if (auto as_int = var_key.tryAs<int>()) {   
              dkey._encoded = var_key;           
              dtab[dkey] = var_val;
            } else if (auto as_float = var_key.tryAs<float>()) {
              dkey._encoded = var_key;           
              dtab[dkey] = var_val;
            } else if (auto as_str = var_key.tryAs<std::string>()) {
              dkey._encoded = var_key;           
              dtab[dkey] = var_val;
            } else {
              OrkAssert(false);
            }
      })
      .def(
          "__repr__",
          [](const DataTable& dtab) -> std::string {
            fxstring<256> fxs;
            fxs.format("DataTable(%p)", & dtab );
            return fxs.c_str();
          })
      .def("dump", [type_codec](const DataTable& dtab) -> std::string {
        fxstring<256> fxs;
        fxs.format("{\n", & dtab );
        for (auto item : dtab._items) {
          const auto& k = item._key;
          const auto& v = item._val;
          //auto conv_k = type_codec->encode(k._encoded);
          //auto conv_v = type_codec->encode(v._encoded);
          //auto pystr_k = k.get<crcstring_ptr_t>();
          //auto pystr_v = py::str(conv_v);
          //auto k_cstr = pystr_k.cast<std::string>().c_str();
          //auto v_cstr = pystr_v.cast<std::string>().c_str();
          //auto v_cstr = v.typestr();
          //fxs.format("  \"%s\": %s,\n", k_cstr, v_cstr);
        }
        fxs.format("}\n");
        return fxs.c_str();
      });
  type_codec->registerStdCodec<DataTable>(d_type);
  /////////////////////////////////////////////////////////////////////////////////
} // void pyinit_entity(py::module& module_ecs) {
/////////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2 {