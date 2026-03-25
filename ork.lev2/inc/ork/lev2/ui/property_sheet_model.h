////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/kernel/varmap.inl>
#include <ork/kernel/sigslot2.h>
#include <ork/util/crc.h>
#include <functional>
#include <vector>
#include <string>
#include <memory>

namespace ork::ui {

////////////////////////////////////////////////////////////////////
// PropertyType: CrcEnum for extensible property types
// - Built-in types defined here
// - Custom types can be registered at runtime using CRC tokens
////////////////////////////////////////////////////////////////////

enum class PropertyType : uint32_t {
  CrcEnum(Unknown),
  CrcEnum(Bool),
  CrcEnum(Int),
  CrcEnum(Float),
  CrcEnum(String),
  CrcEnum(Vec2),
  CrcEnum(Vec3),
  CrcEnum(Vec4),
  CrcEnum(Color),
  CrcEnum(Gradient),
  CrcEnum(Curve),
  CrcEnum(Asset),
  CrcEnum(Quat),   // Quaternion (expanded to axis-angle sub-properties)
  CrcEnum(Enum),   // Enum property (dropdown with choices)
  CrcEnum(Group),  // Container for child properties
};

// Helper to convert CRC token to PropertyType
inline PropertyType propertyTypeFromCrc(uint32_t crc) {
  return static_cast<PropertyType>(crc);
}

// Helper to get CRC value from PropertyType
inline uint32_t propertyTypeToCrc(PropertyType type) {
  return static_cast<uint32_t>(type);
}

////////////////////////////////////////////////////////////////////
// PropertySheetModel: Abstract base class for PropertySheet data models
// - Can be subclassed in C++ or Python
// - Provides data access and manipulation interface
// - Notifies observers of changes
////////////////////////////////////////////////////////////////////

struct PropertySheetModel {
  PropertySheetModel() = default;
  virtual ~PropertySheetModel() = default;

  //////////////////////////////////////////////////////////////
  // Tree structure - override these in subclasses
  //////////////////////////////////////////////////////////////

  // Get child keys for a given parent (empty string = root)
  virtual std::vector<std::string> getChildren(const std::string& parent_key) const = 0;

  // Get display name for a property (typically the last component of the key)
  virtual std::string getDisplayName(const std::string& key) const = 0;

  // Check if a property has children (is a group)
  virtual bool hasChildren(const std::string& key) const = 0;

  //////////////////////////////////////////////////////////////
  // Property access - override these in subclasses
  //////////////////////////////////////////////////////////////

  // Get the value of a property
  virtual svar128_t getValue(const std::string& key) const = 0;

  // Set the value of a property
  virtual void setValue(const std::string& key, svar128_t value) = 0;

  // Get the type of a property
  virtual PropertyType getPropertyType(const std::string& key) const = 0;

  // Get annotations (metadata) for a property
  // Returns a VarMap with keys like "min", "max", "step", "readonly", etc.
  virtual varmap::varmap_ptr_t getAnnotations(const std::string& key) const;

  // Get choice list for a property (if any).
  // When non-empty, the property sheet shows a dropdown instead of a normal editor.
  virtual std::vector<std::string> getChoices(const std::string& key) const { return {}; }

  //////////////////////////////////////////////////////////////
  // Map property support (override in subclasses that have maps)
  //////////////////////////////////////////////////////////////

  virtual bool isMapProperty(const std::string& key) const { return false; }
  virtual bool isMapConst(const std::string& key) const { return true; }
  virtual void addMapElement(const std::string& key, const std::string& name) {}
  virtual void removeMapElement(const std::string& key, const std::string& name) {}
  virtual void renameMapElement(const std::string& key, const std::string& old_name, const std::string& new_name) {}

  //////////////////////////////////////////////////////////////
  // Null object map entry / factory support
  //////////////////////////////////////////////////////////////

  // Is this entry a map entry whose object value is null?
  virtual bool isNullObjectMapEntry(const std::string& key) const { return false; }
  // Get available factory class names for a null object map entry
  virtual std::vector<std::string> getFactoryClasses(const std::string& key) const { return {}; }
  // Create an object from factory class name and set it in the map
  virtual void setMapElementFromFactory(const std::string& key, const std::string& class_name) {}

  // Is this entry a DirectObjectBase property whose sub-object is null?
  virtual bool isNullDirectObjectEntry(const std::string& key) const { return false; }
  // Get available factory class names for a null direct object property
  virtual std::vector<std::string> getDirectObjectFactoryClasses(const std::string& key) const { return {}; }
  // Create an object from factory class name and set it on the direct object property
  virtual void setDirectObjectFromFactory(const std::string& key, const std::string& class_name) {}

  //////////////////////////////////////////////////////////////
  // Untyped variant map entry / type-picker support
  // (for maps of svar128_t / rendervar_t whose entries have no type yet)
  //////////////////////////////////////////////////////////////

  // Is this a map entry whose value is an untyped/empty svar128_t?
  virtual bool isUntypedVariantMapEntry(const std::string& key) const { return false; }
  // Set the map entry to a default value of the given type name.
  // Type names: "float", "int", "bool", "fvec3", "fvec4", "string"
  virtual void setVariantMapEntryType(const std::string& key, const std::string& type_name) {}

  //////////////////////////////////////////////////////////////
  // Read-only support
  //////////////////////////////////////////////////////////////

  bool isReadOnly() const { return _read_only; }
  void setReadOnly(bool read_only) { _read_only = read_only; }

  //////////////////////////////////////////////////////////////
  // Change notifications - call these when data changes
  //////////////////////////////////////////////////////////////

  void notifyPropertyChanged(const std::string& key);
  void notifyStructureChanged();  // When properties are added/removed

  // Signal emitted when a value changes externally (e.g. manipulator)
  // Argument is the property key that changed (empty string = refresh all).
  // PropertySheet connects to this to refresh editor widgets without full rebuild.
  sigslot::signal<std::string> _sigExternalValueChanged;

  // Convenience: emit the signal for a specific key
  void notifyExternalValueChanged(const std::string& key) {
    _sigExternalValueChanged(key);
  }

  //////////////////////////////////////////////////////////////
  // Callbacks for observers (PropertySheet subscribes to these)
  //////////////////////////////////////////////////////////////

  std::function<void(const std::string& key)> _onPropertyChanged;
  std::function<void()> _onStructureChanged;

protected:
  bool _read_only = false;
};

using property_sheet_model_ptr_t = std::shared_ptr<PropertySheetModel>;

////////////////////////////////////////////////////////////////////
// VarMapPropertyModel: Built-in model implementation backed by VarMap
////////////////////////////////////////////////////////////////////

struct VarMapPropertyModel : public PropertySheetModel {
  VarMapPropertyModel();
  VarMapPropertyModel(varmap::varmap_ptr_t data);
  ~VarMapPropertyModel() override = default;

  // Set/get the backing VarMap
  void setData(varmap::varmap_ptr_t data);
  varmap::varmap_ptr_t getData() const { return _data; }

  // Set annotations for a property
  void setAnnotations(const std::string& key, varmap::varmap_ptr_t annotations);

  // PropertySheetModel interface
  std::vector<std::string> getChildren(const std::string& parent_key) const override;
  std::string getDisplayName(const std::string& key) const override;
  bool hasChildren(const std::string& key) const override;
  svar128_t getValue(const std::string& key) const override;
  void setValue(const std::string& key, svar128_t value) override;
  PropertyType getPropertyType(const std::string& key) const override;
  varmap::varmap_ptr_t getAnnotations(const std::string& key) const override;

private:
  // Navigate to a node by key path, returns nullptr if not found
  varmap::varmap_ptr_t _getNode(const std::string& key) const;
  // Get parent node and child name from a key
  std::pair<varmap::varmap_ptr_t, std::string> _getParentAndName(const std::string& key) const;

  varmap::varmap_ptr_t _data;
  std::unordered_map<std::string, varmap::varmap_ptr_t> _annotations;
};

using varmap_property_model_ptr_t = std::shared_ptr<VarMapPropertyModel>;

} // namespace ork::ui
