////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
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
