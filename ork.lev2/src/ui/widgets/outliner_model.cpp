#include <ork/pch.h>
#include <ork/lev2/ui/outliner_model.h>
#include <algorithm>

namespace ork::ui {

/////////////////////////////////////////////////////////////////////////
// OutlinerModel base class
/////////////////////////////////////////////////////////////////////////

void OutlinerModel::addItem(const std::string& parent_key, const std::string& name, svar128_t value) {
  // Default implementation does nothing - override in subclasses
}

void OutlinerModel::removeItem(const std::string& key) {
  // Default implementation does nothing - override in subclasses
}

void OutlinerModel::moveItem(const std::string& key, const std::string& new_parent_key) {
  // Default implementation does nothing - override in subclasses
}

void OutlinerModel::updateItem(const std::string& key, svar128_t value) {
  // Default implementation does nothing - override in subclasses
}

std::string OutlinerModel::renameItem(const std::string& old_key, const std::string& new_name) {
  // Default implementation does nothing - override in subclasses
  return "";
}

outliner_factory_list_t OutlinerModel::getFactories(const std::string& parent_key) const {
  // Default implementation returns empty list - override in subclasses
  return {};
}

std::string OutlinerModel::createItem(const std::string& parent_key, const std::string& name, const std::string& factory_id) {
  // Default implementation does nothing - override in subclasses
  return "";
}

void OutlinerModel::notifyItemAdded(const std::string& key) {
  if (_onItemAdded) {
    _onItemAdded(key);
  }
}

void OutlinerModel::notifyItemRemoved(const std::string& key) {
  if (_onItemRemoved) {
    _onItemRemoved(key);
  }
}

void OutlinerModel::notifyItemChanged(const std::string& key) {
  if (_onItemChanged) {
    _onItemChanged(key);
  }
}

void OutlinerModel::notifyModelReset() {
  if (_onModelReset) {
    _onModelReset();
  }
}

/////////////////////////////////////////////////////////////////////////
// VarMapModel implementation
/////////////////////////////////////////////////////////////////////////

VarMapModel::VarMapModel() : _data(std::make_shared<varmap::VarMap>()) {
}

VarMapModel::VarMapModel(varmap::varmap_ptr_t data) : _data(data) {
  if (!_data) {
    _data = std::make_shared<varmap::VarMap>();
  }
}

void VarMapModel::setData(varmap::varmap_ptr_t data) {
  _data = data;
  if (!_data) {
    _data = std::make_shared<varmap::VarMap>();
  }
  notifyModelReset();
}

varmap::varmap_ptr_t VarMapModel::_getNode(const std::string& key) const {
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

std::pair<varmap::varmap_ptr_t, std::string> VarMapModel::_getParentAndName(const std::string& key) const {
  size_t last_slash = key.rfind('/');
  if (last_slash == std::string::npos) {
    // No slash, parent is root
    return {_data, key};
  }
  std::string parent_key = key.substr(0, last_slash);
  std::string name = key.substr(last_slash + 1);
  return {_getNode(parent_key), name};
}

std::vector<std::string> VarMapModel::getChildren(const std::string& parent_key) const {
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

std::string VarMapModel::getDisplayName(const std::string& key) const {
  size_t last_slash = key.rfind('/');
  if (last_slash == std::string::npos) {
    return key;
  }
  return key.substr(last_slash + 1);
}

bool VarMapModel::hasChildren(const std::string& key) const {
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

svar128_t VarMapModel::getValue(const std::string& key) const {
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

void VarMapModel::addItem(const std::string& parent_key, const std::string& name, svar128_t value) {
  varmap::varmap_ptr_t parent = _getNode(parent_key);
  if (!parent) {
    return;
  }

  // If value is empty, create a new VarMap (container node)
  if (!value.isSet()) {
    parent->_themap[name] = std::make_shared<varmap::VarMap>();
  } else {
    parent->_themap[name] = value;
  }

  std::string full_key = parent_key.empty() ? name : parent_key + "/" + name;
  notifyItemAdded(full_key);
}

void VarMapModel::removeItem(const std::string& key) {
  auto [parent, name] = _getParentAndName(key);
  if (!parent) {
    return;
  }

  auto it = parent->_themap.find(name);
  if (it != parent->_themap.end()) {
    parent->_themap.erase(it);
    notifyItemRemoved(key);
  }
}

void VarMapModel::updateItem(const std::string& key, svar128_t value) {
  auto [parent, name] = _getParentAndName(key);
  if (!parent) {
    return;
  }

  auto it = parent->_themap.find(name);
  if (it != parent->_themap.end()) {
    it->second = value;
    notifyItemChanged(key);
  }
}

outliner_factory_list_t VarMapModel::getFactories(const std::string& parent_key) const {
  // VarMapModel provides two basic factories: group (VarMap) and item (string value)
  outliner_factory_list_t factories;

  // Only provide factories if the parent exists and is a container (VarMap)
  varmap::varmap_ptr_t parent = _getNode(parent_key);
  if (parent) {
    factories.push_back({"group", "Group", svar128_t()});
    factories.push_back({"item", "Item", svar128_t(std::string("value"))});
  }

  return factories;
}

std::string VarMapModel::createItem(const std::string& parent_key, const std::string& name, const std::string& factory_id) {
  if (!_allow_add) {
    return "";
  }

  varmap::varmap_ptr_t parent = _getNode(parent_key);
  if (!parent) {
    return "";
  }

  // Check if name already exists
  if (parent->_themap.find(name) != parent->_themap.end()) {
    return "";  // Name collision
  }

  // Create based on factory type
  if (factory_id == "group") {
    parent->_themap[name] = std::make_shared<varmap::VarMap>();
  } else {
    // Default to string value
    parent->_themap[name] = svar128_t(std::string("value"));
  }

  std::string new_key = parent_key.empty() ? name : parent_key + "/" + name;
  notifyItemAdded(new_key);
  return new_key;
}

std::string VarMapModel::renameItem(const std::string& old_key, const std::string& new_name) {
  if (!_allow_rename) {
    return "";
  }

  auto [parent, old_name] = _getParentAndName(old_key);
  if (!parent) {
    return "";
  }

  auto it = parent->_themap.find(old_name);
  if (it == parent->_themap.end()) {
    return "";
  }

  // Check if new name already exists in parent
  if (parent->_themap.find(new_name) != parent->_themap.end()) {
    return "";  // Name collision
  }

  // Calculate new key
  std::string new_key;
  size_t last_slash = old_key.rfind('/');
  if (last_slash != std::string::npos) {
    new_key = old_key.substr(0, last_slash + 1) + new_name;
  } else {
    new_key = new_name;
  }

  // Move the value to the new name
  svar128_t value = it->second;
  parent->_themap.erase(it);
  parent->_themap[new_name] = value;

  // Note: We don't call notifyItemChanged here because the outliner
  // handles the key updates and triggers a rebuild
  return new_key;
}

/////////////////////////////////////////////////////////////////////////
} // namespace ork::ui
