////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 


#include <ork/pch.h>

#include <ork/util/dependency/Dependent.h>
#include <ork/util/dependency/Provider.h>
#include <ork/util/RingLink.hpp>

namespace ork { namespace util {
		
template class RingLink<ork::util::dependency::Dependent>;
//template class RingLink<ork::util::dependency::Dependent>::iterator;
	
namespace dependency {

Dependent::Dependent()
	: mDependencyProvided(false)
{
}

void Dependent::SetDependencyProvided()
{
	if(false == mDependencyProvided)
	{
		mDependencyProvided = true;
		OnDependencyProvided();
	}
}

void Dependent::SetDependencyRevoked()
{
	if(true == mDependencyProvided)
	{
		mDependencyProvided = false;
		OnDependencyRevoked();
	}
}

void Dependent::SetProvider(Provider *provider)
{
	if(DependencyProvided())
	{
		SetDependencyRevoked();
	}
	
	Unlink();
	
	if(provider)
		provider->AddDependent(this);
}

bool Dependent::DependencyProvided() const
{
	return mDependencyProvided;
}

void Dependent::OnDependencyProvided()
{
}

void Dependent::OnDependencyRevoked()
{
}

} } }
