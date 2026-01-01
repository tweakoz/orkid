#include <ork/pch.h>
#include <ork/lev2/ui/property_sheet_model.h>
#include <ork/math/cvector2.h>
#include <ork/math/cvector3.h>
#include <ork/math/cvector4.h>
#include <algorithm>

namespace ork::ui {

/////////////////////////////////////////////////////////////////////////
// PropertySheetModel base class
/////////////////////////////////////////////////////////////////////////

varmap::varmap_ptr_t PropertySheetModel::getAnnotations(const std::string& key) const {
  // Default implementation returns nullptr (no annotations)
  return nullptr;
}

void PropertySheetModel::notifyPropertyChanged(const std::string& key) {
  if (_onPropertyChanged) {
    _onPropertyChanged(key);
  }
}

void PropertySheetModel::notifyStructureChanged() {
  if (_onStructureChanged) {
    _onStructureChanged();
  }
}

/////////////////////////////////////////////////////////////////////////
// VarMapPropertyModel implementation
/////////////////////////////////////////////////////////////////////////

VarMapPropertyModel::VarMapPropertyModel() : _data(std::make_shared<varmap::VarMap>()) {
}

VarMapPropertyModel::VarMapPropertyModel(varmap::varmap_ptr_t data) : _data(data) {
  if (!_data) {
    _data = std::make_shared<varmap::VarMap>();
  }
}

void VarMapPropertyModel::setData(varmap::varmap_ptr_t data) {
  _data = data;
  if (!_data) {
    _data = std::make_shared<varmap::VarMap>();
  }
  notifyStructureChanged();
}

void VarMapPropertyModel::setAnnotations(const std::string& key, varmap::varmap_ptr_t annotations) {
  _annotations[key] = annotations;
}

varmap::varmap_ptr_t VarMapPropertyModel::_getNode(const std::string& key) const {
  if (key.empty()) {
    return _data;
  }

  // Split key by '/'
  std::vector<std::string> parts;
  size_t start = 0;
  size_t end = key.find('/');
  while (end != std::string::npos) {
    parts.push_back(key.substr(start, end - start));
    start = end + 1;
    end = key.find('/', start);
  }
  parts.push_back(key.substr(start));

  // Navigate to node
  varmap::varmap_ptr_t current = _data;
  for (const auto& part : parts) {
    auto it = current->_themap.find(part);
    if (it == current->_themap.end()) {
      return nullptr;
    }
    auto child = it->second.tryAs<varmap::varmap_ptr_t>();
    if (!child) {
      // This is a leaf node, not a container
      return nullptr;
    }
    current = child.value();
  }
  return current;
}

std::pair<varmap::varmap_ptr_t, std::string> VarMapPropertyModel::_getParentAndName(const std::string& key) const {
  size_t last_slash = key.rfind('/');
  if (last_slash == std::string::npos) {
    // No slash, parent is root
    return {_data, key};
  }
  std::string parent_key = key.substr(0, last_slash);
  std::string name = key.substr(last_slash + 1);
  return {_getNode(parent_key), name};
}

std::vector<std::string> VarMapPropertyModel::getChildren(const std::string& parent_key) const {
  std::vector<std::string> children;

  varmap::varmap_ptr_t node = _getNode(parent_key);
  if (!node) {
    return children;
  }

  for (const auto& [name, val] : node->_themap) {
    std::string full_key = parent_key.empty() ? name : parent_key + "/" + name;
    children.push_back(full_key);
  }

  // Sort for consistent ordering
  std::sort(children.begin(), children.end());
  return children;
}

std::string VarMapPropertyModel::getDisplayName(const std::string& key) const {
  size_t last_slash = key.rfind('/');
  if (last_slash == std::string::npos) {
    return key;
  }
  return key.substr(last_slash + 1);
}

bool VarMapPropertyModel::hasChildren(const std::string& key) const {
  auto [parent, name] = _getParentAndName(key);
  if (!parent) {
    return false;
  }

  auto it = parent->_themap.find(name);
  if (it == parent->_themap.end()) {
    return false;
  }

  auto child_map = it->second.tryAs<varmap::varmap_ptr_t>();
  if (!child_map) {
    return false;
  }

  return !child_map.value()->_themap.empty();
}

svar128_t VarMapPropertyModel::getValue(const std::string& key) const {
  auto [parent, name] = _getParentAndName(key);
  if (!parent) {
    return svar128_t();
  }

  auto it = parent->_themap.find(name);
  if (it == parent->_themap.end()) {
    return svar128_t();
  }

  return it->second;
}

void VarMapPropertyModel::setValue(const std::string& key, svar128_t value) {
  if (_read_only) {
    return;
  }

  auto [parent, name] = _getParentAndName(key);
  if (!parent) {
    return;
  }

  auto it = parent->_themap.find(name);
  if (it != parent->_themap.end()) {
    it->second = value;
    notifyPropertyChanged(key);
  }
}

PropertyType VarMapPropertyModel::getPropertyType(const std::string& key) const {
  // Check annotations first for type override
  auto annotations = getAnnotations(key);
  if (annotations) {
    auto type_it = annotations->_themap.find("type");
    if (type_it != annotations->_themap.end()) {
      // Type can be specified as CrcString (from Python tokens), PropertyType enum, or CRC integer
      if (auto crcstr = type_it->second.tryAs<crcstring_ptr_t>()) {
        return propertyTypeFromCrc(crcstr.value()->hashed());
      }
      if (auto pt = type_it->second.tryAs<PropertyType>()) {
        return pt.value();
      }
      if (auto crc = type_it->second.tryAs<uint32_t>()) {
        return propertyTypeFromCrc(crc.value());
      }
    }
  }

  auto [parent, name] = _getParentAndName(key);
  if (!parent) {
    return PropertyType::Unknown;
  }

  auto it = parent->_themap.find(name);
  if (it == parent->_themap.end()) {
    return PropertyType::Unknown;
  }

  const auto& value = it->second;

  // Check if it's a group (VarMap)
  if (value.tryAs<varmap::varmap_ptr_t>()) {
    return PropertyType::Group;
  }

  // Check common types
  if (value.tryAs<bool>()) {
    return PropertyType::Bool;
  }
  if (value.tryAs<int>() || value.tryAs<int32_t>() || value.tryAs<int64_t>()) {
    return PropertyType::Int;
  }
  if (value.tryAs<float>() || value.tryAs<double>()) {
    return PropertyType::Float;
  }
  if (value.tryAs<std::string>()) {
    return PropertyType::String;
  }
  if (value.tryAs<fvec2>()) {
    return PropertyType::Vec2;
  }
  if (value.tryAs<fvec3>()) {
    return PropertyType::Vec3;
  }
  if (value.tryAs<fvec4>()) {
    return PropertyType::Vec4;
  }

  return PropertyType::Unknown;
}

varmap::varmap_ptr_t VarMapPropertyModel::getAnnotations(const std::string& key) const {
  auto it = _annotations.find(key);
  if (it != _annotations.end()) {
    return it->second;
  }
  return nullptr;
}

/////////////////////////////////////////////////////////////////////////
} // namespace ork::ui
