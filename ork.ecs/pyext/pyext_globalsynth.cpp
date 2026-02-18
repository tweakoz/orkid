////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "pyext.h"
#include <ork/ecs/GlobalSynthSystem.h>

///////////////////////////////////////////////////////////////////////////////

namespace ork::ecs {
void pyinit_globalsynth(py::module& module_ecs) {
  auto type_codec = python::pb11_typecodec_t::instance();
  /////////////////////////////////////////////////////////////////////////////////
  // SynthBusConfig
  /////////////////////////////////////////////////////////////////////////////////
  auto buscfg_type = //
      py::class_<SynthBusConfig, ork::Object, synthbuscfg_ptr_t>(
          module_ecs, "SynthBusConfig")
          .def(py::init<>())
          .def(
              "__repr__",
              [](const synthbuscfg_ptr_t& c) -> std::string {
                fxstring<256> fxs;
                fxs.format("ecs::SynthBusConfig(%p)", c.get());
                return fxs.c_str();
              })
          .def_property(
              "effectPreset",
              [](synthbuscfg_ptr_t c) -> std::string { return c->_effectPreset; },
              [](synthbuscfg_ptr_t c, std::string val) { c->_effectPreset = val; })
          .def_property(
              "gainDB",
              [](synthbuscfg_ptr_t c) -> float { return c->_gainDB; },
              [](synthbuscfg_ptr_t c, float val) { c->_gainDB = val; })
          .def_property(
              "pan",
              [](synthbuscfg_ptr_t c) -> float { return c->_pan; },
              [](synthbuscfg_ptr_t c, float val) { c->_pan = val; })
          .def_property(
              "mute",
              [](synthbuscfg_ptr_t c) -> bool { return c->_mute; },
              [](synthbuscfg_ptr_t c, bool val) { c->_mute = val; })
          .def_property(
              "solo",
              [](synthbuscfg_ptr_t c) -> bool { return c->_solo; },
              [](synthbuscfg_ptr_t c, bool val) { c->_solo = val; });
  type_codec->registerStdCodec<synthbuscfg_ptr_t>(buscfg_type);
  /////////////////////////////////////////////////////////////////////////////////
  // GlobalSynthSystemData
  /////////////////////////////////////////////////////////////////////////////////
  auto sysdata_type = //
      py::class_<GlobalSynthSystemData, SystemData, globalsynthsysdata_ptr_t>(
          module_ecs, "GlobalSynthSystemData")
          .def(
              "__repr__",
              [](globalsynthsysdata_ptr_t sd) -> std::string {
                fxstring<256> fxs;
                fxs.format("ecs::GlobalSynthSystemData(%p)", sd.get());
                return fxs.c_str();
              })
          .def_property_readonly(
              "busConfigs",
              [](globalsynthsysdata_ptr_t sd) -> std::map<std::string, synthbuscfg_ptr_t>& { return sd->_busConfigs; },
              py::return_value_policy::reference_internal)
          .def(
              "addBusConfig",
              [](globalsynthsysdata_ptr_t sd, std::string name, synthbuscfg_ptr_t cfg) { sd->_busConfigs[name] = cfg; })
          .def(
              "removeBusConfig",
              [](globalsynthsysdata_ptr_t sd, std::string name) { sd->_busConfigs.erase(name); })
          .def_property(
              "masterGainDB",
              [](globalsynthsysdata_ptr_t sd) -> float { return sd->_masterGainDB; },
              [](globalsynthsysdata_ptr_t sd, float val) { sd->_masterGainDB = val; });
  type_codec->registerStdCodec<globalsynthsysdata_ptr_t>(sysdata_type);
}
} // namespace ork::ecs
