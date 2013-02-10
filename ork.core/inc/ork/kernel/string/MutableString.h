////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 

#pragma once

///////////////////////////////////////////////////////////////////////////////

#include <ork/kernel/string/PieceString.h>
#include <ork/kernel/string/ConstString.h>
#include <ork/orkstd.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork {
///////////////////////////////////////////////////////////////////////////////

/// A string <i>referencing</i> a fixed-length array for its data.
///
/// All OrkStrings operate in a manner compatible with std::string
/// wherever feasable in the functions implemented.  MutableString
/// have a maximum size, but the data they contain is on loan from
/// whatever constructed them.
///
/// All mutating algorithms on strings supported by the OrkString
/// suite should be implemented on MutableStrings.  ResizableStrings
/// use the MutableString implementations, with a possible
/// resize to prevent clipping.
///
/// MutableStrings require both a buffer, and a reference to the length
/// of the string.  They are not meant to be constructed directly by
/// client code.  Instead, use them by cast-constructing them from
/// ArrayStrings or ResizableStrings.
///
/// @see PieceString
/// @see ConstString
/// @see MutableString
/// @see ResizableString
class MutableString
{
public:
	typedef PieceString::size_type size_type;
	static const size_type npos = size_type(PieceString::npos);

	typedef char *iterator;
	typedef const char *const_iterator;

	/// Constructs a mutable string.
	///
	/// @param buffer   The data for the string
	/// @param buffersize   The size of the buffer, (including the NUL terminator)
	/// @param length   A reference to the length of the string.
	MutableString(char *buffer, size_type buffersize, size_type &length);

	/// @return a chararcter pointer to the data.
	const char *data() const;
	/// @return a chararcter pointer to NUL terminated data.
	const char *c_str() const;
	/// @return the current length of the string.
	size_type length() const;
	/// @return the current length of the string.
	size_type size() const;
	/// @return the maximum length of the string.
	size_type capacity() const;

	/// prints into the string, like printf.
	/// @return the number of characters formatted ignoring truncation.
    size_type format(const char *format, ...);
	/// @return the number of characters formatted ignoring truncation.
    size_type vformat(const char *format, va_list);

	/// @return a const_iterator at the begining of the string.
	const_iterator begin() const;
	/// @return a const_iterator at the end of the string.
	const_iterator end() const;
	/// @return an iterator at the end of the string.
	iterator begin();
	/// @returns an iterator at the end of the string.
	iterator end();

	/// @see PieceString::find(char,size_type) const
	size_type find(char ch, size_type pos = 0) const;
	/// @see PieceString::find(const char *,size_type,size_type) const
	size_type find(const char *s, size_type pos = 0, size_type len = npos) const;
	/// @see PieceString::find(const PieceString &,size_type) const
	size_type find(const PieceString &s, size_type pos = 0) const;
	/// @see PieceString::find_first_of(char,size_type) const
	size_type find_first_of(char ch, size_type pos = 0) const;
	/// @see PieceString::find_first_of(const char *,size_type) const
	size_type find_first_of(const char *s, size_type pos = 0) const;

	/// @see PieceString::rfind(char,size_type) const
	size_type rfind(char ch, size_type pos = npos) const;
	/// @see PieceString::rfind(const char *,size_type,size_type) const
	size_type rfind(const char *s, size_type pos = npos, size_type len = npos) const;
	/// @see PieceString::rfind(const PieceString &,size_type,size_type) const
	size_type rfind(const PieceString &s, size_type pos = npos) const;
	/// @see PieceString::find_last_of(char,size_type) const
	size_type find_last_of(char ch, size_type pos = npos) const;
	/// @see PieceString::find_last_of(const char *,size_type) const
	size_type find_last_of(const char *s, size_type pos = npos) const;
	
	/// @see PieceString::substr(size_type,size_type) const
	PieceString substr(size_type index, size_type amount = npos) const;

	/// @see PieceString::empty() const
	bool empty() const;

	operator PieceString() const;
	operator ConstString() const;

	bool operator ==(const PieceString &other) const;
	bool operator <=(const PieceString &other) const;
	bool operator >=(const PieceString &other) const;
	bool operator < (const PieceString &other) const;
	bool operator > (const PieceString &other) const;
	bool operator !=(const PieceString &other) const;

	const MutableString &operator =(const PieceString &other);
	const MutableString &operator =(const MutableString &other);
	const MutableString &operator +=(const PieceString &other);
	const MutableString &operator +=(char);

	/// Replaces text from offset to offset+size with value.
	/// <code>
	/// ArrayString&lt;16&gt;("0123456789").replace(3, 3, "replace") == "012replace6789";
	/// </code>
	/// @param offset the index of the first character to replace
	/// @param size the number of characters to replace
	/// @param value the string to replace the characters with
	void replace(size_type offset, size_type size, PieceString value = PieceString());
private:
	char *mpBuffer;
	size_type &mLength;
	size_type mMaxSize;
};

///////////////////////////////////////////////////////////////////////////////
}
///////////////////////////////////////////////////////////////////////////////
