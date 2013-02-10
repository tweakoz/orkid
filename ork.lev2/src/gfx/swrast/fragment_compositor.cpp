////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#include "lev3_test.h"
#include <math.h>
#include <ork/kernel/orkpool.h>
#include <ork/kernel/Array.hpp>
#include <ork/kernel/gstack.hpp>
#include <ork/math/collision_test.h>
#include <ork/math/sphere.h>
#include <ork/math/plane.hpp>
#include <ork/math/line.h>
#include "render_graph.h"
#include <IL/il.h>
#include <IL/ilut.h>

namespace devil { void InitDevIL(); }

///////////////////////////////////////////////////////////////////////////////

void FragmentCompositorZBuffer::Reset()
{
	opaqueZ=1.0e30f;
	mpOpaqueFragment=0;
}
void FragmentCompositorZBuffer::Visit( const rend_fragment* pfrag ) // virtual
{
	if( pfrag->mZ > opaqueZ ) return; // we can still z cull on fully opaque fragments but it might not catch everything
	if( pfrag->mZ < opaqueZ )
	{
		opaqueZ = pfrag->mZ;
		mpOpaqueFragment = pfrag;
	}
}
////////////////////////////////////////////////////////////
ork::CVector3 FragmentCompositorZBuffer::Composite(const ork::CVector3&clrcolor)
{
	////////////////////////////////////////////////////////////
	ork::CVector3 rgb = clrcolor;
	if(mpOpaqueFragment!=0) rgb = mpOpaqueFragment->mRGBA.GetXYZ();
	////////////////////////////////////////////////////////////
	//rgb.Saturate();
	return rgb;
}
///////////////////////////////////////////////////////////////////////////////
void FragmentCompositorREYES::Reset()
{
	miNumFragments=0;
	mpOpaqueFragment = 0;
	opaqueZ=1.0e30f;
}

void FragmentCompositorREYES::Visit( const rend_fragment* pfrag ) // virtual
{
	if( pfrag->mZ > opaqueZ ) return; // we can still z cull on fully opaque fragments but it might not catch everything

	bool bisopaque = ( pfrag->mRGBA.GetW() == 1.0f );

	if( bisopaque )
	{
		if( pfrag->mZ < opaqueZ )
		{
			opaqueZ = pfrag->mZ;
			mpOpaqueFragment = pfrag;
		}
	}
	mpFragments[ miNumFragments ] = pfrag;
	mFragmentZ[ miNumFragments ] = pfrag->mZ;

	miNumFragments++;
	OrkAssert( miNumFragments<kmaxfrags );
}
////////////////////////////////////////////
// sort and hide occluded
////////////////////////////////////////////
void FragmentCompositorREYES::SortAndHide()
{
	//mRadixSorter.ResetIndices();
	//if( miNumFragments<2 ) return;

	for( int i=0; i<miNumFragments; i++ ) 
		mpSortedFragments[i] = mpFragments[i];

	bool bsorted = false;

	//static int ginumacc = 0;
	//static int gicounter = 0;
	//ginumacc += miNumFragments;
	//gicounter++;
	//int iavg = ginumacc/gicounter;
	//////////////////////////////////////////
	while( false == bsorted )
	{
		bsorted = true;
		for( int i=0; i<(miNumFragments-1); i++ )
		{
			int j=i+1;
			const rend_fragment* pi = mpSortedFragments[i];
			const rend_fragment* pj = mpSortedFragments[j];

			if( pi->mZ > pj->mZ )
			{
				std::swap( pi, pj );
				mpSortedFragments[i] = pi;
				mpSortedFragments[j] = pj;
				bsorted = false;
			}
		}
	}
	//////////////////////////////////////////
	// must be sorted back to front here
	//////////////////////////////////////////
	for( int i=0; i<miNumFragments; i++ )
	{	const rend_fragment* pfrag = mpSortedFragments[ i ];
		if( pfrag==mpOpaqueFragment )
		{	miNumFragments = i+1;
			return;
		}
	}
	//////////////////////////////////////////
	mRadixSorter.Sort( mFragmentZ, miNumFragments );
	U32* pidx = mRadixSorter.GetIndices();
	for( int i=0; i<miNumFragments; i++ )
	{
		int idx = pidx[i];
		const rend_fragment* pfrag = mpFragments[ idx ];
		mpSortedFragments[i] = pfrag;
		if( pfrag==mpOpaqueFragment )
		{
			miNumFragments = i+1;
			return;
		}
	}

}
////////////////////////////////////////////////////////////
ork::CVector3 FragmentCompositorREYES::Composite(const ork::CVector3&clrcolor)
{	////////////////////////////////////////////////////////////
	// sort, hide fragments
	////////////////////////////////////////////////////////////
	SortAndHide();
	////////////////////////////////////////////////////////////
	// blend fragments
	// ALWAYS back to front order (closest opaque fragment will always be the last)
	// atmospheric processing/volume shaders can go in here
	//  remember to disable backface culling for volume shaders
	////////////////////////////////////////////////////////////
	ork::CVector3 rgb = clrcolor;
	const u32 uthreadmask = 1<<miThreadID;

	const int ilast = miNumFragments-1;
	const rend_fragment* plastfrag = 0;
	ork::fixed_stack<const rend_fragment*,32> VolumeStack;
	VolumeStack.push(0);

	for( int i=0; i<miNumFragments; i++ ) 
	{	int j = ilast-i;
		const rend_fragment* pfrag = mpSortedFragments[ j ];
		const ork::CVector3& wpos = pfrag->mWorldPos;		
		const ork::CVector4& fragrgba = pfrag->mRGBA;

		const rend_volume_shader* pcurrvolumeshader = ( pfrag->mpShader!=0) ? pfrag->mpShader->mpVolumeShader : 0;
		const rend_fragment* plastfragment = VolumeStack.top();
		const rend_volume_shader* plastvolumeshader = plastfragment ? plastfragment->mpShader->mpVolumeShader : 0;
		////////////////////////////////////
		bool bleavelast = false;
		if( pcurrvolumeshader )
		{
			if( pcurrvolumeshader==plastvolumeshader ) // LEAVE pcurrvolumeshader
			{
				bleavelast = true;
				VolumeStack.pop();
			}
			else // ENTER
			{
				VolumeStack.push(pfrag);
			
				if( plastvolumeshader ) // LEAVE plastvolumeshader
				{
					//bleavelast = true;
				}
			}
		}
		else if( plastvolumeshader ) // LEAVE plastvolumeshader
		{
			VolumeStack.pop();		
			bleavelast = true;
		}
		////////////////////////////////////
		if( bleavelast ) // composite in volume
		{
			if( fragrgba.GetW() < 1.0f )
			{
				const ork::CVector3& cnrm = pfrag->mWldSpaceNrm;
				const ork::CVector3& lnrm = plastfragment->mWldSpaceNrm;

//				if( cnrm.Dot(lnrm) > 0.0f )
				{

				const ork::CVector3& lpos = plastfragment->mWorldPos;
				OrkAssert( plastvolumeshader );
				ork::CVector4 shcol = plastvolumeshader->ShadeVolume( lpos, wpos );
				float falpha = shcol.GetW();
				ork::CVector3 res;
				res.Lerp( rgb, shcol.GetXYZ(), falpha );
				rgb = res;			
				}
			}
		}
		////////////////////////////////////

		ork::CVector3 res;
		res.Lerp( rgb, fragrgba.GetXYZ(), fragrgba.GetW() );
		rgb = res;
	}
//	OrkAssert(VolumeShaderStack.size()==1);
	////////////////////////////////////////////////////////////
	rgb.Saturate();
	return rgb;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
rend_texture2D::rend_texture2D()
	: miWidth(256)
	, miHeight(256)
	//, mCLhandle(0)
{
	ork::CPerlin2D perlin;

	mpData = new ork::CVector4[ 256*256 ];
	for( int iy=0; iy<256; iy++ )
	{
		float fiy = float(iy)/256.0f;
		for( int ix=0; ix<256; ix++ )
		{
			float fix = float(ix)/256.0f;

            //static f32 PlaneNoiseFunc( f32 fu, f32 fv, f32 fou, f32 fov, f32 fAmp, f32 fFrq )
			float fr = perlin.PlaneNoiseFunc( fix*1.0f, fiy*1.0f,      0.0f,0.0f,1.0f,1.0f )*3.0f;
			float fg = perlin.PlaneNoiseFunc( fix*2.0f, fiy*2.0f,      0.0f,0.0f,1.0f,1.0f )*3.0f;
			float fb = perlin.PlaneNoiseFunc( fix*3.0f, fiy*3.0f,      0.0f,0.0f,1.0f,1.0f )*3.0f;
			
			int idx = iy*256+ix;
			mpData[idx].SetXYZ( fr, fg, fb );

		}
	}
}

void rend_texture2D::Init( /*const CLDevice* pdev*/ ) const
{
/*	if( 0 == mCLhandle )
	{
		cl_image_format format;
		format.image_channel_order = CL_RGBA;
		format.image_channel_data_type = CL_FLOAT;

		int error = 0;
		mCLhandle = clCreateImage2D(	pdev->GetContext(),
										CL_MEM_READ_ONLY| CL_MEM_USE_HOST_PTR,
										& format,
										this->miWidth,
										this->miHeight,
										miWidth*16,
										mpData, & error );


	}*/
}

void rend_texture2D::Load( const std::string& pth )
{
	devil::InitDevIL();
	ILuint image;
	ilGenImages(1, &image);
	ilBindImage(image);
	ILboolean OriginOK = ilOriginFunc( IL_ORIGIN_LOWER_LEFT );
	bool bv = ilLoadImage( (const ILstring) pth.c_str() );
	if( bv )
	{
		ILuint Width = ilGetInteger(IL_IMAGE_WIDTH);
		ILuint Height = ilGetInteger(IL_IMAGE_HEIGHT);
		ILuint Depth = ilGetInteger(IL_IMAGE_DEPTH);
		ILuint BPP = ilGetInteger(IL_IMAGE_BPP);
		ILuint datasize = ilGetInteger(IL_IMAGE_SIZE_OF_DATA);
		ILubyte* Data = ilGetData();

		if( Width*Height!=0 )
		{
			miWidth = Width;
			miHeight = Height;
			
			if( mpData ) delete[] mpData;

			mpData = new ork::CVector4[ miWidth*miHeight ];

			for( int iy=0; iy<miHeight; iy++ )
			{
				for( int ix=0; ix<miWidth; ix++ )
				{
					int idstindex = (iy*miWidth)+ix;
					int isrcindex = idstindex*BPP;

					ILubyte R = Data[isrcindex+0];
					ILubyte G = Data[isrcindex+1];
					ILubyte B = Data[isrcindex+2];
					ILubyte A = (BPP==4) ? Data[isrcindex+3] : 255;

					float fR = float(R)/255.0f;
					float fG = float(G)/255.0f;
					float fB = float(B)/255.0f;
					float fA = float(A)/255.0f;

					mpData[idstindex].Set( fR, fG, fB, fA );

				}
			}		
		}
	}
	ilDeleteImage(image);
}

rend_texture2D::~rend_texture2D() { delete[] mpData; }

ork::CVector4 rend_texture2D::sample_point( float u, float v, bool wrapu, bool wrapv ) const
{
	if( wrapu )
	{
		if( u < 0.0f )
		{
			// -1.2
			float fau = std::abs(u); // 1.2
			u = 1.0f-fmod( fau, 1.0f ); // 1.0f-.2 == .8
		}
		else
		{
			u = fmod( u, 1.0f );
		}
	}
	if( wrapv )
	{
		if( v < 0.0f )
		{
			float fav = std::abs(v);
			v = 1.0f-fmod( fav, 1.0f );
		}
		else
		{
			v = fmod( v, 1.0f );
		}
	}

	if( u>=1.0 ) u = 0.0f; 
	if( u<0.0 ) u = 0.0f; 
	if( v>=1.0 ) v = 0.0f; 
	if( v<0.0 ) v = 0.0f;

	int ix = int(std::floor(u*miWidth));
	int iy = int(std::floor(v*miHeight));
	int idx = iy*miWidth+ix;
	ork::CVector4 rval = mpData[idx];
	return rval;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
rend_fraglist::rend_fraglist()
	: mpHead( 0 )
{
}
///////////////////////////////////////////////////////////////////////////////
void rend_fraglist::AddFragment( rend_fragment* pfrag )
{
	pfrag->mpNext = mpHead;
	mpHead = pfrag;
}
///////////////////////////////////////////////////////////////////////////////
void rend_fraglist::Reset()
{
	mpHead = 0;
}
///////////////////////////////////////////////////////////////////////////////
int rend_fraglist::DepthComplexity() const
{
	struct countvisitor : public rend_fraglist_visitor
	{
		countvisitor() : icount(0) {}
		int icount;
		void Visit( const rend_fragment* pnode ) // virtual
		{
			icount++;
		}
	};

	countvisitor myvisitor;
	Visit( myvisitor );
	return myvisitor.icount;
}
///////////////////////////////////////////////////////////////////////////////
void rend_fraglist::Visit(rend_fraglist_visitor& visitor) const
{
	const rend_fragment* pfrag = mpHead;
	while( 0 != pfrag )
	{
		visitor.Visit( pfrag );
		pfrag = pfrag->mpNext;
	}
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void FragmentPoolNode::Reset()
{
	mFragmentIndex=0;
}
///////////////////////////////////////////////////////////////////////////////
rend_fragment* FragmentPoolNode::AllocFragment()
{
	int idx = mFragmentIndex++;
	if( idx<kNumFragments )
	{
		rend_fragment* rval = & mFragments[idx];
		rval->mpNext = 0;
		rval->mpPrimitive = 0;
		return rval;
	}
	return 0;
}
void FragmentPoolNode::AllocFragments(rend_fragment** ppfrag, int icount)
{
	OrkAssert((mFragmentIndex+icount)<=kNumFragments);
	for( int i=0; i<icount; i++ )
	{
		int idx = mFragmentIndex++;
		rend_fragment* rval = & mFragments[idx];
		rval->mpNext = 0;
		rval->mpPrimitive = 0;
		ppfrag[i] = rval;
	}
}
///////////////////////////////////////////////////////////////////////////////
FragmentPoolNode::FragmentPoolNode()
{
	mFragmentIndex = 0;
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
FragmentPool::FragmentPool()
{
	mFragmentPoolNodes.resize(1);
	mFragmentPoolNodes[0] = new FragmentPoolNode;
}
///////////////////////////////////////////////////////////////////////////////
void FragmentPool::Reset()
{
	int inumnodes = mFragmentPoolNodes.size();
	for( int i=0; i<inumnodes; i++ )
	{
		mFragmentPoolNodes[i]->Reset();
	}
	miPoolNodeIndex = 0;
	if( inumnodes )
	{
		mpCurNode = mFragmentPoolNodes[0];
	}
}
///////////////////////////////////////////////////////////////////////////////
rend_fragment* FragmentPool::AllocFragment()
{	rend_fragment* rval = 0;
	if( mFreeFragments.size() )
	{	orkvector<rend_fragment*>::iterator it = mFreeFragments.end()-1;
		rval = *it;
		mFreeFragments.erase(it);
	}
	else
	{	int inumnodes = mFragmentPoolNodes.size();
		while( rval == 0 )
		{	if( miPoolNodeIndex>=inumnodes )
			{	
				mFragmentPoolNodes.resize(inumnodes*2);
				FragmentPoolNode* newnodes = new FragmentPoolNode[ inumnodes ];
				for( int i=inumnodes; i<(inumnodes*2); i++ )
				{
					mFragmentPoolNodes[i] = newnodes+(i-inumnodes);
				}
			}
			FragmentPoolNode* node = mFragmentPoolNodes[ miPoolNodeIndex ];
			rval = node->AllocFragment();
			if( 0==rval ) miPoolNodeIndex++;
		}
	}
	return rval;
}
///////////////////////////////////////////////////////////////////////////////
bool FragmentPool::AllocFragments(rend_fragment** ppfrags, int icount)
{	bool bOK = true;

	FragmentPoolNode* curnode = mFragmentPoolNodes[ miPoolNodeIndex ];
	for( int i=0; i<icount; )
	{	int inumnodes = mFragmentPoolNodes.size();
		if( miPoolNodeIndex>=inumnodes )
		{	if( 0 == inumnodes )
			{	mFragmentPoolNodes.resize(1);
				mFragmentPoolNodes[0] = new FragmentPoolNode;
				
			}
			else
			{	mFragmentPoolNodes.resize(inumnodes*2);
				OrkAssert( inumnodes==miPoolNodeIndex );
				for( int j=inumnodes; j<(inumnodes*2); j++ )
				{
					mFragmentPoolNodes[j] = new FragmentPoolNode;
				}
			}
		}
		FragmentPoolNode* node = mFragmentPoolNodes[ miPoolNodeIndex ];
		/////////////////////////////////////////////////////
		int inumfree = node->GetNumAvailable();
		if( 0 == inumfree )
		{
			miPoolNodeIndex++;
		}
		else
		{
			int inum2alloc = ((i+icount)>inumfree) ? inumfree : (icount-i);
			node->AllocFragments( ppfrags+i, inum2alloc );
			i += inum2alloc;
		}
		/////////////////////////////////////////////////////
	} // for( int i=0; i<icount; i++ )
	return bOK;
}
///////////////////////////////////////////////////////////////////////////////
void FragmentPool::FreeFragment(rend_fragment*pfrag)
{
	mFreeFragments.push_back(pfrag);
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
rend_prefrags::rend_prefrags()
	: miNumPreFrags(0)
	, miMaxPreFrags(1<<16)
{
	mPreFrags.resize(miMaxPreFrags);
}
void rend_prefrags::Reset()
{
	miNumPreFrags = 0;
}
rend_prefragment& rend_prefrags::AllocPreFrag()
{
	if( (miNumPreFrags+1)>miMaxPreFrags )
	{
		miMaxPreFrags*=2;
		mPreFrags.resize(miMaxPreFrags);
	}
	int idx = miNumPreFrags;
	miNumPreFrags++;
	rend_prefragment& pfrag = mPreFrags[idx];

//	pfrag.mpSrcPrimitive = 0;
//	pfrag.srcvtxR = 0;
//	pfrag.srcvtxS = 0;
//	pfrag.srcvtxT = 0;

	return pfrag;
}
///////////////////////////////////////////////////////////////////////////////
