////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 


#ifndef _ORKMEM_HPP
#define _ORKMEM_HPP

#ifndef USESTDNEW
void* operator new(size_t size) __ORKMEM_ALLOC_NO_THROW
{
	return operator new(size __GENERATE_DEBUG_ARGS_APPEND);
}

void* operator new[](size_t size) __ORKMEM_ALLOC_NO_THROW
{
	return operator new[](size __GENERATE_DEBUG_ARGS_APPEND);
}

void operator delete(void* data)
{
	return operator delete(data __GENERATE_DEBUG_ARGS_APPEND);
}

void operator delete[](void* data)
{
	return operator delete[](data __GENERATE_DEBUG_ARGS_APPEND);
}
#endif

///////////////////////////////////////////////////////////
// not used directly in game, or even directly in tool....
//  ork's memmgr will call this to get its raw zone block
//  also used by some external libraries (libxml,fcollada,nvtt)
///////////////////////////////////////////////////////////

extern "C" void* myrealloc( void* porig, unsigned int isize )
{
	return std::realloc( porig, isize );
}

///////////////////////////////////////////////////////////

extern "C" void* myalloc( unsigned int isize )
{
#if defined(_XBOX)
	static int gtotal = 0;
	static int gmegs = 0;
	gtotal += isize;
	if((gtotal>>20)>gmegs)
	{
		orkprintf( "gmegs<%d>\n", gmegs );
		gmegs = (gtotal>>20);
	}
	return (void*) GlobalAlloc(GPTR,isize);
#else
	return std::malloc(isize);
#endif
}

///////////////////////////////////////////////////////////

extern "C" void myfree( void* ptr )
{
	std::free(ptr);
}

///////////////////////////////////////////////////////////
//
///////////////////////////////////////////////////////////

#endif
