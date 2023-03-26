////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
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
