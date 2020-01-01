////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 

#pragma once

///////////////////////////////////////////////////////////////////////////////

#include <ork/kernel/string/PieceString.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork {
///////////////////////////////////////////////////////////////////////////////

class ConstString
{
public:
	typedef PieceString::size_type size_type;
	static const size_type npos = size_type(PieceString::npos);

	ConstString(const char *s);
	ConstString();

	const char *c_str() const;
	const char *data() const;
	size_type length() const;
	size_type size() const;
	
	typedef const char *const_iterator;
	const_iterator begin() const;
	const_iterator end() const;

	size_type find(char ch, size_type pos = 0) const;
	size_type find(const char *s, size_type pos = 0, size_type len = npos) const;
	size_type find(const PieceString &s, size_type pos = 0) const;
	size_type find_first_of(char ch, size_type pos = 0) const;
	size_type find_first_of(const char *s, size_type pos = 0) const;

	size_type rfind(char ch, size_type pos = npos) const;
	size_type rfind(const char *s, size_type pos = npos, size_type len = npos) const;
	size_type rfind(const PieceString &s, size_type pos = npos) const;
	size_type find_last_of(char ch, size_type pos = npos) const;
	size_type find_last_of(const char *s, size_type pos = npos) const;
	
	PieceString substr(size_type index, size_type amount = npos) const;

	bool empty() const;

	operator PieceString() const;

	bool operator ==(const PieceString &other) const;
	bool operator <=(const PieceString &other) const;
	bool operator >=(const PieceString &other) const;
	bool operator < (const PieceString &other) const;
	bool operator > (const PieceString &other) const;
	bool operator !=(const PieceString &other) const;
private:
	const char *mpString;
	size_type mLength;
};

///////////////////////////////////////////////////////////////////////////////
}
///////////////////////////////////////////////////////////////////////////////
