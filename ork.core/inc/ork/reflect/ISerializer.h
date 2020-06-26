////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/orktypes.h>
#include <ork/kernel/string/PieceString.h>
#include <stdint.h>
#include <unordered_set>
#include <boost/uuid/uuid.hpp>

namespace ork { namespace rtti {
class ICastable;
}} // namespace ork::rtti

namespace ork { namespace reflect {

class AbstractProperty;
class ObjectProperty;
class IArray;
class Command;
// typedef ork::Object Serializable;

class ISerializer {
public:
  void referenceObject(object_constptr_t);
  void referenceObject(object_rawconstptr_t);

  virtual void Serialize(const bool&)        = 0;
  virtual void Serialize(const char&)        = 0;
  virtual void Serialize(const short&)       = 0;
  virtual void Serialize(const int&)         = 0;
  virtual void Serialize(const long&)        = 0;
  virtual void Serialize(const float&)       = 0;
  virtual void Serialize(const double&)      = 0;
  virtual void Serialize(const PieceString&) = 0;

  virtual void Serialize(const AbstractProperty*) = 0;

  virtual void serializeObject(const rtti::ICastable*) = 0;
  inline void serializeSharedObject(rtti::castable_constptr_t obj) {
    return serializeObject(obj.get());
  }
  virtual void serializeObjectProperty(const ObjectProperty*, const Object*) = 0;

  virtual void SerializeData(unsigned char*, size_t) = 0;

  virtual void Hint(const PieceString&)                = 0;
  virtual void Hint(const PieceString&, intptr_t ival) = 0;

  virtual void beginCommand(const Command&) = 0;
  virtual void endCommand(const Command&)   = 0;

  virtual ~ISerializer();

  std::unordered_set<std::string> _serialized;
  const Command* _currentCommand;
};

}} // namespace ork::reflect
