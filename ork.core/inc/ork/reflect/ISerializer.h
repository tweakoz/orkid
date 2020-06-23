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

namespace ork { namespace rtti {
class ICastable;
}} // namespace ork::rtti

namespace ork { namespace reflect {

class IProperty;
class IObjectProperty;
class IObjectArrayProperty;
class Command;
// typedef ork::Object Serializable;

class ISerializer {
public:
  virtual bool Serialize(const bool&)                     = 0;
  virtual bool Serialize(const char&)                     = 0;
  virtual bool Serialize(const short&)                    = 0;
  virtual bool Serialize(const int&)                      = 0;
  virtual bool Serialize(const long&)                     = 0;
  virtual bool Serialize(const float&)                    = 0;
  virtual bool Serialize(const double&)                   = 0;
  virtual bool Serialize(const rtti::ICastable*)          = 0;
  virtual bool serializeObject(rtti::castable_constptr_t) = 0;
  virtual bool Serialize(const PieceString&)              = 0;
  virtual void Hint(const PieceString&)                   = 0;
  virtual void Hint(const PieceString&, intptr_t ival)    = 0;

  virtual bool Serialize(const IProperty*)                      = 0;
  virtual bool Serialize(const IObjectProperty*, const Object*) = 0;

  virtual bool SerializeData(unsigned char*, size_t) = 0;

  virtual bool ReferenceObject(const rtti::ICastable*) = 0;
  virtual bool BeginCommand(const Command&)            = 0;
  virtual bool EndCommand(const Command&)              = 0;

  virtual ~ISerializer();
};

}} // namespace ork::reflect
