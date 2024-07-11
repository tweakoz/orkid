////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include "pyext.h"
#include <ork/ecs/simulation.inl>
#include <ork/ecs/system.h>
#include <ork/ecs/system.h>
#include <ork/ecs/datatable.h>

///////////////////////////////////////////////////////////////////////////////
namespace nb = obind;
namespace ork::ecs {
  struct PythonSystem;
  using pypythonsystem_ptr_t = ::ork::python::unmanaged_ptr<PythonSystem>;
}
namespace ork::ecssim {
using namespace ork::ecs;
///////////////////////////////////////////////////////////////////////////////

void register_system(nb::module_& module_ecssim, python::obind_typecodec_ptr_t type_codec) {
  /////////////////////////////////////////////////////////////////////////////////
  auto system_type = clazz<nanobindadapter, pysystem_ptr_t>(module_ecssim, "SimSystem")
                         .prop_ro("vars", [](pysystem_ptr_t system) -> varmap::varmap_ptr_t {
                           return system->varmap();
                         })
                         .def(
                             "__repr__",
                             [](pysystem_ptr_t system) -> std::string {
                               fxstring<256> fxs;
                               auto clazz = system->sysdata()->objectClass();
                               fxs.format("ecssim::System(%p) class<%s>", system.get(), clazz->Name().c_str());
                               return fxs.c_str();
                             })
                         .def("notify", [type_codec](pysystem_ptr_t system, //
                                                     crcstring_ptr_t eventID, //
                                                     nb::object evdata) { //
                           evdata_t decoded;
                           if (nb::isinstance<nb::dict>(evdata)){
                             auto as_dict = nb::cast<nb::dict>(evdata);
                             auto dtab    = decoded.makeShared<DataTable>();
                             DataKey dkey;
                             for (auto item : as_dict) {
                               auto key = nb::cast<crcstring_ptr_t>(item.first);
                               auto val = nb::cast<nb::object>(item.second);
                               auto var_val = type_codec->decode64(val);
                               dkey._encoded = *key;
                               (*dtab)[dkey] = var_val;
                             }
                           } else {
                             // decoded = type_codec->decode64(evdata);
                           }
                           // auto event = std::make_shared<CrcString>(eventname.c_str());
                           system->_notify(*eventID, decoded);
                         });
  type_codec->registerStdCodec<pysystem_ptr_t>(system_type);
  /////////////////////////////////////////////////////////////////////////////////
  /*auto pysys_type = clazz<nanobindadapter, pypythonsystem_ptr_t, pysystem_ptr_t>(module_ecssim, "SimPythonSystem")
  .def("__repr__", [](pypythonsystem_ptr_t system) -> std::string {
    fxstring<256> fxs;
    fxs.format("ecssim::PythonSystem(%p)", system.get());
    return fxs.c_str();
  });
  type_codec->registerStdCodec<pypythonsystem_ptr_t>(pysys_type);*/
} // void pyinit_system(nb::module& module_ecssim) {
/////////////////////////////////////////////////////////////////////////////////
} // namespace ork::ecssim