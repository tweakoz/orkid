////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2014, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <orktool/orktool_pch.h>
#include <ork/math/plane.h>
#include <ork/dataflow/dataflow.h>
#include <ork/math/misc_math.h>
#include <orktool/filter/gfx/meshutil/meshutil.h>
#include <ork/kernel/prop.h>
#include <orktool/toolcore/dataflow.h>
#include <pkg/ent/editor/editor.h>
#include <ork/application/application.h>
#include "terrain_synth.h"
#if 0

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace terrain {
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////

TerrainSynth::TerrainSynth( ent::GradientSet& gset, int igl, float fwsize ) 
	: mhf_target( gset, 1 )
	, mhfMutex( "shfMutex" )
{

	mhf_perlin.HeightMapData().SetWorldSize(fwsize);
	//mhf_target.HeightMapData().SetWorldSize(fwsize);
	SetSize(igl);

	////////////////////////////////////
	// explicit dataflow graph (until user-specified is working)
	////////////////////////////////////
	
	mhf_graph.AddChild( AddPooledString("perlin"), & mhf_perlin ); 
	mhf_graph.AddChild( AddPooledString("erode1"), & mhf_erode1 ); 
	mhf_graph.AddChild( AddPooledString("target"), & mhf_target ); 

	#if 0
	dataflow::inplug<sheightmap>* target_input = 0;
	dataflow::inplug<sheightmap>* erode1_input = 0;

	const dataflow::outplug<sheightmap>* perlin_output = 0;
	const dataflow::outplug<sheightmap>* erode1_output = 0;

	mhf_target.GetTypedInput<sheightmap>(0,target_input);
	mhf_erode1.GetTypedInput<sheightmap>(0,erode1_input);
	mhf_perlin.GetTypedOutput<sheightmap>(0,perlin_output);
	mhf_erode1.GetTypedOutput<sheightmap>(0,erode1_output);
	
	erode1_input->Connect( perlin_output );
	target_input->Connect( erode1_output );
#endif

	//mhf_graph.RefreshTopology();
	//mhf_graph.SetScheduler( tool::GetGlobalDataFlowScheduler() );
	////////////////////////////////////

}

void TerrainSynth::SetSize( int isiz )
{
	mhfMutex.Lock();
	{
		int icount = (isiz*isiz);
		
		mhf_perlin.HeightMapData().SetGridSize(isiz);
		mhf_perlin.OutputPlug().SetDirty(true);
	}
	mhfMutex.UnLock();
}
///////////////////////////////////////////////////////////////////////////////

float TerrainSynth::MinY() const
{
	return mhf_target.HeightMapData().GetMinHeight();
}

float TerrainSynth::MaxY() const
{
	return mhf_target.HeightMapData().GetMaxHeight();
}

float TerrainSynth::RangeY() const
{
	return mhf_target.HeightMapData().GetHeightRange();
}

///////////////////////////////////////////////////////////////////////////////

void TerrainSynth::LockVisMap(lev2::GfxTarget*pt) const
{
	mhf_target.LockVisMap();
}
void TerrainSynth::UnLockVisMap() const
{
	mhf_target.UnLockVisMap();
}

///////////////////////////////////////////////////////////////////////////////

CVector3 TerrainSynth::XYZ( int iX, int iZ ) const
{
	return mhf_target.XYZ(iX,iZ);
}

///////////////////////////////////////////////////////////////////////////////

float TerrainSynth::GetHeight( int iX, int iZ ) const
{
	return mhf_target.HeightMapData().GetHeight(iX,iZ);
}

///////////////////////////////////////////////////////

void TerrainSynth::MergeToolMesh( MeshUtil::toolmesh& outmesh )
{
	int iw = mhf_target.HeightMapData().GetGridSize();

	for( int iX1=0; iX1<iw-1; iX1++ )
	{
		int iX2 = (iX1+1)%iw;

		for( int iZ1=0; iZ1<iw-1; iZ1++ )
		{
			int iZ2 = (iZ1+1)%iw;

			/////////////////////////////////////////////////////

			MeshUtil::vertex outvtx_x1z1, outvtx_x2z1, outvtx_x2z2, outvtx_x1z2;

			outvtx_x1z1.mPos = XYZ(iX1,iZ1);
			outvtx_x2z1.mPos = XYZ(iX2,iZ1);
			outvtx_x2z2.mPos = XYZ(iX2,iZ2);
			outvtx_x1z2.mPos = XYZ(iX1,iZ2);

			outvtx_x1z1.mNrm = Normal(iX1,iZ1);
			outvtx_x2z1.mNrm = Normal(iX2,iZ1);
			outvtx_x2z2.mNrm = Normal(iX2,iZ2);
			outvtx_x1z2.mNrm = Normal(iX1,iZ2);

			outvtx_x1z1.mCol[0] = Color(iX1,iZ1);
			outvtx_x2z1.mCol[0] = Color(iX2,iZ1);
			outvtx_x2z2.mCol[0] = Color(iX2,iZ2);
			outvtx_x1z2.mCol[0] = Color(iX1,iZ2);

			//outvtx_x1z1.mUV[0].mMapTexCoord = UV(iX1,iZ1);
			//outvtx_x2z1.mUV[0].mMapTexCoord = UV(iX2,iZ1);
			//outvtx_x2z2.mUV[0].mMapTexCoord = UV(iX2,iZ2);
			//outvtx_x1z2.mUV[0].mMapTexCoord = UV(iX1,iZ2);

			/////////////////////////////////////////////////////

			//int idx_x1z1 = outmesh.MergeVertex( outvtx_x1z1 );
			//int idx_x2z1 = outmesh.MergeVertex( outvtx_x2z1 );
			//int idx_x2z2 = outmesh.MergeVertex( outvtx_x2z2 );
			//int idx_x1z2 = outmesh.MergeVertex( outvtx_x1z2 );

			/////////////////////////////////////////////////////

			//outmesh.MergePoly( MeshUtil::poly( idx_x1z1, idx_x1z2, idx_x2z2 ) );
			//outmesh.MergePoly( MeshUtil::poly( idx_x1z1, idx_x2z2, idx_x2z1 ) );
		}
	}

}

////////////////////////////////////////////////////

void BuildTerrain( MeshUtil::toolmesh& outmesh, const tokenlist& options )
{
	/////////////////////////////////////////////////////////////////

	/////////////////////////////////////////////////////////////////
	// Compute Heightfield

	//sheightfield heightfield( iNumGroundLines, fLineSize );
	//heightfield.MergeToolMesh( outmesh );
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void TerrainTest(const tokenlist& options)
{
	ork::tool::FilterOptMap	OptionsMap;
	OptionsMap.SetDefault( "-outobj", "tertest.dae" );
	OptionsMap.SetOptions(options);
	OptionsMap.DumpOptions();

	MeshUtil::toolmesh outmesh;
	BuildTerrain( outmesh, options );
		
	ork::tool::FilterOption* popt = OptionsMap.GetOption( "-outobj" );

	if( popt )
	{
		//ork::file::Path outpath( popt->GetValue().c_str() );
		//outmesh.WriteToDaeFile( outpath );
	}

}

}}
#endif