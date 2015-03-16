#pragma once

#include <ork/math/cvector4.h>
#include <ork/kernel/mutex.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/file/path.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace ent {
///////////////////////////////////////////////////////////////////////////////

typedef orkvector<lev2::CVtxBuffer<lev2::SVtxV12C4T16>*> TerVtxBuffersType;
/*
template <typename T> class t_map2d
{
	int								miGridX;
	int								miGridZ;
	orkvector<T>					mData;

public:
	t_map2d(int sizx, int sizz) 
		: miGridX(0)
		, miGridZ(0)
	{
		SetSizeX(sizx);
		SetSizeZ(sizz);
	}

	inline int CalcAddress( int ix, int iz ) const { return (miGridX*iz)+ix; }
	
	const T& GetVal( int ix, int iz ) const { return mData[CalcAddress(ix,iz)]; }
	T& GetVal( int ix, int iz ) { return mData[CalcAddress(ix,iz)]; }
	
	int CalcNumGrids() const { return miGridX*miGridZ; }

	void SetSizeX( int x )
	{
		if( miGridX!=x )
		{
			int numelem = CalcNumGrids();
			mData.resize( numelem );
			miGridX = x;
		}

	}
	void SetSizeZ( int z )
	{
		if( miGridZ!=z )
		{
			int numelem = CalcNumGrids();
			mData.resize( numelem );
			miGridZ = z;
		}
	}
};

typedef t_map2d<CVector2> cv2_map2d;
typedef t_map2d<CVector3> cv3_map2d;
typedef t_map2d<CVector4> cv4_map2d;
*/

class sheightmap
{
	int								miGridSizeX;
	int								miGridSizeZ;
	float							mfWorldSizeX;
	float							mfWorldSizeZ;
	float 							mWorldHeight;
	orkvector<float>				mHeightData;
	float							mMin;
	float							mMax;
	float							mRange;
	mutex							mMutex;
	float							mIndexToUnitX;
	float							mIndexToUnitZ;

public:

	static sheightmap gdefhm;

	sheightmap( int isx, int isz );
	~ sheightmap();

	bool Load( const ork::file::Path& pth );

	mutex& GetLock() { return mMutex; }

	void SetGridSize( int iw, int ih );
	void SetWorldSize( float fwsize, float fhsize ) { mfWorldSizeX=fwsize; mfWorldSizeZ=fhsize; }
	
	void SetWorldHeight( float fh ) { mWorldHeight=fh; }
	inline int CalcAddress( int ix, int iz ) const { return (miGridSizeX*iz)+ix; }
	float GetMaxHeight() const { return mMax; }
	float GetMinHeight() const { return mMin; }
	float GetHeightRange() const { return mRange; }
	void ResetMinMax();
	int GetGridSizeX() const { return miGridSizeX; }
	int GetGridSizeZ() const { return miGridSizeZ; }

	float GetWorldSizeX() const { return mfWorldSizeX; }
	float GetWorldSizeZ() const { return mfWorldSizeZ; }

	float GetWorldHeight() const { return mWorldHeight; }
	
	bool CalcClosestAddress( const CVector3& to, float& outx, float& outz ) const;

	float GetHeight( int ix, int iz ) const;
	void SetHeight( int ix, int iz, float fh );

	const void* GetHeightData() const { return (const void*) & mHeightData.at(0); }

	CVector3 Min() const;
	CVector3 Max() const;
	CVector3 Range() const;
	CVector3 XYZ( int iX, int iZ ) const;
	CVector3 ComputeNormal( int iX, int iZ ) const;
	void ReadSurface( bool bfilter, const CVector3& xyz, CVector3& pos, CVector3& nrm ) const;

};

///////////////////////////////////////////////////////////////////////////////

struct GradientSet
{
	const orkmap<float,CVector4>*			mGradientLo;					
	const orkmap<float,CVector4>*			mGradientHi;
	float									mHeightLo;
	float									mHeightHi;

	GradientSet() 
		: mGradientLo(0)
		, mGradientHi(0)
		, mHeightLo(0.0f)
		, mHeightHi(0.0f)
	{}

	CVector4 Lerp( float fu, float fv ) const;
};

///////////////////////////////////////////////////////////////////////////////
}}
///////////////////////////////////////////////////////////////////////////////
