////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2014, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#if 0
///////////////////////////////////////////////////////////////////////////////
#include <pkg/ent/heightmap.h>
///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace terrain {
///////////////////////////////////////////////////////////////////////////////

typedef dataflow::inplug<ent::HeightMap> HeightMapInPlugType;
typedef dataflow::outplug<ent::HeightMap> HeightMapOutPlugType;

typedef dataflow::inplug<ent::cv2_map2d>		V2Map2dInPlugType;
typedef dataflow::outplug<ent::cv2_map2d>	V2Map2dOutPlugType;
typedef dataflow::inplug<ent::cv3_map2d>		V3Map2dInPlugType;
typedef dataflow::outplug<ent::cv3_map2d>	V3Map2dOutPlugType;
typedef dataflow::inplug<ent::cv4_map2d>		V4Map2dInPlugType;
typedef dataflow::outplug<ent::cv4_map2d>	V4Map2dOutPlugType;

class heightfield_compute_buffer;

///////////////////////////////////////////////////////////////////////////////
void GpGpuTask	(	const CVector4& ClearColor,
					const CVector4& ModColor,
					lev2::GfxMaterial& material,
					int iwidth, int iheight,
					heightfield_compute_buffer& MyComputeBuffer
				);
///////////////////////////////////////////////////////////////////////////////

struct HeightMap_datablock
{
	ent::HeightMap	mHeightMap;
	int miX1, miX2, miZ1, miZ2;
	HeightMap_datablock();
	void Copy( const HeightMap_datablock& oth );
};

///////////////////////////////////////////////////////////////////////////////

class heightfield_compute_buffer : public lev2::GfxBuffer
{
public:
	
	static const int kx = 0;
	static const int ky = 0;
	static const int kw;
	static const int kh;

	lev2::CaptureBuffer tex1buffer;//, tex2buffer;
	lev2::Texture tex1;//, tex2;
	lev2::CaptureBuffer MyCaptureBuffer;

	heightfield_compute_buffer();
	void OutputToTexture( lev2::Texture* ptex );
};

///////////////////////////////////////////////////////////////////////////////
// module which generates height only data with perlin noise
///////////////////////////////////////////////////////////////////////////////

class hmap_perlin_module : public dataflow::dgmodule
{
	struct datablock : public HeightMap_datablock
	{
		int			minumoct;
		float		mfampsca;
		float		mfampbas;
		float		mffrqsca;
		float		mffrqbas;
		float		mfrotate;
		float		mfmapfrq;
		float		mfmapamp;
		datablock();
		void Copy( const datablock& oth );
	};
	
	HeightMapOutPlugType								mOutputPlug;
	datablock											mDefDataBlock;
	lev2::Texture*										mpDepthModTexture;
	lev2::Texture*										mpNoiseModTexture;
	mutable lev2::StaticVertexBuffer<lev2::SVtxV12C4T16> mVertexBuffer;

	/*virtual*/ void DoDivideWork( const dataflow::scheduler& sch, dataflow::cluster* clus ) const; 

	void ComputeCPU( dataflow::workunit* wu ) const;
	void ComputeGPU( dataflow::workunit* wu ) const;

public:

	////////////////////////////////////////////
	ent::HeightMap& HeightMapData() { return mDefDataBlock.mHeightMap; }
	const ent::HeightMap& HeightMapData() const { return mDefDataBlock.mHeightMap; }
	////////////////////////////////////////////
	HeightMapOutPlugType& OutputPlug() { return mOutputPlug; }
	////////////////////////////////////////////
	lev2::Texture*	GetDepthTexture() const { return mpDepthModTexture; }
	void SetDepthTexture(lev2::Texture*ptex); 
	////////////////////////////////////////////
	lev2::Texture*	GetNoiseTexture() const { return mpNoiseModTexture; }
	void SetNoiseTexture(lev2::Texture*ptex); 
	////////////////////////////////////////////
	int GetNumOctaves() const { return mDefDataBlock.minumoct; }
	float GetOctaveAmpScale() const { return mDefDataBlock.mfampsca; }
	float GetOctaveFrqScale() const { return mDefDataBlock.mffrqsca; }
	float GetAmpBase() const { return mDefDataBlock.mfampbas; }
	float GetFrqBase() const { return mDefDataBlock.mffrqbas; }
	float GetMapFrq() const { return mDefDataBlock.mfmapfrq; }
	float GetMapAmp() const { return mDefDataBlock.mfmapamp; }
	float GetRotate() const { return mDefDataBlock.mfrotate; }
	////////////////////////////////////////////
	void SetNumOctaves( int inumo );
	void SetOctaveAmpScale( float oas );
	void SetOctaveFrqScale( float ofs );
	void SetAmpBase( float ab );
	void SetFrqBase( float fb );
	void SetRotate( float fb );
	void SetMapFrq( float fb );
	void SetMapAmp( float fb );
	////////////////////////////////////////////
	/*virtual*/ int GetNumInputs() const;
	/*virtual*/ int GetNumOutputs() const;
	/*virtual*/ dataflow::inplugbase* GetInput(int idx);
	/*virtual*/ const dataflow::outplugbase* GetOutput(int idx) const;
	/*virtual*/ void Compute(dataflow::workunit* wu);
	////////////////////////////////////////////
	/*virtual*/ void CombineWork( const dataflow::cluster* clus );
	/*virtual*/ void ReleaseWorkUnit( dataflow::workunit* wu );
	////////////////////////////////////////////
	hmap_perlin_module();
};

///////////////////////////////////////////////////////////////////////////////
// module which generates height only data with random triangles
// looks like terrorist rubble
///////////////////////////////////////////////////////////////////////////////

class hmap_911_module : public dataflow::dgmodule
{
	////////////////////////////

	struct datablock : public HeightMap_datablock
	{
		int			mitriangles;
		datablock();
		void Copy( const datablock& oth );
	};

	////////////////////////////

	HeightMapOutPlugType	mOutputPlug;
	datablock				mDefDataBlock;

	/*virtual*/ void DoDivideWork( const dataflow::scheduler& sch, dataflow::cluster* clus );

	void Compute( dataflow::workunit* wu );

public:

	int GetNumTriangles() const { return mDefDataBlock.mitriangles; }

	void SetNumTriangles( int inumt );

	/*virtual*/ int GetNumInputs() const;
	/*virtual*/ int GetNumOutputs() const;
	/*virtual*/ dataflow::inplugbase* GetInput(int idx);
	/*virtual*/ const dataflow::outplugbase* GetOutput(int idx) const;
	///*virtual*/ void Compute(dataflow::workunit* wu) const;
	////////////////////////////////////////////
	/*virtual*/ void CombineWork( const dataflow::cluster* clus );
	/*virtual*/ void ReleaseWorkUnit( dataflow::workunit* wu );
	////////////////////////////////////////////
	ent::HeightMap& HeightMapData() { return mDefDataBlock.mHeightMap; }
	const ent::HeightMap& HeightMapData() const { return mDefDataBlock.mHeightMap; }
	////////////////////////////////////////////
	HeightMapOutPlugType& OutputPlug() { return mOutputPlug; }
	////////////////////////////////////////////
	hmap_911_module();
};

///////////////////////////////////////////////////////////////////////////////
// module which combines height, colors, uvs, normals
///////////////////////////////////////////////////////////////////////////////

const int kterchunksize = 1024;

struct TerrainChunk
{
	lev2::CVtxBuffer<lev2::SVtxV12C4T16>*	mVertexBuffer;
	lev2::StaticIndexBuffer<U16>*			mIndexBuffer;
	lev2::Texture*							mColorTexture;
};

class hmap_hfield_module : public dataflow::dgmodule
{
	HeightMap_datablock	mDefDataBlock;
	
	mutex							mVisMutex;

	orkvector<U32>			mrgb;
	orkvector<CVector3>		mnormals;

	HeightMapInPlugType		mInputPlug;
	HeightMapOutPlugType	mHeightOutputPlug;

	float					mInverseGridSize;
	float					mIndexToWorld;

	lev2::Texture*			mpLightEnvTexture;
	lev2::Texture*			mpColorMapTexture;

	int						miSize;
	
	orkmap<int,ent::TerVtxBuffersType*>				vtxbufmap;
	orkmap<int,lev2::StaticIndexBuffer<U16>*>	idxbufmap;
	
	orkvector<TerrainChunk>				mTerrainChunks;

	////////////////////////////////////////////////

	void					SetSize( int i );

	/*virtual*/ void DoDivideWork( const dataflow::scheduler& sch, dataflow::cluster* clus ) const;

	////////////////////////////////////////////////
	void ComputeNormals();
	void ComputeGeometry();
	void ComputeColors();
	////////////////////////////////////////////////
	U32& Color( int ix, int iz );
	CVector3& Normal(int ix,int iz);
	////////////////////////////////////////////////


public:
	int	GetSize() const { return miSize; }
	////////////////////////////////////////////////
	hmap_hfield_module( ent::GradientSet& gset, int isize );
	////////////////////////////////////////////////
	/*virtual*/ int GetNumInputs() const;
	/*virtual*/ int GetNumOutputs() const;
	/*virtual*/ dataflow::inplugbase* GetInput(int idx);
	/*virtual*/ const dataflow::outplugbase* GetOutput(int idx) const;
	/*virtual*/ void Compute(dataflow::workunit* wu);
	////////////////////////////////////////////////
	U32 Color( int ix, int iz ) const;
	const CVector3& Normal(int ix,int iz) const;
	////////////////////////////////////////////////
	/*virtual*/ void CombineWork( const dataflow::cluster* clus );
	const ent::HeightMap& HeightMapData() const { return mDefDataBlock.mHeightMap; }
	////////////////////////////////////////////////
	CVector3 XYZ( int iX, int iZ ) const;
	CVector3 ComputeNormal(int ix1,int iz1) const;
	////////////////////////////////////////////////
	void LockVisMap() const;
	void UnLockVisMap() const;
	////////////////////////////////////////////////
	HeightMapOutPlugType& HeightOutPlug() { return mHeightOutputPlug; }
	////////////////////////////////////////////////
	const orkmap<int,ent::TerVtxBuffersType*>& VertexBuffers() const { return vtxbufmap; }
	const orkmap<int,lev2::StaticIndexBuffer<U16>*>& IndexBuffers() const { return idxbufmap; }
	////////////////////////////////////////////
	lev2::Texture*	GetLightEnvTexture() const { return mpLightEnvTexture; }
	void SetLightEnvTexture(lev2::Texture*ptex);// { mpLightEnvTexture=ptex; }
	void SetColorMapTexture(lev2::Texture*ptex); // { mpColorMapTexture=ptex; }
	////////////////////////////////////////////////
	void ReadSurface( const CVector3& xyz, CVector3& pos, CVector3& nrm ) const;
	////////////////////////////////////////////////
	void SaveNormalsToTexture( const file::Path& filename ) const;
	void SaveColorsToTexture( const file::Path& filename ) const;
	void SaveLightingToTexture( const file::Path& filename ) const;
	void SaveHeightToTexture( const file::Path& filename ) const;
	////////////////////////////////////////////////
};

///////////////////////////////////////////////////////////////////////////////
// module which modifies height only data with smoothstep
///////////////////////////////////////////////////////////////////////////////

class hmap_terrace_module : public dataflow::dgmodule
{
	ent::HeightMap				mOutputData;
	HeightMapInPlugType		mInput;
	HeightMapOutPlugType	mOutput;

	float			mQuantizeAmt;

public:
	/*virtual*/ int GetNumInputs() const;
	/*virtual*/ int GetNumOutputs() const;
	/*virtual*/ dataflow::inplugbase* GetInput(int idx);
	/*virtual*/ const dataflow::outplugbase* GetOutput(int idx) const;
	/*virtual*/ void Compute(dataflow::workunit* wu) {}

	hmap_terrace_module();
};

///////////////////////////////////////////////////////////////////////////////
// module which modifies height only data with erosion
///////////////////////////////////////////////////////////////////////////////

class hmap_erode1_module : public dataflow::dgmodule
{
	struct datablock : public HeightMap_datablock
	{
		ent::HeightMap				mElevationData;
		ent::HeightMap				mUphillAreaData;
		ent::HeightMap				mBasinData;

		datablock();
		void Copy( const datablock& oth );
	};

	HeightMapInPlugType		mInput;
	HeightMapOutPlugType	mOutputElevation;
	HeightMapOutPlugType	mOutputUphillArea;
	HeightMapOutPlugType	mOutputBasinAccum;
	datablock				mDefDataBlock;

	ent::HeightMap				mElevationData;
	ent::HeightMap				mUphillAreaData;
	ent::HeightMap				mBasinData;

	int						miEnable;
	int						miNumErosionCycles;
	int						miFillBasinsInitial;
	int						miFillBasinsCycle;
	int						miItersPerCycle;
	float					mSmoothingRate;
	float					mErosionRate;
	float					mSlumpScale;

	/*virtual*/ void DoDivideWork( const dataflow::scheduler& sch, dataflow::cluster* clus ) const;
	/*virtual*/ void CombineWork( const dataflow::cluster* clus );

public:
	/*virtual*/ int GetNumInputs() const;
	/*virtual*/ int GetNumOutputs() const;
	/*virtual*/ dataflow::inplugbase* GetInput(int idx);
	/*virtual*/ const dataflow::outplugbase* GetOutput(int idx) const;
	/*virtual*/ void Compute(dataflow::workunit* wu);
	////////////////////////////////////////////
	/*virtual*/ void ReleaseWorkUnit( dataflow::workunit* wu );
	////////////////////////////////////////////
	int GetEnable() const { return miEnable; }
	int GetNumErosionCycles() const { return miNumErosionCycles; }
	int GetFillBasinsInitial() const { return miFillBasinsInitial; }
	int GetFillBasinsCycle() const { return miFillBasinsCycle; }
	int GetItersPerCycle() const { return miItersPerCycle; }
	float GetSmoothingRate() const { return mSmoothingRate; }
	float GetErosionRate() const { return mErosionRate; }
	float GetSlumpScale() const { return mSlumpScale; }

	void SetEnable( int iena );
	void SetNumErosionCycles( int nec );
	void SetFillBasinsInitial( int fbi );
	void SetFillBasinsCycle( int fbc );
	void SetItersPerCycle( int ipc );
	void SetSmoothingRate( float sr );
	void SetErosionRate( float sr );
	void SetSlumpScale( float sc );
	
	hmap_erode1_module();
};

///////////////////////////////////////////////////////////////////////////////

class hmap_fout_module : public dataflow::dgmodule
{
	HeightMapInPlugType	mInput;

public:
	/*virtual*/ int GetNumInputs() const;
	/*virtual*/ int GetNumOutputs() const;
	/*virtual*/ dataflow::inplugbase* GetInput(int idx);
	/*virtual*/ const dataflow::outplugbase* GetOutput(int idx) const;
	/*virtual*/ void Compute(dataflow::workunit* wu) {}

	hmap_fout_module();
	void Save( const char* filename ) const;
};

///////////////////////////////////////////////////////////////////////////////

extern lev2::CaptureBuffer& HeightMapGPGPUCaptureBuffer();
extern heightfield_compute_buffer& HeightMapGPGPUComputeBuffer(int idb=0);

///////////////////////////////////////////////////////////////////////////////
}}
///////////////////////////////////////////////////////////////////////////////
#endif
