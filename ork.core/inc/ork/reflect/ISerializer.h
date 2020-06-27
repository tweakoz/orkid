////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/orktypes.h>
#include <ork/kernel/string/PieceString.h>
#include <ork/kernel/svariant.h>
#include <stdint.h>
#include <unordered_set>
#include <boost/uuid/uuid.hpp>

namespace ork { namespace rtti {
class ICastable;
}} // namespace ork::rtti

namespace ork { namespace reflect {

class ObjectProperty;
class IArray;
class Command;
// typedef ork::Object Serializable;

struct ISerializer {
public:
  using hintvar_t = svar64_t;

  void referenceObject(object_constptr_t);

  virtual void serialize(const bool&)        = 0;
  virtual void serialize(const char&)        = 0;
  virtual void serialize(const short&)       = 0;
  virtual void serialize(const int&)         = 0;
  virtual void serialize(const long&)        = 0;
  virtual void serialize(const float&)       = 0;
  virtual void serialize(const double&)      = 0;
  virtual void serialize(const PieceString&) = 0;

  virtual void serializeSharedObject(object_constptr_t)                          = 0;
  virtual void serializeObjectProperty(const ObjectProperty*, object_constptr_t) = 0;

  virtual void serializeData(const uint8_t*, size_t) = 0;

  virtual void Hint(const PieceString&, hintvar_t val) = 0;

  virtual void beginCommand(const Command&) = 0;
  virtual void endCommand(const Command&)   = 0;

  virtual ~ISerializer();

  std::unordered_set<std::string> _reftracker;
  const Command* _currentCommand = nullptr;
};

}} // namespace ork::reflect
