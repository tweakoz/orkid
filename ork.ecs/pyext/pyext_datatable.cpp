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
  auto type_codec = python::typecodec_t::instance();
/////////////////////////////////////////////////////////////////////////////////
  auto d_type = py::class_<DataTable,datatable_ptr_t>(module_ecs, "DataTable")
      .def(py::init<>())
      .def(py::init<>([type_codec](py::dict dict) -> datatable_ptr_t {
        auto dtab = std::make_shared<DataTable>();
        DataKey dkey;
        for (auto item : dict) {
          auto key = py::cast<crcstring_ptr_t>(item.first);
          auto val = py::reinterpret_borrow<py::object>(item.second);
          auto var_val = type_codec->decode64(val);
          dkey._encoded = *key;
          (*dtab)[dkey] = var_val;
        }
        return dtab;
      }))
      .def("set",[type_codec](datatable_ptr_t dtab, crcstring_ptr_t key, py::object val) {
        auto var_val = type_codec->decode64(val);
        DataKey dkey;
        dkey._encoded = (*key);           
        (*dtab)[dkey] = var_val;
      })
      .def("__getitem__", [type_codec](datatable_ptr_t dtab, crcstring_ptr_t key) -> py::object {
        DataKey dkey;
        dkey._encoded = (*key);
        auto var_val = (*dtab)[dkey];
        return type_codec->encode64(var_val);
      })
      .def(
          "__setitem__",
          [type_codec](datatable_ptr_t dtab, crcstring_ptr_t key, py::object val) {
            auto var_key = (*key); //type_codec->decode64(key);
            auto var_val = type_codec->decode64(val);
            DataKey dkey;
            dkey._encoded = var_key;           
            (*dtab)[dkey] = var_val;
      })
      .def(
          "__repr__",
          [](datatable_ptr_t dtab) -> std::string {
            fxstring<256> fxs;
            fxs.format("DataTable(%p)", & dtab );
            return fxs.c_str();
          })
      .def("dump", [type_codec](datatable_ptr_t dtab) -> std::string {
        fxstring<256> fxs;
        fxs.format("{\n", & dtab );
        for (auto item : dtab->_items) {
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
  type_codec->registerStdCodec<datatable_ptr_t>(d_type);
  /////////////////////////////////////////////////////////////////////////////////
} // void pyinit_entity(py::module& module_ecs) {
/////////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2 {