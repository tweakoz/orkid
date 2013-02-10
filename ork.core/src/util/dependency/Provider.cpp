////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 


#include <ork/pch.h>

#include <ork/util/dependency/Provider.h>
#include <ork/util/dependency/Dependent.h>


namespace ork { namespace util { namespace dependency {

Provider::Provider()
	: mProviding(false)
{
}

Provider::~Provider()
{
	if(Providing())
		Revoke();
}

bool Provider::Providing() const
{
	return mProviding;
}

void Provider::Provide()
{
	if(false == mProviding)
	{
		mProviding = true;

		for(RingLink<Dependent>::iterator it = mDependents.begin(), next = it;
			it != mDependents.end();
			it = next)
		{
			++(next = it);
			
			it->SetDependencyProvided();
		}
	}
}

void Provider::Revoke()
{
	if(true == mProviding)
	{
		mProviding = false;

		for(RingLink<Dependent>::iterator it = mDependents.begin(), next = it;
			it != mDependents.end();
			it = next)
		{
			++(next = it);
			
			it->SetDependencyRevoked();
		}
	}
}

void Provider::AddDependent(Dependent *dependent)
{
	dependent->LinkBefore(&mDependents);
	
	if(Providing())
	{
		dependent->SetDependencyProvided();
	}
}

} } }
