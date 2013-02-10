////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 

#pragma once

#include <ork/reflect/Serializable.h>
#include <ork/rtti/RTTI.h>
#include <ork/kernel/tempstring.h>
#include <ork/kernel/prop.h>

#include <ork/config/config.h>

namespace ork { namespace reflect {

class ISerializer;
class IDeserializer;

class  IObjectProperty : public rtti::ICastable
{
	DECLARE_TRANSPARENT_CASTABLE(IObjectProperty, rtti::ICastable)
	orklut<ConstString,ConstString> mAnnotations;
public:

    virtual bool Deserialize(IDeserializer &, Object *) const = 0;
    virtual bool Serialize(ISerializer &, const Object *) const = 0;
	void			Annotate( const ConstString& key, const ConstString& val ) { mAnnotations.AddSorted(key,val); }
	ConstString		GetAnnotation( const ConstString& key ) const
	{
		ConstString rval = "";
		orklut<ConstString,ConstString>::const_iterator it = mAnnotations.find(key);
		if( it!=mAnnotations.end() )
		{
			rval = (*it).second;
		}
		return rval;
	}
	IObjectProperty() {}
};


} }

