////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
//////////////////////////////////////////////////////////////// 


#include <ork/pch.h>
#include <ork/rtti/ICastable.h>
#include <ork/orkstd.h>

namespace ork {	namespace rtti {

Class *ICastable::GetClassStatic() { return NULL; }

Class *ForceLink(Class *c)
{
	return c;
}

} }
