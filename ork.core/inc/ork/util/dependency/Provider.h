////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
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