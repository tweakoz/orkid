///////////////////////////////////////////////////////////////////////////////
// Orkid
// Copyrigh 1996-2004, Michael T. Mayers
// See License at OrkidRoot/license.html or http://www.tweakoz.com/orkid/license.html
///////////////////////////////////////////////////////////////////////////////

#include <orktool/orktool_pch.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/gfxmodel.h>
#include <orktool/filter/gfx/collada/collada.h>
#include <orktool/filter/gfx/meshutil/meshutil_fixedgrid.h>
#include <orktool/filter/gfx/meshutil/clusterizer.h>
#include "../meshutil/meshutil_stripper.h"

const bool gbFORCEDICE = true;
const int kDICESIZE = 512;

using namespace ork::tool;

namespace ork::MeshUtil {
///////////////////////////////////////////////////////////////////////////////

bool XgmRigidClusterBuilder::AddTriangle( const XgmClusterTri& Triangle )
{
	size_t ivcount = mSubMesh.RefVertexPool().GetNumVertices();
	int iicount = (int) mSubMesh.GetNumPolys();

	static const int kvtresh = (1<<16)-4;
	static const int kithresh = (1<<16)/3;

	if( ivcount > kvtresh )
	{
		return false;
	}
	if( iicount > kithresh )
	{
		return false;
	}

	int iv0 = mSubMesh.MergeVertex( Triangle.Vertex[0] );
	int iv1 = mSubMesh.MergeVertex( Triangle.Vertex[1] );
	int iv2 = mSubMesh.MergeVertex( Triangle.Vertex[2] );
	poly the_poly( iv0, iv1, iv2 );
	mSubMesh.MergePoly( the_poly );

	return true;
}

///////////////////////////////////////////////////////////////////////////////

void XgmRigidClusterBuilder::BuildVertexBuffer( const MeshUtil::ToolMaterialGroup& matgroup )
{
	switch( matgroup.GetVtxStreamFormat() )
	{
		case lev2::EVTXSTREAMFMT_V12N6C2T4: // basic wii environmen
		{	BuildVertexBuffer_V12N6C2T4();
			break;
		}
		case lev2::EVTXSTREAMFMT_V12N12B12T8C4: // basic pc environment
		{	BuildVertexBuffer_V12N12B12T8C4();
			break;
		}
		case lev2::EVTXSTREAMFMT_V12N12B12T16: // basic pc environment
		{	BuildVertexBuffer_V12N12B12T16();
			break;
		}
		case lev2::EVTXSTREAMFMT_V12N12T16C4: // basic pc environment
		{	BuildVertexBuffer_V12N12T16C4();
			break;
		}
		default:
		{	OrkAssert(false);
			break;
		}
	}
}

///////////////////////////////////////////////////////////////////////////////

void XgmRigidClusterBuilder::BuildVertexBuffer_V12N6C2T4() // basic wii environment
{
	const float kVertexScale(1.0f);
	const fvec2 UVScale( 1.0f,1.0f );
	int NumVertexIndices = mSubMesh.RefVertexPool().GetNumVertices();
	lev2::GfxTargetDummy DummyTarget;
	lev2::VtxWriter<ork::lev2::SVtxV12N6C2T4> vwriter;
	mpVertexBuffer = new ork::lev2::StaticVertexBuffer<ork::lev2::SVtxV12N6C2T4>( NumVertexIndices, 0, ork::lev2::EPRIM_MULTI );
	vwriter.Lock( &DummyTarget, mpVertexBuffer, NumVertexIndices );
	for( int iv=0; iv<NumVertexIndices; iv++ )
	{	ork::lev2::SVtxV12N6C2T4 OutVtx;
		const MeshUtil::vertex & InVtx = mSubMesh.RefVertexPool().GetVertex(iv);

		OutVtx.mX = InVtx.mPos.GetX()*kVertexScale;
		OutVtx.mY = InVtx.mPos.GetY()*kVertexScale;
		OutVtx.mZ = InVtx.mPos.GetZ()*kVertexScale;

		OutVtx.mNX = s16( InVtx.mNrm.GetX() * float(32767.0f) );
		OutVtx.mNY = s16( InVtx.mNrm.GetY() * float(32767.0f) );
		OutVtx.mNZ = s16( InVtx.mNrm.GetZ() * float(32767.0f) );

		OutVtx.mU = s16( InVtx.mUV[0].mMapTexCoord.GetX() * float(1024.0f) );
		OutVtx.mV = s16( InVtx.mUV[0].mMapTexCoord.GetY() * float(1024.0f) );

		int ir = int(InVtx.mCol[0].GetY()*255.0f);
		int ig = int(InVtx.mCol[0].GetZ()*255.0f);
		int ib = int(InVtx.mCol[0].GetW()*255.0f);

		OutVtx.mColor = U16(((ir>>3)<<11)|((ig>>2)<<5)|((ib>>3)<<0));
		vwriter.AddVertex(OutVtx);
	}
	vwriter.UnLock(&DummyTarget);
	mpVertexBuffer->SetNumVertices( NumVertexIndices );
}

///////////////////////////////////////////////////////////////////////////////

void XgmRigidClusterBuilder::BuildVertexBuffer_V12N12B12T8C4() // basic pc environment
{
	const float kVertexScale(1.0f);
	const fvec2 UVScale( 1.0f,1.0f );
	int NumVertexIndices = mSubMesh.RefVertexPool().GetNumVertices();
	lev2::GfxTargetDummy DummyTarget;
	lev2::VtxWriter<ork::lev2::SVtxV12N12B12T8C4> vwriter;
	mpVertexBuffer = new ork::lev2::StaticVertexBuffer<ork::lev2::SVtxV12N12B12T8C4>( NumVertexIndices, 0, ork::lev2::EPRIM_MULTI );
	vwriter.Lock( &DummyTarget, mpVertexBuffer, NumVertexIndices );
	for( int iv=0; iv<NumVertexIndices; iv++ )
	{	ork::lev2::SVtxV12N12B12T8C4 OutVtx;
		const MeshUtil::vertex & InVtx = mSubMesh.RefVertexPool().GetVertex(iv);
		OutVtx.mPosition = InVtx.mPos*kVertexScale;
		OutVtx.mUV0 = InVtx.mUV[0].mMapTexCoord * UVScale;
		OutVtx.mNormal = InVtx.mNrm;
		OutVtx.mBiNormal = InVtx.mUV[0].mMapBiNormal;
		OutVtx.mColor = InVtx.mCol[0].GetRGBAU32();
		vwriter.AddVertex(OutVtx);
	}
	vwriter.UnLock(&DummyTarget);
	mpVertexBuffer->SetNumVertices( NumVertexIndices );
}

///////////////////////////////////////////////////////////////////////////////

void XgmRigidClusterBuilder::BuildVertexBuffer_V12N12T16C4() // basic pc environment
{
	const float kVertexScale(1.0f);
	const fvec2 UVScale( 1.0f,1.0f );
	int NumVertexIndices = mSubMesh.RefVertexPool().GetNumVertices();
	lev2::GfxTargetDummy DummyTarget;
	lev2::VtxWriter<ork::lev2::SVtxV12N12T16C4> vwriter;
	mpVertexBuffer = new ork::lev2::StaticVertexBuffer<ork::lev2::SVtxV12N12T16C4>( NumVertexIndices, 0, ork::lev2::EPRIM_MULTI );
	vwriter.Lock( &DummyTarget, mpVertexBuffer, NumVertexIndices );
	for( int iv=0; iv<NumVertexIndices; iv++ )
	{	ork::lev2::SVtxV12N12T16C4 OutVtx;
		const MeshUtil::vertex & InVtx = mSubMesh.RefVertexPool().GetVertex(iv);
		OutVtx.mPosition = InVtx.mPos*kVertexScale;
		OutVtx.mUV0 = InVtx.mUV[0].mMapTexCoord * UVScale;
		OutVtx.mUV1 = InVtx.mUV[1].mMapTexCoord * UVScale;
		OutVtx.mNormal = InVtx.mNrm;
		OutVtx.mColor = InVtx.mCol[0].GetRGBAU32();
		vwriter.AddVertex(OutVtx);
	}
	vwriter.UnLock(&DummyTarget);
	mpVertexBuffer->SetNumVertices( NumVertexIndices );
}

///////////////////////////////////////////////////////////////////////////////

void XgmRigidClusterBuilder::BuildVertexBuffer_V12N12B12T16() // basic pc environment
{
	const float kVertexScale(1.0f);
	const fvec2 UVScale( 1.0f,1.0f );
	int NumVertexIndices = mSubMesh.RefVertexPool().GetNumVertices();
	lev2::GfxTargetDummy DummyTarget;
	lev2::VtxWriter<ork::lev2::SVtxV12N12B12T16> vwriter;
	mpVertexBuffer = new ork::lev2::StaticVertexBuffer<ork::lev2::SVtxV12N12B12T16>( NumVertexIndices, 0, ork::lev2::EPRIM_MULTI );
	vwriter.Lock( &DummyTarget, mpVertexBuffer, NumVertexIndices );
	for( int iv=0; iv<NumVertexIndices; iv++ )
	{	ork::lev2::SVtxV12N12B12T16 OutVtx;
		const MeshUtil::vertex & InVtx = mSubMesh.RefVertexPool().GetVertex(iv);
		OutVtx.mPosition = InVtx.mPos*kVertexScale;
		OutVtx.mUV0 = InVtx.mUV[0].mMapTexCoord * UVScale;
		OutVtx.mUV1 = InVtx.mUV[1].mMapTexCoord * UVScale;
		OutVtx.mNormal = InVtx.mNrm;
		OutVtx.mBiNormal = InVtx.mUV[0].mMapBiNormal;
		//OutVtx.mColor = InVtx.mCol[0].GetRGBAU32();
		vwriter.AddVertex(OutVtx);
	}
	vwriter.UnLock(&DummyTarget);
	mpVertexBuffer->SetNumVertices( NumVertexIndices );
}

///////////////////////////////////////////////////////////////////////////////
} //namespace ork::MeshUtil {
