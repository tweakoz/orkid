////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#ifndef _ORK_UTIL_DEPENDENCY_FUNCTORDEPENDENT_H_
#define _ORK_UTIL_DEPENDENCY_FUNCTORDEPENDENT_H_

#include <ork/util/dependency/Dependent.h>

namespace ork { namespace util { namespace dependency {

class Provider;

template<typename ClassType>
class FunctorDependent : public Dependent
{
public:
	FunctorDependent(
		ClassType *callback,
		void (ClassType::*provided)(),
		void (ClassType::*revoked)())
		: mCallbackObject(callback)
		, mOnDependencyProvided(provided)
		, mOnDependencyRevoked(revoked)
	{
	}
	
	/*virtual*/ void OnDependencyProvided()
	{
		if(0 != mOnDependencyProvided)
		{
			(mCallbackObject->*mOnDependencyProvided)();
		}
	}
	
	/*virtual*/ void OnDependencyRevoked()
	{
		if(0 != mOnDependencyRevoked)
		{
			(mCallbackObject->*mOnDependencyRevoked)();
		}
	}
	
private:
	
	ClassType *mCallbackObject;
	void (ClassType::*mOnDependencyProvided)();
	void (ClassType::*mOnDependencyRevoked)();
};

} } }

#endif