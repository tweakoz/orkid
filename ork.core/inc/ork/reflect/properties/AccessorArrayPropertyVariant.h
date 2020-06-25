////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 

#pragma once

#include <ork/reflect/IObjectArrayProperty.h>

#include <ork/config/config.h>

namespace ork { namespace reflect {

class  AccessorArrayPropertyVariant : public IObjectArrayProperty
{
	static void GetClassStatic(); // Kill inherited GetClassStatic()
public:
	AccessorArrayPropertyVariant(
		bool (Object::*serialize_item)(ISerializer &, size_t) const,
		bool (Object::*deserialize_item)(IDeserializer &, size_t),
		size_t (Object::*count)() const,
		bool (Object::*resize)(size_t));

private:
    /*virtual*/ bool SerializeItem(ISerializer &, const Object *, size_t) const;
    /*virtual*/ bool DeserializeItem(IDeserializer &, Object *, size_t) const;
	/*virtual*/ size_t Count(const Object *) const;
	/*virtual*/ bool Resize(Object *, size_t) const;

	bool (Object::*mSerializeItem)(ISerializer &, size_t) const;
	bool (Object::*mDeserializeItem)(IDeserializer &, size_t);
	size_t (Object::*mCount)() const;
	bool (Object::*mResize)(size_t);
};

} }

