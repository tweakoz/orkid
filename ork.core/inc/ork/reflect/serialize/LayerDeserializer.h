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

class LayerDeserializer : public IDeserializer
{
public:
	LayerDeserializer(IDeserializer &deserializer);

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

protected:
    IDeserializer &mDeserializer;
	const Command *mCurrentCommand;
};

inline
LayerDeserializer::LayerDeserializer(IDeserializer &deserializer)
	: mDeserializer(deserializer)
	, mCurrentCommand(NULL)
{
}

inline
bool LayerDeserializer::Deserialize(bool &value)
{
	return mDeserializer.Deserialize(value);
}

inline
bool LayerDeserializer::Deserialize(char &value)
{
	return mDeserializer.Deserialize(value);
}

inline
bool LayerDeserializer::Deserialize(short &value)
{
	return mDeserializer.Deserialize(value);
}

inline
bool LayerDeserializer::Deserialize(int &value)
{
	return mDeserializer.Deserialize(value);
}

inline
bool LayerDeserializer::Deserialize(long &value)
{
	return mDeserializer.Deserialize(value);
}

inline
bool LayerDeserializer::Deserialize(float &value)
{
	return mDeserializer.Deserialize(value);
}

inline
bool LayerDeserializer::Deserialize(double &value)
{
	return mDeserializer.Deserialize(value);
}

inline
bool LayerDeserializer::Deserialize(rtti::ICastable *&value)
{
	return mDeserializer.Deserialize(value);
}


inline
bool LayerDeserializer::Deserialize(const IProperty *prop)
{
	return prop->Deserialize(*this);
}

inline
bool LayerDeserializer::Deserialize(const IObjectProperty *prop, Object *object)
{
	return prop->Deserialize(*this, object);
}

inline
bool LayerDeserializer::Deserialize(const rtti::Category *category, rtti::ICastable *&object)
{
	return category->DeserializeReference(*this, object);
}

inline
bool LayerDeserializer::Deserialize(MutableString &text)
{
	return mDeserializer.Deserialize(text);
}
 
inline
bool LayerDeserializer::Deserialize(ResizableString &text)
{
	return mDeserializer.Deserialize(text);
}
 
inline
bool LayerDeserializer::DeserializeData(unsigned char *data, size_t size)
{
	return mDeserializer.DeserializeData(data, size);
}


inline
bool LayerDeserializer::ReferenceObject(rtti::ICastable *object)
{
	return mDeserializer.ReferenceObject(object);
}

inline
bool LayerDeserializer::BeginCommand(Command &command)
{
	const Command *previous_command = mCurrentCommand;

	command.PreviousCommand() = previous_command;
	
	if(mDeserializer.BeginCommand(command))
	{
		mCurrentCommand = &command;
		OrkAssert(command.PreviousCommand() == previous_command);
		return true;
	}

	return false;
}

inline
bool LayerDeserializer::EndCommand(const Command &command)
{	
	OrkAssert(mCurrentCommand == &command);
	
	mCurrentCommand = mCurrentCommand->PreviousCommand();
	
	return mDeserializer.EndCommand(command);
}

} } }

