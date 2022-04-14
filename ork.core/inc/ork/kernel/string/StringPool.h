////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 

#pragma once

///////////////////////////////////////////////////////////////////////////////

#include <ork/kernel/string/PoolString.h>

#include <ork/orkstl.h>
#include <ork/kernel/mutex.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork {
///////////////////////////////////////////////////////////////////////////////

class StringPool
{
public:
	typedef orkvector<const char * > VecType;

	explicit StringPool(const StringPool *parent = NULL);
	PoolString String(const PieceString &);
	PoolString Literal(const ConstString &);
	PoolString Find(const PieceString &) const;

	int LiteralIndex(const ConstString &);
	int StringIndex(const PieceString &);
	int FindIndex(const PieceString &) const;
	int Size() const;
	PoolString FromIndex(int) const;
private:
	const char *FindRecursive(const PieceString &) const;
	const char *FindFirst(const PieceString &, VecType::iterator &);
	VecType::size_type BinarySearch(const PieceString &, bool &) const;

	VecType										mStringPool;
	mutable ork::recursive_mutex				mStringPoolMutex;

	void Lock() const { mStringPoolMutex.Lock(); }
	void UnLock() const { mStringPoolMutex.UnLock(); }

protected:
	const StringPool *_parent;
};

///////////////////////////////////////////////////////////////////////////////
}
///////////////////////////////////////////////////////////////////////////////
