////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#ifndef _ORKTOOL_RENDERER_H
#define _ORKTOOL_RENDERER_H

#include <ork/lev2/gfx/renderer.h>
#include <ork/lev2/gfx/lighting/gfx_lighting.h>

namespace ork { namespace tool {

class Renderer : public lev2::Renderer
{
public:

	virtual U32 GetSortKey( const lev2::IRenderable* pRenderable  ) const { return 0; }
	/*virtual*/ void RenderBillboard( const lev2::CBillboardRenderable & BBRen ) const;
	/*virtual*/ void RenderBox( const lev2::CBoxRenderable & BoxRen ) const;
	/*virtual*/ void RenderModel( const lev2::CModelRenderable & ModelRen, ork::lev2::RenderGroupState rgs=ork::lev2::ERGST_NONE ) const;
	/*virtual*/ void RenderModelGroup( const lev2::CModelRenderable** ModelRens, int inumr ) const;
	/*virtual*/ void RenderFrustum( const lev2::FrustumRenderable & FrusRen ) const;
	/*virtual*/ void RenderSphere( const lev2::SphereRenderable & FrusRen ) const;
	/*virtual*/ void RenderCallback( const lev2::CallbackRenderable & cbren ) const;


	Renderer( lev2::GfxTarget *ptarg=0 );

	/*virtual*/ U32 ComposeSortKey( U32 texIndex, U32 depthIndex, U32 passIndex, U32 transIndex ) const;

	void Init();

	lev2::Texture*		mTopSkyEnvMap;
	lev2::Texture*		mBotSkyEnvMap;

};

}
}

#endif
