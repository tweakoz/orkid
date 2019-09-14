////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/lev2/gfx/renderer.h>
#include <ork/lev2/gfx/lighting/gfx_lighting.h>

namespace ork { namespace ent {
class SceneEditorBase;
}};

namespace ork { namespace tool {

class Renderer : public lev2::Renderer
{

public:

	Renderer( ent::SceneEditorBase& editor, lev2::GfxTarget* ptarg=nullptr );
	void Init();

private:

	void RenderModel( const lev2::ModelRenderable & ModelRen, ork::lev2::RenderGroupState rgs=ork::lev2::ERGST_NONE ) const override;
	void RenderModelGroup( const lev2::ModelRenderable** ModelRens, int inumr ) const override;
	void RenderCallback( const lev2::CallbackRenderable & cbren ) const override;


	U32 ComposeSortKey( U32 texIndex, U32 depthIndex, U32 passIndex, U32 transIndex ) const override;


	lev2::Texture*			mTopSkyEnvMap;
	lev2::Texture*			mBotSkyEnvMap;
	ent::SceneEditorBase& 	mEditor;

};

}} // namespace ork { namespace tool {
