////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/kernel/varmap.inl>
#include <functional>
#include <vector>
#include <string>
#include <memory>

namespace ork::ui {

////////////////////////////////////////////////////////////////////
// OutlinerModel: Abstract base class for Outliner data models
// - Can be subclassed in C++ or Python
// - Provides data access and manipulation interface
// - Notifies observers of changes
////////////////////////////////////////////////////////////////////

struct OutlinerModel {
  OutlinerModel() = default;
  virtual ~OutlinerModel() = default;

  //////////////////////////////////////////////////////////////
  // Data access - override these in subclasses
  //////////////////////////////////////////////////////////////

  // Get child keys for a given parent (empty string = root)
  virtual std::vector<std::string> getChildren(const std::string& parent_key) const = 0;

  // Get display name for an item (typically the last component of the key)
  virtual std::string getDisplayName(const std::string& key) const = 0;

  // Check if an item has children
  virtual bool hasChildren(const std::string& key) const = 0;

  // Get optional value/metadata for an item
  virtual svar128_t getValue(const std::string& key) const { return svar128_t(); }

  //////////////////////////////////////////////////////////////
  // Rename support
  //////////////////////////////////////////////////////////////

  // Whether this model allows renaming items
  bool allowRename() const { return _allow_rename; }
  void setAllowRename(bool allow) { _allow_rename = allow; }

  // Whether this model allows deleting items
  bool allowDelete() const { return _allow_delete; }
  void setAllowDelete(bool allow) { _allow_delete = allow; }

  // Rename an item - returns the new key, or empty string on failure
  // Override this if your model supports renaming
  virtual std::string renameItem(const std::string& old_key, const std::string& new_name);

  //////////////////////////////////////////////////////////////
  // Data manipulation - override if your model supports editing
  //////////////////////////////////////////////////////////////

  // Add a new item under parent_key with given name and value
  virtual void addItem(const std::string& parent_key, const std::string& name, svar128_t value = svar128_t());

  // Remove an item by key
  virtual void removeItem(const std::string& key);

  // Move an item to a new parent
  virtual void moveItem(const std::string& key, const std::string& new_parent_key);

  // Update an item's value
  virtual void updateItem(const std::string& key, svar128_t value);

  //////////////////////////////////////////////////////////////
  // Change notifications - call these when data changes
  //////////////////////////////////////////////////////////////

  void notifyItemAdded(const std::string& key);
  void notifyItemRemoved(const std::string& key);
  void notifyItemChanged(const std::string& key);
  void notifyModelReset();

  //////////////////////////////////////////////////////////////
  // Callbacks for observers (Outliner subscribes to these)
  //////////////////////////////////////////////////////////////

  std::function<void(const std::string& key)> _onItemAdded;
  std::function<void(const std::string& key)> _onItemRemoved;
  std::function<void(const std::string& key)> _onItemChanged;
  std::function<void()> _onModelReset;

protected:
  bool _allow_rename = false;
  bool _allow_delete = false;
};

using outliner_model_ptr_t = std::shared_ptr<OutlinerModel>;

////////////////////////////////////////////////////////////////////
// VarMapModel: Built-in model implementation backed by VarMap
// - Provides backward compatibility with existing setData() API
////////////////////////////////////////////////////////////////////

struct VarMapModel : public OutlinerModel {
  VarMapModel();
  VarMapModel(varmap::varmap_ptr_t data);
  ~VarMapModel() override = default;

  // Set/get the backing VarMap
  void setData(varmap::varmap_ptr_t data);
  varmap::varmap_ptr_t getData() const { return _data; }

  // OutlinerModel interface
  std::vector<std::string> getChildren(const std::string& parent_key) const override;
  std::string getDisplayName(const std::string& key) const override;
  bool hasChildren(const std::string& key) const override;
  svar128_t getValue(const std::string& key) const override;

  // Editing operations
  void addItem(const std::string& parent_key, const std::string& name, svar128_t value = svar128_t()) override;
  void removeItem(const std::string& key) override;
  void updateItem(const std::string& key, svar128_t value) override;
  std::string renameItem(const std::string& old_key, const std::string& new_name) override;

private:
  // Navigate to a node by key path, returns nullptr if not found
  varmap::varmap_ptr_t _getNode(const std::string& key) const;
  // Get parent node and child name from a key
  std::pair<varmap::varmap_ptr_t, std::string> _getParentAndName(const std::string& key) const;

  varmap::varmap_ptr_t _data;
};

using varmap_model_ptr_t = std::shared_ptr<VarMapModel>;

} // namespace ork::ui
