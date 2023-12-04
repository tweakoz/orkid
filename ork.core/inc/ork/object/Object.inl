#include "Object.h"
#include <ork/reflect/Description.h>
#include <ork/kernel/string/ConstString.h>

namespace ork {

  template <typename interface_type> //
  std::shared_ptr<interface_type> //
  Object::queryInterface(const ConstString& interfaceID) const{
    using factory_t = std::function<std::shared_ptr<interface_type>()>;
    auto clazz = objectClass();
    auto& description = clazz->Description();
    auto anno = description.classAnnotationTyped<factory_t>(interfaceID);
    if(anno){
        return anno.value()();
    }
    else {
        return nullptr;
    }
  }

} //namespace ork {
