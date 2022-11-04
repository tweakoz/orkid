////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#include <ork/lev2/gfx/gfxctxdummy.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/gfxmaterial_test.h>
#include <ork/lev2/gfx/gfxmaterial_ui.h>
#include <ork/lev2/gfx/gfxprimitives.h>
#include <ork/lev2/gfx/rtgroup.h>

#include <ork/application/application.h>
#include <ork/kernel/any.h>
#include <ork/kernel/orklut.hpp>
#include <ork/lev2/gfx/renderer/frametek.h>
#include <ork/lev2/gfx/renderer/renderable.h>
#include <ork/lev2/gfx/renderer/rendercontext.h>
#include <ork/lev2/gfx/renderer/renderer.h>
#include <ork/lev2/gfx/renderer/irendertarget.h>
#include <ork/lev2/gfx/shadman.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/lev2/ui/viewport.h>
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/gfx/renderer/compositor.h>

template class ork::orklut<ork::CrcString, ork::lev2::rendervar_t>;

namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////
RenderingModel::RenderingModel(ERenderModelID id) //
	: _modelID(id) { //
}
///////////////////////////////////////////////////////////////////////////////
bool RenderingModel::isDeferred() const { //
	bool rval = false;
	switch(_modelID){
		case ERenderModelID::DEFERRED_PBR:
			rval = true;
			break;
		default:
			break;
	}
	return rval;
}
///////////////////////////////////////////////////////////////////////////////
bool RenderingModel::isForward() const{
	bool rval = false;
	switch(_modelID){
		case ERenderModelID::FORWARD_UNLIT:
		case ERenderModelID::FORWARD_PBR:
			rval = true;
			break;
		default:
			break;
	}
	return rval;
}
///////////////////////////////////////////////////////////////////////////////
bool RenderingModel::isDeferredPBR() const { //
	return _modelID==ERenderModelID::DEFERRED_PBR; //
}
///////////////////////////////////////////////////////////////////////////////
bool RenderingModel::isForwardUnlit() const{
	return _modelID==ERenderModelID::FORWARD_UNLIT; //
}
///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
