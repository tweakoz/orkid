////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 

#pragma once

#include <ork/rtti/Category.h>

#include <ork/config/config.h>

namespace ork { namespace rtti { class ICastable; class RTTIData; } }
namespace ork { namespace reflect { class ISerializer; class IDeserializer; } }

namespace ork { namespace object {

class  ObjectCategory : public rtti::Category
{
public:
	ObjectCategory(const rtti::RTTIData &);
private:
	/*virtual*/ bool SerializeReference(reflect::ISerializer &, const rtti::ICastable *) const;
	/*virtual*/ bool DeserializeReference(reflect::IDeserializer &, rtti::ICastable *&) const;
};

} }
