////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/kernel/gstack.h>

namespace ork { 

template<typename T>
orkstack<T*>& gstack<T>::GetStack()
{
	if( 0 == gpItems )
	{
		gpItems = new orkstack<T*>();
	}
	return *gpItems;
}

template<typename T>
T& gstack<T>::top()
{
	return *GetStack().top();
}

template<typename T> void gstack<T>::push( T& item )
{
	GetStack().push( & item );
}

template<typename T> void gstack<T>::pop()
{
	GetStack().pop();
}

template<typename T> orkstack<T*>* gstack<T>::gpItems = 0;

///////////////////////////////////////////////////////////////////////

template<typename T>
orkstack<T*>& gthreadedstack<T>::GetStack()
{
	if( 0 == gpItems )
	{
		gpItems = new orkstack<T*>();
	}
	return *gpItems;
}

template<typename T>
T& gthreadedstack<T>::top()
{
	return *GetStack().top();
}

template<typename T> void gthreadedstack<T>::push( T& item )
{
	GetStack().push( & item );
}

template<typename T> void gthreadedstack<T>::pop()
{
	GetStack().pop();
}

template<typename T> ThreadLocal orkstack<T*>* gthreadedstack<T>::gpItems = 0;

} // namespace ork
