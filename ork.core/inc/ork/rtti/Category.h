////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 

#pragma once

#include <ork/rtti/RTTI.h>
#include <ork/rtti/Class.h>

#include <ork/config/config.h>

namespace ork { namespace reflect {
  class ISerializer;
  class IDeserializer;
} }

namespace ork { namespace rtti {

class  Category : public Class
{
	RttiDeclareExplicit(Category, Class, NamePolicy, Category)
public:
	Category(const RTTIData &data)
		: Class(data)
	{}

	virtual bool SerializeReference(reflect::ISerializer &, const ICastable *) const;
	virtual bool DeserializeReference(reflect::IDeserializer &, ICastable *&) const;
};

} }

