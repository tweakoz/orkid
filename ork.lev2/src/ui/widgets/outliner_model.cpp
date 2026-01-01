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

/////////////////////////////////////////////////////////////////////////
} // namespace ork::ui
