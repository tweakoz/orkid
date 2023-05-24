////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/kernel/prop.h>
#include <ork/kernel/tempstring.h>
#include <ork/kernel/svariant.h>
#include <ork/reflect/ISerializer.h>
#include <ork/reflect/IDeserializer.h>
#include <ork/rtti/RTTI.h>

#include <ork/config/config.h>

namespace ork::reflect {

////////////////////////////////////////////////////////////////////////////////

struct PropGroup {
  PropGroup(std::string name, std::string plist) : _name(name), _proplist(plist) {}
  std::string _name;
  std::string _proplist;
};
struct PropGroupList {
  void addGroup(std::string n, std::string p){
    _groups.push_back(PropGroup(n, p));
  }
  std::vector<PropGroup> _groups;
};
using group_list_ptr_t = std::shared_ptr<PropGroupList>;

////////////////////////////////////////////////////////////////////////////////
struct ObjectProperty {

  using anno_t = ork::svar64_t;

  virtual void deserialize(serdes::node_ptr_t) const          = 0;
  virtual void serialize(serdes::node_ptr_t) const = 0;
  /////////////////////////////////////////////////////////////////
  // old string only annotations
  /////////////////////////////////////////////////////////////////
  void Annotate(const ConstString& key, const ConstString& val) {
    anno_t wrapped;
    wrapped.set<ConstString>(val);
    _annotations.AddSorted(key, wrapped);
  }
  /////////////////////////////////////////////////////////////////
  ConstString GetAnnotation(const ConstString& key) const {
    ConstString rval = "";
    auto it          = _annotations.find(key);
    if (it != _annotations.end()) {
      const anno_t& val = it->second;
      assert(val.isA<ConstString>());
      rval = val.get<ConstString>();
    }
    return rval;
  }
  /////////////////////////////////////////////////////////////////
  // loose annotations
  /////////////////////////////////////////////////////////////////
  void annotate(const ConstString& key, const anno_t& val) {
    _annotations.AddSorted(key, val);
  }
  /////////////////////////////////////////////////////////////////
  template <typename T>
  attempt_cast_const<T> typedAnnotation(const ConstString& key) const {
    auto it = _annotations.find(key);
    if (it != _annotations.end()) {
      return it->second.tryAs<T>();
    }
    return attempt_cast_const<T>(nullptr);
  }
  anno_t annotation(const ConstString& key) const {
    anno_t rval(nullptr);
    auto it = _annotations.find(key);
    if (it != _annotations.end()) {
      rval = it->second;
    }
    return rval;
  }
  /////////////////////////////////////////////////////////////////
  ObjectProperty() {
  }
  /////////////////////////////////////////////////////////////////
  orklut<ConstString, anno_t> _annotations;
  std::string _name;
}; // namespace reflect
////////////////////////////////////////////////////////////////////////////////
} // namespace ork::reflect
