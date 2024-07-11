#include "pyext.h"
#include <ork/ecs/datatable.h>

namespace ork::ecssim {

void register_component(typename py::module_& module, typename nanobindadapter::codec_ptr_t type_codec) {
  using namespace ::ork::python;
  /////////////////////////////////////////////////////////////////////////////////
  auto comp_t = clazz<nanobindadapter, pycomponent_ptr_t>(module, "Component")
                    .prop_ro(
                        "entity",
                        [](pycomponent_ptr_t comp) -> pyentity_ptr_t {
                          auto wrapped = pyentity_ptr_t(comp->GetEntity());
                          return wrapped;
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
                        [type_codec](pycomponent_ptr_t comp,
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
};
} // namespace ork::ecssim