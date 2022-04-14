////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#ifndef _ORK_UTIL_DEPENDENCY_PROVIDER_H_
#define _ORK_UTIL_DEPENDENCY_PROVIDER_H_

#include <ork/util/RingLink.h>

namespace ork { namespace util { namespace dependency {

class Dependent;

class Provider
{
public:
	Provider();
	~Provider();
	
	bool Providing() const;
	
	void Provide();
	void Revoke();

private:
	friend class Dependent;
	void AddDependent(Dependent *dependent);

	bool mProviding;
	RingLink<Dependent> mDependents;
};

} } }

#endif