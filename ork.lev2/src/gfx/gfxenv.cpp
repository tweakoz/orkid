////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/gfxprimitives.h>
#include <ork/lev2/gfx/gfxctxdummy.h>
#include <ork/lev2/gfx/gfxmaterial_ui.h>
#include <ork/lev2/gfx/gfxmaterial_test.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/lev2/gfx/shadman.h>
#include <ork/lev2/ui/ui.h>
#include <ork/lev2/gfx/renderable.h>
#include <ork/kernel/prop.h>
#include <ork/kernel/prop.hpp>
#include <ork/kernel/opq.h>
#include <ork/reflect/enum_serializer.h>
#include <ork/lev2/gfx/pickbuffer.h>

#include <ork/reflect/RegisterProperty.h>
//#include <orktool/qtui/gfxbuffer.h>

INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::IManipInterface,"IManipInterface");

///////////////////////////////////////////////////////////////////////////////

BEGIN_ENUM_SERIALIZER(ork::lev2, EBlending)
	DECLARE_ENUM(EBLENDING_OFF)
	DECLARE_ENUM(EBLENDING_PREMA)
	DECLARE_ENUM(EBLENDING_ALPHA)
	DECLARE_ENUM(EBLENDING_DSTALPHA)
	DECLARE_ENUM(EBLENDING_ADDITIVE)
	DECLARE_ENUM(EBLENDING_ALPHA_ADDITIVE)
	DECLARE_ENUM(EBLENDING_SUBTRACTIVE)
	DECLARE_ENUM(EBLENDING_ALPHA_SUBTRACTIVE)
	DECLARE_ENUM(EBLENDING_MODULATE)
END_ENUM_SERIALIZER()

BEGIN_ENUM_SERIALIZER(ork::lev2, EPrimitiveType)
	DECLARE_ENUM(EPRIM_NONE)
	DECLARE_ENUM(EPRIM_POINTS)
	DECLARE_ENUM(EPRIM_LINES)
	DECLARE_ENUM(EPRIM_LINESTRIP)
	DECLARE_ENUM(EPRIM_LINELOOP)
	DECLARE_ENUM(EPRIM_TRIANGLES)
	DECLARE_ENUM(EPRIM_QUADS)
	DECLARE_ENUM(EPRIM_TRIANGLESTRIP)
	DECLARE_ENUM(EPRIM_TRIANGLEFAN)
	DECLARE_ENUM(EPRIM_QUADSTRIP)
	DECLARE_ENUM(EPRIM_MULTI)
	DECLARE_ENUM(EPRIM_POINTSPRITES)
	DECLARE_ENUM(EPRIM_END)
END_ENUM_SERIALIZER()

///////////////////////////////////////////////////////////////////////////////

INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::DrawHudEvent, "DrawHudEvent");

void ork::lev2::DrawHudEvent::Describe()
{
}

///////////////////////////////////////////////////////////////////////////////

using namespace ork::lev2; //too many things to add ork::lev2:: in front of in this file...

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

static const std::string VertexFormatStrings[EVTXSTREAMFMT_END+2] = 
{
	"EVTXSTREAMFMT_V16",		
	"EVTXSTREAMFMT_V4T4",		
	"EVTXSTREAMFMT_V4C4",		
	"EVTXSTREAMFMT_V4T4C4",		
	"EVTXSTREAMFMT_V12C4T16",		
	
	"EVTXSTREAMFMT_V12N6I1T4" ,
	"EVTXSTREAMFMT_V12N6C2T4" ,
	
	"EVTXSTREAMFMT_V16T16C16",	
	"EVTXSTREAMFMT_V12I4N12T8",	
	"EVTXSTREAMFMT_V12C4N6I2T8",	
	"EVTXSTREAMFMT_V6I2C4N3T2",   
	"EVTXSTREAMFMT_V12I4N6W4T4",

	"EVTXSTREAMFMT_V12N12T8I4W4",	
	"EVTXSTREAMFMT_V12N12B12T8",	
	"EVTXSTREAMFMT_V12N12T16C4", 
	"EVTXSTREAMFMT_V12N12B12T8C4",
	"EVTXSTREAMFMT_V12N12B12T16",
	"EVTXSTREAMFMT_V12N12B12T8I4W4",

	"EVTXSTREAMFMT_MODELERRIGID",

	"EVTXSTREAMFMT_XP_VCNT",	
	"EVTXSTREAMFMT_XP_VCNTI",
	"EVTXSTREAMFMT_END",
	""
};
static const std::string PrimTypeStrings[EPRIM_END+2] = 
{
	"EPRIM_NONE",
	"EPRIM_POINTS",
	"EPRIM_LINES",
	"EPRIM_LINESTRIP",
	"EPRIM_LINELOOP",
	"EPRIM_TRIANGLES",
	"EPRIM_QUADS",
	"EPRIM_TRIANGLESTRIP",
	"EPRIM_TRIANGLEFAN",
	"EPRIM_QUADSTRIP",
	"EPRIM_MULTI",
	"EPRIM_POINTSPRITES",
	"EPRIM_END",
	""
};
static const std::string BlendingStrings[EBLENDING_END+2] = 
{
	"EBLENDING_OFF",
	"EBLENDING_ALPHA",				
	"EBLENDING_DSTALPHA",			
	"EBLENDING_ADDITIVE",			
	"EBLENDING_ALPHA_ADDITIVE",		
	"EBLENDING_SUBTRACTIVE",			
	"EBLENDING_ALPHA_SUBTRACTIVE",	
	"EBLENDING_MODULATE"
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
namespace ork {
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
template<> const EPropType CPropType<EVtxStreamFormat>::meType = EPROPTYPE_ENUM;
template<> const EPropType CPropType<EPrimitiveType>::meType = EPROPTYPE_ENUM;
template<> const EPropType CPropType<EBlending>::meType = EPROPTYPE_ENUM;
///////////////////////////////////////////////////////////////////////////////
template<> const char * CPropType<EVtxStreamFormat>::mstrTypeName = "GfxEnv::EVtxStreamFormat";
template<> const char * CPropType<EPrimitiveType>::mstrTypeName = "GfxEnv::EPrimitiveType";
template<> const char * CPropType<EBlending>::mstrTypeName = "GfxEnv::EBlending";
///////////////////////////////////////////////////////////////////////////////
template<> EVtxStreamFormat CPropType<EVtxStreamFormat>::FromString(const PropTypeString& String)
{
	return CPropType::FindValFromStrings<EVtxStreamFormat>( String.c_str(), VertexFormatStrings, EVTXSTREAMFMT_END );
}
template<> EPrimitiveType CPropType<EPrimitiveType>::FromString(const PropTypeString& String)
{
	return CPropType::FindValFromStrings<EPrimitiveType>( String.c_str(), PrimTypeStrings, EPRIM_END );
}
template<> EBlending CPropType<EBlending>::FromString(const PropTypeString& String)
{
	return CPropType::FindValFromStrings<EBlending>( String.c_str(), BlendingStrings, EBLENDING_END );
}
///////////////////////////////////////////////////////////////////////////////
template<> void CPropType<EVtxStreamFormat>::ToString( const EVtxStreamFormat & e, PropTypeString& tstr )
{	
	tstr.set( VertexFormatStrings[ int(e) ].c_str() );
}
template<> void CPropType<EPrimitiveType>::ToString( const EPrimitiveType & e, PropTypeString& tstr )
{	
	tstr.set( PrimTypeStrings[ int(e) ].c_str() );
}
template<> void CPropType<EBlending>::ToString( const EBlending & e, PropTypeString& tstr )
{	
	tstr.set( BlendingStrings[ int(e) ].c_str() );
}
///////////////////////////////////////////////////////////////////////////////
template<> void CPropType<EVtxStreamFormat>::GetValueSet( const std::string * & ValueStrings, int & NumStrings )
{	
	NumStrings = EVTXSTREAMFMT_END+1;
	ValueStrings = VertexFormatStrings;
}
template<> void CPropType<EPrimitiveType>::GetValueSet( const std::string * & ValueStrings, int & NumStrings )
{	
	NumStrings = EPRIM_END+1;
	ValueStrings = PrimTypeStrings;
}
template<> void CPropType<EBlending>::GetValueSet( const std::string * & ValueStrings, int & NumStrings )
{	
	NumStrings = EBLENDING_END+1;
	ValueStrings = BlendingStrings;
}
};
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////


ECullTest GlobalCullTest = ECULLTEST_PASS_FRONT;


////////////////////////////////////////////////////////////////////////////////

void IManipInterface::Describe()
{
}


/////////////////////////////////////////////////////////////////////////
SRasterState::SRasterState()
{
	mPointSize = 1;
	SetScissorTest( ESCISSORTEST_OFF );
	SetAlphaTest( EALPHATEST_OFF, 0 );
	SetBlending( EBLENDING_OFF );
	SetDepthTest( EDEPTHTEST_LEQUALS );
	SetShadeModel( ESHADEMODEL_SMOOTH );
	SetCullTest( ECULLTEST_PASS_FRONT );
	SetZWriteMask( true );
	SetRGBAWriteMask( true, true );
	SetStencilMode( ESTENCILTEST_OFF, ESTENCILOP_KEEP, ESTENCILOP_KEEP, 0, 0 );
	SetSortID( 0 );
	SetTransparent( false );
}

/////////////////////////////////////////////////////////////////////////

const ork::rtti::Class* GfxEnv::gpTargetClass = 0;

void GfxEnv::SetRuntimeEnvironmentVariable( const std::string& key, const std::string& val )
{
	mRuntimeEnvironment[key] = val;
}
const std::string& GfxEnv::GetRuntimeEnvironmentVariable( const std::string& key ) const
{
	static const std::string EmptyString("");
	orkmap<std::string,std::string>::const_iterator it=mRuntimeEnvironment.find(key);
	return (it==mRuntimeEnvironment.end()) ? EmptyString : it->second;
}

DynamicVertexBuffer<SVtxV12C4T16>& GfxEnv::GetSharedDynamicVB()
{
	return GetRef().mVtxBufSharedVect;
}

DynamicVertexBuffer<SVtxV12N12B12T8C4>& GfxEnv::GetSharedDynamicVB2()
{
	return GetRef().mVtxBufSharedVect2;
}

GfxEnv::GfxEnv()
	: NoRttiSingleton< GfxEnv >()
	, mpMainWindow(nullptr)
	, mpUIMaterial( nullptr )
	, mp3DMaterial( nullptr )
	, mGfxEnvMutex( "GfxEnvGlobalMutex" )
	, gLoaderTarget( nullptr )
	, mVtxBufSharedVect( 2<<20, 0, EPRIM_TRIANGLES ) // SVtxV12C4T16==32bytes
	, mVtxBufSharedVect2( 2<<20, 0, EPRIM_TRIANGLES ) // SvtxV12N12B12T8C4==48bytes
{
	mVtxBufSharedVect.SetRingLock(true);
	mVtxBufSharedVect2.SetRingLock(true);
	GfxTargetCreationParams params;
	params.miNumSharedVerts = 64<<10;

	PushCreationParams( params );
	Texture::RegisterLoaders();

}													   

/////////////////////////////////////////////////////////////////////////

void GfxEnv::RegisterWinContext( GfxWindow *pWin )
{	
    orkprintf( "GfxEnv::RegisterWinContext\n" );

	//gfxenvlateinit();
}

void GfxEnv::SetLoaderTarget(GfxTarget* target)
{
	gLoaderTarget = target;

	auto gfxenvlateinit = [=]()
	{
		if( nullptr != mpUIMaterial )
		{
			delete GetRef().mpUIMaterial;
			delete GetRef().mp3DMaterial;
		}

		mpUIMaterial = new GfxMaterialUI();
		mp3DMaterial = new GfxMaterial3DSolid();

		mpUIMaterial->Init( gLoaderTarget );
		mp3DMaterial->Init( gLoaderTarget );
		ork::lev2::CGfxPrimitives::Init( gLoaderTarget );

		gLoaderTarget->BeginFrame();
		gLoaderTarget->EndFrame();

	};
	MainThreadOpQ().push(gfxenvlateinit);

}
/////////////////////////////////////////////////////////////////////////

GetPixelContext::GetPixelContext()
	: mAsBuffer(nullptr)
	, mRtGroup(nullptr)
	, miMrtMask(0)
	, mUserData(nullptr)
{
	for( int i=0; i<kmaxitems; i++ )
	{
		mPickColors[i] = CColor4( 0.0f, 0.0f, 0.0f, 0.0f );
		mUsage[i] = EPU_FLOAT;
	}
}

/////////////////////////////////////////////////////////////////////////

ork::rtti::ICastable *GetPixelContext::GetObject( PickBufferBase*pb, int ichan ) const
{
	if(nullptr==pb) return nullptr;
	
	uint32_t pid = (uint32_t) (uint64_t) GetPointer(ichan);
	void *uobj = pb->GetObjectFromPickId(pid);
	if( 0 != uobj )
	{
		ork::rtti::ICastable *pObj = reinterpret_cast<ork::rtti::ICastable *>(uobj);
		return pObj;
	}
	return 0;
}

void* GetPixelContext::GetPointer( int ichan ) const
{
	const CVector4& TestColor = mPickColors[ichan];
	//U32 TestObj = TestColor.GetRGBAU32();
	S32 r = U32(TestColor.GetX()*256.0f);
	S32 g = U32(TestColor.GetY()*256.0f);
	S32 b = U32(TestColor.GetZ()*256.0f);
	S32 a = U32(TestColor.GetW()*256.0f);

	if( r<0 ) r=0;
	if( g<0 ) g=0;
	if( b<0 ) b=0;
	if( a<0 ) a=0;

	U32 uobj = ( (r<<24)|(g<<16)|(b<<8)|a );
	
	//CVector4 PickRGBA; PickRGBA.SetRGBAU32( TestObj );
	//U32 uobj = PickRGBA.GetRGBAU32();
	if( 0 != uobj )
	{
		void *pObj = reinterpret_cast<void *>(uobj);
		return pObj;
	}
	return 0;
}

/////////////////////////////////////////////////////////////////////////

CaptureBuffer::CaptureBuffer()
	: mpData( 0 )
	, meFormat( EBUFFMT_END )
{
}
CaptureBuffer::~CaptureBuffer()
{
	if( mpData )
	{
		delete[] (char*)(mpData);
	}
}
int CaptureBuffer::GetStride() const
{
	int istride = 0;
	switch( meFormat )
	{
		case EBUFFMT_RGBA32:
			istride = 4;
			break;
		case EBUFFMT_RGBA64:
			istride = 8;
			break;
		case EBUFFMT_RGBA128:
			istride = 16;
			break;
		case EBUFFMT_F32:
			istride = 4;
			break;
		default:
			OrkAssert(false);
			break;
	}
	return istride;
}
int CaptureBuffer::CalcDataIndex( int ix, int iy ) const
{
	return ix+(iy*miW);
}
void CaptureBuffer::SetWidth( int iw )
{
	miW = iw;
}
void CaptureBuffer::SetHeight( int ih )
{
	miH = ih;
}
int CaptureBuffer::GetWidth() const
{
	return miW;
}
int CaptureBuffer::GetHeight() const
{
	return miH;
}
void CaptureBuffer::SetFormat( EBufferFormat efmt )
{
	if( (efmt != meFormat) || (mpData==0) )
	{
		meFormat = efmt;

		int inumpix = miW*miH;

		if( inumpix )
		{
			if( mpData )
			{
				delete[] (char*)(mpData);
			}
			int idatasize = inumpix*GetStride() ;
			mpData = (void*) new char[idatasize];
		}
		else
		{
			if( mpData )
			{
				delete[] (char*)(mpData);
			}
		}
	}
}
EBufferFormat CaptureBuffer::GetFormat() const
{
	return meFormat;
}
void CaptureBuffer::CopyData( const void* pfrom, int isize )
{
	int icapsize = GetStride()*miW*miH;
	OrkAssert( isize == icapsize );
	memcpy( mpData, pfrom, isize );
}

