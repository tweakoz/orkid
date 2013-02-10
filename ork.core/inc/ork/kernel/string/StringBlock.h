////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 

#pragma once

///////////////////////////////////////////////////////////////////////////////

#include <ork/kernel/string/BlockString.h>
#include <ork/kernel/string/ResizableString.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork {
///////////////////////////////////////////////////////////////////////////////

class PieceString;

///////////////////////////////////////////////////////////////////////////////

class StringBlock
{
public:
	StringBlock();
	void Load(const PieceString &data);
	typedef PieceString::size_type size_type;

	BlockString AddString(const PieceString &);
	BlockString FindString(const PieceString &) const;
	//BlockString FromIndex(int index) const;
	const char *data();
	size_type size();
	int NumStrings() const { return mStringOffsets.size(); }
	BlockString GetSeqString(int seq) const;
private:
	const char *GetStringData(int index) const;
	ResizableString::size_type FindSortIndex(const PieceString &, bool &success) const;

	ResizableString mStringData;
	orkvector<int> mStringOffsets;

	friend class BlockString;
};

///////////////////////////////////////////////////////////////////////////////
}
///////////////////////////////////////////////////////////////////////////////
