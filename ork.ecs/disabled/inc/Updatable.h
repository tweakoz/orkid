////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#ifndef ORK_ENT_UPDATABLE_H
#define ORK_ENT_UPDATABLE_H

namespace ork { namespace ent {

class Simulation;

class IUpdatable
{
public:
	virtual void Update(Simulation*) = 0;
};

} } // ork::ent

#endif // ORK_ENT_UPDATABLE_H
