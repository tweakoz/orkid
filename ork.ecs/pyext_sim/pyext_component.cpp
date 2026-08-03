#include "pyext.h"
#include <ork/ecs/datatable.h>
#include <ork/ecs/pysys/PythonComponent.h>

namespace ork::ecssim {

void register_component(typename py::module_& module, typename nanobindadapter::codec_ptr_t type_codec) {
  using namespace ::ork::python;
  /////////////////////////////////////////////////////////////////////////////////
  auto comp_t = clazz<nanobindadapter, pycomponent_ptr_t>(module, "SimComponent")
                    .prop_ro(
                        "entity",
                        [](pycomponent_ptr_t comp) -> pyentity_ptr_t {
                          auto wrapped = pyentity_ptr_t(comp->GetEntity());
                          return wrapped;
                        })
                    // The SIMULATION this component runs in — the same object a
                    // system script is handed. Components reach entity state
                    // through comp.entity; this is the SIM-GLOBAL channel
                    // (simulation.vars), which is how a behavior script shares
                    // state with a system script (one clock, one control block)
                    // without either of them knowing the other's entity names.
                    // Per-sim by construction: the varmap dies with the
                    // simulation, so a restarted sim never inherits stale state
                    // the way a python module global would.
                    .prop_ro(
                        "sim",
                        [](pycomponent_ptr_t comp) -> pysim_ptr_t {
                          return pysim_ptr_t(comp->sceneInst());
                        })
                    // The author's payload for THIS component, carried in the
                    // .ecs (PythonComponentData::_scriptData) rather than in the
                    // process environment — so a behavior script reads the same
                    // value in the authoring process and in a separately
                    // launched player. Empty string on any non-python component.
                    .prop_ro(
                        "script_data",
                        [](pycomponent_ptr_t comp) -> std::string {
                          auto as_python = dynamic_cast<const PythonComponent*>(comp.get());
                          if (nullptr == as_python)
                            return std::string();
                          return as_python->GetCD()._scriptData;
                        })
                    .def(
                        "__repr__",
                        [](pycomponent_ptr_t comp) -> std::string {
                          auto clazz     = comp->GetClass();
                          auto clazzname = clazz->Name();
                          return FormatString("%s<%p>", clazzname.c_str(), comp.get());
                        })
                    .def(
                        "notify",
                        [type_codec](
                            pycomponent_ptr_t comp,
                            crcstring_ptr_t eventID, //
                            py::object evdata) {
                          evdata_t decoded;
                          if (py::isinstance<py::dict>(evdata)) {
                            auto as_dict = py::cast<py::dict>(evdata);
                            auto dtab    = decoded.makeShared<DataTable>();
                            DataKey dkey;
                            for (auto item : as_dict) {
                              auto key      = py::cast<crcstring_ptr_t>(item.first);
                              auto val      = py::cast<py::object>(item.second);
                              auto var_val  = type_codec->decode64(val);
                              dkey._encoded = *key;
                              (*dtab)[dkey] = var_val;
                            }
                          } else {
                            decoded = type_codec->decode64(evdata);
                          }
                          // auto event = std::make_shared<CrcString>(eventname.c_str());
                          comp->_notify(comp->sceneInst(), *eventID, decoded);
                        });
  type_codec->registerStdCodec<pycomponent_ptr_t>(comp_t);
  /// component array
  auto comparray_t =
      clazz<nanobindadapter, ComponentArray, pycomponentarray_ptr_t>(module, "SimComponentArray")
          .prop_ro("size", [](pycomponentarray_ptr_t carr) -> size_t { return carr->size(); })
          .def("__getitem__", [](pycomponentarray_ptr_t carr, int idx) -> pycomponent_ptr_t { return carr->get(idx); });
  type_codec->registerStdCodec<pycomponentarray_ptr_t>(comparray_t);
};
} // namespace ork::ecssim