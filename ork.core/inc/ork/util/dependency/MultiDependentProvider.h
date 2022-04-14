////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#ifndef _ORK_UTIL_DEPENDENCY_MULTIDEPENDENTPROVIDER_
#define _ORK_UTIL_DEPENDENCY_MULTIDEPENDENTPROVIDER_

#include <ork/util/dependency/Dependent.h>
#include <ork/util/dependency/Provider.h>

namespace ork { namespace util { namespace dependency {

namespace multi_impl {
	
class MultiDependentProviderBase : public Provider
{
protected:
	class Part : public Dependent
	{
	public:
		void SetGroup(MultiDependentProviderBase *group);
	private:
		/*virtual*/ void OnDependencyProvided();
		/*virtual*/ void OnDependencyRevoked();

		MultiDependentProviderBase *mGroup;
	};
	
	friend class Part;
	
public:
	MultiDependentProviderBase(Part *dependencies, int count);
	
	void SetProvider(int index, Provider *);

private:
	int mCurrentRequirementCount;
	Part *mDependencies;
	const int mDependentCount;
};

} // namespace multi_impl

template<int num_dependencies>
class MultiDependentProvider : public multi_impl::MultiDependentProviderBase
{
public:
	MultiDependentProvider()
		: multi_impl::MultiDependentProviderBase(mDependencies, num_dependencies)
	{
		for(int i = 0; i < num_dependencies; i++)
			mDependencies[i].SetGroup(this);
	}

private:
	multi_impl::MultiDependentProviderBase::Part mDependencies[num_dependencies];
};

} } } // ork::util::dependency

#endif