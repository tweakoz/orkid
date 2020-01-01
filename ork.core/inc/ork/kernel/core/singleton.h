////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 

#pragma once

#include <ork/kernel/core/kerneltypes.h> 
#include <ork/orkstd.h>

#include <ork/config/config.h>

namespace ork {

class CClass;

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template <typename TClass> class  NoRttiSingleton
{
	//friend TClass;

	static TClass* sSingleton;

public:

	static TClass& GetRef()
	{
		if( sSingleton == 0 )
		{
			static char buffer[sizeof(TClass)+128];
			sSingleton = reinterpret_cast<TClass*>( buffer );//malloc(isize) );
			new( sSingleton ) TClass();
		}
		return *sSingleton;
	}

	NoRttiSingleton<TClass>()
	{
		//OrkAssertI(!sSingleton, "Singleton class is already instantiated") ;
		//U32 offset = (U32)(TClass *)1 - (U32)(NoRttiSingleton <TClass> *)(TClass *)1 ;
		//sSingleton = (TClass *)((U32)this + offset);
	}

};

template <typename TClass> TClass * NoRttiSingleton<TClass>::sSingleton = 0;

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

class  Singleton 
{
	public:

	Singleton();
	//static void ClassInit() {}
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

}
