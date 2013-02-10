////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 

#pragma once

///////////////////////////////////////////////////////////////////////////////

#include <ork/kernel/string/PieceString.h>
#include <ork/kernel/string/MutableString.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork {
///////////////////////////////////////////////////////////////////////////////

/// A ResizableString is the most complex string type in the OrkString suite.
/// 
/// ResizableStrings own their data, like ArrayString&nbsp;s,
/// but will never overflow since they can reallocate.
class ResizableString
{
public:
	typedef PieceString::size_type size_type;
	static const size_type npos = size_type(PieceString::npos);
	typedef char *iterator;
	typedef const char *const_iterator;

	/// Constructs an empty ResizableString
	ResizableString();
	/// Constructs a ResizableString from a pointer to NUL terminated data.
	ResizableString(const char *s);
	/// Constructs a ResizableString from a PieceString.
	ResizableString(const PieceString &s);

	/// @return a chararcter pointer to the data.
	const char *data() const;
	/// @return a chararcter pointer to NUL terminated data.
	const char *c_str() const;
	/// @return the current length of the string.
	size_type length() const;
	/// @return the current length of the string.
	size_type size() const;
	/// @return the current maximum length of the string.
	size_type capacity() const;

	/// releases the memory held by the string
	~ResizableString();

	/// prints formated text into the string, (like printf)
    size_type format(const char *format, ...);
	/// prints formated text into the string, (like vprintf)
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

	/// @see MutableString::replace()
	void replace(size_type offset, size_type size, PieceString value = PieceString("", 0));

	bool empty() const;

	bool operator ==(const PieceString &other) const;
	bool operator <=(const PieceString &other) const;
	bool operator >=(const PieceString &other) const;
	bool operator < (const PieceString &other) const;
	bool operator > (const PieceString &other) const;
	bool operator !=(const PieceString &other) const;

	const ResizableString &operator =(const ResizableString &other);
	const ResizableString &operator =(const PieceString &other);
	const ResizableString &operator =(const char *other);
	const ResizableString &operator +=(const PieceString &other);
	const ResizableString &operator +=(const char *other);
	const ResizableString &operator +=(char);

	operator PieceString() const;
	operator ConstString() const;
	operator MutableString();

	void reserve(size_type size);

private:
	void copy(const char *s, size_type len);

	/// Resizes the string to newsize if size is less than required,
	/// retaining the number of characters specified by keep.
	/// @param required the threshold below which resize will occur.
	/// @param keep the amount of text to keep,
	///        (sometimes we don't need to retain the old text after a resize.)
    void resize(size_type required, size_type keep, size_type newsize);

	char *mpBuffer;
	size_type mLength;
	size_type mMaxSize;
};

///////////////////////////////////////////////////////////////////////////////
}
///////////////////////////////////////////////////////////////////////////////
