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

namespace ork { namespace rtti {
class ICastable;
}} // namespace ork::rtti

namespace ork {
class Object;
}

namespace ork { namespace reflect {

class AbstractProperty;
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

  virtual void deserialize(const AbstractProperty*)                      = 0;
  virtual void deserializeObject(rtti::ICastable*&)                      = 0;
  virtual void deserializeSharedObject(rtti::castable_ptr_t&)            = 0;
  virtual void deserializeObjectProperty(const ObjectProperty*, Object*) = 0;

  virtual void deserialize(MutableString&)             = 0;
  virtual void deserialize(ResizableString&)           = 0;
  virtual void deserializeData(unsigned char*, size_t) = 0;

  virtual void referenceObject(rtti::ICastable*) = 0;
  virtual void beginCommand(Command&)            = 0;
  virtual void endCommand(const Command&)        = 0;
  virtual void Hint(const PieceString&) {
  }

  virtual ~IDeserializer();

  const reflect::ObjectProperty* _currentProperty = nullptr;
  Object* _currentObject                          = nullptr;
  const Command* _currentCommand                  = nullptr;
};

}} // namespace ork::reflect
