////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#ifndef _ORK_LEV2_TESTVP_H
#define _ORK_LEV2_TESTVP_H
///////////////////////////////////////////////////////////////////////////
#include <ork/lev2/gfx/gfxprimitives.h>
#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/lev2/gfx/particle/particle.h>
#include <ork/lev2/gfx/gfxmaterial_test.h>
#include <ork/lev2/ui/ui.h>
#include <ork/lev2/gfx/renderer.h>
#include <ork/lev2/gfx/lighting/gfx_lighting.h>
#include <ork/gfx/camera.h>
#include <ork/lev2/gfx/renderable.h>
#include <ork/lev2/gfx/dbgfontman.h>
#include <ork/lev2/gfx/texman.h>

///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
namespace ork { namespace lev2 {
///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////

struct TestLightManager : public LightManager
{
	lev2::LightManagerData mLmd;

	TestLightManager() : LightManager(mLmd) {} 
	/*virtual*/
	/*void GetStationaryLights( const Frustum& frustum, lev2::LightContainer& container )
	{
		for( orklut<CReal,lev2::Light*>::const_iterator	it =  mGlobalStationaryLights.mPrioritizedLights.begin();
														it != mGlobalStationaryLights.mPrioritizedLights.end();
														it++ )
		{
			CReal prio			= it->first;
			lev2::Light* plight	= it->second;

			if( plight->IsInFrustum( frustum ) )
			{
				container.AddLight( plight );
			}

		}
	}*/
	/*virtual*/
	void GetMovingLights( const Frustum& frustum, lev2::LightContainer& container )
	{
		for( LightContainer::map_type::const_iterator	it =  mGlobalMovingLights.mPrioritizedLights.begin();
														it != mGlobalMovingLights.mPrioritizedLights.end();
														it++ )
		{
			CReal prio			= it->first;
			lev2::Light* plight	= it->second;

			if( plight->IsInFrustum( frustum ) )
			{
				container.AddLight( plight );
			}

		}
	}
};

///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////

class CTestVP2Renderer : public Renderer
{
public:

	virtual U32 GetSortKey( const IRenderable* pRenderable  ) const { return 0; }
	/*virtual*/ void RenderBillboard( const CBillboardRenderable & CameraRen ) const {}
//	/*virtual*/ void RenderFrustum( const FrustumRenderable & FrusRen ) const {}
	/*virtual*/ U32 ComposeSortKey( U32 texIndex, U32 depthIndex, U32 passIndex, U32 transIndex ) const	{ return 0;	}

	/*virtual*/ void RenderBox( const CBoxRenderable & BoxRen ) const
	{
		/*bool bpickbuffer = ( GetTarget()->FBI()->IsOffscreenTarget() );
		GetTarget()->BindMaterial( lev2::GfxEnv::GetRef().GetDefault3DMaterial() );
		GetTarget()->PushModColor( CColor4::White() );*/
		/*GetTarget()->MTXI()->PushMMatrix( BoxRen.GetTransformNode()->GetTransform()->GetMatrix() );
		lev2::CGfxPrimitives::GetRef().RenderAxis( GetTarget() );
		GetTarget()->MTXI()->PopMMatrix();
		GetTarget()->PopModColor();*/
	}

	/*virtual*/ void RenderModelGroup( const ork::lev2::CModelRenderable** Renderables, int inumr ) const {}
	/*virtual*/ void RenderModel( const CModelRenderable & ModelRen, RenderGroupState rgs=ERGST_NONE ) const
	{
		/*const lev2::XgmModelInst* minst = ModelRen.GetModelInst();
		const CMatrix4& wmat = ModelRen.GetTransformNode()->GetTransform()->GetMatrix();
		ork::lev2::RenderContextInstData MatCtx;
		ork::lev2::RenderContextInstModelData MdlCtx;

		MdlCtx.mMesh = minst->GetXgmModel()->GetMesh(0); // SetSubMeshIndex(0);
		MdlCtx.SetSkinned(false);

		MatCtx.SetMaterialIndex(0);
		MatCtx.SetRenderer( this );

		CColor4 ObjColor;
		ObjColor.SetRGBAU32( reinterpret_cast<U32>( ModelRen.GetObject() ) ); // FIXME: needs to be a CPickIDNode

		bool bpick = GetTarget()->FBI()->IsPickState();

		if( bpick )
		{
			orkprintf( "yo\n" );
		}

		CColor4 color = GetTarget()->FBI()->IsPickState() ?  ObjColor : CColor4::White();
		minst->GetXgmModel()->RenderRigid( color, wmat, GetTarget(), MatCtx, MdlCtx );*/

	}

//	/*virtual*/ void RenderHeightField( const HeightFieldRenderable & HFRen ) const	{}
	/*virtual*/ void RenderSphere( const SphereRenderable & HFRen ) const {}
	/*virtual*/ void RenderFrustum( const FrustumRenderable & HFRen ) const {}
	/*virtual*/ void RenderCallback( const CallbackRenderable & cbren ) const {}
//	/*virtual*/ void RenderGlyphs( const ork::lev2::CGlyphsRenderable & cglren ) const {}


	CTestVP2Renderer( ::ork::lev2::GfxTarget *ptarg=0 )
		: lev2::Renderer( ptarg )
	{
	}
};

///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////

class CTestVP2 : public ork::lev2::CUIViewport
{
	ork::lev2::DynamicVertexBuffer<ork::lev2::SVtxV12C4T16>	mParticleVerts;
	ork::lev2::DynamicVertexBuffer<ork::lev2::SVtxV12C4T16>	mSolidVerts;
	ork::lev2::DisplayList									mPerlinDisplayList;
	ork::lev2::DisplayList									mModelTestDisplayList;
	TestLightManager										mLightManager;

	particle::SpiralEmitterData							mPtcEmitterData;
	particle::SpiralEmitter<particle::BasicParticle>	mPtcEmitter;

	//particle::TestControllerData					mPtcControllerData;
	//particle::TestController						mPtcController;
	CTestVP2Renderer								mRenderer;
	float											mPhase;
	
	Texture*										mTerAmbOccMap;

	Texture*										mTopEnvMap;
	Texture*										mBotEnvMap;
	Texture*										mSpotLightMap;
	bool mbinit;

	static const int								kmaxptcl = 1000;


	static const int kcarolights = 8;

	lev2::SpotLightData								mSkyLightData;
	lev2::SpotLightData								mSpotLightData;
	lev2::PointLightData							mPointLightData;

	//lev2::SpotLight								mSkyLight;
	//lev2::SpotLight								mSpotLights[kcarolights];
	//lev2::PointLight							mPointLights[kcarolights];

	//CMatrix4					
public:

	//particle::System<particle::BasicParticle>		mParticleSystem;

	///////////////////////////////////////////////////////////////////////////////
	///////////////////////////////////////////////////////////////////////////////
	///////////////////////////////////////////////////////////////////////////////

	CTestVP2( int x, int y, int w, int h )
		: ork::lev2::CUIViewport( "testvp", x, y, w, h, ork::CColor3::Red(), 1.0f )
		, mParticleVerts( kmaxptcl, kmaxptcl, ork::lev2::EPRIM_LINESTRIP )
		, mSolidVerts( 256, 256, ork::lev2::EPRIM_TRIANGLES )
		, mbinit( true )
		//, mParticleSystem( &mPtcEmitter, &mPtcController )
		, mPhase(0.0f)
		, mBotEnvMap( 0 )
		, mTopEnvMap( 0 )
		, mSpotLightMap( 0 )
		, mPtcEmitter( mPtcEmitterData )
		//mPtcController( mPtcControllerData )
	{
		//mParticleSystem.SetMaxParticles(kmaxptcl);
//		mParticleSystem.AddForceField( new CTestField() );
		//mPtcEmitter.mfEmissionRate = 1.0f;
		//mPtcEmitter.mfLifespan = 100.0f;
		//mPtcEmitter.mfEmitScale = 1.0f;

		//mLightManager.mGlobalMovingLights.AddLight( & mSkyLight );

		for( int i=0; i<kcarolights; i++ )
		{
			//mLightManager.mGlobalMovingLights.AddLight( & mSpotLights[i] );
			//mLightManager.mGlobalMovingLights.AddLight( & mPointLights[i] );
		}

	}

	///////////////////////////////////////////////////////////////////////////////
	///////////////////////////////////////////////////////////////////////////////
	///////////////////////////////////////////////////////////////////////////////

	void PlaceLights() // move our lights
	{
		float fstep = 1.0f/float(kcarolights);
		float pi2 = CFloat::Pi()*2.0f;

		for( int i=0; i<kcarolights; i++ )
		{
			float fi = float(i)*fstep;

			float carooffs = mPhase*0.3f;

			////////////////////////////////////////

			float fspoty = 30.0f;
			float carorad = 80.0f;

			float fiphas = (fi*pi2);

			float fspx0 = ork::sinf(fiphas+carooffs)*carorad;
			float fspz0 = ork::cosf(fiphas+carooffs)*carorad;
			
			float fspx1 = ork::sinf(((fi+fstep)*pi2)+carooffs)*carorad;
			float fspz1 = ork::cosf(((fi+fstep)*pi2)+carooffs)*carorad;

			float fang = 20.0f; //+10.0f*std::sinf( mPhase*1.7f );

			CVector3 spotpos(fspx0,fspoty+15.0f,fspz0);
			CVector3 spottgt(fspx1,fspoty,fspz1);
			CVector3 spotdir =(spottgt-spotpos).Normal();
			float    fdist = (spottgt-spotpos).Mag();
			         spottgt = spotpos+(spotdir*fdist*0.7f);
			
			CVector3 spotcd = CVector3( ork::sinf(fiphas*3.0f),
										-1.0f,
										ork::cosf(fiphas*3.0f) ).Normal();		 
			CVector3 spotclr = (spotcd*0.5f)+CVector3(0.5f,0.5f,0.5f);
			
			CVector3 spotup = (CVector3(0.0f,1.0f,0.0f).Cross(spotdir)).Normal().Cross(spotdir);
			//CVector3 spotup = CVector3(0.0f,1.0f,0.0f);

			//mSpotLights[i].Set(	spotpos, spottgt, spotup, fang );
			//mSpotLights[i].mColor = spotclr;

			//mSpotLights[i].SetTexture( mSpotLightMap );

			////////////////////////////////////////

			float pointy = 30.0f+30.0f*ork::sinf(mPhase*0.77f);

			float fspx = ork::sinf((fi*pi2))*70.0f;
			float fspz = ork::cosf((fi*pi2))*70.0f;
			float frad = 20.0f; //+10.0f*ork::sinf( mPhase*0.3f );

			CVector3 pntpos(fspx,pointy,fspz);
			CVector3 pntdir = pntpos.Normal();

			CVector3 pntcd = CVector3( 0.0f, //sinf(carooffs),
										1.0f,
										0.0f ); //cosf(carooffs) ).Normal();		 

			CVector3 pntclr = (pntcd); //+CVector3(0.5f,0.5f,0.5f);

			//mPointLights[i].SetPosition( CVector3(fspx,pointy,fspz) );
			//mPointLights[i].SetRadius( frad );
			//mPointLights[i].mColor = pntclr;

			////////////////////////////////////////

		}

		//mSkyLight.Set(	CVector3( 0.0f, -200.0f, 0.0f ), 
		//				CVector3( 0.0f, 200.0f, 0.0f ),
		//				CVector3( 1.0f, 0.0f, 0.0f ),
		//				45.0f );
		//mSkyLight.mColor = CVector3( 1.0f, 1.0f, 1.0f );
						
	}

	///////////////////////////////////////////////////////////////////////////////
	///////////////////////////////////////////////////////////////////////////////
	///////////////////////////////////////////////////////////////////////////////

	void Animate( ork::lev2::XgmModelInst* pmodelinst )
	{
		const ork::lev2::XgmModel * XgmMdl = pmodelinst->GetXgmModel();
		const ork::lev2::XgmSkeleton  & XgmSkl = XgmMdl->RefSkel();
		ork::lev2::XgmLocalPose& LocalPose = pmodelinst->RefLocalPose();
		//XgmAnimInstList& AIL = pmodelinst->RefAnimInstList();

		/////////////////////////////////////////////
		int imax_animated_bones = 0;
		int inumanimsplaying = 0; //AIL.GetNumAnimInst();
		/////////////////////////////////////////////
		if( 0!=inumanimsplaying )
		{	///////////////////////////////////////////
			// iterate through each bound anim
			//const XgmAnimInst * FirstAnimInst = AIL.GetFirstAnimInst();
			//const XgmAnimInst * paniminst = FirstAnimInst;
			//while( paniminst )
			//{	LocalPose.ApplyAnimInst( * paniminst );
			//	paniminst = AIL.GetNextAnimInst();
			//}
		}
	}

	/*virtual*/
	void DoDraw( /*ork::lev2::GfxTarget* pTARG*/ )
	{	
		ork::lev2::GfxTarget* pTARG = 0;
		#if 0
		mPhase += 0.03f;

		mRenderer.SetTarget( pTARG );

		static ork::lev2::XgmModel* model = 0;
		static ork::lev2::XgmAnim* anim = 0;
		static ork::lev2::XgmAnimInst AnimInst;
		static ork::lev2::XgmModelInst* modelinst = 0;

		if( mbinit )
		{
			mbinit = false;

			ork::lev2::CGfxPrimitives::GetRef().Init( pTARG );

			pTARG->GBI()->DisplayListBegin( mPerlinDisplayList );
			pTARG->GBI()->DisplayListAddPrimitiveEML( mPerlinDisplayList, ork::lev2::CGfxPrimitives::GetRef().GetPerlinVB() );
			pTARG->GBI()->DisplayListEnd( mPerlinDisplayList );

			model = ork::TAssetManager<XgmModel>::GetRef().Load( ork::AssetPath( "data://models/test/test.xgm" ), false );
			model->InitDisplayLists( pTARG );
			
			OrkAssertNotImpl();
			//anim = ork::lev2::XgmAnim::LoadUnManaged( anim, ork::AssetPath( "data://models/test/t_run.xga" ) );
			AnimInst.BindAnim( anim );

			OrkAssert( model );

			modelinst = new ork::lev2::XgmModelInst( model );

			//modelinst->RefAnimInstList().BindAnimInst( & AnimInst );

			static int inumframes = anim->GetNumFrames();
			static int inumchannels = anim->GetNumJointChannels();

			orkprintf( "Anim NumFrames<%d> NumChannels<%d>\n", inumframes, inumchannels );

			mBotEnvMap = Texture::LoadUnManaged( "data://yo_dualparamap_bot.tga" );
			mTopEnvMap = Texture::LoadUnManaged( "data://yo_dualparamap_top.tga" );

			mSpotLightMap = Texture::LoadUnManaged( "data://spotlight.tga" );


			mTerAmbOccMap = 0; //Texture::LoadUnManaged( "lev2://textures/perlin_vb_ambocc.tga" );
		}

		///////////////////////////////////////////////////
		PlaceLights();
		///////////////////////////////////////////////////

		XgmLocalPose& LocalPose = modelinst->RefLocalPose();

		LocalPose.BindPose();
		Animate( modelinst );
		LocalPose.BuildPose();

		float fr = AnimInst.GetCurrentFrame()+1.0f;
		if( fr >= AnimInst.GetNumFrames() )
		{
			fr = 0.0f; 
		}
		AnimInst.SetCurrentFrame( fr );

		if( mWidgetFlags.IsSizeDirty() )
		{
			resize();
		}

		int itx0 = GetX();
		int itx1 = GetX()+GetW();
		int ity0 = GetY();
		int ity1 = GetY()+GetH();

		pTARG->PushScissor( SRect( GetX(), GetY(), GetW(), GetH() ) );
		pTARG->PushViewport( SRect( GetX(), GetY(), GetW(), GetH() ) );
		{
			////////////////////////////////////////////////////////
			////////////////////////////////////////////////////////
			////////////////////////////////////////////////////////
			// draw some 2d crap

			float aspect = float(GetW())/float(GetH());

			ork::lev2::GfxEnv::SetUIColorMode( ork::lev2::EUICOLOR_MODVTX );
			ork::lev2::NUI::EBoxStyle eBoxStyle( ork::lev2::NUI::EBOXSTYLE_FILLED_GREY );
			ork::lev2::NUI::DrawStyledBox( pTARG, itx0+64, ity0+64, itx1-64, ity1-64, eBoxStyle );
			pTARG->IMI()->QueFlush();
			pTARG->IMI()->DrawLine( s16(itx0), s16(ity0), s16(itx1), s16(ity1), 0xffffffff );
			pTARG->IMI()->DrawLine( s16(itx1), s16(ity0), s16(itx0), s16(ity1), 0xffffffff );
			pTARG->IMI()->QueFlush();

			////////////////////////////////////////////////////////
			////////////////////////////////////////////////////////
			////////////////////////////////////////////////////////
			////////////////////////////////////////////////////////
			// draw some basic 3d crap

			static ork::lev2::GfxMaterial3DSolid matsolid(pTARG);

			matsolid.mRasterState.SetBlending( ork::lev2::EBLENDING_OFF );

			pTARG->BindMaterial( & matsolid );

			ork::CMatrix4 P = pTARG->MTXI()->Persp( 90.0f, aspect, 1.0f, 1000.0f );
			
			ork::CMatrix4 V = ork::CMatrix4::Identity;

			CVector3 v_eye( 0.0f, 70, -90.0f );
			CVector3 v_tgt( 0.0f, 0.0f, 0.0f );
			CVector3 v_yn( 0.0f, 1.0f, 0.0f );
			CVector3 v_dir = (v_tgt-v_eye).Normal();
			CVector3 v_cross = v_dir.Cross(v_yn).Normal();
			CVector3 v_up = v_cross.Cross(v_dir).Normal();

			V = pTARG->MTXI()->LookAt(	v_eye,	v_tgt, v_up );

			ork::CMatrix4 M = ork::CMatrix4::Identity;

			M.RotateY( mPhase*0.4f );

			ork::CCameraData camdata;

			camdata.mNear = 1.0f;
			camdata.mFar = 1000.0f;
			camdata.mMatProj = P;
			///////////////////////////////////////////////////////////////
			///////////////////////////////////////////////////////////////
			camdata.mMatView = V; 
			camdata.CalcCameraData();

			mRenderer.SetCameraData( & camdata );

			////////////////////////////////////////////////////////
			////////////////////////////////////////////////////////
			// setup lighting

			mLightManager.EnumerateInFrustum( camdata.mFrustum );
			int inumlightsinfrustum = mLightManager.miNumLightsInFrustum;

			////////////////////////////////////////////////////////
			////////////////////////////////////////////////////////

			pTARG->MTXI()->PushMMatrix(M);
			pTARG->MTXI()->PushVMatrix(V);
			pTARG->MTXI()->PushPMatrix(P);
			pTARG->MTXI()->MatrixRefresh();
			{
				//////////////////////////////////////////
				// compute basic Dynamic primitive

				U32 ucolor1 = pTARG->CColor4ToU32( ork::CColor4(0.3f,0.4f,0.5f,1.0f) );
				U32 ucolor2 = pTARG->CColor4ToU32( ork::CColor4(0.5f,0.4f,0.3f,1.0f) );
				U32 ucolor3 = pTARG->CColor4ToU32( ork::CColor4(0.3f,0.5f,0.3f,1.0f) );
				U32 ucolor4 = pTARG->CColor4ToU32( ork::CColor4(0.4f,0.4f,0.4f,1.0f) );

				U32 ucolor5 = pTARG->CColor4ToU32( ork::CColor4(0.0f,0.4f,0.5f,1.0f) );
				U32 ucolor6 = pTARG->CColor4ToU32( ork::CColor4(0.0f,0.4f,0.3f,1.0f) );
				U32 ucolor7 = pTARG->CColor4ToU32( ork::CColor4(0.0f,0.5f,0.3f,1.0f) );
				U32 ucolor8 = pTARG->CColor4ToU32( ork::CColor4(0.0f,0.4f,0.4f,1.0f) );
				matsolid.SetColorMode( ork::lev2::GfxMaterial3DSolid::EMODE_VERTEX_COLOR );

				pTARG->GBI()->LockVB( mSolidVerts );
				{
					const float kfmin = -50.05f;
					const float kfmax = 50.0f;
					mSolidVerts.AddVertex( ork::lev2::SVtxV12C4T16(kfmin,kfmin,0.0f, 0.0f,0.0f, ucolor1) );
					mSolidVerts.AddVertex( ork::lev2::SVtxV12C4T16(kfmax,kfmin,0.0f, 0.0f,0.0f, ucolor2) );
					mSolidVerts.AddVertex( ork::lev2::SVtxV12C4T16(kfmax,kfmax,0.0f, 0.0f,0.0f, ucolor3) );

					mSolidVerts.AddVertex( ork::lev2::SVtxV12C4T16(kfmin,kfmin,0.0f, 0.0f,0.0f, ucolor1) );
					mSolidVerts.AddVertex( ork::lev2::SVtxV12C4T16(kfmax,kfmax,0.0f, 0.0f,0.0f, ucolor3) );
					mSolidVerts.AddVertex( ork::lev2::SVtxV12C4T16(kfmin,kfmax,0.0f, 0.0f,0.0f, ucolor4) );

					mSolidVerts.AddVertex( ork::lev2::SVtxV12C4T16(kfmin,0.0f,kfmin, 0.0f,0.0f, ucolor5) );
					mSolidVerts.AddVertex( ork::lev2::SVtxV12C4T16(kfmax,0.0f,kfmin, 0.0f,0.0f, ucolor6) );
					mSolidVerts.AddVertex( ork::lev2::SVtxV12C4T16(kfmax,0.0f,kfmax, 0.0f,0.0f, ucolor7) );

					mSolidVerts.AddVertex( ork::lev2::SVtxV12C4T16(kfmin,0.0f,kfmin, 0.0f,0.0f, ucolor5) );
					mSolidVerts.AddVertex( ork::lev2::SVtxV12C4T16(kfmax,0.0f,kfmax, 0.0f,0.0f, ucolor7) );
					mSolidVerts.AddVertex( ork::lev2::SVtxV12C4T16(kfmin,0.0f,kfmax, 0.0f,0.0f, ucolor8) );
				}
				pTARG->GBI()->UnLockVB( mSolidVerts );

				//////////////////////////////////////////
				// Draw basic Dynamic primitive

				pTARG->GBI()->DrawPrimitive( mSolidVerts );

				//////////////////////////////////////////
				// Draw static Display List

				static ork::lev2::GfxMaterial3DSolid matterrain(pTARG);

				pTARG->BindMaterial( & matterrain );
				{
					matterrain.SetTexture( mTerAmbOccMap );
					matterrain.SetColorMode( lev2::GfxMaterial3DSolid::EMODE_TEX_COLOR );
					ork::lev2::CGfxPrimitives::GetRef().RenderPerlinTerrain( pTARG );
					pTARG->GBI()->DisplayListDraw( mPerlinDisplayList );
				}
				pTARG->BindMaterial( & matsolid );

				//////////////////////////////////////////
				// compute pseudo particles (just a dynamic vertex buffer)

				mParticleSystem.update(1.0f);
				particle::TestController* pcontroller = (particle::TestController*) mParticleSystem.GetController();

				pTARG->GBI()->LockVB( mParticleVerts );
				{
					ork::CColor4 CL;
					
					int icnt = mParticleSystem.GetNumAlive();
					if( icnt>kmaxptcl ) icnt=kmaxptcl;

					for( int i=0; i<icnt; i++ )
					{
						const particle::BasicParticle* ptcl = mParticleSystem.GetActiveParticle(i);

						f32 fx = ptcl->position.GetX();
						f32 fy = ptcl->position.GetY();
						f32 fz = ptcl->position.GetZ();
						f32 fu = ork::CFloat::Rand( 0.0f, 1.0f );
						f32 fv = ork::CFloat::Rand( 0.0f, 1.0f );


						//CL = pcontroller->GetGradientColor( ptcl->fage )*CReal(pcontroller->GetColorScale());
						U32 ucolor = 0xfffffff; //pTARG->CColor4ToU32( CL );

						mParticleVerts.AddVertex( ork::lev2::SVtxV12C4T16(fx,fy,fz,fu,fv, ucolor) );
					}

				}				
				pTARG->GBI()->UnLockVB( mParticleVerts );
				
				//////////////////////////////////////////
				// Draw Pseudo Particles 

				ork::CReal fsin = ork::CFloat::Sin( mPhase*3.5f ) + ork::CReal(1.0f);
				matsolid.mRasterState.SetPointSize(0.0f); //float(pcontroller->GetPointSize()));
				matsolid.mRasterState.SetAlphaTest( ork::lev2::EALPHATEST_OFF, 0.0f );

				ork::CMatrix4 PR = ork::CMatrix4::Identity;
				ork::CMatrix4 PT1 = ork::CMatrix4::Identity;
				ork::CMatrix4 PT2 = ork::CMatrix4::Identity;
				PR.RotateY( mPhase*0.1f );
				PT1.Translate( 50.0f, 0.0f, 0.0f );
				PT2.Translate( -50.0f, 0.0f, 0.0f );
				ork::CMatrix4 MP1 = PT1*PR;;
				ork::CMatrix4 MP2 = PT2*PR;;
			/////////////////////////
				matsolid.mRasterState.SetDepthTest( lev2::EDEPTHTEST_OFF );
				//matsolid.mRasterState.SetBlending( pcontroller->GetBlendMode() );
			/////////////////////////
				pTARG->MTXI()->PushMMatrix(MP1);
				/*switch(pcontroller->GetPrimType())
				{
					case lev2::EPRIM_LINESTRIP :
					case lev2::EPRIM_POINTSPRITES :
					case lev2::EPRIM_TRIANGLESTRIP :
						pTARG->GBI()->DrawPrimitive( mParticleVerts, pcontroller->GetPrimType() );
					case lev2::EPRIM_TRIANGLES :
						mParticleVerts.SetNumVertices(3*(mParticleVerts.GetNum()/3));
						pTARG->GBI()->DrawPrimitive( mParticleVerts, pcontroller->GetPrimType() );
					case lev2::EPRIM_TRIANGLEFAN :
						pTARG->GBI()->DrawPrimitive( mParticleVerts, pcontroller->GetPrimType() );
					case lev2::EPRIM_LINES :
						mParticleVerts.SetNumVertices(2*(mParticleVerts.GetNum()/2));
						pTARG->GBI()->DrawPrimitive( mParticleVerts, pcontroller->GetPrimType() );
					break;
				}*/
				pTARG->MTXI()->PopMMatrix();
				pTARG->MTXI()->PushMMatrix(MP2);
				/*switch(pcontroller->GetPrimType())
				{
					case lev2::EPRIM_LINESTRIP :
					case lev2::EPRIM_POINTSPRITES :
					case lev2::EPRIM_TRIANGLESTRIP :
					case lev2::EPRIM_TRIANGLES :
					case lev2::EPRIM_TRIANGLEFAN :
					case lev2::EPRIM_LINES :
						pTARG->GBI()->DrawPrimitive( mParticleVerts, pcontroller->GetPrimType() );
					break;
				}*/
				pTARG->MTXI()->PopMMatrix();

				matsolid.mRasterState.SetDepthTest( lev2::EDEPTHTEST_LEQUALS );

				///////////////////////////////////////////////////////////////////////////
				///////////////////////////////////////////////////////////////////////////
				// Render Xgm Models
				///////////////////////////////////////////////////////////////////////////
				///////////////////////////////////////////////////////////////////////////

				////////////////////////////////////////////////////
				//const CVector3& ModelBoundingSphere =  model->mBoundingCenter;
				//float ModelBoundingSphereRadius = model->mBoundingRadius*1.0f;
				////////////////////////////////////////////////////
				CVector3 ModelInstBoundingSphere =  LocalPose.RefObjSpaceBoundingSphere().GetXYZ();
				float ModelInstBoundingSphereRadius = LocalPose.RefObjSpaceBoundingSphere().GetW();

				const AABox& ObjSpaceAABox = LocalPose.RefObjSpaceAABoundingBox();

				////////////////////////////////////////////////////
				//float sc_mdltoinst = ModelBoundingSphereRadius/ModelInstBoundingSphereRadius;
				//ModelInstBoundingSphere = ModelInstBoundingSphere*sc_mdltoinst;
				//ModelInstBoundingSphereRadius = ModelBoundingSphereRadius;
				////////////////////////////////////////////////////
				matsolid.mRasterState.SetBlending( lev2::EBLENDING_OFF );

				ork::CReal fsin2 = ork::CFloat::Sin( mPhase*1.1f ) + ork::CReal(1.0f);
				ork::lev2::RenderContextInstData MatCtx;
				ork::lev2::RenderContextInstModelData MdlCtx;

				MatCtx.SetRenderer( & mRenderer );
				MatCtx.SetBotEnvMap( mBotEnvMap );
				MatCtx.SetTopEnvMap( mTopEnvMap );
				MdlCtx.SetSkinned(true);

				float fmscal = 25.0f;

				float TestRadius = ModelInstBoundingSphereRadius*fmscal*1.0f;

				int inummesh = model->GetNumMeshes();

				for( int im=0; im<inummesh; im++ )
				{
					//MdlCtx.SetSubMeshIndex(im);
					XgmMesh& mesh = * model->GetMesh(im);
					MdlCtx.mMesh = & mesh;
					int inumsubmesh = mesh.GetNumSubMeshes();
					for( int imat=0; imat<inumsubmesh; imat++ )
					{
						MatCtx.SetMaterialIndex(imat);
						MdlCtx.mSubMesh = mesh.GetSubMesh(imat);

						ork::CMatrix4 MtxModel = M;
						MtxModel.Scale( fmscal, fmscal, fmscal);

						for( float fx=-100.0; fx<100.0f; fx+=20.0f )
						{
							for( float fz=-100.0; fz<100.0f; fz+=20.0f )
							{

								CVector3 position( fx, 0.0f, fz );
								MtxModel.SetTranslation( position );


								CVector3 wmin = CVector4( ObjSpaceAABox.Min(), 1.0f ).Transform( MtxModel ).GetXYZ();
								CVector3 wmax = CVector4( ObjSpaceAABox.Max(), 1.0f ).Transform( MtxModel ).GetXYZ();

								AABox WldSpaceAABox( wmin, wmax );

								LightMask lmask;
								//lmask.AddLight( & mSkyLight );
								//lmask.AddLight( & mPointLights[0] );
								//lmask.AddLight( & mPointLights[1] );
								//lmask.AddLight( & mPointLights[2] );
								//lmask.AddLight( & mPointLights[3] );
								//lmask.AddLight( & mPointLights[4] );

								CVector3 WorldInstSphereCenter = CVector4( ModelInstBoundingSphere, 1.0f ).Transform(MtxModel);

								CVector2 Pos2D;
								Pos2D.SetX( WorldInstSphereCenter.GetX() );
								Pos2D.SetY( WorldInstSphereCenter.GetZ() );
								Circle cirXZ( Pos2D, 27.0f );

								for( int ilight=0; ilight<inumlightsinfrustum; ilight++ )
								{
									Light* plight = mLightManager.mLightsInFrustum[ ilight ];
									if( plight->AffectsCircleXZ( cirXZ ) )
									//if( plight->AffectsSphere( WorldInstSphereCenter, TestRadius ) )
									//if( plight->AffectsAABox( WldSpaceAABox ) )
									{
										//lmask.AddLight( & mSkyLight );
										lmask.AddLight( plight );
									}
								}

								mLightManager.QueueInstance( lmask, MtxModel );
							}
						}

						int inumlightgroups = mLightManager.GetNumLightGroups();

						int inuminstrend = 0;
						for( int ilg=0; ilg<inumlightgroups; ilg++ )
						{
							const LightingGroup& lgroup = mLightManager.GetGroup( ilg );

							////////////////////////////////////////
							// CHANGE TARGET LIGHTING STATE HERE
							////////////////////////////////////////

							MatCtx.SetLightingGroup( & lgroup );

							////////////////////////////////////////
							// CHANGE TARGET LIGHTING STATE HERE
							////////////////////////////////////////

							const CMatrix4* matrices = lgroup.GetMatrices();
							int inummatrices = lgroup.GetNumMatrices();
							inuminstrend += inummatrices;
							model->RenderMultipleSkinned( modelinst, CColor4::White(), matrices, inummatrices, pTARG, MatCtx, MdlCtx );
						}

						int totinstrend = inuminstrend;
					}
				}

				//////////////////////////////////////////
				// render lights

				#ifndef WII
				for( int ilight=0; ilight<inumlightsinfrustum; ilight++ )
				{
					Light* plight = mLightManager.mLightsInFrustum[ ilight ];
					plight->ImmRender( mRenderer );
				}
				#endif

				//////////////////////////////////////////
			}
			pTARG->MTXI()->PopMMatrix();
			pTARG->MTXI()->PopVMatrix();
			pTARG->MTXI()->PopPMatrix();

			/////////////////////////////////////
			/////////////////////////////////////
			// HUD
			int inumlightgroups = mLightManager.GetNumLightGroups();

#ifndef WII
			pTARG->PushModColor( CColor4::Yellow() );
			{

				CFontMan::DrawText( pTARG, 40, 8, "NumLightGroups<%d>",  inumlightgroups );

				int sumlights = 0;
				int numinst = 0;
				for( int ilg=0; ilg<inumlightgroups; ilg++ )
				{
					const LightingGroup& lgroup = mLightManager.GetGroup( ilg );
					int inumlights = lgroup.GetNumLights();
					sumlights += inumlights;
					numinst += lgroup.mInstances.size();
					//CFontMan::DrawText( pTARG, 48, 24+(16*ilg), "LightGroup<%d> NumLights<%d>", ilg, inumlights );
				}
				float favglightspergroup = float(sumlights)/float(inumlightgroups);

				CFontMan::DrawText( pTARG, 48, 40, "AvgLightsPerGrp<%f> TotInst<%d>", favglightspergroup, numinst );

				CFontMan::FlushQue(pTARG);
			}
			pTARG->PopModColor();
#endif
			//orkprintf( "NumLightInFrustum<%d>\n",  inumlightsinfrustum );
			//orkprintf( "NumLightGroups<%d>\n",  inumlightgroups );

			// HUD
			/////////////////////////////////////
			/////////////////////////////////////
		}
		pTARG->PopViewport();
		pTARG->PopScissor();
#endif
	}
};

///////////////////////////////////////////////////////////////////////////

template<typename VP> class CTestGfxWindow : public ork::lev2::GfxWindow
{
	public:

	bool		mbinit;
	VP			mviewport;
	
	inline CTestGfxWindow( )
		: GfxWindow( 0, 0, 640, 448, "yo" )
		, mviewport( 0, 0, 640, 448 )
	{
	}

	/*virtual*/
	inline void OnShow( void )
	{
		this->SetViewport( & mviewport );
	}

	/*virtual*/
	inline void GotFocus( void )
	{
	}

	/*virtual*/
	inline void LostFocus( void )
	{
	}
};

///////////////////////////////////////////////////////////////////////////
} }
#endif
