////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/lev2/ui/reflection_property_model.h>
#include <ork/reflect/properties/ITyped.h>
#include <ork/reflect/properties/DirectObject.h>
#include <ork/reflect/properties/IObjectMap.h>
#include <ork/reflect/properties/IObjectArray.h>
#include <ork/reflect/properties/IMap.h>
#include <ork/reflect/properties/IArray.h>
#include <ork/reflect/properties/AccessorVariant.h>
#include <ork/object/ObjectClass.h>
#include <ork/rtti/RTTIX.inl>
#include <ork/math/cvector2.h>
#include <ork/math/cvector3.h>
#include <ork/math/cvector4.h>
#include <ork/math/quaternion.h>
#include <ork/kernel/string/PoolString.h>
#include <ork/kernel/string/string.h>
#include <ork/file/path.h>

namespace ork::ui {

///////////////////////////////////////////////////////////////////////////////

ReflectionPropertySheetModel::ReflectionPropertySheetModel() {
}

///////////////////////////////////////////////////////////////////////////////

void ReflectionPropertySheetModel::addKeyOverride(const std::string& key, const KeyOverride& ovr) {
  _key_overrides[key] = ovr;
}

///////////////////////////////////////////////////////////////////////////////

void ReflectionPropertySheetModel::clearKeyOverrides() {
  _key_overrides.clear();
}

///////////////////////////////////////////////////////////////////////////////

void ReflectionPropertySheetModel::setObject(object_ptr_t obj) {
  _object = obj;
  _entries.clear();
  _by_key.clear();
  if (_object) {
    _buildPropertyList();
  }
  notifyStructureChanged();
}

///////////////////////////////////////////////////////////////////////////////

object_ptr_t ReflectionPropertySheetModel::getObject() const {
  return _object;
}

///////////////////////////////////////////////////////////////////////////////

void ReflectionPropertySheetModel::_buildPropertyList() {
  if (!_object)
    return;
  _addPropertiesFromObject(_object, "");
}

///////////////////////////////////////////////////////////////////////////////

void ReflectionPropertySheetModel::_addPropertiesFromObject(
    object_ptr_t obj,
    const std::string& parent_key) {
  if (!obj)
    return;
  auto* clazz = dynamic_cast<object::ObjectClass*>(obj->GetClass());
  if (!clazz)
    return;
  const auto& desc = clazz->Description();
  _addPropertiesFromDescription(&desc, obj, parent_key);
}

///////////////////////////////////////////////////////////////////////////////

void ReflectionPropertySheetModel::_addPropertiesFromDescription(
    const reflect::Description* desc,
    object_ptr_t obj,
    const std::string& parent_key) {
  if (!desc)
    return;

  // Walk parent chain first (so base class properties appear first)
  if (desc->parent()) {
    _addPropertiesFromDescription(desc->parent(), obj, parent_key);
  }

  // Then add this description's own properties
  const auto& props = desc->properties();
  for (auto it = props.begin(); it != props.end(); ++it) {
    const auto& prop_name = it->first;
    auto* prop            = it->second;

    if (_isHidden(prop))
      continue;

    std::string name_str = prop_name.c_str();
    std::string full_key = parent_key.empty() ? name_str : (parent_key + "/" + name_str);

    // Skip if already added (parent chain may have been walked)
    if (_by_key.count(full_key))
      continue;

    PropertyEntry entry;
    entry.name       = name_str;
    entry.full_key   = full_key;
    entry.parent_key = parent_key;
    entry.property   = prop;
    entry.type       = _mapPropertyType(prop);

    // For DirectObjectBase, resolve the sub-object and recurse
    auto* direct_obj = dynamic_cast<const reflect::DirectObjectBase*>(prop);
    if (direct_obj) {
      entry.sub_object = direct_obj->getObject(obj);
      if (!entry.sub_object) {
        entry.is_null_direct_object = true;
      }
      size_t idx       = _entries.size();
      _entries.push_back(entry);
      _by_key[full_key] = idx;
      // Recurse into sub-object's properties
      if (entry.sub_object) {
        _addPropertiesFromObject(entry.sub_object, full_key);
      }
    } else if (auto* map_prop = dynamic_cast<const reflect::IMap*>(prop)) {
      // Map property: enumerate elements as children
      entry.is_map_property = true;
      size_t idx = _entries.size();
      _entries.push_back(entry);
      _by_key[full_key] = idx;
      _addMapElements(map_prop, obj, full_key);
    } else {
      size_t idx = _entries.size();
      _entries.push_back(entry);
      _by_key[full_key] = idx;
    }
  }
}

///////////////////////////////////////////////////////////////////////////////

void ReflectionPropertySheetModel::_addMapElements(
    const reflect::IMap* map_prop,
    object_ptr_t obj,
    const std::string& parent_key) {
  if (!map_prop || !obj)
    return;

  auto elements = map_prop->enumerateElements(obj);

  // Sort by key for consistent display
  std::sort(elements.begin(), elements.end(),
      [](const reflect::map_pair_t& a, const reflect::map_pair_t& b) -> bool {
        auto& ka = a.first;
        auto& kb = b.first;
        if (auto k_str = ka.tryAs<std::string>())
          return k_str.value() < kb.get<std::string>();
        if (auto k_int = ka.tryAs<int>())
          return k_int.value() < kb.get<int>();
        return false;
      });

  for (const auto& elem : elements) {
    const auto& elem_key = elem.first;
    const auto& elem_val = elem.second;

    // Determine the key name string
    std::string key_name;
    if (auto k_str = elem_key.tryAs<std::string>())
      key_name = k_str.value();
    else if (auto k_int = elem_key.tryAs<int>())
      key_name = std::to_string(k_int.value());
    else
      continue; // Skip unsupported key types

    std::string elem_full_key = parent_key + "/" + key_name;

    // Skip if already added
    if (_by_key.count(elem_full_key))
      continue;

    PropertyEntry entry;
    entry.name         = key_name;
    entry.full_key     = elem_full_key;
    entry.parent_key   = parent_key;
    entry.is_map_entry = true;
    entry.map_key      = elem_key;
    entry.map_property = map_prop;
    entry.map_owner    = obj;

    // Check if value is an object (IObjectMap entries)
    if (auto obj_val = elem_val.tryAs<object_ptr_t>()) {
      entry.sub_object = obj_val.value();
      entry.type       = PropertyType::Group;

      if (!entry.sub_object) {
        entry.is_null_object_entry = true;
      }

      size_t idx       = _entries.size();
      _entries.push_back(entry);
      _by_key[elem_full_key] = idx;

      // Recurse into the object's reflected properties
      if (entry.sub_object) {
        _addPropertiesFromObject(entry.sub_object, elem_full_key);
      }
    } else {
      // Non-object map value - show as leaf
      // Try to determine type from the svar128_t
      if (elem_val.isA<bool>())
        entry.type = PropertyType::Bool;
      else if (elem_val.isA<int>())
        entry.type = PropertyType::Int;
      else if (elem_val.isA<float>())
        entry.type = PropertyType::Float;
      else if (elem_val.isA<std::string>())
        entry.type = PropertyType::String;
      else
        entry.type = PropertyType::Unknown;

      size_t idx = _entries.size();
      _entries.push_back(entry);
      _by_key[elem_full_key] = idx;
    }
  }
}

///////////////////////////////////////////////////////////////////////////////

bool ReflectionPropertySheetModel::isMapProperty(const std::string& key) const {
  auto it = _by_key.find(key);
  if (it == _by_key.end())
    return false;
  return _entries[it->second].is_map_property;
}

///////////////////////////////////////////////////////////////////////////////

bool ReflectionPropertySheetModel::isMapConst(const std::string& key) const {
  auto it = _by_key.find(key);
  if (it == _by_key.end())
    return true; // Default to const (safe)
  const auto& entry = _entries[it->second];
  if (!entry.is_map_property || !entry.property)
    return true;
  auto anno = entry.property->typedAnnotation<ConstString>("editor.map.policy.const");
  if (anno && anno.value().length() > 0 && strcmp(anno.value().c_str(), "true") == 0)
    return true;
  return false;
}

///////////////////////////////////////////////////////////////////////////////

void ReflectionPropertySheetModel::addMapElement(
    const std::string& map_key,
    const std::string& element_name) {
  auto it = _by_key.find(map_key);
  if (it == _by_key.end())
    return;
  const auto& entry = _entries[it->second];
  if (!entry.is_map_property || !entry.property)
    return;

  auto* map_prop = dynamic_cast<const reflect::IMap*>(entry.property);
  if (!map_prop)
    return;

  // Find the owning object for this map property
  object_ptr_t owner = _object;
  if (!entry.parent_key.empty()) {
    auto parent_it = _by_key.find(entry.parent_key);
    if (parent_it != _by_key.end()) {
      const auto& parent_entry = _entries[parent_it->second];
      if (parent_entry.sub_object) {
        owner = parent_entry.sub_object;
      }
    }
  }

  // Insert a default element with the given name as key
  reflect::map_abstract_item_t key_item;
  key_item.set<std::string>(element_name);
  map_prop->insertDefaultElement(owner, key_item);

  // Rebuild the property list
  setObject(_object);
}

///////////////////////////////////////////////////////////////////////////////

void ReflectionPropertySheetModel::removeMapElement(
    const std::string& map_key,
    const std::string& element_name) {
  auto it = _by_key.find(map_key);
  if (it == _by_key.end())
    return;
  const auto& entry = _entries[it->second];
  if (!entry.is_map_property || !entry.property)
    return;

  auto* map_prop = dynamic_cast<const reflect::IMap*>(entry.property);
  if (!map_prop)
    return;

  // Find the owning object
  object_ptr_t owner = _object;
  if (!entry.parent_key.empty()) {
    auto parent_it = _by_key.find(entry.parent_key);
    if (parent_it != _by_key.end()) {
      const auto& parent_entry = _entries[parent_it->second];
      if (parent_entry.sub_object) {
        owner = parent_entry.sub_object;
      }
    }
  }

  // Remove the element
  reflect::map_abstract_item_t key_item;
  key_item.set<std::string>(element_name);
  map_prop->removeElement(owner, key_item);

  // Rebuild the property list
  setObject(_object);
}

///////////////////////////////////////////////////////////////////////////////

PropertyType ReflectionPropertySheetModel::_mapPropertyType(
    const reflect::ObjectProperty* prop) const {
  // Check for DirectObjectBase first (group/container)
  if (dynamic_cast<const reflect::DirectObjectBase*>(prop))
    return PropertyType::Group;
  if (dynamic_cast<const reflect::IObjectMap*>(prop))
    return PropertyType::Group;
  if (dynamic_cast<const reflect::IObjectArray*>(prop))
    return PropertyType::Group;
  if (dynamic_cast<const reflect::IMap*>(prop))
    return PropertyType::Group;
  if (dynamic_cast<const reflect::IArray*>(prop))
    return PropertyType::Group;

  // Typed properties
  if (dynamic_cast<const reflect::ITyped<bool>*>(prop))
    return PropertyType::Bool;
  if (dynamic_cast<const reflect::ITyped<int>*>(prop))
    return PropertyType::Int;
  if (dynamic_cast<const reflect::ITyped<float>*>(prop))
    return PropertyType::Float;
  if (dynamic_cast<const reflect::ITyped<std::string>*>(prop))
    return PropertyType::String;
  if (dynamic_cast<const reflect::ITyped<fvec2>*>(prop))
    return PropertyType::Vec2;
  if (dynamic_cast<const reflect::ITyped<fvec3>*>(prop))
    return PropertyType::Vec3;
  if (dynamic_cast<const reflect::ITyped<fvec4>*>(prop))
    return PropertyType::Vec4;
  if (dynamic_cast<const reflect::ITyped<fquat>*>(prop))
    return PropertyType::Quat;
  // PoolString maps to String
  if (dynamic_cast<const reflect::ITyped<PoolString>*>(prop))
    return PropertyType::String;
  // file::Path maps to Asset
  if (dynamic_cast<const reflect::ITyped<file::Path>*>(prop))
    return PropertyType::Asset;

  // AccessorVariant and unknown types
  return PropertyType::Unknown;
}

///////////////////////////////////////////////////////////////////////////////

bool ReflectionPropertySheetModel::_isHidden(
    const reflect::ObjectProperty* prop) const {
  auto anno = prop->typedAnnotation<ConstString>("editor.visible");
  if (anno && anno.value().length() > 0 && strcmp(anno.value().c_str(), "false") == 0) {
    return true;
  }
  return false;
}

///////////////////////////////////////////////////////////////////////////////

std::vector<std::string> ReflectionPropertySheetModel::getChildren(
    const std::string& parent_key) const {
  std::vector<std::string> children;
  for (const auto& entry : _entries) {
    if (entry.parent_key == parent_key) {
      children.push_back(entry.full_key);
    }
  }
  return children;
}

///////////////////////////////////////////////////////////////////////////////

std::string ReflectionPropertySheetModel::getDisplayName(
    const std::string& key) const {
  auto it = _by_key.find(key);
  if (it != _by_key.end()) {
    return _entries[it->second].name;
  }
  // Fallback: last component of the key
  auto pos = key.rfind('/');
  if (pos != std::string::npos) {
    return key.substr(pos + 1);
  }
  return key;
}

///////////////////////////////////////////////////////////////////////////////

bool ReflectionPropertySheetModel::hasChildren(
    const std::string& key) const {
  // Check if any entry has this key as parent
  for (const auto& entry : _entries) {
    if (entry.parent_key == key) {
      return true;
    }
  }
  return false;
}

///////////////////////////////////////////////////////////////////////////////

svar128_t ReflectionPropertySheetModel::getValue(
    const std::string& key) const {
  auto ovr_it = _key_overrides.find(key);
  if (ovr_it != _key_overrides.end() && ovr_it->second.getter) {
    return ovr_it->second.getter();
  }

  auto it = _by_key.find(key);
  if (it == _by_key.end())
    return svar128_t();

  const auto& entry = _entries[it->second];
  if (!entry.property || !_object)
    return svar128_t();

  // For group/container types, return empty
  if (entry.type == PropertyType::Group)
    return svar128_t();

  // Determine which object to read from:
  // Navigate parent chain to find the owning object
  object_ptr_t owner = _object;
  if (!entry.parent_key.empty()) {
    auto parent_it = _by_key.find(entry.parent_key);
    if (parent_it != _by_key.end()) {
      const auto& parent_entry = _entries[parent_it->second];
      if (parent_entry.sub_object) {
        owner = parent_entry.sub_object;
      }
    }
  }

  svar128_t result;

  // Read value based on type
  if (auto* typed_bool = dynamic_cast<const reflect::ITyped<bool>*>(entry.property)) {
    bool val;
    typed_bool->get(val, owner);
    result.set<bool>(val);
  } else if (auto* typed_int = dynamic_cast<const reflect::ITyped<int>*>(entry.property)) {
    int val;
    typed_int->get(val, owner);
    result.set<int>(val);
  } else if (auto* typed_float = dynamic_cast<const reflect::ITyped<float>*>(entry.property)) {
    float val;
    typed_float->get(val, owner);
    result.set<float>(val);
  } else if (auto* typed_string = dynamic_cast<const reflect::ITyped<std::string>*>(entry.property)) {
    std::string val;
    typed_string->get(val, owner);
    result.set<std::string>(val);
  } else if (auto* typed_poolstr = dynamic_cast<const reflect::ITyped<PoolString>*>(entry.property)) {
    PoolString val;
    typed_poolstr->get(val, owner);
    result.set<std::string>(std::string(val.c_str()));
  } else if (auto* typed_fvec2 = dynamic_cast<const reflect::ITyped<fvec2>*>(entry.property)) {
    fvec2 val;
    typed_fvec2->get(val, owner);
    result.set<fvec2>(val);
  } else if (auto* typed_fvec3 = dynamic_cast<const reflect::ITyped<fvec3>*>(entry.property)) {
    fvec3 val;
    typed_fvec3->get(val, owner);
    result.set<fvec3>(val);
  } else if (auto* typed_fvec4 = dynamic_cast<const reflect::ITyped<fvec4>*>(entry.property)) {
    fvec4 val;
    typed_fvec4->get(val, owner);
    result.set<fvec4>(val);
  } else if (auto* typed_fquat = dynamic_cast<const reflect::ITyped<fquat>*>(entry.property)) {
    fquat val;
    typed_fquat->get(val, owner);
    result.set<fquat>(val);
  } else if (auto* typed_path = dynamic_cast<const reflect::ITyped<file::Path>*>(entry.property)) {
    file::Path val;
    typed_path->get(val, owner);
    result.set<std::string>(val.toStdString());
  }

  return result;
}

///////////////////////////////////////////////////////////////////////////////

void ReflectionPropertySheetModel::setValue(
    const std::string& key,
    svar128_t value) {
  if (_read_only)
    return;

  auto ovr_it = _key_overrides.find(key);
  if (ovr_it != _key_overrides.end() && ovr_it->second.setter) {
    ovr_it->second.setter(value);
    notifyPropertyChanged(key);
    return;
  }

  auto it = _by_key.find(key);
  if (it == _by_key.end())
    return;

  const auto& entry = _entries[it->second];
  if (!entry.property || !_object)
    return;

  // Navigate to owning object
  object_ptr_t owner = _object;
  if (!entry.parent_key.empty()) {
    auto parent_it = _by_key.find(entry.parent_key);
    if (parent_it != _by_key.end()) {
      const auto& parent_entry = _entries[parent_it->second];
      if (parent_entry.sub_object) {
        owner = parent_entry.sub_object;
      }
    }
  }

  // Write value based on type
  if (auto* typed_bool = dynamic_cast<const reflect::ITyped<bool>*>(entry.property)) {
    if (value.isA<bool>()) {
      typed_bool->set(value.get<bool>(), owner);
    }
  } else if (auto* typed_int = dynamic_cast<const reflect::ITyped<int>*>(entry.property)) {
    if (value.isA<int>()) {
      typed_int->set(value.get<int>(), owner);
    }
  } else if (auto* typed_float = dynamic_cast<const reflect::ITyped<float>*>(entry.property)) {
    if (value.isA<float>()) {
      typed_float->set(value.get<float>(), owner);
    }
  } else if (auto* typed_string = dynamic_cast<const reflect::ITyped<std::string>*>(entry.property)) {
    if (value.isA<std::string>()) {
      typed_string->set(value.get<std::string>(), owner);
    }
  } else if (auto* typed_poolstr = dynamic_cast<const reflect::ITyped<PoolString>*>(entry.property)) {
    if (value.isA<std::string>()) {
      typed_poolstr->set(AddPooledString(value.get<std::string>().c_str()), owner);
    }
  } else if (auto* typed_fvec2 = dynamic_cast<const reflect::ITyped<fvec2>*>(entry.property)) {
    if (value.isA<fvec2>()) {
      typed_fvec2->set(value.get<fvec2>(), owner);
    }
  } else if (auto* typed_fvec3 = dynamic_cast<const reflect::ITyped<fvec3>*>(entry.property)) {
    if (value.isA<fvec3>()) {
      typed_fvec3->set(value.get<fvec3>(), owner);
    }
  } else if (auto* typed_fvec4 = dynamic_cast<const reflect::ITyped<fvec4>*>(entry.property)) {
    if (value.isA<fvec4>()) {
      typed_fvec4->set(value.get<fvec4>(), owner);
    }
  } else if (auto* typed_fquat = dynamic_cast<const reflect::ITyped<fquat>*>(entry.property)) {
    if (value.isA<fquat>()) {
      typed_fquat->set(value.get<fquat>(), owner);
    }
  } else if (auto* typed_path = dynamic_cast<const reflect::ITyped<file::Path>*>(entry.property)) {
    if (value.isA<std::string>()) {
      typed_path->set(file::Path(value.get<std::string>().c_str()), owner);
    }
  }

  notifyPropertyChanged(key);
}

///////////////////////////////////////////////////////////////////////////////

PropertyType ReflectionPropertySheetModel::getPropertyType(
    const std::string& key) const {
  auto ovr_it = _key_overrides.find(key);
  if (ovr_it != _key_overrides.end()) {
    return ovr_it->second.type;
  }
  auto it = _by_key.find(key);
  if (it != _by_key.end()) {
    return _entries[it->second].type;
  }
  return PropertyType::Unknown;
}

///////////////////////////////////////////////////////////////////////////////

varmap::varmap_ptr_t ReflectionPropertySheetModel::getAnnotations(
    const std::string& key) const {
  auto it = _by_key.find(key);
  if (it == _by_key.end())
    return nullptr;

  const auto& entry = _entries[it->second];
  if (!entry.property)
    return nullptr;

  // Convert property annotations to a VarMap
  const auto& annos = entry.property->_annotations;
  if (annos.size() == 0)
    return nullptr;

  auto result = std::make_shared<varmap::VarMap>();
  for (auto ait = annos.begin(); ait != annos.end(); ++ait) {
    const auto& akey = ait->first;
    const auto& aval = ait->second;
    if (aval.isA<ConstString>()) {
      result->set(std::string(akey.c_str()), std::string(aval.get<ConstString>().c_str()));
    }
  }
  return result;
}

///////////////////////////////////////////////////////////////////////////////

bool ReflectionPropertySheetModel::isNullObjectMapEntry(const std::string& key) const {
  auto it = _by_key.find(key);
  if (it == _by_key.end())
    return false;
  return _entries[it->second].is_null_object_entry;
}

///////////////////////////////////////////////////////////////////////////////

std::vector<std::string> ReflectionPropertySheetModel::getFactoryClasses(const std::string& key) const {
  std::vector<std::string> result;

  auto it = _by_key.find(key);
  if (it == _by_key.end())
    return result;

  const auto& entry = _entries[it->second];
  if (!entry.is_null_object_entry || !entry.map_property)
    return result;

  // Get editor.factorylistbase annotation from the parent map property
  auto anno = entry.map_property->typedAnnotation<ConstString>("editor.factorylistbase");
  if (!anno || anno.value().length() == 0)
    return result;

  // Parse space-separated base class names
  std::vector<std::string> base_classes;
  SplitString(std::string(anno.value().c_str()), " ", base_classes);

  // Enumerate factory classes from each base class
  for (const auto& base_name : base_classes) {
    auto base_clazz = rtti::Class::FindClass(base_name.c_str());
    auto as_obj_clazz = dynamic_cast<object::ObjectClass*>(base_clazz);
    if (!as_obj_clazz)
      continue;

    // Walk subclass hierarchy
    orkstack<object::ObjectClass*> stack;
    stack.push(as_obj_clazz);

    while (!stack.empty()) {
      auto pclass = stack.top();
      stack.pop();

      if (pclass->hasFactory()) {
        auto instanno = pclass->Description().classAnnotation("editor.instantiable");
        bool ok = instanno.isA<bool>() ? instanno.get<bool>() : true;
        if (ok) {
          result.push_back(pclass->Name());
        }
      }

      // Push children
      rtti::Class* const first_child = pclass->FirstChild();
      rtti::Class* child = first_child;
      while (child) {
        auto obj_child = rtti::downcast<object::ObjectClass*>(child);
        if (obj_child)
          stack.push(obj_child);
        child = (child->NextSibling() == first_child) ? nullptr : child->NextSibling();
      }
    }
  }

  return result;
}

///////////////////////////////////////////////////////////////////////////////

void ReflectionPropertySheetModel::setMapElementFromFactory(
    const std::string& key,
    const std::string& class_name) {
  auto it = _by_key.find(key);
  if (it == _by_key.end())
    return;

  const auto& entry = _entries[it->second];
  if (!entry.is_null_object_entry || !entry.map_property || !entry.map_owner)
    return;

  // Find and instantiate the class
  auto clazz = rtti::Class::FindClass(class_name.c_str());
  auto obj_clazz = dynamic_cast<object::ObjectClass*>(clazz);
  if (!obj_clazz)
    return;

  auto instance = obj_clazz->createShared();
  if (!instance)
    return;

  // Set the element in the map
  entry.map_property->setElement(entry.map_owner, entry.map_key, svar128_t(instance));

  // Rebuild the property list
  setObject(_object);
}

///////////////////////////////////////////////////////////////////////////////

bool ReflectionPropertySheetModel::isNullDirectObjectEntry(const std::string& key) const {
  auto it = _by_key.find(key);
  if (it == _by_key.end())
    return false;
  return _entries[it->second].is_null_direct_object;
}

///////////////////////////////////////////////////////////////////////////////

std::vector<std::string> ReflectionPropertySheetModel::getDirectObjectFactoryClasses(const std::string& key) const {
  std::vector<std::string> result;

  auto it = _by_key.find(key);
  if (it == _by_key.end())
    return result;

  const auto& entry = _entries[it->second];
  if (!entry.is_null_direct_object || !entry.property)
    return result;

  // Get editor.factorylistbase annotation from the DirectObjectBase property
  auto anno = entry.property->typedAnnotation<ConstString>("editor.factorylistbase");
  if (!anno || anno.value().length() == 0)
    return result;

  // Parse space-separated base class names
  std::vector<std::string> base_classes;
  SplitString(std::string(anno.value().c_str()), " ", base_classes);

  // Enumerate factory classes from each base class
  for (const auto& base_name : base_classes) {
    auto base_clazz = rtti::Class::FindClass(base_name.c_str());
    auto as_obj_clazz = dynamic_cast<object::ObjectClass*>(base_clazz);
    if (!as_obj_clazz)
      continue;

    // Walk subclass hierarchy
    orkstack<object::ObjectClass*> stack;
    stack.push(as_obj_clazz);

    while (!stack.empty()) {
      auto pclass = stack.top();
      stack.pop();

      if (pclass->hasFactory()) {
        auto instanno = pclass->Description().classAnnotation("editor.instantiable");
        bool ok = instanno.isA<bool>() ? instanno.get<bool>() : true;
        if (ok) {
          result.push_back(pclass->Name());
        }
      }

      // Push children
      rtti::Class* const first_child = pclass->FirstChild();
      rtti::Class* child = first_child;
      while (child) {
        auto obj_child = rtti::downcast<object::ObjectClass*>(child);
        if (obj_child)
          stack.push(obj_child);
        child = (child->NextSibling() == first_child) ? nullptr : child->NextSibling();
      }
    }
  }

  return result;
}

///////////////////////////////////////////////////////////////////////////////

void ReflectionPropertySheetModel::setDirectObjectFromFactory(
    const std::string& key,
    const std::string& class_name) {
  auto it = _by_key.find(key);
  if (it == _by_key.end())
    return;

  const auto& entry = _entries[it->second];
  if (!entry.is_null_direct_object || !entry.property)
    return;

  auto* direct_obj = dynamic_cast<const reflect::DirectObjectBase*>(entry.property);
  if (!direct_obj)
    return;

  // Find the owning object
  object_ptr_t owner = _object;
  if (!entry.parent_key.empty()) {
    auto parent_it = _by_key.find(entry.parent_key);
    if (parent_it != _by_key.end()) {
      const auto& parent_entry = _entries[parent_it->second];
      if (parent_entry.sub_object) {
        owner = parent_entry.sub_object;
      }
    }
  }

  // Find and instantiate the class
  auto clazz = rtti::Class::FindClass(class_name.c_str());
  auto obj_clazz = dynamic_cast<object::ObjectClass*>(clazz);
  if (!obj_clazz)
    return;

  auto instance = obj_clazz->createShared();
  if (!instance)
    return;

  // Set the object on the direct property
  direct_obj->setObject(owner, instance);

  // Rebuild the property list
  setObject(_object);
}

///////////////////////////////////////////////////////////////////////////////

std::vector<std::string> ReflectionPropertySheetModel::getChoices(
    const std::string& key) const {
  auto ovr_it = _key_overrides.find(key);
  if (ovr_it != _key_overrides.end() && ovr_it->second.choices) {
    return ovr_it->second.choices();
  }
  return {};
}

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::ui
