////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 

#pragma once

///////////////////////////////////////////////////////////////////////////////
#include <ork/kernel/string/MutableString.h>
#include <ork/kernel/string/PieceString.h>
#include <ork/reflect/serialize/XMLDeserializer.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace stream {
///////////////////////////////////////////////////////////////////////////////

class IInputStream;

///////////////////////////////////////////////////////////////////////////////
} } // namespace ork::stream
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace file {
///////////////////////////////////////////////////////////////////////////////

bool ReadLine(ork::stream::InputStreamBuffer<128>& file, char *buffer, int maxlen);

///////////////////////////////////////////////////////////////////////////////

template<typename IsDelim>
void Word(ork::MutableString result, const char *buffer, int &index, int buf_size, IsDelim delim)
{
	while(index < buf_size && buffer[index] && delim(buffer[index])) index++;
	int wordstart = index;
	while(index < buf_size && buffer[index] && !delim(buffer[index])) index++;
	int wordend = index;
	while(index < buf_size && buffer[index] && delim(buffer[index])) index++;
	result = ork::PieceString(buffer + wordstart, ork::PieceString::size_type(wordend - wordstart));
}

///////////////////////////////////////////////////////////////////////////////
void Word(ork::MutableString result, const char *buffer, int &index, int buf_size);
///////////////////////////////////////////////////////////////////////////////

class FlatDeserializer : public ork::reflect::serialize::XMLDeserializer
{
public:
	FlatDeserializer(ork::stream::IInputStream &stream)
		: ork::reflect::serialize::XMLDeserializer(stream)
	{}

	/*virtual*/ bool Deserialize(ork::rtti::ICastable *&value)
	{
		long pointer;
		bool result = ork::reflect::serialize::XMLDeserializer::Deserialize(pointer);
		value = reinterpret_cast<ork::rtti::ICastable *>(pointer);
		return result;
	}
};

///////////////////////////////////////////////////////////////////////////////
} } // namespace ork::file
///////////////////////////////////////////////////////////////////////////////
