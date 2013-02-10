////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 


#include <ork/pch.h>

#include <ork/util/dependency/MultiDependentProvider.h>

namespace ork { namespace util { namespace dependency {

namespace multi_impl {

MultiDependentProviderBase::MultiDependentProviderBase(Part *dependencies, int count)
	: mDependencies(dependencies)
	, mDependentCount(count)
	, mCurrentRequirementCount(count)
{}

void MultiDependentProviderBase::SetProvider(int index, Provider *provider)
{
	OrkAssert(index >= 0 && index < mDependentCount);
	return mDependencies[index].SetProvider(provider);
}

void MultiDependentProviderBase::Part::SetGroup(MultiDependentProviderBase *group)
{
	mGroup = group;
}

void MultiDependentProviderBase::Part::OnDependencyProvided()
{
	mGroup->mCurrentRequirementCount--;

	if(mGroup->mCurrentRequirementCount == 0)
	{
		mGroup->Provide();
	}
}

void MultiDependentProviderBase::Part::OnDependencyRevoked()
{
	if(mGroup->mCurrentRequirementCount == 0)
	{
		mGroup->Revoke();
	}

	mGroup->mCurrentRequirementCount++;
}

}

} } }
