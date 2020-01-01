////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 


#include <ork/pch.h>
#include <ork/orkconfig.h>

#include <ork/reflect/BidirectionalSerializer.h>
#include <ork/file/path.h>
#include <ork/object/Object.h>

namespace ork { namespace rtti { class ICastable; } }

namespace ork { namespace reflect {

typedef rtti::ICastable *ICastablePointer;
typedef ork::Object*	 ObjectPointer;

template<typename T>
inline void BidirectionalSerializer::Serialize(const T &value)
{
	bool result = mSerializer->Serialize(value);

	if(false == result) Fail();
}

template<typename T>
inline void BidirectionalSerializer::Deserialize(T &value)
{
	bool result = mDeserializer->Deserialize(value);

	if(false == result) Fail();
}

template<typename T>
void Serialize(const T *in, T *out, BidirectionalSerializer &bidi)
{
	if(bidi.Serializing())
	{
		bidi.Serialize(*in);
	}
	else
	{
		bidi.Deserialize(*out);
	}
}

#define FOREACH_BASIC_SERIALIZATION_TYPE(MACRO) \
    MACRO(bool);   \
    MACRO(char);   \
    MACRO(short);  \
    MACRO(int);    \
    MACRO(long);   \
    MACRO(float);  \
    MACRO(double);

#define INSTANTIATE_SERIALIZE_FUNCTION(Type) \
	template void Serialize<Type>(const Type *, Type *, BidirectionalSerializer &);

FOREACH_BASIC_SERIALIZATION_TYPE(INSTANTIATE_SERIALIZE_FUNCTION)

template<> void Serialize(rtti::ICastable *const *in, rtti::ICastable **out, BidirectionalSerializer &bidi)
{
	if(bidi.Serializing())
	{
		bidi.Serialize(*in);
	}
	else
	{
		rtti::ICastable *result = NULL;
		bidi.Deserialize(result);
		*out = result;
	}
}

template<> void Serialize(const rtti::ICastable *const *in, const rtti::ICastable **out, BidirectionalSerializer &bidi)
{
	if(bidi.Serializing())
	{
		bidi.Serialize(*in);
	}
	else
	{
		rtti::ICastable *result = NULL;
		bidi.Deserialize(result);
		*out = result;
	}
}


template<> void Serialize(const std::string *in, std::string *out, BidirectionalSerializer &bidi)
{
	if(bidi.Serializing())
	{
		bidi.Serialize(ork::PieceString(in->c_str()));
	}
	else
	{
		ResizableString result;
		bidi.Deserialize(result);
		*out = result.c_str();
	}
}
template<> void Serialize(const file::Path *in, file::Path *out, BidirectionalSerializer &bidi)
{
	if(bidi.Serializing())
	{
		bidi.Serialize(ork::PieceString(in->c_str()));
	}
	else
	{
		ResizableString result;
		bidi.Deserialize(result);
		*out = result.c_str();
	}
}

template<> void Serialize(const Char4 *in, Char4 *out, BidirectionalSerializer &bidi)
{
	if(bidi.Serializing())
	{
		bidi.Serialize(ork::PieceString(in->c_str()));
	}
	else
	{
		ResizableString result;
		bidi.Deserialize(result);
		OrkAssert( result.length()<=4 );
		out->SetCString( result.c_str() );
	}
}
template<> void Serialize(const Char8 *in, Char8 *out, BidirectionalSerializer &bidi)
{
	if(bidi.Serializing())
	{
		bidi.Serialize(ork::PieceString(in->c_str()));
	}
	else
	{
		ResizableString result;
		bidi.Deserialize(result);
		OrkAssert( result.length()<=8 );
		out->SetCString( result.c_str() );
	}
}

} }
