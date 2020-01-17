////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 


#include <ork/pch.h>

#include <ork/kernel/string/PieceString.h>
#include <ork/kernel/string/ConstString.h>

namespace ork {

PieceString::PieceString(const char *s)
: mpString((s!=0)?s:"")
, mLength((s!=0)?::strlen(s):0)
{
}

PieceString::PieceString(const char *s, PieceString::size_type length)
	: mpString(s)
	, mLength(length)
{
}

PieceString::PieceString()
	: mpString(0)
	, mLength(0)
{
}

const char *PieceString::data() const { return mpString; }
PieceString::size_type PieceString::length() const { return mLength; }
PieceString::size_type PieceString::size() const { return mLength; }

PieceString::const_iterator PieceString::begin() const { return data(); }
PieceString::const_iterator PieceString::end() const { return data() + size(); }

bool PieceString::empty() const { return 0 == size(); }
	
bool PieceString::operator ==(const PieceString &other) const { return compare(other) == 0; }
bool PieceString::operator <=(const PieceString &other) const { return compare(other) <= 0; }
bool PieceString::operator >=(const PieceString &other) const { return compare(other) >= 0; }
bool PieceString::operator < (const PieceString &other) const { return compare(other) <  0; }
bool PieceString::operator > (const PieceString &other) const { return compare(other) >  0; }
bool PieceString::operator !=(const PieceString &other) const { return compare(other) != 0; }
	
PieceString PieceString::substr(PieceString::size_type index, PieceString::size_type amount) const
{
	if(length() < index || index == npos) index = length();
	if(npos == amount || amount > length() - index) amount = length() - index;
	return PieceString(data() + index, amount);
}
	
PieceString::size_type PieceString::find(char ch, PieceString::size_type pos) const
{
	while(pos < length())
	{
		if(mpString[pos] == ch) return pos;
		pos++;
	}

	return npos;
}

PieceString::size_type PieceString::find(const char *s, PieceString::size_type pos, PieceString::size_type len) const
{
	if(len == npos) len = ::strlen(s);
	while(pos < length())
	{
		if(::strncmp(mpString + pos, s, len) == 0) return pos;
		pos++;
	}
	return npos;
}

PieceString::size_type PieceString::find(const PieceString &s, PieceString::size_type pos) const
{
	return find(s.data(), pos, s.length());
}

PieceString::size_type PieceString::rfind(const PieceString &s, PieceString::size_type pos) const
{
	return rfind(s.data(), pos, s.length());
}

PieceString::size_type PieceString::rfind(char ch, PieceString::size_type pos) const
{
	if(empty()) return npos;

	if(npos == pos)
	{
		pos = length() - 1;
	}

	do
	{
		if(mpString[pos] == ch) return pos;
		pos--;
	}
	while(pos != 0);

	return npos;
}

PieceString::size_type PieceString::rfind(const char *s, PieceString::size_type pos, PieceString::size_type len) const
{
	if(empty()) return npos;

	if(npos == pos)
	{
		pos = length() - 1;
	}

	if(len == npos) len = ::strlen(s);

	do
	{
		if(::strncmp(mpString + pos, s, len) == 0) return pos;
		pos--;
	}
	while(pos != 0);

	return npos;
}

PieceString::size_type PieceString::find_first_of(char ch, PieceString::size_type pos) const
{
	return find(ch, pos);
}

PieceString::size_type PieceString::find_last_of(char ch, PieceString::size_type pos) const
{
	return rfind(ch, pos);
}

PieceString::size_type PieceString::find_first_of(const ConstString &s, PieceString::size_type pos) const
{	
	while(pos < length())
	{
		if(::strchr(s.c_str(), mpString[pos])) return pos;
		pos++;
	}
	return npos;
}

PieceString::size_type PieceString::find_last_of(const ConstString &s, PieceString::size_type pos) const
{
	if(npos == pos)
	{
		pos = length() - 1;
	}
	while(pos < length())
	{
		if(::strchr(s.c_str(), mpString[pos])) return pos;
		pos--;
	}

	return npos;
}

int PieceString::compare(const PieceString &other) const
{
	size_type common_length = length() < other.length() ? length() : other.length();
	int compare = ::memcmp(data(), other.data(), common_length);
	if(compare == 0) compare = int(length() - other.length());
	return compare < 0 ? -1 : compare > 0 ? 1 : 0;
}

}
