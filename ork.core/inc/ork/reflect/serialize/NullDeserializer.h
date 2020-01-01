////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 

#pragma once

#include <ork/reflect/IDeserializer.h>
#include <ork/reflect/IProperty.h>
#include <ork/reflect/IObjectProperty.h>
#include <ork/reflect/Command.h>

#include <ork/rtti/Category.h>

namespace ork { namespace stream { class IOutputStream; } }

namespace ork { namespace reflect { namespace serialize {

class NullDeserializer : public IDeserializer
{
public:
    /*virtual*/ bool Deserialize(bool &);
	/*virtual*/ bool Deserialize(char &);
    /*virtual*/ bool Deserialize(short &);
    /*virtual*/ bool Deserialize(int &);
    /*virtual*/ bool Deserialize(long &);
    /*virtual*/ bool Deserialize(float &);
    /*virtual*/ bool Deserialize(double &);
	/*virtual*/ bool Deserialize(rtti::ICastable *&);

	/*virtual*/ bool Deserialize(const IProperty *);
	/*virtual*/ bool Deserialize(const IObjectProperty *, Object *);

    /*virtual*/ bool Deserialize(MutableString &); 
    /*virtual*/ bool Deserialize(ResizableString &); 
    /*virtual*/ bool DeserializeData(unsigned char *, size_t);

    /*virtual*/ bool ReferenceObject(rtti::ICastable *);
    /*virtual*/ bool BeginCommand(Command &);
    /*virtual*/ bool EndCommand(const Command &);
	
protected:
	/*virtual*/ bool Deserialize(const rtti::Category *, rtti::ICastable *&);
};

inline
bool NullDeserializer::Deserialize(bool &value)
{
	return false;
}

inline
bool NullDeserializer::Deserialize(char &value)
{
	return false;
}

inline
bool NullDeserializer::Deserialize(short &value)
{
	return false;
}

inline
bool NullDeserializer::Deserialize(int &value)
{
	return false;
}

inline
bool NullDeserializer::Deserialize(long &value)
{
	return false;
}

inline
bool NullDeserializer::Deserialize(float &value)
{
	return false;
}

inline
bool NullDeserializer::Deserialize(double &value)
{
	return false;
}

inline
bool NullDeserializer::Deserialize(rtti::ICastable *&value)
{
	return false;
}


inline
bool NullDeserializer::Deserialize(const IProperty *prop)
{
	return prop->Deserialize(*this);
}

inline
bool NullDeserializer::Deserialize(const IObjectProperty *prop, Object *object)
{
	return prop->Deserialize(*this, object);
}

inline
bool NullDeserializer::Deserialize(const rtti::Category *category, rtti::ICastable *&object)
{
	return category->DeserializeReference(*this, object);
}

inline
bool NullDeserializer::Deserialize(MutableString &text)
{
	return false;
}
 
inline
bool NullDeserializer::Deserialize(ResizableString &text)
{
	return false;
}
 
inline
bool NullDeserializer::DeserializeData(unsigned char *data, size_t size)
{
	return false;
}


inline
bool NullDeserializer::ReferenceObject(rtti::ICastable *object)
{
	return true;
}

inline
bool NullDeserializer::BeginCommand(Command &command)
{
	return true;
}

inline
bool NullDeserializer::EndCommand(const Command &command)
{	
	return true;
}

} } }

