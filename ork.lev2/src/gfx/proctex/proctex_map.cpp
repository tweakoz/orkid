///////////////////////////////////////////////////////////////////////////////
// Orkid
// Copyright 1996-2020, Michael T. Mayers
///////////////////////////////////////////////////////////////////////////////
#include <ork/pch.h>
#include <ork/lev2/gfx/proctex/proctex.h>
#include <ork/lev2/gfx/gfxmaterial_test.h>

#include <ork/reflect/RegisterProperty.h>
#include <ork/reflect/DirectObjectMapPropertyType.h>
#include <ork/reflect/DirectObjectPropertyType.hpp>

#include <ork/reflect/enum_serializer.inl>
#include <ork/math/polar.h>
#include <ork/math/plane.hpp>

///////////////////////////////////////////////////////////////////////////////

INSTANTIATE_TRANSPARENT_RTTI(ork::proctex::SphMap,"proctex::SphMap");
INSTANTIATE_TRANSPARENT_RTTI(ork::proctex::SphRefract,"proctex::SphRefract");
INSTANTIATE_TRANSPARENT_RTTI(ork::proctex::H2N,"proctex::H2N");
INSTANTIATE_TRANSPARENT_RTTI(ork::proctex::UvMap,"proctex::UvMap");
INSTANTIATE_TRANSPARENT_RTTI(ork::proctex::Kaled,"proctex::Kaled");

BEGIN_ENUM_SERIALIZER(ork::proctex, EKaledMode)
	DECLARE_ENUM(EKM_4SQU)
	DECLARE_ENUM(ESH_8TRI)
	DECLARE_ENUM(ESH_24TRI)
END_ENUM_SERIALIZER()

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace proctex {
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void Colorize::Describe()
{	RegisterObjInpPlug( Colorize, InputA );
	RegisterObjInpPlug( Colorize, InputB );
	ork::reflect::RegisterProperty( "AntiAlias", & Colorize::mbAA );
}
Colorize::Colorize()
	: meColorizeType( ECT_1D )
	, ConstructInpPlug( InputA,dataflow::EPR_UNIFORM,gNoCon )
	, ConstructInpPlug( InputB,dataflow::EPR_UNIFORM,gNoCon )
	, mbAA(false)
{
}
void Colorize::compute( ProcTex& ptex )
{	auto proc_ctx = ptex.GetPTC();
	auto pTARG = ptex.GetTarget();
	Buffer& buffer = GetWriteBuffer(ptex);
	const ImgOutPlug* conplugA = rtti::autocast(mPlugInpInputA.GetExternalOutput());
	const ImgOutPlug* conplugB = rtti::autocast(mPlugInpInputB.GetExternalOutput());
	if(conplugA && conplugB )
	{	struct AA16RenderCells : public AA16Render
		{	virtual void DoRender( float left, float right, float top, float bot, Buffer& buf  )
			{	mPTX.GetTarget()->PushModColor( ork::fvec4::Red() );
				fmtx4 mtxortho = mPTX.GetTarget()->MTXI()->Ortho( 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f );
				mPTX.GetTarget()->BindMaterial( & mtl );
				mPTX.GetTarget()->MTXI()->PushPMatrix( mtxortho );

				float u0 = (0.5f+(left*0.5f));
				float u1 = (0.5f+(right*0.5f));
				float v0 = 0.5f+(top*0.5f);
				float v1 = 0.5f+(bot*0.5f);
				RenderQuad( mPTX.GetTarget(), 0.0f, 0.0f, 1.0f, 1.0f, u0, v0, u1, v1 );

				mPTX.GetTarget()->MTXI()->PopPMatrix();
				mPTX.GetTarget()->PopModColor();
			}
			lev2::GfxMaterial3DSolid mtl;
			AA16RenderCells(	ProcTex& ptex,
								Buffer& bo,
								const ImgOutPlug* cpa,
								const ImgOutPlug* cpb )
				: AA16Render( ptex, bo )
				, mtl( ptex.GetTarget(), "orkshader://proctex", "colorize" )
			{
				auto targ = mPTX.GetTarget();
				mtl.SetColorMode( lev2::GfxMaterial3DSolid::EMODE_USER );
				mtl._rasterstate.SetAlphaTest( ork::lev2::EALPHATEST_OFF );
				mtl._rasterstate.SetCullTest( ork::lev2::ECULLTEST_OFF );
				mtl._rasterstate.SetBlending( ork::lev2::EBLENDING_OFF );
				mtl._rasterstate.SetDepthTest( ork::lev2::EDEPTHTEST_ALWAYS );
				auto inptexa = cpa->GetValue().GetTexture(ptex);
				auto inptexb = cpb->GetValue().GetTexture(ptex);
				inptexa->TexSamplingMode().PresetTrilinearWrap();
				inptexb->TexSamplingMode().PresetTrilinearWrap();
				targ->TXI()->ApplySamplingMode(inptexa);
				targ->TXI()->ApplySamplingMode(inptexb);
				mtl.SetTexture( inptexa );
				mtl.SetTexture2( inptexb );
				mtl.SetUser0( fvec4(0.0f,0.0f,0.0f,float(bo.miW)) );

				mOrthoBoxXYWH = fvec4( -1.0f, -1.0f, 2.0f, 2.0f );
				//mOrthoBoxXYWH = fvec4( 1.0f, 1.0f, -2.0f, -2.0f );
			}
		};

		////////////////////////////////////////////////////////////////

		AA16RenderCells renderer( ptex, buffer, conplugA, conplugB );
		renderer.Render( mbAA );
	}
	MarkClean();
	//buffer.mHash = testhash;
}
ork::dataflow::inplugbase* Colorize::GetInput(int idx)
{	ork::dataflow::inplugbase* rval = 0;
	switch( idx )
	{	case 0:	rval = & mPlugInpInputA; break; 
		case 1:	rval = & mPlugInpInputB; break; 
	}
	return rval;
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void UvMap::Describe()
{	RegisterObjInpPlug( UvMap, InputA );
	RegisterObjInpPlug( UvMap, InputB );
	ork::reflect::RegisterProperty( "AntiAlias", & UvMap::mbAA );
}
UvMap::UvMap()
	: ConstructInpPlug( InputA,dataflow::EPR_UNIFORM,gNoCon )
	, ConstructInpPlug( InputB,dataflow::EPR_UNIFORM,gNoCon )
	, mbAA(false)
{
}
void RenderMapQuad(lev2::Context* targ, lev2::GfxMaterial3DSolid& mtl, float l, float r, float t, float b)
{	auto mtxi = targ->MTXI();
	targ->PushModColor( ork::fvec4::Red() );
	fmtx4 mtxortho = mtxi->Ortho( l, r, t, b, 0.0f, 1.0f );
	targ->BindMaterial( & mtl );
	mtxi->PushPMatrix( mtxortho );
	RenderQuad( targ, -1,1,1,-1 );
	mtxi->PopPMatrix();
	targ->PopModColor();
}
void UvMap::compute( ProcTex& ptex )
{	auto proc_ctx = ptex.GetPTC();
	auto pTARG = ptex.GetTarget();
	Buffer& buffer = GetWriteBuffer(ptex);
	const ImgOutPlug* conplugA = rtti::autocast(mPlugInpInputA.GetExternalOutput());
	const ImgOutPlug* conplugB = rtti::autocast(mPlugInpInputB.GetExternalOutput());
	if(conplugA && conplugB )
	{	struct AA16RenderCells : public AA16Render
		{	virtual void DoRender( float left, float right, float top, float bot, Buffer& buf  )
			{	RenderMapQuad(mPTX.GetTarget(),mtl,left,right,top,bot);
			}
			lev2::GfxMaterial3DSolid mtl;
			AA16RenderCells(	ProcTex& ptex,
								Buffer& bo,
								const ImgOutPlug* cpa,
								const ImgOutPlug* cpb )
				: AA16Render( ptex, bo )
				, mtl( ptex.GetTarget(), "orkshader://proctex", "uvmap" )
			{
				auto targ = mPTX.GetTarget();
				mtl.SetColorMode( lev2::GfxMaterial3DSolid::EMODE_USER );
				mtl._rasterstate.SetAlphaTest( ork::lev2::EALPHATEST_OFF );
				mtl._rasterstate.SetCullTest( ork::lev2::ECULLTEST_OFF );
				mtl._rasterstate.SetBlending( ork::lev2::EBLENDING_OFF );
				mtl._rasterstate.SetDepthTest( ork::lev2::EDEPTHTEST_ALWAYS );
				auto inptexa = cpa->GetValue().GetTexture(ptex);
				auto inptexb = cpb->GetValue().GetTexture(ptex);
				inptexa->TexSamplingMode().PresetTrilinearWrap();
				inptexb->TexSamplingMode().PresetTrilinearWrap();
				targ->TXI()->ApplySamplingMode(inptexa);
				targ->TXI()->ApplySamplingMode(inptexb);
				mtl.SetTexture( inptexa );
				mtl.SetTexture2( inptexb );
				mtl.SetUser0( fvec4(0.0f,0.0f,0.0f,float(bo.miW)) );

				mOrthoBoxXYWH = fvec4( -1.0f, -1.0f, 2.0f, 2.0f );
			}
		};

		////////////////////////////////////////////////////////////////

		AA16RenderCells renderer( ptex, buffer, conplugA, conplugB );
		renderer.Render( mbAA );
	}
	MarkClean();
	//buffer.mHash = testhash;
}
ork::dataflow::inplugbase* UvMap::GetInput(int idx)
{	ork::dataflow::inplugbase* rval = 0;
	switch( idx )
	{	case 0:	rval = & mPlugInpInputA; break; 
		case 1:	rval = & mPlugInpInputB; break; 
	}
	return rval;
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void SphMap::Describe()
{	RegisterObjInpPlug( SphMap, InputN );
	RegisterObjInpPlug( SphMap, InputR );
	ork::reflect::RegisterProperty( "AntiAlias", & SphMap::mbAA );
	RegisterFloatXfPlug( SphMap, Directionality, 0.0f, 1.0f, ged::OutPlugChoiceDelegate );
}
SphMap::SphMap()
	: ConstructInpPlug( InputN,dataflow::EPR_UNIFORM,gNoCon )
	, ConstructInpPlug( InputR,dataflow::EPR_UNIFORM,gNoCon )
	, mPlugInpDirectionality( this,dataflow::EPR_UNIFORM,mfDirectionality, "dir" )
	, mbAA(false)
{
}
void SphMap::compute( ProcTex& ptex )
{	auto proc_ctx = ptex.GetPTC();
	auto pTARG = ptex.GetTarget();
	Buffer& buffer = GetWriteBuffer(ptex);
	const ImgOutPlug* conplugA = rtti::autocast(mPlugInpInputN.GetExternalOutput());
	const ImgOutPlug* conplugB = rtti::autocast(mPlugInpInputR.GetExternalOutput());
	if(conplugA && conplugB )
	{	struct AA16RenderSphMap : public AA16Render
		{
			virtual void DoRender( float left, float right, float top, float bot, Buffer& buf  )
			{	RenderMapQuad(mPTX.GetTarget(),mtl,left,right,top,bot);
			}
			lev2::GfxMaterial3DSolid mtl;
			AA16RenderSphMap(	ProcTex& ptex,
								Buffer& bo,
								const ImgOutPlug* cpa,
								const ImgOutPlug* cpb,
								float direc )
				: AA16Render( ptex, bo )
				, mtl( ptex.GetTarget(), "orkshader://proctex", "sphmap" )
			{
				auto targ = mPTX.GetTarget();
				mtl.SetColorMode( lev2::GfxMaterial3DSolid::EMODE_USER );
				mtl._rasterstate.SetAlphaTest( lev2::EALPHATEST_OFF );
				mtl._rasterstate.SetCullTest( lev2::ECULLTEST_OFF );
				mtl._rasterstate.SetBlending( lev2::EBLENDING_OFF );
				mtl._rasterstate.SetDepthTest( lev2::EDEPTHTEST_ALWAYS );
				auto inptexa = cpa->GetValue().GetTexture(ptex);
				auto inptexb = cpb->GetValue().GetTexture(ptex);
				inptexa->TexSamplingMode().PresetTrilinearWrap();
				inptexb->TexSamplingMode().PresetTrilinearWrap();
				targ->TXI()->ApplySamplingMode(inptexa);
				targ->TXI()->ApplySamplingMode(inptexb);
				mtl.SetTexture( inptexa );
				mtl.SetTexture2( inptexb );
				mtl.SetUser0( fvec4(0.0f,0.0f,direc,float(bo.miW)) );

				mOrthoBoxXYWH = fvec4( -1.0f, -1.0f, 2.0f, 2.0f );
			}
		};

		////////////////////////////////////////////////////////////////

		AA16RenderSphMap renderer(	ptex, buffer,
									conplugA, conplugB,
									mPlugInpDirectionality.GetValue() );
		renderer.Render( mbAA );
	}
	MarkClean();
	//buffer.mHash = testhash;
}
ork::dataflow::inplugbase* SphMap::GetInput(int idx)
{	ork::dataflow::inplugbase* rval = 0;
	switch( idx )
	{	case 0:	rval = & mPlugInpInputN; break; 
		case 1:	rval = & mPlugInpInputR; break; 
		case 2:	rval = & mPlugInpDirectionality; break; 
	}
	return rval;
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void SphRefract::Describe()
{	RegisterObjInpPlug( SphRefract, InputA );
	RegisterObjInpPlug( SphRefract, InputB );
	RegisterFloatXfPlug( SphRefract, IOR, -10.0f, 10.0f, ged::OutPlugChoiceDelegate );
	RegisterFloatXfPlug( SphRefract, Directionality, 0.0f, 1.0f, ged::OutPlugChoiceDelegate );

	ork::reflect::RegisterProperty( "AntiAlias", & SphRefract::mbAA );
}
SphRefract::SphRefract()
	: ConstructInpPlug( InputA,dataflow::EPR_UNIFORM,gNoCon )
	, ConstructInpPlug( InputB,dataflow::EPR_UNIFORM,gNoCon )
	, mPlugInpIOR( this,dataflow::EPR_UNIFORM, mfIOR, "ior" )
	, mPlugInpDirectionality( this,dataflow::EPR_UNIFORM, mfDirectionality, "dir" )
	, mbAA(false)
{
}
void SphRefract::compute( ProcTex& ptex )
{	auto proc_ctx = ptex.GetPTC();
	auto pTARG = ptex.GetTarget();
	Buffer& buffer = GetWriteBuffer(ptex);
	const ImgOutPlug* conplugA = rtti::autocast(mPlugInpInputA.GetExternalOutput());
	const ImgOutPlug* conplugB = rtti::autocast(mPlugInpInputB.GetExternalOutput());
	if(conplugA && conplugB )
	{	struct AA16RenderRefr : public AA16Render
		{
			virtual void DoRender( float left, float right, float top, float bot, Buffer& buf  )
			{	RenderMapQuad(mPTX.GetTarget(),mtl,left,right,top,bot);
			}
			lev2::GfxMaterial3DSolid mtl;
			AA16RenderRefr(	ProcTex& ptex,
							Buffer& bo,
							const ImgOutPlug* cpa,
							const ImgOutPlug* cpb,
							float direc, float ior )
				: AA16Render( ptex, bo )
				, mtl( ptex.GetTarget(), "orkshader://proctex", "sphrefract" )
			{
				mtl.SetColorMode( lev2::GfxMaterial3DSolid::EMODE_USER );
				mtl._rasterstate.SetAlphaTest( ork::lev2::EALPHATEST_OFF );
				mtl._rasterstate.SetCullTest( ork::lev2::ECULLTEST_OFF );
				mtl._rasterstate.SetBlending( ork::lev2::EBLENDING_OFF );
				mtl._rasterstate.SetDepthTest( ork::lev2::EDEPTHTEST_ALWAYS );
				mtl.SetTexture( cpa->GetValue().GetTexture(ptex) );
				mtl.SetTexture2( cpb->GetValue().GetTexture(ptex) );
				mtl.SetUser0( fvec4(0.0f,direc,ior,float(bo.miW)) );

				mOrthoBoxXYWH = fvec4( -1.0f, -1.0f, 2.0f, 2.0f );
			}
		};

		////////////////////////////////////////////////////////////////

		AA16RenderRefr renderer(	ptex, buffer,
									conplugA, conplugB,
									mPlugInpDirectionality.GetValue(),
									mPlugInpIOR.GetValue() );
		renderer.Render( mbAA );
		
	}
	MarkClean();
	//buffer.mHash = testhash;
}
ork::dataflow::inplugbase* SphRefract::GetInput(int idx)
{	ork::dataflow::inplugbase* rval = 0;
	switch( idx )
	{	case 0:	rval = & mPlugInpInputA; break; 
		case 1:	rval = & mPlugInpInputB; break; 
		case 2:	rval = & mPlugInpIOR; break; 
		case 3:	rval = & mPlugInpDirectionality; break; 
	}
	return rval;
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void H2N::Describe()
{	RegisterObjInpPlug( H2N, Input );
	RegisterFloatXfPlug( H2N, ScaleY, 0.0f, 1.0f, ged::OutPlugChoiceDelegate );
	ork::reflect::annotatePropertyForEditor< H2N >( "ScaleY", "editor.range.log", "true" );
	ork::reflect::RegisterProperty( "AntiAlias", & H2N::mbAA );
}
ork::dataflow::inplugbase* H2N::GetInput(int idx)
{	ork::dataflow::inplugbase* rval = 0;
	switch( idx )
	{	case 0:	rval = & mPlugInpInput; break; 
		case 1:	rval = & mPlugInpScaleY; break; 
	}
	return rval;
}
H2N::H2N()
	: ConstructInpPlug(Input,dataflow::EPR_UNIFORM,gNoCon)
	, ConstructInpPlug( ScaleY,dataflow::EPR_UNIFORM,mfScaleY )
	, mbAA(false)
	, mfScaleY(1.0f)
	, mMTL( ork::lev2::GfxEnv::GetRef().loadingContext(), "orkshader://proctex", "h2n" )
{
	mMTL._rasterstate.SetAlphaTest( ork::lev2::EALPHATEST_OFF );
	mMTL._rasterstate.SetCullTest( ork::lev2::ECULLTEST_OFF );
	mMTL._rasterstate.SetBlending( ork::lev2::EBLENDING_OFF );
	mMTL._rasterstate.SetDepthTest( ork::lev2::EDEPTHTEST_ALWAYS );
	mMTL._rasterstate.SetZWriteMask( false );
	mMTL.SetColorMode( lev2::GfxMaterial3DSolid::EMODE_USER );
}
void H2N::compute( ProcTex& ptex )
{	auto proc_ctx = ptex.GetPTC();
	auto pTARG = ptex.GetTarget();
	Buffer& buffer = GetWriteBuffer(ptex);
	const ImgOutPlug* conplug = rtti::autocast(mPlugInpInput.GetExternalOutput());
	if(nullptr==conplug) return;
	////////////////////////////////////////////////////////////////
	float fscy = mPlugInpScaleY.GetValue();
	fmtx4 mtxS;
	mtxS.Scale( 1.0f, fscy, 1.0f );
	////////////////////////////////////////////////////////////////
	auto inptex = conplug->GetValue().GetTexture(ptex);
	inptex->TexSamplingMode().PresetPointAndClamp();
	pTARG->TXI()->ApplySamplingMode(inptex);
	//printf( "HSNinputtex<%p>\n", inptex );
	mMTL.SetTexture( inptex );
	mMTL.SetAuxMatrix( mtxS );
	mMTL.SetUser0( fvec4(0.0f,fscy,0.0f,buffer.miW) );
	////////////////////////////////////////////////////////////////
	struct AA16RenderH2N : public AA16Render
	{
		H2N& mH2N;
		virtual void DoRender( float left, float right, float top, float bot, Buffer& buf  )
		{	
			RenderMapQuad(mPTX.GetTarget(),mH2N.mMTL,left,right,top,bot);
			/*auto pTARG = mPTX.GetTarget();
			pTARG->PushMaterial( & mH2N.mMTL );
			fmtx4 mtxortho = pTARG->MTXI()->Ortho( left, right, top, bot, 0.0f, 1.0f );
			pTARG->MTXI()->PushPMatrix( mtxortho );
			RenderQuad( pTARG, -1.0f, -1.0f, 1.0f, 1.0f );
			pTARG->MTXI()->PopPMatrix();
			pTARG->PopMaterial();*/
		}
		AA16RenderH2N(	H2N& h2n, ProcTex& ptex, Buffer& bo )
			: AA16Render( ptex, bo )
			, mH2N(h2n)
		{
			mOrthoBoxXYWH = fvec4( -1.0f, -1.0f, 2.0f, 2.0f );
		}
	};
	AA16RenderH2N renderer(	*this, ptex, buffer );
	renderer.Render( mbAA );
	////////////////////////////////////////////////////////////////
	MarkClean();
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void Kaled::Describe()
{	RegisterObjInpPlug( Kaled, Input );
	RegisterFloatXfPlug( Kaled, Size, -4.0f, 4.0f, ged::OutPlugChoiceDelegate );
	RegisterFloatXfPlug( Kaled, OffsetX, -1.0f, 1.0f, ged::OutPlugChoiceDelegate );
	RegisterFloatXfPlug( Kaled, OffsetY, -1.0f, 1.0f, ged::OutPlugChoiceDelegate );	
	ork::reflect::RegisterProperty( "Mode", & Kaled::meMode );
	ork::reflect::annotatePropertyForEditor< Kaled >( "Mode", "editor.class", "ged.factory.enum" );
}
///////////////////////////////////////////////////////////////////////////////
Kaled::Kaled()
	: ConstructInpPlug(Input,dataflow::EPR_UNIFORM,gNoCon)
	, ConstructInpPlug( OffsetX,dataflow::EPR_UNIFORM, mfOffsetX )
	, ConstructInpPlug( OffsetY,dataflow::EPR_UNIFORM, mfOffsetY )
	, mPlugInpSize( this,dataflow::EPR_UNIFORM, mfSize, "si" )
	, mVertexBuffer(256, 0, ork::lev2::EPrimitiveType::MULTI)
	, meMode( EKM_4SQU )
	, mfSize( 0.5f )
	, mfOffsetX( 0.5f )
	, mfOffsetY( 0.5f )
{
}
///////////////////////////////////////////////////////////////////////////////
dataflow::inplugbase* Kaled::GetInput(int idx)
{	dataflow::inplugbase* rval = 0;
	switch( idx )
	{	case 0:	rval = & mPlugInpInput; break;
		case 1:	rval = & mPlugInpSize; break;
		case 2:	rval = & mPlugInpOffsetX; break;
		case 3:	rval = & mPlugInpOffsetY; break;
		default:
			OrkAssert(false);
			break;
	}
	return rval;
}
///////////////////////////////////////////////////////////////////////////////
void Kaled::addvtx( float fx, float fy, float fu, float fv )
{	U32 uc = 0xffffffff;
	const float kz = 0.0f;
	mVW.AddVertex( vtxt( fvec3(fx,fy,kz), fvec2( fu, fv ), uc ) );
}
///////////////////////////////////////////////////////////////////////////////
void Kaled::compute( ProcTex& ptex )
{	auto proc_ctx = ptex.GetPTC();
	auto pTARG = ptex.GetTarget();
	Buffer& buffer = GetWriteBuffer(ptex);
	const ImgOutPlug* conplug = rtti::autocast(mPlugInpInput.GetExternalOutput());
	if(conplug)
	{	auto targ = ptex.GetTarget();
		lev2::GfxMaterial3DSolid gridmat( targ );
		gridmat.SetColorMode( lev2::GfxMaterial3DSolid::EMODE_TEX_COLOR );
		gridmat._rasterstate.SetAlphaTest( ork::lev2::EALPHATEST_OFF );
		gridmat._rasterstate.SetCullTest( ork::lev2::ECULLTEST_OFF );
		gridmat._rasterstate.SetBlending( ork::lev2::EBLENDING_OFF );
		gridmat._rasterstate.SetDepthTest( ork::lev2::EDEPTHTEST_ALWAYS );
		auto inptexa = conplug->GetValue().GetTexture(ptex);

		inptexa->TexSamplingMode().PresetTrilinearWrap();
		targ->TXI()->ApplySamplingMode(inptexa);
		gridmat.SetTexture( inptexa );
		gridmat.SetUser0( fvec4(0.0f,0.0f,0.0f,float(buffer.miW)) );
		////////////////////////////////////////////////////////////////
		auto mtxi = targ->MTXI();
		fmtx4 mtxortho = mtxi->Ortho( 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f );
		buffer.PtexBegin(targ,true,false);
		mtxi->PushPMatrix( mtxortho );
		mtxi->PushVMatrix( fmtx4::Identity );
		mtxi->PushMMatrix( fmtx4::Identity );
		{	float fsize = mPlugInpSize.GetValue();
			float foffsetx = mPlugInpOffsetX.GetValue();
			float foffsety = mPlugInpOffsetY.GetValue();
			targ->BindMaterial( & gridmat );
			mVertexBuffer.Reset();
			mVW.Lock( targ, & mVertexBuffer, 128 );
			{	float fuc = foffsetx;
				float fvc = foffsety;
				float fu0 = foffsetx-(fsize*0.5f);
				float fu1 = foffsetx+(fsize*0.5f);
				float fv0 = foffsety-(fsize*0.5f);
				float fv1 = foffsety+(fsize*0.5f);
				switch( meMode )
				{	case EKM_4SQU:
					{	addvtx(0.0f,0.0f, fu0, fv0 );	addvtx(0.5f,0.0f, fu1, fv0 );	addvtx(0.5f,0.5f, fu1, fv1 );
						addvtx(0.0f,0.0f, fu0, fv0 );	addvtx(0.5f,0.5f, fu1, fv1 );	addvtx(0.0f,0.5f, fu0, fv1 );
						addvtx(1.0f,0.0f, fu0, fv0 );	addvtx(0.5f,0.0f, fu1, fv0 );	addvtx(0.5f,0.5f, fu1, fv1 );
						addvtx(1.0f,0.0f, fu0, fv0 );	addvtx(0.5f,0.5f, fu1, fv1 );	addvtx(1.0f,0.5f, fu0, fv1 );
						addvtx(0.0f,1.0f, fu0, fv0 );	addvtx(0.5f,1.0f, fu1, fv0 );	addvtx(0.5f,0.5f, fu1, fv1 );
						addvtx(0.0f,1.0f, fu0, fv0 );	addvtx(0.5f,0.5f, fu1, fv1 );	addvtx(0.0f,0.5f, fu0, fv1 );
						addvtx(1.0f,1.0f, fu0, fv0 );	addvtx(0.5f,1.0f, fu1, fv0 );	addvtx(0.5f,0.5f, fu1, fv1 );
						addvtx(1.0f,1.0f, fu0, fv0 );	addvtx(0.5f,0.5f, fu1, fv1 );	addvtx(1.0f,0.5f, fu0, fv1 );
						break;
					}
					case ESH_8TRI:
					{	for( int ix=0; ix<2; ix++ )
						{	float fx = float(ix)/2.0f;
							bool bx = (ix==0);
							float ftx0 = bx ? 0.0f : 0.5f;
							float ftx1 = bx ? 0.5f : 0.0f;
							for( int iy=0; iy<2; iy++ )
							{	bool by = (iy==0);
								float fy = float(iy)/2.0f;
								float fty0 = by ? 0.0f : 0.5f;
								float fty1 = by ? 0.5f : 0.0f;
								addvtx(fx+ftx0,fy+fty0, fu0, fv0 );
								addvtx(fx+ftx0,fy+fty1, fu0, fv1 );
								addvtx(fx+ftx1,fy+fty1, fu1, fv1 );
								addvtx(fx+ftx0,fy+fty0, fu0, fv0 );
								addvtx(fx+ftx1,fy+fty1, fu1, fv1 );
								addvtx(fx+ftx1,fy+fty0, fu0, fv1 );
							}
						}
						break;
					}
					case ESH_24TRI:
					{	float fdivx = 1.0f/6.0f;
						float fdivy = 1.0f/4.0f;
						for( int ix=0; ix<7; ix++ )
						{	float fx = (float(ix)*fdivx);
							bool bx = (ix&1);
							float ftx0 = -fdivx;
							float ftx1 = +fdivx;
							for( int iy=0; iy<4; iy++ )
							{	bool by = bx ? (iy&1) : ! (iy&1);
								float fy = float(iy)*fdivy;
								float fty0 = by ? 0.0f : fdivy;
								float fty1 = by ? fdivy : 0.0f;
								float fbu0=0.0f, fbu1=0.0f, fbu2=0.0f;
								float fbv0=0.0f, fbv1=0.0f, fbv2=0.0f;
								switch( ix%7 )
								{	case 0:	case 3:	case 6:	
										fbu0=fu0;	fbu1=fuc;	fbu2=fu1;
										fbv0=fv0;	fbv1=fv1;	fbv2=fv1;
										break;
									case 1:	case 4:	
										fbu0=fu1;	fbu1=fu0;	fbu2=fuc;
										fbv0=fv1;	fbv1=fv0;	fbv2=fv1;
										break;
									case 2:	case 5:	
										fbu0=fuc;	fbu1=fu1;	fbu2=fu0;
										fbv0=fv1;	fbv1=fv1;	fbv2=fv0;
										break;
								}
								addvtx(fx+0000,fy+fty0, fbu0, fbv0 );
								addvtx(fx+ftx0,fy+fty1, fbu1, fbv1 );
								addvtx(fx+ftx1,fy+fty1, fbu2, fbv2 );
							}
						}
						break;
					}
					default:
						break;
				}
			}
			mVW.UnLock(targ);
			targ->GBI()->DrawPrimitive( mVW, ork::lev2::EPrimitiveType::TRIANGLES );
		}
		mtxi->PopPMatrix();
		mtxi->PopVMatrix();
		mtxi->PopMMatrix();

		buffer.PtexEnd(true);
	}
	MarkClean();
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
}}
