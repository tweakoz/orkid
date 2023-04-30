////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/rtti/RTTI.h>
#include <ork/rtti/Class.h>
#include <ork/config/config.h>

namespace ork { namespace reflect {
class ISerializer;
class IDeserializer;
}} // namespace ork::reflect

namespace ork { namespace rtti {

class Category : public Class {
  RttiDeclareExplicit(Category, Class, NamePolicy, Category); //
  public: //
  Category(const RTTIData& data) //
      : Class(data) { //
  }
  
  inline void make_abstract() override {
  }
};

}} // namespace ork::rtti
