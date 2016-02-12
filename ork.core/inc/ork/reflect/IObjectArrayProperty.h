////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 

#pragma once

#include <cstddef>
#include <ork/reflect/IObjectProperty.h>

#include <ork/config/config.h>

namespace ork { namespace reflect {

class  IObjectArrayProperty : public IObjectProperty
{
	DECLARE_TRANSPARENT_CASTABLE(IObjectArrayProperty, IObjectProperty)
public:
    virtual bool DeserializeItem(IDeserializer &, Object *, size_t) const = 0;
    virtual bool SerializeItem(ISerializer &, const Object *, size_t) const = 0;
	virtual size_t Count(const Object *) const = 0;
	virtual bool Resize(Object *obj, size_t size) const = 0;
private:
    bool Deserialize(IDeserializer &, Object *) const override;
    bool Serialize(ISerializer &, const Object *) const override;
protected:
	IObjectArrayProperty() {}
};

} }

