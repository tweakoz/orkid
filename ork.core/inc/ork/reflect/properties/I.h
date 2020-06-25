////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/kernel/prop.h>
#include <ork/kernel/tempstring.h>
#include <ork/reflect/Serializable.h>
#include <ork/rtti/RTTI.h>

#include <ork/config/config.h>

namespace ork { namespace reflect {

class ISerializer;
class IDeserializer;

class I : public rtti::ICastable {
  DECLARE_TRANSPARENT_CASTABLE(I, rtti::ICastable)

public:

  typedef ork::svar64_t anno_t;

  virtual bool Deserialize(IDeserializer&, Object*) const   = 0;
  virtual bool Serialize(ISerializer&, const Object*) const = 0;
  /////////////////////////////////////////////////////////////////
  // old string only annotations
  /////////////////////////////////////////////////////////////////
  void Annotate(const ConstString& key, const ConstString& val) {
    anno_t wrapped;
    wrapped.Set<ConstString>(val);
    _annotations.AddSorted(key, wrapped);
  }
  ConstString GetAnnotation(const ConstString& key) const {
    ConstString rval                                    = "";
    auto it = _annotations.find(key);
    if (it != _annotations.end()) {
      const anno_t& val = it->second;
      assert(val.IsA<ConstString>());
      rval = val.Get<ConstString>();
    }
    return rval;
  }
  /////////////////////////////////////////////////////////////////
  // loose annotations
  /////////////////////////////////////////////////////////////////
  void annotate(const ConstString& key, anno_t val) {
    _annotations.AddSorted(key, val);
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
  I() {}
private:
  orklut<ConstString, anno_t> _annotations;

};

}} // namespace ork::reflect
