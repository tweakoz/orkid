////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 


#include<ork/pch.h>

#include<ork/lev2/gfx/renderqueue.h>
#include<ork/lev2/gfx/renderer.h>
#include <ork/kernel/Array.hpp>

///////////////////////////////////////////////////////////////////////////////

template class ork::fixedvector<ork::lev2::RenderQueue::Node,ork::lev2::RenderQueue::krqmaxsize>;

namespace ork { namespace lev2 {

///////////////////////////////////////////////////////////////////////////////

void RenderQueue::QueueRenderable(const IRenderable *pRenderable, Renderer *pRenderer)
{
	new (&mNodes.create()) Node(pRenderer->GetCurrentObject(), pRenderable);
}

///////////////////////////////////////////////////////////////////////////////

void RenderQueue::ExportRenderableNodes(ork::fixedvector<const RenderQueue::Node*,krqmaxsize>& nodes)
{
	nodes.resize(Size());
	int idx = 0;
	for(ork::fixedvector<Node,krqmaxsize>::const_iterator it=mNodes.begin(); it!=mNodes.end(); it++ )
	{
		nodes[idx++] = &(*it);
	}
}

///////////////////////////////////////////////////////////////////////////////

void RenderQueue::Reset()
{
	mNodes.clear();
}

///////////////////////////////////////////////////////////////////////////////
} } // namespace ork
///////////////////////////////////////////////////////////////////////////////
