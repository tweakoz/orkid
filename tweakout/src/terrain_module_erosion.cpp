////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2014, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
//
// Synthetic Fluvial Erosion
//	based on "erode" code from John P. Beale 6/10/95-7/7/95
//
///////////////////////////////////////////////////////////////////////////////
//
//  notes from original erode:
//   
//	1. the algorithm may "bomb" on perfectly flat surfaces
//
//	2  sometimes there's some kind of instability where there is suddently
//		a spike pushing up out of the surface like a new volcano or something; this
//		seems to happen at the higher erosion or smoothing rates. It might not
//		appear with the default values. Then again it might.
//
///////////////////////////////////////////////////////////////////////////////
#include <orktool/orktool_pch.h>
#include <ork/math/plane.h>
#include <ork/math/misc_math.h>
#include <orktool/filter/gfx/meshutil/meshutil.h>
#include <ork/kernel/prop.h>
#include <ork/dataflow/scheduler.h>
///////////////////////////////////////////////////////////////////////////////
#include "terrain_synth.h"
#include "terrain_erosion.h"
///////////////////////////////////////////////////////////////////////////////
#if 0
///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace terrain {
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
int hmap_erode1_module::GetNumInputs() const { return 1; }
///////////////////////////////////////////////////////////////////////////////
int hmap_erode1_module::GetNumOutputs() const { return 1; }
///////////////////////////////////////////////////////////////////////////////
dataflow::inplugbase* hmap_erode1_module::GetInput(int idx)
{	OrkAssert( idx==0 );
	return & mInput;
}
///////////////////////////////////////////////////////////////////////////////
const dataflow::outplugbase* hmap_erode1_module::GetOutput(int idx) const 
{	const dataflow::outplugbase* rval = 0;
	OrkAssert( idx>=0 );
	OrkAssert( idx<3 );
	switch( idx )
	{	case 0 :
			rval = & mOutputElevation;
			break;
		case 1 :
			rval = & mOutputUphillArea;
			break;
		case 2 :
			rval = & mOutputBasinAccum;
			break;
	}
	return rval;
}
///////////////////////////////////////////////////////////////////////////////
hmap_erode1_module::datablock::datablock()
{
}
void hmap_erode1_module::datablock::Copy( const datablock& oth )
{
	sheightmap_datablock::Copy( oth );
}
///////////////////////////////////////////////////////////////////////////////
hmap_erode1_module::hmap_erode1_module()
	//: mInput( this, sheightmap::gdefhm )
	//, mOutputElevation( this, & mElevationData )
	//, mOutputUphillArea( this, & mUphillAreaData )
	//, mOutputBasinAccum( this, & mBasinData )

	: mInput( this, dataflow::EPR_UNIFORM, ent::sheightmap::gdefhm, "Input" )
	, mOutputElevation(this,dataflow::EPR_UNIFORM, &mElevationData, "Elevation" )
	, mOutputUphillArea(this,dataflow::EPR_UNIFORM, &mUphillAreaData, "UphillAreaData" )
	, mOutputBasinAccum(this,dataflow::EPR_UNIFORM, &mBasinData, "BasinData" )


	, miNumErosionCycles( 1 ) // 10
	, miFillBasinsInitial( 10 )
	, miFillBasinsCycle( 1 )
	, mSmoothingRate( 1.0f )
	, miItersPerCycle( 1 ) // 5
	, miEnable( 0 )
	, mErosionRate( errate )
	, mSlumpScale(3.0f)
{
	AddDependency( mOutputElevation, mInput );
	AddDependency( mOutputUphillArea, mInput );
	AddDependency( mOutputBasinAccum, mInput );
}
///////////////////////////////////////////////////////////////////////////////
void hmap_erode1_module::Compute(dataflow::workunit* wu)
{
	#if 0
	datablock* hcw = (datablock*) wu->GetContextData();
	sheightmap& hm = hcw->mHeightMap;

	// alloc output hmap
	const sheightmap& inhmap = mInput.GetValue();
	int inpsize = inhmap.GetGridSize();
	float fwsize = inhmap.GetWorldSize();
	OrkAssert( inpsize>=32 );

	hcw->mElevationData.SetGridSize(inpsize);
	hcw->mUphillAreaData.SetGridSize(inpsize);
	hcw->mBasinData.SetGridSize(inpsize);

	hcw->mElevationData.SetWorldSize(fwsize);
	hcw->mUphillAreaData.SetWorldSize(fwsize);
	hcw->mBasinData.SetWorldSize(fwsize);
	//////////////////////////////////////////////
	//////////////////////////////////////////////
	//////////////////////////////////////////////
	if( miEnable )
	{
		ErosionContext ec;
		ec.Init( inpsize, (const float*) inhmap.GetHeightData() );
		ec.mfTerrainSize = fwsize;
		ec.mfTerrainHeight = inhmap.GetHeightRange();
		
		ec.miFillBasinsCycle = miFillBasinsCycle;
		ec.mErosionRate = mErosionRate;
		ec.mSmoothingRate = mSmoothingRate;

		ec.miItersPerCycle = miItersPerCycle;
		ec.miNumErosionCycles = miNumErosionCycles;

		ec.mSlumpScale = mSlumpScale;

		float inscale = ec.normalize();

		//////////////////////////////////////////////////////
		//////////////////////////////////////////////////////
		ec.Execute();
		//////////////////////////////////////////////////////
		//////////////////////////////////////////////////////

		//////////////////////////////////////////////////////
		// store results to output hmap plug
		//////////////////////////////////////////////////////
		for( int iz=0; iz<inpsize; iz++ )
		{	for( int ix=0; ix<inpsize; ix++ )
			{	int outaddr = mElevationData.CalcAddress(ix,iz);
				float fin_elev = ec.mHeightMap.Read( ix, iz );
				//float fin_ua = ec.mUphillAreaMap.Read( ix, iz );
				//float fin_ba = ec.mBasinAccumMap.Read( ix, iz );
				hcw->mElevationData.SetHeight(ix,iz, fin_elev/inscale );
				//hcw->mUphillAreaData.SetHeight(ix,iz, fin_ua);
				//hcw->mBasinData.SetHeight(ix,iz, fin_ba);
			}
		}
	}
	//////////////////////////////////////////////////////
	else // BYPASS
	//////////////////////////////////////////////////////
	{	// store input to output hmap plug
		//////////////////////////////////////////////////////
		for( int iz=0; iz<inpsize; iz++ )
		{	for( int ix=0; ix<inpsize; ix++ )
			{	int outaddr = mElevationData.CalcAddress(ix,iz);
				float fin_elev = inhmap.GetHeight( ix, iz );
				hcw->mElevationData.SetHeight(ix,iz, fin_elev );
				//hcw->mUphillAreaData.SetHeight(ix,iz, 0.0f);
				//hcw->mBasinData.SetHeight(ix,iz, 0.0f);
			}
		}
	}
	#endif
}
///////////////////////////////////////////////////////////////////////////////
void hmap_erode1_module::CombineWork( const dataflow::cluster* clus )
{
	#if 0
	const LockedResource< orkvector<dataflow::workunit*> >& WorkUnits = clus->GetWorkUnits();
	
	const orkvector<dataflow::workunit*>& wuvect = WorkUnits.LockForRead();

	int inumwu = wuvect.size();

	OrkAssert( inumwu == 1 );

	dataflow::workunit* wu = wuvect[0];

	if( wu->GetModule() == this )
	{
		int wuidx = wu->GetModuleWuIndex();

		datablock* hcw = (datablock*) wu->GetContextData();

		int igsize = hcw->mElevationData.GetGridSize();
		float fworldsize = hcw->mElevationData.GetWorldSize();

		if( 0 == wuidx ) // set my size from this
		{
			mElevationData.SetGridSize( igsize );
			mElevationData.SetWorldSize( fworldsize );
			mElevationData.ResetMinMax();
		}


		for( int iz=0; iz<igsize; iz++ )
		{
			for( int ix=0; ix<igsize; ix++ )
			{
				float fh = hcw->mElevationData.GetHeight( ix, iz );
				mElevationData.SetHeight( ix, iz, fh );
			}
		}
	}
	WorkUnits.UnLock();
	mOutputElevation.SetDirty(false);
	#endif	
}
///////////////////////////////////////////////////////////////////////////////
void hmap_erode1_module::DoDivideWork( const dataflow::scheduler& sch, dataflow::cluster* clus ) const
{
	#if 0
	datablock* hcw = OrkNew datablock;
	hcw->Copy( mDefDataBlock );

	dataflow::workunit* wu = OrkNew dataflow::workunit(this,clus,0);
	wu->SetContextData(hcw);
	wu->SetAffinity( dataflow::scheduler::CpuAffinity );
	clus->AddWorkUnit(wu);
	#endif	
}
void hmap_erode1_module::ReleaseWorkUnit( dataflow::workunit* wu )
{
	#if 0
	dgmodule::ReleaseWorkUnit( wu );
	#endif	
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void hmap_erode1_module::SetEnable( int iena )
{	if( iena!=miEnable ) mOutputElevation.SetDirty(true);
	miEnable = iena;
}
void hmap_erode1_module::SetNumErosionCycles( int nec )
{	if( nec!=miNumErosionCycles ) mOutputElevation.SetDirty(true);
	miNumErosionCycles=nec;
}
void hmap_erode1_module::SetFillBasinsInitial( int fbi )
{	if( fbi!=miFillBasinsInitial ) mOutputElevation.SetDirty(true);
	miFillBasinsInitial=fbi;
}
void hmap_erode1_module::SetFillBasinsCycle( int fbc )
{	if( fbc!=miFillBasinsCycle ) mOutputElevation.SetDirty(true);
	miFillBasinsCycle=fbc;
}
void hmap_erode1_module::SetItersPerCycle( int ipc )
{	if( ipc!=miItersPerCycle ) mOutputElevation.SetDirty(true);
	miItersPerCycle=ipc;
}
void hmap_erode1_module::SetSmoothingRate( float sr )
{	if( sr!=mSmoothingRate ) mOutputElevation.SetDirty(true);
	mSmoothingRate=sr;
}
void hmap_erode1_module::SetErosionRate( float er )
{	if( er!=mErosionRate ) mOutputElevation.SetDirty(true);
	mErosionRate=er;
}
void hmap_erode1_module::SetSlumpScale( float sc )
{	if( sc!=mSlumpScale ) mOutputElevation.SetDirty(true);
	mSlumpScale=sc;
}
///////////////////////////////////////////////////////////////////////////////
}}
#endif