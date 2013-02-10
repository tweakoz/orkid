////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#ifndef _ORK_LEV2_RENDERER_BASE_H_
#define _ORK_LEV2_RENDERER_BASE_H_

namespace ork
{
	namespace lev2
	{
	
		enum RenderGroupState
		{
			ERGST_NONE = 0,
			ERGST_FIRST,
			ERGST_CONTINUE,
			ERGST_LAST,
		};
	}
}

#endif
