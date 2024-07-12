#include "pyext.h"
#include <ork/ecs/datatable.h>

namespace ork::ecssim {
using eref_ptr_t = std::shared_ptr<EntityRef>;

void register_entity(typename py::module_& module, typename nanobindadapter::codec_ptr_t type_codec) {
  using namespace ::ork::python;
  /////////////////////////////////////////////////////////////////////////////////
  auto eref_t =
      clazz<nanobindadapter, EntityRef>(module, "EntityRef").prop_ro("id", [](EntityRef eref) -> uint64_t { return eref._entID; });
  type_codec->registerStdCodec<EntityRef>(eref_t);
  /////////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////////
  auto ent_t = clazz<nanobindadapter, pyentity_ptr_t>(module, "Entity")
                   .prop_ro("vars", [](pyentity_ptr_t ent) -> varmap::varmap_ptr_t { return ent->_varmap; })
                   .prop_ro("id", [](pyentity_ptr_t ent) -> uint64_t { return ent->_entref; })
                   .prop_rw(
                       "translation",
                       [](pyentity_ptr_t ent) -> fvec3 {
                         auto trans = ent->transform();
                         return trans->_translation;
                       },
                       [](pyentity_ptr_t ent, const fvec3& pos) {
                         auto trans          = ent->transform();
                         trans->_translation = pos;
                       })
                   .prop_rw(
                       "orientation",
                       [](pyentity_ptr_t ent) -> fquat {
                         auto trans = ent->transform();
                         return trans->_rotation;
                       },
                       [](pyentity_ptr_t ent, const fquat& rot) {
                         auto trans          = ent->transform();
                         trans->_rotation = rot;
                       })
                   .def(
                       "findComponentByName",
                       [](pyentity_ptr_t ent, const std::string& classname) -> pycomponent_ptr_t {
                         auto comp    = ent->GetComponentByClassName(classname);
                         auto wrapped = pycomponent_ptr_t(comp);
                         return wrapped;
                       })

                   .def("__repr__", [](pyentity_ptr_t ent) -> std::string { return FormatString("ent<%p>", ent.get()); });
  type_codec->registerStdCodec<pyentity_ptr_t>(ent_t);
};
} // namespace ork::ecssim