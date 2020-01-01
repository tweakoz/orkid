////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 

#pragma once

#include <stddef.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork {
///////////////////////////////////////////////////////////////////////////////

class ConstString;

/// A PieceString is the most basic string type in the OrkString suite.
/// 
/// It consists of a pointer and a length.  It does not own its data.
/// It can and should provide any const std::string functions we need
/// that do not require a NUL terminator.
class PieceString
{
public:
	typedef size_t size_type;
	static const size_type npos = size_type(0xffffffff);
	typedef const char *const_iterator;

	/// Constructs a piece string from a character pointer to NUL terminated data.
	/// length is found by strlen()
	/// @param s the string to make into a piece string.
	PieceString(const char *s);
	/// Constructs a piece string from a character pointer, length provided.
	/// @param s the character data
	/// @param length the length of the string
	PieceString(const char *s, size_type length);
	/// Constructs an empty piece string with NULL data and no length.
	PieceString();

	/// @return a chararcter pointer to the data.
	const char *data() const;
	/// @return the current length of the string.
	size_type length() const;
	/// @return the current length of the string.
	size_type size() const;

	/// @return a const_iterator at the begining of the string.
	const_iterator begin() const;
	/// @return a const_iterator at the end of the string.
	const_iterator end() const;

	/// Finds a character in the string.
	/// @param ch the character to search for
	/// @param pos the position to start from
	/// @return index of ch in string or npos if not found.
	size_type find(char ch, size_type pos = 0) const;
	/// Finds a string in the string.
	/// @param s the string to search for
	/// @param pos the position to start from
	/// @return index of s in string or npos if not found.
	size_type find(const char *s, size_type pos = 0, size_type len = npos) const;
	/// Finds a string in the string.
	/// @param s the string to search for
	/// @param pos the position to start from
	/// @return index of s in string or npos if not found.
	size_type find(const PieceString &s, size_type pos = 0) const;
	/// Finds a character in the string.
	/// @param[in] ch the character to search for
	/// @param[in] pos the position to start from
	/// @return index of ch in string or npos if not found.
	size_type find_first_of(char ch, size_type pos = 0) const;
	/// Finds any of a set of characters in the string.
	/// @param s the set of characters to search for
	/// @param pos the position to start from
	/// @return index of s in string or npos if not found.
	size_type find_first_of(const ConstString &, size_type pos = 0) const;

	/// Finds a character in the string, in reverse.
	/// @param ch the character to search for
	/// @param pos the position to start from
	/// @return index of ch in string or npos if not found.
	size_type rfind(char ch, size_type pos = npos) const;
	/// Finds a string in the string, in reverse.
	/// @param s the string to search for
	/// @param pos the position to start from
	/// @return index of the begining of s in string or npos if not found.
	size_type rfind(const char *s, size_type pos = npos, size_type length = npos) const;
	/// Finds a string in the string, in reverse.
	/// @param s the string to search for
	/// @param pos the position to start from
	/// @return index of the begining of s in string or npos if not found.
	size_type rfind(const PieceString &s, size_type pos = npos) const;
	/// Finds a character in the string, in reverse.
	/// @param ch the character to search for
	/// @param pos the position to start from
	/// @return index of the last character in string matching ch or npos if not found.
	size_type find_last_of(char ch, size_type pos = npos) const;
	/// Finds any of a set of characters in the string, in reverse.
	/// @param s the set of characters to search for
	/// @param pos the position to start from
	/// @return index of the last character in string contained in s or npos if not found.
	size_type find_last_of(const ConstString &, size_type pos = npos) const;
	
	/// @param index the begining of the substring
	/// @param amount the maximum length of the substring
	/// @return a PieceString substring
	PieceString substr(size_type index, size_type amount = npos) const;

	/// @return true if the string's length is 0
	bool empty() const;

	/// @return true if the other string is equal.
	bool operator ==(const PieceString &other) const;
	/// @return true if the other string is <= (ASCII sort).
	bool operator <=(const PieceString &other) const;
	/// @return true if the other string is >= (ASCII sort).
	bool operator >=(const PieceString &other) const;
	/// @return true if the other string is < (ASCII sort).
	bool operator < (const PieceString &other) const;
	/// @return true if the other string is > (ASCII sort).
	bool operator > (const PieceString &other) const;
	/// @return true if the other string is unequal.
	bool operator !=(const PieceString &other) const;

	/// @return (+1, 0, or -1) if the string is
	///         (greater, equal, or less) than the other string. 
	int compare(const PieceString &other) const;
private:
	/// the character pointer for this string's data
	const char *mpString;
	/// the length of the string.
	size_type mLength;
};

///////////////////////////////////////////////////////////////////////////////
}
///////////////////////////////////////////////////////////////////////////////
