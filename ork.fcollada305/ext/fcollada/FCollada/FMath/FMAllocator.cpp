/*
	Copyright (C) 2005-2007 Feeling Software Inc.
	Portions of the code are:
	Copyright (C) 2005-2007 Sony Computer Entertainment America
	
	MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#include "StdAfx.h"
#include "FMAllocator.h"

//void* myalloc( size_t isize );
//void myfree( void* pdata );

namespace fm
{
	// default to something: static initialization!
	AllocateFunc gaf = malloc;
	FreeFunc gff = free;
	
	void SetAllocationFunctions(AllocateFunc a, FreeFunc f)
	{
		gaf = a;
		gff = f;
	}

	// These two are simple enough, but have the advantage of
	// always allocating/releasing memory from the same heap.
	void* Allocate(size_t byteCount)
	{
		static AllocateFunc af = malloc;
		return (*af)(byteCount);
	}

	void Release(void* buffer)
	{
		static FreeFunc ff = free;
		(*ff)(buffer);
	}
};

