////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2014, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#if 0
namespace ork { namespace terrain {
///////////////////////////////////////////////////////////////////////////////
static const float errate = float(1e-5);			// rate of erosion
static const float er_thresh = float(2.0);			// elevation above which no erosion happens
static const float er_sedfac = float(1.6);			// slope ratio thresh. for sedimentation
static const float pow_fac = float(0.0);			// power of altititude in smoothing rate
static const float er_pow = float(1.0);				// power of flow in erosion rate
static const float pow_offset = float(1.0);			// constant term in erosion rate 
static const float PCONST = float(0.6366197723);	// constant value 2/pi
///////////////////////////////////////////////////////////////////////////////
template <typename T> struct Map2D
{
	int misize;
	int misizesq;
	orkvector<T>	mData;

	Map2D() : misize(0), misizesq(0) {}

	void Resize( int isize )
	{	misize = isize;
		misizesq = isize*isize;
		mData.resize( isize*isize );
	}
	int Address( int ix, int iy ) const
	{
		OrkAssert(ix<misize);
		OrkAssert(iy<misize);
		int iaddr = (iy*misize)+ix;
		return iaddr;	
	}
	const T& Read( int ix, int iy ) const
	{
		return mData[Address(ix,iy)];
	}
	const T& ReadClamped( int ix, int iy ) const
	{
		if( ix < 0 ) ix=0;
		if( ix > (misize-1) ) ix=misize-1;
		if( iy < 0 ) iy=0;
		if( iy > (misize-1) ) iy=misize-1;

		return mData[Address(ix,iy)];
	}
	const T& ReadWrapped( int ix, int iy ) const
	{	int iwx = ix%misize;
		int iwy = iy%misize;
		return mData[Address(iwx,iwy)];
	}
	T& Write( int ix, int iy )
	{
		return mData[Address(ix,iy)];
	}
};
///////////////////////////////////////////////////////////////////////////////
struct ErosionDataSet
{
	int misize;
	Map2D<u8>		mMapU[4];
	Map2D<float>	mMapF[4];	
	ErosionDataSet(int isize)
	{
		for( int i=0; i<4; i++ ) mMapU[i].Resize(isize);
		for( int i=0; i<4; i++ ) mMapF[i].Resize(isize);
	}
};
///////////////////////////////////////////////////////////////////////////////
struct ErosionContext
{	int xsize;
	int ysize;
	float			mfTerrainSize;
	float			mfTerrainHeight;
	float			mErosionRate;
	float			mSlumpScale;
	int				miNumErosionCycles;
	int				miItersPerCycle;
	int				miFillBasinsCycle;
	float			mSmoothingRate;
	int				miGridSize;
	//////////////////////////////////////////////
	//Map2D<u8>	mFlowDirMap;						// array of flow directions 
	//Map2D<u8>	mPeakFlagMap;						// array of checked, peak flags 
	//Map2D<u8>	mhftmp_u8;							// temp u8 map
	//////////////////////////////////////////////
	//Map2D<float>	mhftmp_float;						
	Map2D<float>	mHeightMap;							// elevation array (height field) 
	Map2D<float>	mUphillAreaMap;						// uphill area array 
	Map2D<float>	mBasinAccumMap;						// basin accumulation array 
	//////////////////////////////////////////////
	ErosionContext();
	//////////////////////////////////////////////
	void Init( int isize, const float*psrc );
	//////////////////////////////////////////////
	//////////////////////////////////////////////
	//////////////////////////////////////////////
	//////////////////////////////////////////////
	//////////////////////////////////////////////
	//////////////////////////////////////////////
	float normalize();
	//////////////////////////////////////////////
	// function controlling erosion rate
	//////////////////////////////////////////////
	float ErosionFactor( float slope_exponent, float flow_exponent ) const;
	//////////////////////////////////////////////
	void Execute();
	//////////////////////////////////////////////
};
///////////////////////////////////////////////////////////////////////////////
void GpuErodeBegin(	ErosionContext& ec, const Map2D<float>& hfin );
void GpuErodeEnd(	ErosionContext& ec, Map2D<float>& hfin );
///////////////////////////////////////////////////////////////////////////////
void GpuFindFlow2(	ErosionContext& ec );
void GpuFindUpFlow(	ErosionContext& ec );
void GpuFindUphillArea1(	const ErosionContext& ec, Map2D<float>& hfout_ua );
void GpuErode1(	ErosionContext& ec );
void GpuErodeCorrect( ErosionContext& ec );
void GpuSlump(	ErosionContext& ec );
void GpuSmooth(	ErosionContext& ec );
///////////////////////////////////////////////////////////////////////////////

}}
#endif
