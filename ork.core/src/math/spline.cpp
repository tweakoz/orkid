////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 


#include <ork/pch.h>

#include <ork/orkmath.h>
#include <ork/math/spline.hpp>

///////////////////////////////////////////////////////////////////////////////
namespace ork {
///////////////////////////////////////////////////////////////////////////////

SplineV2::SplineV2( const CVector2& data )
	: mData( data )
{

}

float SplineV2::GetComponent(int idx) const
{
	OrkAssert(idx<Nu_components);
	float rval = 0.0f;
	switch( idx )
	{
		case 0:
			rval = mData.GetX();
			break;
		case 1:
			rval = mData.GetY();
			break;
	}
	return rval;
}
void SplineV2::SetComponent(int idx, float fv)
{
	OrkAssert(idx<Nu_components);
	switch( idx )
	{
		case 0:
			mData.SetX(fv);
			break;
		case 1:
			mData.SetY(fv);
			break;
	}
}

///////////////////////////////////////////////////////////////////////////////

SplineV3::SplineV3( const CVector3& data )
	: mData( data )
{

}

float SplineV3::GetComponent(int idx) const
{
	OrkAssert(idx<Nu_components);
	float rval = 0.0f;
	switch( idx )
	{
		case 0:
			rval = mData.GetX();
			break;
		case 1:
			rval = mData.GetY();
			break;
		case 2:
			rval = mData.GetZ();
			break;
	}
	return rval;
}
void SplineV3::SetComponent(int idx, float fv)
{
	OrkAssert(idx<Nu_components);
	switch( idx )
	{
		case 0:
			mData.SetX(fv);
			break;
		case 1:
			mData.SetY(fv);
			break;
		case 2:
			mData.SetZ(fv);
			break;
	}
}

///////////////////////////////////////////////////////////////////////////////

SplineV4::SplineV4( const CVector4& data )
	: mData( data )
{

}

float SplineV4::GetComponent(int idx) const
{
	OrkAssert(idx<Nu_components);
	float rval = 0.0f;
	switch( idx )
	{
		case 0:
			rval = mData.GetX();
			break;
		case 1:
			rval = mData.GetY();
			break;
		case 2:
			rval = mData.GetZ();
			break;
		case 3:
			rval = mData.GetW();
			break;
	}
	return rval;
}
void SplineV4::SetComponent(int idx, float fv)
{
	OrkAssert(idx<Nu_components);
	switch( idx )
	{
		case 0:
			mData.SetX(fv);
			break;
		case 1:
			mData.SetY(fv);
			break;
		case 2:
			mData.SetZ(fv);
			break;
		case 3:
			mData.SetW(fv);
			break;
	}
}

///////////////////////////////////////////////////////////////////////////////

template class CatmullRomSpline<SplineV2>;
template class CatmullRomSpline<SplineV3>;
template class CatmullRomSpline<SplineV4>;

///////////////////////////////////////////////////////////////////////////////

#if 0 //defined( ORK_CONFIG_EDITORBUILD )

//////////////////////////////////////////////////////////

void CatmullRomSpline::DrawLines( GfxTarget *pTarg )
{
	//UniformSample();
	//mVertexBuffer.SetNumVertices( miNumVerts );
	
	static CModelerPickMaterial * ModelerMat = new CModelerPickMaterial( CModelerPickMaterial::GetClass() );

	CModelerGlobal::SetRenderMode( CModelerGlobal::ERENMODE_MOD );

	int inumpasses = ModelerMat->BeginBlock();
	for( int ipass=0; ipass<inumpasses; ipass++ )
	{
		if( ModelerMat->BeginPass( ipass ) )
		{
			//pTarg->VtxBuf_DrawEML( & mVertexBuffer, 0 );

			ModelerMat->EndPass();
		}
	}
	ModelerMat->EndBlock();
}

//////////////////////////////////////////////////////////

void CatmullRomSpline::DrawCVs( GfxTarget *pTarg )
{
	//UniformSample();
	//mCVVertexBuffer.SetNumVertices( miNumCV );
	
	static CModelerPickMaterial * ModelerMat = new CModelerPickMaterial( CModelerPickMaterial::GetClass() );

	pTarg->PushMaterial( ModelerMat );

	ModelerMat->mfParticleSize = 10.0f;

	CMatrix4 Translate;

	for( int icv=0; icv<miNumCV; icv++ )
	{
		//CSplineCV & CV = mpSplineCVObjects[ icv ];
		
		//CVector4 & Vec4 = *GetCVVector( icv );

		//Translate.SetTranslation( Vec4 );

		//pTarg->MTXI()->PushMMatrix( Translate );

		//pTarg->PushObjID( reinterpret_cast<U32>( & CV ) );

		//CGfxPrimitives::RenderDiamond( pTarg );

		//pTarg->PopObjID();

		//int inumpasses = ModelerMat->BeginBlock();
		//for( int ipass=0; ipass<inumpasses; ipass++ )
		//{
		//	if( ModelerMat->BeginPass( ipass ) )
		//	{
		//		pTarg->VtxBuf_DrawEML( & mCVVertexBuffer, 0 );
//
//				ModelerMat->EndPass();
//			}
//		}
//		ModelerMat->EndBlock();
		//pTarg->MTXI()->PopMMatrix();
	}

	pTarg->PopMaterial();

}
#endif

}
