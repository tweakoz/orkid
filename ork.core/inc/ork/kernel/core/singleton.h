////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
//////////////////////////////////////////////////////////////// 

#pragma once

#include <ork/orktypes.h> 
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

	NoRttiSingleton()
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
