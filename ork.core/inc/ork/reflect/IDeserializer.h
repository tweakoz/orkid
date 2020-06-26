////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/kernel/string/ResizableString.h>
#include <ork/kernel/string/MutableString.h>
#include <ork/orktypes.h>
#include <unordered_map>
#include <boost/uuid/uuid.hpp>

namespace ork::reflect {

class ObjectProperty;
class Command;

struct IDeserializer {
  virtual void deserialize(bool&)   = 0;
  virtual void deserialize(char&)   = 0;
  virtual void deserialize(short&)  = 0;
  virtual void deserialize(int&)    = 0;
  virtual void deserialize(long&)   = 0;
  virtual void deserialize(float&)  = 0;
  virtual void deserialize(double&) = 0;

  // virtual void deserialize(const AbstractProperty*)                           = 0;
  virtual void deserializeSharedObject(object_ptr_t&)                         = 0;
  virtual void deserializeObjectProperty(const ObjectProperty*, object_ptr_t) = 0;

  virtual void deserialize(MutableString&)       = 0;
  virtual void deserialize(ResizableString&)     = 0;
  virtual void deserializeData(uint8_t*, size_t) = 0;

  void referenceObject(object_ptr_t);
  virtual void beginCommand(Command&)     = 0;
  virtual void endCommand(const Command&) = 0;
  virtual void Hint(const PieceString&) {
  }

  void trackObject(boost::uuids::uuid id, object_ptr_t instance);
  object_ptr_t findTrackedObject(boost::uuids::uuid id) const;
  virtual ~IDeserializer();

  const reflect::ObjectProperty* _currentProperty = nullptr;
  object_ptr_t _currentObject                     = nullptr;
  const Command* _currentCommand                  = nullptr;
  using trackervect_t                             = std::unordered_map<std::string, object_ptr_t>;
  trackervect_t _reftracker;
};

} // namespace ork::reflect
