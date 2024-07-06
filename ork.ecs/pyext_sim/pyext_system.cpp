////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include "pyext.h"
#include <ork/ecs/simulation.inl>
#include <ork/ecs/system.h>
#include <ork/ecs/datatable.h>

///////////////////////////////////////////////////////////////////////////////

namespace ork::ecssim {
void register_system(py::module& module_ecssim,python::typecodec_ptr_t type_codec) {
  /////////////////////////////////////////////////////////////////////////////////
  auto system_type = py::class_<pysystem_ptr_t>(module_ecssim, "SystemX")
      .def(
          "__repr__",
          [](pysystem_ptr_t system) -> std::string {
            fxstring<256> fxs;
            auto clazz = system->sysdata()->objectClass();
            fxs.format("ecssim::System(%p) class<%s>", system.get(), clazz->Name().c_str());
            return fxs.c_str();
          })
          .def("notify", [type_codec](pysystem_ptr_t system, crcstring_ptr_t eventID, py::object evdata) {
            evdata_t decoded;
            if (py::isinstance<py::dict>(evdata)){
              auto as_dict = evdata.cast<py::dict>();
              auto dtab = decoded.makeShared<DataTable>();
              DataKey dkey;
              for (auto item : as_dict) {
                auto key = py::cast<crcstring_ptr_t>(item.first);
                auto val = py::reinterpret_borrow<py::object>(item.second);
                auto var_val = type_codec->decode64(val);
                dkey._encoded = *key;
                (*dtab)[dkey] = var_val;
              }
            }
            else{
              decoded = type_codec->decode64(evdata);
            }
            //auto event = std::make_shared<CrcString>(eventname.c_str());
            system->_notify(*eventID, decoded);
          });
  type_codec->registerRawPtrCodec<pysystem_ptr_t,System*>(system_type);
} // void pyinit_system(py::module& module_ecssim) {
/////////////////////////////////////////////////////////////////////////////////
} // namespace ork::ecs {