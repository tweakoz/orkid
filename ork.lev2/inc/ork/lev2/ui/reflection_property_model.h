////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/lev2/ui/property_sheet_model.h>
#include <ork/object/Object.h>
#include <ork/reflect/Description.h>
#include <ork/reflect/properties/ObjectProperty.h>
#include <ork/reflect/properties/IMap.h>

namespace ork::ui {

////////////////////////////////////////////////////////////////////
// ReflectionPropertySheetModel: PropertySheetModel that enumerates
// properties from Orkid's C++ reflection system (Description chain).
// Set an object and it auto-generates the property tree.
////////////////////////////////////////////////////////////////////

struct ReflectionPropertySheetModel : public PropertySheetModel {
  ReflectionPropertySheetModel();
  ~ReflectionPropertySheetModel() override = default;

  void setObject(object_ptr_t obj);
  object_ptr_t getObject() const;

  // Map property support (overrides from PropertySheetModel)
  bool isMapProperty(const std::string& key) const override;
  bool isMapConst(const std::string& key) const override;
  void addMapElement(const std::string& key, const std::string& name) override;
  void removeMapElement(const std::string& key, const std::string& name) override;

  // Null object map entry / factory support
  bool isNullObjectMapEntry(const std::string& key) const override;
  std::vector<std::string> getFactoryClasses(const std::string& key) const override;
  void setMapElementFromFactory(const std::string& key, const std::string& class_name) override;

  // Null direct object property / factory support
  bool isNullDirectObjectEntry(const std::string& key) const override;
  std::vector<std::string> getDirectObjectFactoryClasses(const std::string& key) const override;
  void setDirectObjectFromFactory(const std::string& key, const std::string& class_name) override;

  // Per-key override support (for properties like object pointers
  // that need custom get/set/choices from Python)
  struct KeyOverride {
    PropertyType type;
    std::function<svar128_t()> getter;
    std::function<void(svar128_t)> setter;
    std::function<std::vector<std::string>()> choices;
  };

  void addKeyOverride(const std::string& key, const KeyOverride& ovr);
  void clearKeyOverrides();

  // PropertySheetModel interface
  std::vector<std::string> getChildren(const std::string& parent_key) const override;
  std::string getDisplayName(const std::string& key) const override;
  bool hasChildren(const std::string& key) const override;
  svar128_t getValue(const std::string& key) const override;
  void setValue(const std::string& key, svar128_t value) override;
  PropertyType getPropertyType(const std::string& key) const override;
  varmap::varmap_ptr_t getAnnotations(const std::string& key) const override;
  std::vector<std::string> getChoices(const std::string& key) const override;

private:
  std::unordered_map<std::string, KeyOverride> _key_overrides;
  object_ptr_t _object;

  struct PropertyEntry {
    std::string name;
    std::string full_key;     // e.g. "DagNodeData/TransformNode"
    std::string parent_key;   // "" for root
    const reflect::ObjectProperty* property = nullptr;
    PropertyType type = PropertyType::Unknown;
    // For DirectObjectBase properties and object-valued map entries
    object_ptr_t sub_object;
    // For map entries
    bool is_map_entry = false;
    svar128_t map_key;                         // The key within the map
    const reflect::IMap* map_property = nullptr; // The map property (for mutations)
    object_ptr_t map_owner;                    // The object that owns the map property
    // For map property nodes (the parent of map entries)
    bool is_map_property = false;
    // For map entries whose object value is null (needs factory)
    bool is_null_object_entry = false;
    // For DirectObjectBase properties whose sub_object is null (needs factory)
    bool is_null_direct_object = false;
  };

  std::vector<PropertyEntry> _entries;
  std::unordered_map<std::string, size_t> _by_key;  // key -> index into _entries

  void _buildPropertyList();
  void _addPropertiesFromObject(object_ptr_t obj, const std::string& parent_key);
  void _addPropertiesFromDescription(const reflect::Description* desc, object_ptr_t obj, const std::string& parent_key);
  void _addMapElements(const reflect::IMap* map_prop, object_ptr_t obj, const std::string& parent_key);
  PropertyType _mapPropertyType(const reflect::ObjectProperty* prop) const;
  bool _isHidden(const reflect::ObjectProperty* prop) const;
};

using reflection_property_model_ptr_t = std::shared_ptr<ReflectionPropertySheetModel>;

} // namespace ork::ui
