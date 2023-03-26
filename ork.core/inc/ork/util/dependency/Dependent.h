////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#ifndef _ORK_UTIL_DEPENDENCY_DEPENDENT_H_
#define _ORK_UTIL_DEPENDENCY_DEPENDENT_H_

#include <ork/util/RingLink.h>

namespace ork { namespace util { namespace dependency {

class Provider;

class Dependent : public util::RingLink<Dependent>
{
public:
	Dependent();
	
	// override these.
	virtual void OnDependencyProvided();
	virtual void OnDependencyRevoked();
	
	void SetProvider(Provider *provider);
	bool DependencyProvided() const;
private:
	friend class Provider;
	void SetDependencyProvided();
	void SetDependencyRevoked();
	
	bool mDependencyProvided;
};

} } }

#endif