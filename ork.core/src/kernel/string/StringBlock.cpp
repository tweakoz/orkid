////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 


#include <ork/pch.h>
#include <ork/kernel/string/StringBlock.h>

namespace ork {

StringBlock::StringBlock()
{
}

void StringBlock::Load(const PieceString &data)
{
	OrkAssert(mStringData.size() == 0);

	mStringData.reserve(data.size());

	for(PieceString::size_type index = 0; index < data.size(); )
	{
		ConstString item(data.data() + index);

		index += item.length() + 1;

		AddString(item);
	}

	OrkAssert(data == mStringData);
}

BlockString StringBlock::FindString(const PieceString &in) const
{
	bool found;
	
	ResizableString::size_type sort_index = FindSortIndex(in, found);

	if(found)
	{
		BlockString result = BlockString(this, mStringOffsets[sort_index]);
		OrkAssert(in == result.c_str());
		return result;
	}
	else
	{
		return BlockString();
	}
}

BlockString StringBlock::AddString(const PieceString &in)
{
	bool found;
	
	ResizableString::size_type sort_index = FindSortIndex(in, found);

	if(found)
	{		
		return BlockString(this, mStringOffsets[sort_index]);
	}
	else
	{
		int offset = int(mStringData.size());
		mStringData += in;
		mStringData += '\0';
		int seq = NumStrings();
		mStringOffsets.insert(mStringOffsets.begin() + int(sort_index), offset);
		return BlockString(this, offset);
	}
}

const char *StringBlock::GetStringData(int index) const
{
	return mStringData.data() + index;
}

//BlockString StringBlock::FromIndex(int index) const
//{
//	return BlockString(this, index);
//}

BlockString StringBlock::GetSeqString(int seq) const
{
	assert(seq<mStringOffsets.size());
	auto index = mStringOffsets[seq];
	//printf("GetSeqString<%d>==%d\n", seq, index );
	return BlockString(this, mStringOffsets[seq]);
}

ResizableString::size_type StringBlock::FindSortIndex(const PieceString &string, bool &success) const
{
	orkvector<int>::size_type low = 0, high = mStringOffsets.size();

	while(low < high)
	{
		orkvector<int>::size_type mid = (low + high) / 2;
		int cmp = string.compare(GetStringData(mStringOffsets[mid]));
		if(cmp < 0)
		{
			high = mid;
		}
		else if(cmp > 0)
		{
			low = mid + 1;
		}
		else
		{
			success = true;
			return ResizableString::size_type(mid);
		}
	}

	success = false;
	return ResizableString::size_type(low);
}

const char *StringBlock::data()
{
	return mStringData.data();
}

StringBlock::size_type StringBlock::size()
{
	return mStringData.size();
}

}
