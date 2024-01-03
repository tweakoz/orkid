////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
//////////////////////////////////////////////////////////////// 


#include <ork/pch.h>

#include <ork/kernel/string/BlockString.h>
#include <ork/kernel/string/StringBlock.h>

namespace ork {
	
BlockString::BlockString()
	: mStringBlock(NULL)
	, mStringIndex(-1)
{}

BlockString::BlockString(const StringBlock *block, size_t index)
	: mStringBlock(block)
	, mStringIndex(index)
{
}

const char *BlockString::data() const
{
	return c_str();
}

const char *BlockString::c_str() const
{
	if(NULL == mStringBlock) return NULL;
	return mStringBlock->GetStringData(mStringIndex);
}

size_t BlockString::Index() const
{
	return mStringIndex;
}

bool BlockString::operator ==(const BlockString &other) const
{
	if(other.mStringBlock == mStringBlock && other.mStringIndex == mStringIndex)
		return true;

	return compare(other) == 0;
}

bool BlockString::operator <=(const BlockString &other) const
{
	return compare(other) <= 0;
}

bool BlockString::operator >=(const BlockString &other) const
{
	return compare(other) >= 0;
}

bool BlockString::operator < (const BlockString &other) const
{
	return compare(other) <  0;
}

bool BlockString::operator > (const BlockString &other) const
{
	return compare(other) >  0;
}

bool BlockString::operator !=(const BlockString &other) const
{
	if(other.mStringBlock == mStringBlock && other.mStringIndex != mStringIndex)
		return true;

	return compare(other) != 0;
}

int BlockString::compare(const BlockString &other) const
{
	return ::strcmp(c_str(), other.c_str());
}

BlockString::operator bool() const
{
	return NULL != c_str();
}

BlockString::operator PieceString() const
{
	return data() ? PieceString(data()) : NULL;
}

BlockString::operator ConstString() const
{
	return c_str() ? ConstString(c_str()) : NULL;
}

}
