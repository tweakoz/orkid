////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/dataflow/dataflow.h>

namespace ork { namespace dataflow {
///////////////////////////////////////////////////////////////////////////////

class sinplug : public outplug<float>
{
	RttiDeclareAbstract(sinplug,outplug<float>);

	float output;

	sinplug( module*pmod, const char* pname)
		: outplug<float>( pmod,dataflow::EPR_UNIFORM, & output, pname )
	{
	}
};

///////////////////////////////////////////////////////////////////////////////
}}
