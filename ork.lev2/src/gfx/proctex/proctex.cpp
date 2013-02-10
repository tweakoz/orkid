///////////////////////////////////////////////////////////////////////////////
// Orkid
// Copyright 1996-2010, Michael T. Mayers
///////////////////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/lev2/gfx/proctex/proctex.h>
#include <ork/reflect/DirectObjectMapPropertyType.h>

#include <ork/lev2/gfx/gfxmaterial_test.h>
#include <ork/reflect/AccessorObjectPropertyObject.h>
#include <ork/reflect/RegisterProperty.h>
#include <ork/reflect/enum_serializer.h>
#include <ork/file/file.h>
#include <ork/stream/FileInputStream.h>
#include <ork/stream/FileOutputStream.h>
#include <ork/reflect/serialize/XMLSerializer.h>
#include <ork/reflect/serialize/XMLDeserializer.h>
#include <ork/asset/AssetManager.h>

INSTANTIATE_TRANSPARENT_RTTI(ork::proctex::ProcTex,"proctex");
INSTANTIATE_TRANSPARENT_RTTI(ork::proctex::ImgModule,"proctex::ImgModule");
INSTANTIATE_TRANSPARENT_RTTI(ork::proctex::Img32Module,"proctex::Img32Module");
INSTANTIATE_TRANSPARENT_RTTI(ork::proctex::Img64Module,"proctex::Img64Module");
INSTANTIATE_TRANSPARENT_RTTI(ork::proctex::Module,"proctex::Module");

INSTANTIATE_TRANSPARENT_TEMPLATE_RTTI(ork::dataflow::outplug<ork::proctex::ImgBase>,"proctex::OutImgPlug");
INSTANTIATE_TRANSPARENT_TEMPLATE_RTTI(ork::dataflow::inplug<ork::proctex::ImgBase>,"proctex::InImgPlug");

using namespace ork::lev2;

namespace ork { namespace dataflow {
template<> void outplug<ork::proctex::ImgBase>::Describe(){}
template<> void inplug<ork::proctex::ImgBase>::Describe(){}
template<> int MaxFanout<ork::proctex::ImgBase>() { return 0; }
template<> const ork::proctex::ImgBase& outplug<ork::proctex::ImgBase>::GetInternalData() const
{	OrkAssert(mOutputData!=0);
	return *mOutputData;
}
template<> const ork::proctex::ImgBase& outplug<ork::proctex::ImgBase>::GetValue() const
{
	return GetInternalData();
}
}}

namespace ork { namespace proctex {

Img32 ImgModule::gNoCon;
tbb::concurrent_queue<Buffer*>	ProcTexContext::gBuf32Q;
tbb::concurrent_queue<Buffer*>	ProcTexContext::gBuf64Q;

void Buffer::PtexBegin()
{
	GetContext()->FBI()->PushRtGroup(&mRtGroup);
}
void Buffer::PtexEnd()
{
	GetContext()->FBI()->PopRtGroup();
}

ork::lev2::Texture* Img32::GetTexture( ProcTex& ptex ) const
{
	return GetBuffer(ptex).OutputTexture();
}
Buffer& Img32::GetBuffer( ProcTex& ptex ) const
{
	static Buffer32 gnone;
	return (miBufferIndex>=0) ? ptex.GetBuffer32(miBufferIndex) : gnone;
}
ork::lev2::Texture* Img64::GetTexture( ProcTex& ptex ) const
{
	return GetBuffer(ptex).OutputTexture();
}
Buffer& Img64::GetBuffer( ProcTex& ptex ) const
{
	static Buffer64 gnone;
	return (miBufferIndex>=0) ? ptex.GetBuffer64(miBufferIndex) : gnone;
}

///////////////////////////////////////////////////////////////////////////////
static lev2::Texture* GetImgModuleIcon( ork::dataflow::dgmodule* pmod )
{
	ImgModule* pimgmod = rtti::autocast(pmod);
	lev2::GfxBuffer& buffer = pimgmod->GetThumbBuffer();
	return buffer.GetTexture();
}

void ImgModule::Describe()
{
	reflect::AnnotateClassForEditor<ImgModule>( "dflowicon", & GetImgModuleIcon );


}
void Img32Module::Describe()
{	RegisterObjOutPlug( Img32Module, ImgOut );
}
void Img64Module::Describe()
{	RegisterObjOutPlug( Img64Module, ImgOut );
}
ImgModule::ImgModule()
//	: mThumbBuffer( 0, 0, 0, 256, 256, lev2::EBUFFMT_RGBA32, lev2::ETGTTYPE_EXTBUFFER, 	"ProcTexThumbBuffer")
{
	lev2::GfxTargetCreationParams params;
	params.miNumSharedVerts = 4<<10;
	lev2::GfxEnv::GetRef().PushCreationParams( params );
	mThumbBuffer.CreateContext();
	lev2::GfxEnv::GetRef().PopCreationParams( );
}
Img32Module::Img32Module()
	: ConstructOutTypPlug( ImgOut,dataflow::EPR_UNIFORM, typeid(Img32) )
	, ImgModule()
{
}
Img64Module::Img64Module()
	: ConstructOutTypPlug( ImgOut,dataflow::EPR_UNIFORM, typeid(Img64) )
	, ImgModule()
{
}
Buffer& ImgModule::GetWriteBuffer( ProcTex& ptex )
{	ImgOutPlug* outplug = 0;
	GetTypedOutput<ImgBase>(0,outplug);
	const ImgBase& base = outplug->GetValue();
	//printf( "MOD<%p> WBI<%d>\n", this, base.miBufferIndex );
	return base.GetBuffer(ptex);
	//return ptex.GetBuffer(outplug->GetValue().miBufferIndex);
}
void ImgModule::Compute( dataflow::workunit* wu )
{
	lev2::GfxEnv::GetRef().GetGlobalLock().Lock();
	const_cast<ImgModule*>(this)->compute( *wu->GetContextData().Get<ProcTex*>() );
	lev2::GfxEnv::GetRef().GetGlobalLock().UnLock();
}
///////////////////////////////////////////////////////////////////////////////
void ImgModule::UnitTexQuad( ork::lev2::GfxTarget* pTARG )
{	//CMatrix4 mtxortho = pTARG->MTXI()->Ortho( -1.0f, 1.0f, -1.0f, 1.0f, 0.0f, 1.0f );
	pTARG->MTXI()->PushPMatrix( CMatrix4::Identity );
	pTARG->MTXI()->PushVMatrix( CMatrix4::Identity );
	pTARG->MTXI()->PushMMatrix( CMatrix4::Identity );
	pTARG->PushModColor( CVector3::White() );
	{	RenderQuad( pTARG, -1.0f, -1.0f, 1.0f, 1.0f );
	}
	pTARG->PopModColor();
	pTARG->MTXI()->PopPMatrix();
	pTARG->MTXI()->PopVMatrix();
	pTARG->MTXI()->PopMMatrix();
}
void ImgModule::MarkClean()
{	for( int i=0; i<GetNumOutputs(); i++ )
	{	GetOutput(i)->SetDirty(false);
	}
}
///////////////////////////////////////////////////////////////////////////////
void RenderQuad( ork::lev2::GfxTarget* pTARG, float fX1, float fY1, float fX2, float fY2, float fu1,	float fv1, float fu2, float fv2 )
{
	U32 uColor = 0xffffffff; //gGfxEnv.GetColor().GetABGRU32();

	float maxuv = 1.0f;
	float minuv = 0.0f;

	int ivcount = 6;

	lev2::VtxWriter<SVtxV12C4T16> vw;
	vw.Lock( pTARG, & GfxEnv::GetSharedDynamicVB(), ivcount );
	float fZ = 0.0f;

	vw.AddVertex( SVtxV12C4T16( fX1, fY1, fZ, fu1, fv1, uColor ) );
	vw.AddVertex( SVtxV12C4T16( fX2, fY1, fZ, fu2, fv1, uColor ) );
	vw.AddVertex( SVtxV12C4T16( fX2, fY2, fZ, fu2, fv2, uColor ) );

	vw.AddVertex( SVtxV12C4T16( fX1, fY1, fZ, fu1, fv1, uColor ) );
	vw.AddVertex( SVtxV12C4T16( fX2, fY2, fZ, fu2, fv2, uColor ) );
	vw.AddVertex( SVtxV12C4T16( fX1, fY2, fZ, fu1, fv2, uColor ) );

	vw.UnLock(pTARG);

	pTARG->GBI()->DrawPrimitive( vw, EPRIM_TRIANGLES, ivcount );

}

///////////////////////////////////////////////////////////////////////////////
void ImgModule::UpdateThumb( ProcTex& ptex )
{
	Buffer& computebuffer = GetWriteBuffer(ptex);
	lev2::Texture* ptexture = computebuffer.OutputTexture();

	mThumbBuffer.BeginFrame();
	GfxTarget* pTARG = mThumbBuffer.GetContext();
	GfxMaterial3DSolid gridmat( pTARG, "orkshader://proctex", "ttex" );
	gridmat.SetColorMode( lev2::GfxMaterial3DSolid::EMODE_USER );
	gridmat.mRasterState.SetAlphaTest( ork::lev2::EALPHATEST_OFF );
	gridmat.mRasterState.SetCullTest( ork::lev2::ECULLTEST_OFF );
	gridmat.mRasterState.SetBlending( ork::lev2::EBLENDING_OFF );
	gridmat.mRasterState.SetDepthTest( ork::lev2::EDEPTHTEST_ALWAYS );
	gridmat.SetTexture( ptexture );
	gridmat.SetUser0( CVector4(0.0f,0.0f,0.0f,float(Buffer::kw)) );
	pTARG->BindMaterial( & gridmat );
	////////////////////////////////////////////////////////////////
	float ftexw = ptexture ? ptexture->GetWidth() : 1.0f;
	pTARG->PushModColor( ork::CVector4( ftexw, ftexw, ftexw, ftexw ) );
	////////////////////////////////////////////////////////////////
	CMatrix4 mtxortho = pTARG->MTXI()->Ortho( -1.0f, 1.0f, -1.0f, 1.0f, 0.0f, 1.0f );
	//pTARG->MTXI()->PushPMatrix( mtxortho );
	pTARG->MTXI()->PushPMatrix( CMatrix4::Identity );
	pTARG->MTXI()->PushVMatrix( CMatrix4::Identity );
	pTARG->MTXI()->PushMMatrix( CMatrix4::Identity );
	{
		RenderQuad( pTARG, -1.0f, -1.0f, 1.0f, 1.0f );
	}
	pTARG->MTXI()->PopPMatrix();
	pTARG->MTXI()->PopVMatrix();
	pTARG->MTXI()->PopMMatrix();
	pTARG->PopModColor();
	MarkClean();
	mThumbBuffer.EndFrame();
}
///////////////////////////////////////////////////////////////////////////////
void ProcTex::Describe()
{	//ork::reflect::RegisterProperty( "Global", & ProcTex::GlobalAccessor );
	//ork::reflect::AnnotatePropertyForEditor< ProcTex >("Global", "editor.visible", "false" );
	//ork::reflect::AnnotatePropertyForEditor< ProcTex >("Modules", "editor.factorylistbase", "proctex::Module" );
}

///////////////////////////////////////////////////////////////////////////////
ProcTex::ProcTex()
	: mpctx(0)
	, mbTexQuality( false )
	, mpResTex(0)
{
	Global::GetClassStatic();
	RotSolid::GetClassStatic();
	Colorize::GetClassStatic();
	ImgOp2::GetClassStatic();
	ImgOp3::GetClassStatic();
	Transform::GetClassStatic();
	Octaves::GetClassStatic();
	Texture::GetClassStatic();
}
///////////////////////////////////////////////////////////////////////////////
bool ProcTex::CanConnect( const ork::dataflow::inplugbase* pin, const ork::dataflow::outplugbase* pout ) const //virtual 
{	bool brval = false;
	brval |= (&pin->GetDataTypeId()==&typeid(ImgBase))&&(&pout->GetDataTypeId()==&typeid(Img64));
	brval |= (&pin->GetDataTypeId()==&typeid(ImgBase))&&(&pout->GetDataTypeId()==&typeid(Img32));
	brval |= (&pin->GetDataTypeId()==&typeid(float))&&(&pout->GetDataTypeId()==&typeid(float));
	return brval;
}
///////////////////////////////////////////////////////////////////////////////
// compute result to mBuffer
///////////////////////////////////////////////////////////////////////////////
void ProcTex::compute( ProcTexContext& ptctx )
{	if( false == IsComplete() ) return;
	mpctx = & ptctx;
	//printf( "ProcTex<%p>::compute ProcTexContext<%p>\n", this, mpctx );
	//////////////////////////////////
	// build the execution graph
	//////////////////////////////////
	Clear();
	SetAccumulateWork(true);
	RefreshTopology(ptctx.mdflowctx);
	//////////////////////////////////
	// bind to image buffers
	//////////////////////////////////
	#if 1
	const orklut<int,dataflow::dgmodule*>& TopoSorted = LockTopoSortedChildrenForRead(1);
	{	int ilastwritebuf = -1;
		ork::lev2::Texture* restex = 0;
		for( orklut<int,dataflow::dgmodule*>::const_iterator it=TopoSorted.begin(); it!=TopoSorted.end(); it++ )
		{	ork::Object* pobj = it->second;
			ImgModule* pmodule = ork::rtti::autocast(pobj);
			Group* pgroup = rtti::autocast(pmodule);
			if( pgroup )
			{
			}
			if(pmodule)
			{	ImgOutPlug* outplug = 0;
				pmodule->GetTypedOutput<ImgBase>(0,outplug);
				const ImgBase& base = outplug->GetValue();

				int ipixsize = base.PixelSize();
				bool is_32bit = (ipixsize==32);
				int ibufmax = is_32bit ? ProcTexContext::k32buffers : ProcTexContext::k64buffers;

				//printf( "pmod<%p> pixsiz<%d> reg<%p>\n", pmodule, ipixsize, outplug->GetRegister() );

				if( outplug->GetRegister() )
				{
					int ireg = outplug->GetRegister()->mIndex;
	
					OrkAssert( ireg>=0 && ireg<ibufmax );

					ilastwritebuf = ireg%ibufmax;
					outplug->GetValue().miBufferIndex = ireg;

					restex	= is_32bit 
							?	ptctx.GetBuffer32(ilastwritebuf).OutputTexture()
							:	ptctx.GetBuffer64(ilastwritebuf).OutputTexture();
				}
				else
				{
					outplug->GetValue().miBufferIndex = -2;
				}

				/*if( mpProbeImage == pmodule )
				{
					outplug->GetValue().miBufferIndex = ibufmax-1;
				}*/
			}
		}
		mpResTex=restex;
	}
	UnLockTopoSortedChildren();
	#endif
	//////////////////////////////////
	// execute
	//////////////////////////////////
	//dataflow::cluster gcluster;
	//dataflow::workunit gwunit( & mGlobal, & gcluster, 0);
	//gwunit.SetContextData( this );
	//static_cast<dataflow::dgmodule&>(mGlobal).Compute( & gwunit );
	//////////////////////////////////
	int inumwuassigned = 0;
	//while( mGraph.IsDirty() )
	{	const orklut<int,dataflow::dgmodule*>& toposorted = LockTopoSortedChildrenForRead(2);
		{	int inum = toposorted.size();
			for( int itopo=0; itopo<inum; itopo++ )
			{	int inum2 = toposorted.size();
				OrkAssert( inum==inum2 );
				dataflow::dgmodule* pmod = toposorted.GetItemAtIndex( itopo ).second;
				bool bmoddirty = true; //pmod->IsDirty();
				if( bmoddirty )
				{	dataflow::cluster mycluster;
					dataflow::workunit mywunit( pmod, & mycluster, 0);
					mywunit.SetContextData( this );
					pmod->Compute( & mywunit );
					ImgModule* pimgmod = rtti::autocast(pmod);
					if( pimgmod )
					{
						pimgmod->UpdateThumb(*this);
						Buffer& b = pimgmod->GetWriteBuffer( *this );
						mpResTex = b.OutputTexture();
					}
				}
			}
		}
		UnLockTopoSortedChildren();
	}
	//////////////////////////////////
	//mpctx = 0;
}
///////////////////////////////////////////////////////////////////////////////
Buffer::Buffer(ork::lev2::EBufferFormat efmt)
	: lev2::GfxBuffer(
		0,
		kx, ky,
		kw, kh, 
		efmt,
		lev2::ETGTTYPE_EXTBUFFER,
		"ProcTexBuffer" )
	, mRtGroup( this, kw, kh )
{
	CreateContext();

	auto mrt = new ork::lev2::CMrtBuffer(	
		this,
		lev2::ETGTTYPE_MRT0,
		lev2::EBUFFMT_RGBA64,
		0, 0, kw, kh );

	mRtGroup.SetMrt( 0, mrt );

}
lev2::Texture* Buffer::OutputTexture()
{
	return mRtGroup.GetMrt(0)->GetTexture();
}
Buffer32::Buffer32() : Buffer( lev2::EBUFFMT_RGBA32 ) {}	// EBUFFMT_RGBA32
Buffer64::Buffer64() : Buffer( lev2::EBUFFMT_RGBA64 ) {}	// EBUFFMT_RGBA64

///////////////////////////////////////////////////////////////////////////////
void Module::Describe()
{
}
Module::Module()
{
}
///////////////////////////////////////////////////////////////////////////////
ork::lev2::Texture* ProcTex::ResultTexture()
{	return mpResTex;
}
Buffer& ProcTexContext::GetBuffer32(int edest)
{
	if( edest < 0 ) return mTrashBuffer;
	OrkAssert( edest<k32buffers );
	return *mBuffer32[edest]; 
}
Buffer& ProcTexContext::GetBuffer64(int edest)
{
	if( edest < 0 ) return mTrashBuffer;
	OrkAssert( edest<k64buffers );
	return *mBuffer64[edest]; 
}
ProcTexContext::ProcTexContext() 
	: mdflowctx()
	, mFloatRegs(4)
	, mImage32Regs(k32buffers)
	, mImage64Regs(k64buffers)
	, mTrashBuffer()
	, mCurrentTime(0.0f)
{
	mdflowctx.SetRegisters<float>( & mFloatRegs );
	mdflowctx.SetRegisters<Img32>( & mImage32Regs );
	mdflowctx.SetRegisters<Img64>( & mImage64Regs );

	for( int i=0; i<k64buffers; i++ )
		mBuffer64[i] = AllocBuffer64();

	for( int i=0; i<k32buffers; i++ )
		mBuffer32[i] = AllocBuffer64();
}
ProcTexContext::~ProcTexContext() 
{
	for( int i=0; i<k64buffers; i++ )
		ReturnBuffer( mBuffer64[i] );

	for( int i=0; i<k32buffers; i++ )
		ReturnBuffer( mBuffer32[i] );
}
///////////////////////////////////////////////////////////////////////////////
ProcTex* ProcTex::Load( const ork::file::Path& pth )
{	ProcTex* rval = 0;
	ork::file::Path path = pth;
	path.SetExtension( "ptx" );
	lev2::GfxEnv::GetRef().GetGlobalLock().Lock();
	stream::FileInputStream istream(path.c_str());
	reflect::serialize::XMLDeserializer iser(istream);
	rtti::ICastable* pcastable = 0;
	bool bOK = iser.Deserialize( pcastable );
	if( bOK )
	{	ork::asset::AssetManager<ork::lev2::TextureAsset>::AutoLoad();
		rval = rtti::safe_downcast<ProcTex*>( pcastable );
		//ObjModel().Attach( mProcTex );
		//mPreviewVP->SetProcTex( mProcTex );
		//mGraphVP->SetProcTex( mProcTex );
	}
	lev2::GfxEnv::GetRef().GetGlobalLock().UnLock();
	return rval;
}

Buffer* ProcTexContext::AllocBuffer32()
{
	Buffer* rval = nullptr;
	if( false == gBuf32Q.try_pop(rval) )
	{
		rval = new Buffer32;
	}
	return rval;
}
Buffer* ProcTexContext::AllocBuffer64()
{
	Buffer* rval = nullptr;
	if( false == gBuf64Q.try_pop(rval) )
	{
		rval = new Buffer64;
	}
	return rval;
}
void ProcTexContext::ReturnBuffer(Buffer*pbuf)
{
	assert(pbuf!=nullptr);
	if(pbuf->IsBuf32())
		gBuf32Q.push(pbuf);
	else
		gBuf64Q.push(pbuf);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

AA16Render::AA16Render( ProcTex& ptx, Buffer& bo )
	: mPTX(ptx)
	, bufout(bo)
	, downsamplemat( ork::lev2::GfxEnv::GetRef().GetLoaderTarget(), "orkshader://proctex", "downsample16" )
{
	downsamplemat.SetColorMode( lev2::GfxMaterial3DSolid::EMODE_USER );
	downsamplemat.mRasterState.SetAlphaTest( ork::lev2::EALPHATEST_OFF );
	downsamplemat.mRasterState.SetCullTest( ork::lev2::ECULLTEST_OFF );
	downsamplemat.mRasterState.SetBlending( ork::lev2::EBLENDING_ADDITIVE );
	downsamplemat.mRasterState.SetDepthTest( ork::lev2::EDEPTHTEST_ALWAYS );
	downsamplemat.SetUser0( CVector4(0.0f,0.0f,0.0f,float(Buffer::kw)) );
}

///////////////////////////////////////////////////////////////////////////////

struct quad
{
	float fx0;
	float fy0;
	float fx1;
	float fy1;
};

void AA16Render::RenderAA()
{
	CMatrix4 mtxortho;

	float boxx = mOrthoBoxXYWH.GetX();
	float boxy = mOrthoBoxXYWH.GetY();
	float boxw = mOrthoBoxXYWH.GetZ();
	float boxh = mOrthoBoxXYWH.GetW();

	float xa = boxx+(boxw*0.0f);
	float xb = boxx+(boxw*0.25f);
	float xc = boxx+(boxw*0.5f);
	float xd = boxx+(boxw*0.75f);
	float xe = boxx+(boxw*1.0f);

	float ya = boxy+(boxh*0.0f);
	float yb = boxy+(boxh*0.25f);
	float yc = boxy+(boxh*0.5f);
	float yd = boxy+(boxh*0.75f);
	float ye = boxy+(boxh*1.0f);

	quad quads[16] = 
	{
		{ xa, ya, xb, yb }, { xb, ya, xc, yb }, { xc, ya, xd, yb },  { xd, ya, xe, yb }, 
		{ xa, yb, xb, yc }, { xb, yb, xc, yc }, { xc, yb, xd, yc },  { xd, yb, xe, yc }, 
		{ xa, yc, xb, yd }, { xb, yc, xc, yd }, { xc, yc, xd, yd },  { xd, yc, xe, yd }, 
		{ xa, yd, xb, ye }, { xb, yd, xc, ye }, { xc, yd, xd, ye },  { xd, yd, xe, ye }, 
	};

	float ua = 0.0f;
	float ub = 0.25f;
	float uc = 0.5f;
	float ud = 0.75f;
	float ue = 1.0f;

	float va = 0.0f;
	float vb = 0.25f;
	float vc = 0.5f;
	float vd = 0.75f;
	float ve = 1.0f;

	quad quadsUV[16] = 
	{
		{ ua, va, ub, vb }, { ub, va, uc, vb }, { uc, va, ud, vb },  { ud, va, ue, vb }, 
		{ ua, vb, ub, vc }, { ub, vb, uc, vc }, { uc, vb, ud, vc },  { ud, vb, ue, vc }, 
		{ ua, vc, ub, vd }, { ub, vc, uc, vd }, { uc, vc, ud, vd },  { ud, vc, ue, vd }, 
		{ ua, vd, ub, ve }, { ub, vd, uc, ve }, { uc, vd, ud, ve },  { ud, vd, ue, ve }, 
	};
	bufout.GetContext()->FBI()->SetAutoClear(true);
	
	auto temp_buffer = bufout.IsBuf32() ? ProcTexContext::AllocBuffer32() 
										: ProcTexContext::AllocBuffer64();

	for( int i=0; i<16; i++ )
	{
		const quad& q = quads[i];
		const quad& uq = quadsUV[i];
		float left = q.fx0;
		float right = q.fx1;
		float top = q.fy0;
		float bottom = q.fy1;

		//////////////////////////////////////////////////////
		// Render subsection to BufTA
		//////////////////////////////////////////////////////
		
		{
			auto targ = temp_buffer->GetContext();
			auto mtxi = targ->MTXI();
			
			targ->FBI()->SetAutoClear(true);
			temp_buffer->PtexBegin();
			CMatrix4 mtxortho = mtxi->Ortho( left,right,top,bottom, 0.0f, 1.0f );
			mtxi->PushMMatrix( CMatrix4::Identity );
			mtxi->PushVMatrix( CMatrix4::Identity );
			mtxi->PushPMatrix( mtxortho );
			DoRender( left,right,top,bottom, *temp_buffer );
			mtxi->PopPMatrix();
			mtxi->PopVMatrix();
			mtxi->PopMMatrix();
			temp_buffer->PtexEnd();

		}

		//////////////////////////////////////////////////////
		// Resolve to output buffer
		//////////////////////////////////////////////////////

		bufout.PtexBegin();
		{
			float l = boxx;
			float r = boxx+boxw;
			float t = boxy;
			float b = boxy+boxh;


			auto targ = bufout.GetContext();
			auto mtxi = targ->MTXI();
			auto txi = targ->TXI();
			auto tex = temp_buffer->OutputTexture();
			downsamplemat.SetTexture(tex);
			tex->TexSamplingMode().PresetPointAndClamp();
			txi->ApplySamplingMode(tex);

			CMatrix4 mtxortho = mtxi->Ortho( l,r,t,b, 0.0f, 1.0f );
			//mtxortho = mtxi->Ortho( 0, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f );
			mtxi->PushMMatrix( CMatrix4::Identity );
			mtxi->PushVMatrix( CMatrix4::Identity );
			mtxi->PushPMatrix( mtxortho );
			targ->PushMaterial( & downsamplemat );
				RenderQuad( targ, q.fx0,q.fy1,q.fx1,q.fy0, 0.0f, 0.0f, 1.0f, 1.0f );
			targ->PopMaterial();
			mtxi->PopPMatrix();
			mtxi->PopVMatrix();
			mtxi->PopMMatrix();
		}
		bufout.PtexEnd();

		//////////////////////////////////////////////////////
		// disable clear for addittional passes
		//////////////////////////////////////////////////////

		bufout.GetContext()->FBI()->SetAutoClear(false);
	}
	ProcTexContext::ReturnBuffer(temp_buffer);
	bufout.GetContext()->FBI()->SetAutoClear(true);
}

///////////////////////////////////////////////////////////////////////////////

void AA16Render::RenderNoAA()
{
	float x = mOrthoBoxXYWH.GetX();
	float y = mOrthoBoxXYWH.GetY();
	float w = mOrthoBoxXYWH.GetZ();
	float h = mOrthoBoxXYWH.GetW();
	float l = x;
	float r = x+w;
	float t = y;
	float b = y+h;

	auto targ = bufout.GetContext();
	auto mtxi = targ->MTXI();
	
	targ->FBI()->SetAutoClear(true);
	bufout.PtexBegin();
	{	CMatrix4 mtxortho = mtxi->Ortho( l,r,t,b, 0.0f, 1.0f );
		mtxi->PushMMatrix( CMatrix4::Identity );
		mtxi->PushVMatrix( CMatrix4::Identity );
		mtxi->PushPMatrix( mtxortho );
		DoRender( l,r,t,b, bufout );
		mtxi->PopPMatrix();
		mtxi->PopVMatrix();
		mtxi->PopMMatrix();
	}
	bufout.PtexEnd();
}

///////////////////////////////////////////////////////////////////////////////

}}
