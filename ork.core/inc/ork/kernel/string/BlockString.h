////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 

#pragma once

///////////////////////////////////////////////////////////////////////////////

#include <ork/kernel/string/ConstString.h>
#include <ork/kernel/string/PieceString.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork {
///////////////////////////////////////////////////////////////////////////////

class StringBlock;

///
/// BlockStrings are good for serializing strings
/// when read/write order is not guarenteed.
///
/// Replaces old PoolString in the less common usecase.
///
/// BlockStrings' data lives in ResizableString in a string block,
/// and therefore can be moved.
///

class BlockString
{
public:
	/// Constructs an empty block string.
	BlockString();

	int Index() const;

	/// @return a chararcter pointer to the data.
	const char *data() const;
	/// @return a chararcter pointer to NUL terminated data.
	const char *c_str() const;

	/// Casts this string to a PieceString
	operator PieceString() const;
	/// Casts this string to a ConstString
	operator ConstString() const;

	bool operator ==(const BlockString &) const;
	bool operator <=(const BlockString &) const;
	bool operator >=(const BlockString &) const;
	bool operator < (const BlockString &) const;
	bool operator > (const BlockString &) const;
	bool operator !=(const BlockString &) const;

	/// @returns false if the string is not set.
	operator bool() const;
	int compare(const BlockString &) const;
private:
	friend class StringBlock;

	/// Constructs a pool string from a character pointer, used by StringPool.
	BlockString(const StringBlock *, int);

	const StringBlock *mStringBlock; 
	int mStringIndex;
};

///////////////////////////////////////////////////////////////////////////////
}
///////////////////////////////////////////////////////////////////////////////
