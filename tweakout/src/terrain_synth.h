////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2014, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <pkg/ent/scene.h>
#include <pkg/ent/component.h>
#include <ork/lev2/gfx/renderable.h>
//#include <ork/lev2/ui/ui_enum.h>
//#include <ork/lev2/ui/ui.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/dataflow/dataflow.h>
#include <ork/kernel/mutex.h>
#include <pkg/ent/heightmap.h>
#include "terrain_module.h"
#if 0

///////////////////////////////////////////////////////////////////////////////
namespace ork {
namespace MeshUtil { class toolmesh; }	
namespace terrain {
///////////////////////////////////////////////////////////////////////////////

class TerrainSynth 
{
	hmap_perlin_module			mhf_perlin;
	hmap_erode1_module			mhf_erode1;
	hmap_hfield_module			mhf_target;

	dataflow::graph_inst		mhf_graph;

	ork::mutex					mhfMutex;

	//////////////////////////////////////////////////////////
public:
	//////////////////////////////////////////////////////////

	TerrainSynth( ent::GradientSet& gset, int igl, float worldsize );

	orkmap<float,fvec4>		mGradLo;
	orkmap<float,fvec4>		mGradHi;

	//////////////////////////////////////////////////////////

	ork::mutex& GetLock() { return mhfMutex; }

	void LockVisMap(lev2::GfxTarget*pt) const;
	void UnLockVisMap() const;

	//////////////////////////////////////////////////////////

	void SetSize( int isize );


	float MinY() const;
	float MaxY() const;
	float RangeY() const;

	//////////////////////////////////////////////////////////

	float GetHeight( int ix, int iz ) const;	

	//////////////////////////////////////////////////////////

	fvec3 XYZ( int ix, int iz ) const;

	U32 Color( int ix, int iz ) const { return mhf_target.Color(ix,iz); }
	const fvec3& Normal(int ix,int iz) const { return  mhf_target.Normal(ix,iz); }

	//////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////
	
	hmap_hfield_module& GetTargetModule() { return mhf_target; }
	const hmap_hfield_module& GetTargetModule() const { return mhf_target; }

	hmap_erode1_module& GetErodeModule() { return mhf_erode1; }
	const hmap_erode1_module& GetErodeModule() const { return mhf_erode1; }

	hmap_perlin_module& GetPerlinModule() { return mhf_perlin; }
	const hmap_perlin_module& GetPerlinModule() const { return mhf_perlin; }

	dataflow::graph_inst&	GetDataflowGraph() { return mhf_graph; }
	const dataflow::graph_inst&	GetDataflowGraph() const { return mhf_graph; }

	//////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////

	void MergeToolMesh( MeshUtil::toolmesh& outmesh );

};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

struct sheightfield_iface_editor //: public lev2::IHeightFieldRenderTech
{
	const TerrainSynth& mhf;
	lev2::Texture*		mColorTexture;

	//typedef void (*cbtype)( lev2::RenderContextInstData& rcid, lev2::GfxTarget* targ, const CallbackRenderable* pren );

	static void Render( lev2::RenderContextInstData& rcid, lev2::GfxTarget* targ, const lev2::CallbackRenderable* pren);

	void FastRender(	const lev2::Renderer* renderer,
						const lev2::CallbackRenderable& rable,
						const lev2::RenderContextInstData& rcidata ) const;

	sheightfield_iface_editor( const TerrainSynth& hf );
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
}}
//////

#endif
