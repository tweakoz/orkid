#include "pyext.h"
#include <ork/ecs/datatable.h>
#include <ork/ecs/archetype.h>

namespace ork::ecssim {
using eref_ptr_t = std::shared_ptr<EntityRef>;

// SELF-DEFEND. The Entity wrapper is nullable — a lookup that found nothing hands
// back a null-wrapped Entity rather than Python None — and every transform accessor
// below dereferences it. Reaching one with nothing inside is a SCRIPT bug (a missed
// `if not ent:`), so it says so by name instead of faulting inside the engine at
// some offset the traceback cannot explain.
static Entity* _entOrThrow(pyentity_ptr_t ent, const char* what) {
  if (nullptr == ent.get())
    throw std::runtime_error(
        std::string("Entity.") + what + ": the entity wrapper is EMPTY (the lookup found "
        "no entity) - test it with `if ent:` before using it");
  return ent.get();
}

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
                   .prop_ro("name", [](pyentity_ptr_t ent) -> std::string { return ent->name().c_str(); })
                   .prop_ro("archetype_name", [](pyentity_ptr_t ent) -> std::string {
                     auto sd = ent->data();
                     if (sd) {
                       auto arch = sd->GetArchetype();
                       if (arch) return arch->GetName().c_str();
                     }
                     return "";
                   })
                   .prop_ro("spawner_name", [](pyentity_ptr_t ent) -> std::string {
                     auto sd = ent->data();
                     if (sd) return sd->GetName().c_str();
                     return "";
                   })
                   .prop_rw(
                       "translation",
                       [](pyentity_ptr_t ent) -> fvec3 {
                         auto trans = _entOrThrow(ent, "translation")->transform();
                         return trans->_translation;
                       },
                       [](pyentity_ptr_t ent, const fvec3& pos) {
                         auto trans          = _entOrThrow(ent, "translation")->transform();
                         trans->_translation = pos;
                       })
                   .prop_rw(
                       "orientation",
                       [](pyentity_ptr_t ent) -> fquat {
                         auto trans = _entOrThrow(ent, "orientation")->transform();
                         return trans->_rotation;
                       },
                       [](pyentity_ptr_t ent, const fquat& rot) {
                         auto trans          = _entOrThrow(ent, "orientation")->transform();
                         trans->_rotation = rot;
                       })
                   .prop_rw(
                       "scale",
                       [](pyentity_ptr_t ent) -> float {
                         return _entOrThrow(ent, "scale")->transform()->_uniformScale;
                       },
                       [](pyentity_ptr_t ent, float s) {
                         _entOrThrow(ent, "scale")->transform()->_uniformScale = s;
                       })
                   .def(
                       "findComponentByName",
                       [](pyentity_ptr_t ent, const std::string& classname) -> pycomponent_ptr_t {
                         auto comp    = ent->GetComponentByClassName(classname);
                         auto wrapped = pycomponent_ptr_t(comp);
                         return wrapped;
                       })
                   // FALSY when the wrapper is empty. A lookup that found no entity hands back a
                   // null-wrapped Entity rather than Python None, and without this every such
                   // wrapper was TRUTHY — so `if not ent:` passed and the next line faulted.
                   .def("__bool__", [](pyentity_ptr_t ent) -> bool { return ent.get() != nullptr; })
                   .def("__repr__", [](pyentity_ptr_t ent) -> std::string { return FormatString("ent<%p>", ent.get()); });
  type_codec->registerStdCodec<pyentity_ptr_t>(ent_t);
};
} // namespace ork::ecssim