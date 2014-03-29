////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/kernel/opq.h>
#include <pkg/ent/scene.h>
#include <pkg/ent/drawable.h>
#include <pkg/ent/entity.h>
#include <ork/kernel/string/string.h>
#include <ork/reflect/RegisterProperty.h>
#include <ork/reflect/DirectObjectMapPropertyType.h>
#include <ork/reflect/DirectObjectMapPropertyType.hpp>
#include <ork/lev2/gfx/renderer.h>
#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/lev2/gfx/lighting/gfx_lighting.h>
#include <ork/lev2/aud/audiodevice.h>
#include <ork/lev2/lev2_asset.h>
#include <pkg/ent/ReferenceArchetype.h>

#include <ork/stream/ResizableStringOutputStream.h>
#include <ork/kernel/orklut.hpp>
#include <ork/math/collision_test.h>
#include <ork/gfx/camera.h>

INSTANTIATE_TRANSPARENT_RTTI(ork::ent::Drawable, "Drawable");
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::ModelDrawable, "ModelDrawable");
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::CallbackDrawable, "CallbackDrawable");
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::CameraDrawable, "CameraDrawable");

extern bool USE_THREADED_RENDERER;

namespace ork { namespace ent {

ork::MpMcBoundedQueue<RenderSyncToken> DrawableBuffer::mOfflineRenderSynchro;
ork::MpMcBoundedQueue<RenderSyncToken> DrawableBuffer::mOfflineUpdateSynchro;

concurrent_triple_buffer<DrawableBuffer> DrawableBuffer::gBuffers;

bool DrawableBuffer::gbInsideClearAndSync = false;

Layer::Layer()
{
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void Drawable::Describe()
{
}

void ModelDrawable::Describe()
{
}

void CallbackDrawable::Describe()
{
}
void CameraDrawable::Describe()
{
}

///////////////////////////////////////////////////////////////////////////////

void DrawableBuffer::Reset()
{
	//AssertOnOpQ2( UpdateSerialOpQ() );

	miNumLayersUsed = 0;
	mLayers.clear();
	for( int il=0; il<kmaxlayers; il++ )
	{
		mRawLayers[il].Reset(*this);
	}
}

///////////////////////////////////////////////////////////////////////////////

void DrawableBufLayer::Reset(const DrawableBuffer& dB)
{
	//AssertOnOpQ2( UpdateSerialOpQ() );
	miBufferIndex = dB.miBufferIndex;
	miItemIndex = -1;
}

///////////////////////////////////////////////////////////////////////////////

DrawableBufLayer* DrawableBuffer::MergeLayer( const PoolString& layername )
{
	AssertOnOpQ2( UpdateSerialOpQ() );
	DrawableBufLayer* player = 0;
	LayerLut::const_iterator itL = mLayerLut.find(layername);
	if( itL != mLayerLut.end() )
	{
		player = itL->second;
	}
	else
	{
		OrkAssert( miNumLayersUsed<kmaxlayers );
		player = & mRawLayers[miNumLayersUsed++];
		mLayers.insert(layername);
		mLayerLut.AddSorted(layername,player);
	}
	return player;
}

///////////////////////////////////////////////////////////////////////////////

DrawableBufItem& DrawableBufLayer::Queue()
{
	AssertOnOpQ2( UpdateSerialOpQ() );
	//mDrawBufItems.push_back(DrawableBufItem()); // replace std::vector with an array so we can amortize construction costs
	miItemIndex++;
	OrkAssert(miItemIndex<kmaxitems );
	DrawableBufItem& item = mDrawBufItems[miItemIndex];
	item.SetDrawable(0);
	item.mUserData0 = 0;
	item.mUserData1 = 0;
	item.mMatrix = CMatrix4::Identity;
	item.miBufferIndex = miBufferIndex;
	return item;
}

///////////////////////////////////////////////////////////////////////////////

DrawableBuffer::DrawableBuffer(int ibidx)
	: miBufferIndex(ibidx)
	, miNumLayersUsed(0)
{
}

DrawableBufLayer::DrawableBufLayer()
	: miItemIndex(-1)
	, miBufferIndex(-1)
{
}

///////////////////////////////////////////////////////////////////////////////

DrawableBuffer::~DrawableBuffer()
{
}

/////////////////////////////////////////////////////////////////////
const DrawableBuffer* DrawableBuffer::LockReadBuffer(int lid)
{
	const DrawableBuffer* rbuf = gBuffers.begin_pull();
	//if( rbuf )
	//	rbuf->mBufferMutex.Lock();
	//printf( "rbuf<%p> beginpull\n", rbuf );
	return rbuf;
}
/////////////////////
void DrawableBuffer::UnLockReadBuffer(const DrawableBuffer*db)
{
	//db->mBufferMutex.UnLock();
	gBuffers.end_pull(db);
	//printf( "rbuf<%p> endpull\n", db );
}
/////////////////////////////////////////////////////////////////////
DrawableBuffer* DrawableBuffer::LockWriteBuffer(int lid)
{	AssertOnOpQ2( UpdateSerialOpQ() );
	DrawableBuffer* wbuf = gBuffers.begin_push();
	//printf( "wbuf<%p> beginpush\n", wbuf );
	//wbuf->mBufferMutex.Lock();
	return wbuf;
}
void DrawableBuffer::UnLockWriteBuffer(DrawableBuffer*db)
{	AssertOnOpQ2( UpdateSerialOpQ() );
	//db->mBufferMutex.UnLock();
	gBuffers.end_push(db);
	//printf( "wbuf<%p> endpush\n", db );
}
/////////////////////////////////////////////////////////////////////
void DrawableBuffer::BeginClearAndSync()
{
	gBuffers.disable();
	//////////////////////////////////////////////////////////
	//ork::lev2::GfxEnv::GetRef().GetGlobalLock().Lock(); // InterThreadLock
	//////////////////////////////////////////////////////////
	gbInsideClearAndSync = true;
}
/////////////////////////////////////////////////////////////////////
void DrawableBuffer::EndClearAndSync()
{
	////////////////////
	gBuffers.enable();
	gbInsideClearAndSync = false;
	//////////////////////////////////////////////////////////
	//ork::lev2::GfxEnv::GetRef().GetGlobalLock().UnLock(); // InterThreadLock
	//////////////////////////////////////////////////////////
}
/////////////////////////////////////////////////////////////////////
void DrawableBuffer::ClearAndSync()
{
	//AssertOnOpQ2( UpdateSerialOpQ() );
	BeginClearAndSync();
	EndClearAndSync();
}
///////////////////////////////////////////////////////////////////////////////
CameraDrawable::CameraDrawable( Entity* pent, const CCameraData* camData )
	: Drawable( pent )
	, mCameraData( camData )
{
}
/////////////////////////////////////////////////////////////////////
void CameraDrawable::QueueToRenderer(const DrawableBufItem& item, lev2::Renderer* renderer) const
{
	AssertOnOpQ2( MainThreadOpQ() );
	const ork::lev2::RenderContextFrameData* fdata = renderer->GetTarget()->GetRenderContextFrameData();
	//printf( "CameraDrawable::QueueToRenderer mcd<%p>\n", mCameraData );
	if( 0 == mCameraData ) return;
	lev2::FrustumRenderable& frusrend = renderer->QueueFrustum();
	frusrend.SetColor( ork::CColor4::White() );
	
	CameraCalcContext cctx;
	int iw = fdata->GetTarget()->GetW();
	int ih = fdata->GetTarget()->GetH();
	float faspect = float(iw)/float(ih);
	mCameraData->CalcCameraMatrices(cctx,faspect);
	
	frusrend.SetFrustum( cctx.mFrustum );
	frusrend.SetObjSpace( false );
	
}
/////////////////////////////////////////////////////////////////////
void CameraDrawable::QueueToBuffer(DrawableBufLayer&buffer) const
{
	AssertOnOpQ2( UpdateSerialOpQ() );
	DrawableBufItem& item = buffer.Queue();
	item.SetDrawable(this);
}
///////////////////////////////////////////////////////////////////////////////
Drawable::Drawable(Entity* pent)
	: mEntity(pent)
	, mOwner( 0 )
	, mData( 0 )

{
	AssertOnOpQ2( UpdateSerialOpQ() );
	//printf( "Drawable<%p>::Drawable(Entity<%p>)\n", this, pent );
	fflush(stdout);
	OrkAssert(mEntity);
}
Drawable::~Drawable()
{
	AssertOnOpQ2( UpdateSerialOpQ() );
	//printf( "Delete Drawable<%p>\n", this );
}

///////////////////////////////////////////////////////////////////////////////
const DagNode *Drawable::GetDagNode() const
{	
	//printf( "Drawable<%p>::GetDagNode() Entity<%p>\n", this, mEntity );
	fflush(stdout);
	OrkAssert(mEntity!=0);
	if(ESCENEMODE_RUN == mEntity->GetSceneInst()->GetSceneInstMode())
		return &mEntity->GetDagNode();
	else
		return &mEntity->GetEntData().GetDagNode();
}

///////////////////////////////////////////////////////////////////////////////
ModelDrawable::ModelDrawable( Entity* pent )
	: Drawable( pent )
	, mModelInst( NULL )
	, mfScale( 1.0f )
	, mRotate(0.0f,0.0f,0.0f)
	, mOffset(0.0f,0.0f,0.0f)
	, mOverrideXF(0)
	, mpWorldPose( 0 )
	, mbShowBoundingSphere(false)
{
	for(int i = 0; i < kMaxEngineParamFloats; i++)
		mEngineParamFloats[i] = 0.0f;
}
/////////////////////////////////////////////////////////////////////
ModelDrawable::~ModelDrawable()
{
	if( mpWorldPose )
	{
		delete mpWorldPose;
	}
}
void ModelDrawable::SetEngineParamFloat(int idx, float fv)
{
	OrkAssert(idx >= 0 && idx < kMaxEngineParamFloats);

	mEngineParamFloats[idx] = fv;
}

float ModelDrawable::GetEngineParamFloat(int idx) const
{
	OrkAssert(idx >= 0 && idx < kMaxEngineParamFloats);

	return mEngineParamFloats[idx];
}

///////////////////////////////////////////////////////////////////////////////

void ModelDrawable::SetModelInst(lev2::XgmModelInst* pModelInst)
{
	mModelInst = pModelInst;
	const lev2::XgmModel* Model = mModelInst->GetXgmModel();
	bool IsSkinned = Model->IsSkinned();
	ork::lev2::XgmWorldPose* pworldpose = 0;
	if( IsSkinned )
	{
		mpWorldPose = new ork::lev2::XgmWorldPose( Model->RefSkel(), mModelInst->RefLocalPose() );
	}
	anyp ap;
	ap.Set( mpWorldPose );
	SetData(ap);
}

///////////////////////////////////////////////////////////////////////////////

void ModelDrawable::QueueToBuffer(DrawableBufLayer&buffer) const
{
	AssertOnOpQ2( UpdateSerialOpQ() );
	const lev2::XgmModel* Model = mModelInst->GetXgmModel();
	bool IsSkinned = Model->IsSkinned();

	const ork::TransformNode3D* xf = & GetDagNode()->GetTransformNode();

	if( mOverrideXF ) xf = mOverrideXF;
		
	ork::CMatrix4 matw;
	xf->GetMatrix(matw);

	DrawableBufItem& item = buffer.Queue();

	item.mMatrix = matw;
	item.SetDrawable(this);

	//orkprintf( " ModelDrawable::QueueToBuffer() mdl<%p> IsSkinned<%d>\n", Model, int(IsSkinned) );

	if( IsSkinned )
	{
		ork::lev2::XgmWorldPose* pworldpose = GetData().Get<ork::lev2::XgmWorldPose*>();
		if( pworldpose )
		{
			const ork::lev2::XgmSkeleton & Skeleton = Model->RefSkel();

			const ork::lev2::XgmLocalPose& locpos = mModelInst->RefLocalPose();
			orkvector<CMatrix4>& WorldMatrices = pworldpose->GetMatrices();
			int inumch = locpos.NumJoints();
			for( int ich=0; ich<inumch; ich++ )
			{
				//orkprintf( " mdrwq2b setmtxblk ich<%d>\n", ich );
				const CMatrix4 & MatIBind = Skeleton.RefInverseBindMatrix(ich);
				const CMatrix4 & MatJ = Skeleton.RefJointMatrix( ich );
				const CMatrix4& MatAnimJCat = locpos.RefLocalMatrix(ich);

				WorldMatrices[ich]=(MatIBind * MatAnimJCat);
			}

		}
	}
}

///////////////////////////////////////////////////////////////////////////////

void ModelDrawable::QueueToRenderer( const DrawableBufItem& item, lev2::Renderer* renderer ) const
{
	AssertOnOpQ2( MainThreadOpQ() );
	const ork::lev2::RenderContextFrameData* fdata = renderer->GetTarget()->GetRenderContextFrameData();
	const lev2::XgmModel* Model = mModelInst->GetXgmModel();
	const CCameraData* camdat = fdata->GetCameraData(); 
	OrkAssert(camdat!=0);

	const CameraCalcContext& ccctx = fdata->GetCameraCalcCtx();
	bool bvisicd = camdat->GetVisibilityCamDat() != 0;
	if( bvisicd )
	{
		camdat = camdat->GetVisibilityCamDat();
	}
	const Frustum& frus = bvisicd ? camdat->GetFrustum() : ccctx.mFrustum; //camdat->GetFrustum();
	//if( mOverrideXF ) xf = mOverrideXF;
	const ork::CMatrix4& matw = item.mMatrix;
	//xf->GetMatrix(matw);
	
	bool IsPickState = renderer->GetTarget()->FBI()->IsPickState();
	bool IsSkinned = Model->IsSkinned();
	//OrkAssert( renderer->GetTarget()->FBI()->IsPickState() == false );

	//orkprintf( " ModelDrawable::QueueToRenderer() mdl<%p> \n", Model );

	ork::CVector3 center_plus_offset = mOffset + Model->GetBoundingCenter();

	ork::CVector3 ctr = ork::CVector4(center_plus_offset*mfScale).Transform(matw);

	ork::CVector3 vwhd = Model->GetBoundingAA_WHD();
	float frad = vwhd.GetX();
	if( vwhd.GetY() > frad ) frad = vwhd.GetY();
	if( vwhd.GetZ() > frad ) frad = vwhd.GetZ();
	frad*=0.6f;

	bool bCenterInFrustum = frus.Contains(ctr);

	//////////////////////////////////////////////////////////////////////

	const ork::lev2::XgmWorldPose* pworldpose = GetData().Get<ork::lev2::XgmWorldPose*>();
	
	ork::CVector3 matw_trans;
	ork::CQuaternion matw_rot;
	float matw_scale;

	matw.DecomposeMatrix( matw_trans, matw_rot, matw_scale );

	//////////////////////////////////////////////////////////////////////
	// render bounding sphere ?
	//////////////////////////////////////////////////////////////////////

	if( false==IsPickState && mbShowBoundingSphere )
	{
		lev2::SphereRenderable& sphrend = renderer->QueueSphere();
		sphrend.SetColor( ork::CColor4::White() );
		sphrend.SetPosition( ctr );
		sphrend.SetRadius( frad*mfScale );
	}
		
	//////////////////////////////////////////////////////////////////////
	// generate coarse light mask

	ork::lev2::LightMask mdl_lmask;

	ork::lev2::LightManager* light_manager = fdata->GetLightManager();

	size_t inuml = 0;

	if( light_manager )
	{
		inuml = light_manager->mLightsInFrustum.size();

		for( size_t il=0; il<inuml; il++ )
		{
			ork::lev2::Light* plight = light_manager->mLightsInFrustum[il];
			OrkAssert( plight );

			bool baf = plight->AffectsSphere( ctr, frad );
			if( baf )
			{
				mdl_lmask.AddLight( plight );
			}
		}
	}

	//////////////////////////////////////////////////////////////////////

	int inumacc = 0;
	int inumrej = 0;
	
	int inummeshes = Model->GetNumMeshes();
	for( int imesh=0; imesh<inummeshes; imesh++ )
	{
		const lev2::XgmMesh& mesh = * Model->GetMesh(imesh);
		
		//if( 0 == strcmp(mesh.GetMeshName().c_str(),"fg_2_1_3_ground_SG_ground_GeoDaeId") )
		//{
		//	orkprintf( "yo\n" );
		//}

		if(mModelInst->IsMeshEnabled(imesh))
		{
			int inumclusset = mesh.GetNumSubMeshes();

			for( int ics=0; ics<inumclusset; ics++ )
			{
				const lev2::XgmSubMesh& submesh = * mesh.GetSubMesh(ics);
				const lev2::GfxMaterial* material = submesh.mpMaterial;

				int inumclus = submesh.miNumClusters;

				for( int ic=0; ic<inumclus; ic++ )
				{
					bool btest = true;

					const lev2::XgmCluster& cluster = submesh.RefCluster(ic);

					ork::lev2::LightMask lmask;

					if( IsSkinned )
					{
						lmask = mdl_lmask;


						float fdb = frus.mBottomPlane.GetPointDistance(ctr);
						float fdt = frus.mTopPlane.GetPointDistance(ctr);
						float fdl = frus.mLeftPlane.GetPointDistance(ctr);
						float fdr = frus.mRightPlane.GetPointDistance(ctr);
						float fdn = frus.mNearPlane.GetPointDistance(ctr);
						float fdf = frus.mFarPlane.GetPointDistance(ctr);



						const float kdist = -5.0f;
						btest	=	(fdb>kdist)
								&&	(fdt>kdist)
								&&	(fdl>kdist)
								&&	(fdr>kdist)
								&&	(fdn>kdist)
								//&&	(fdn<100.0f); // 50m actors
								&&	(fdf>kdist);
						if( false == btest )
						{
							
						}
					}
					else
					{
						const Sphere& bsph = cluster.mBoundingSphere;
						
						float clussphrad = bsph.mRadius*matw_scale*mfScale;
						CVector3 clussphctr = ((bsph.mCenter+mOffset)*mfScale).Transform(matw);
						Sphere sph2( clussphctr, clussphrad );

						btest = CollisionTester::FrustumSphereTest( frus, sph2 );

						////////////////////////////////////////////////
						// per cluster light assignment
						////////////////////////////////////////////////

						if( btest )
						{
							////////////////////////////////////////////////
							// draw per cluster vis spheres
							////////////////////////////////////////////////

							if( false==IsPickState && mbShowBoundingSphere )
							{
								bool binside = frus.Contains(clussphctr);
							
								lev2::SphereRenderable& sphrend = renderer->QueueSphere();
								sphrend.SetColor( binside ? ork::CColor4::Green()*0.3f : ork::CColor4::Green() );
								sphrend.SetPosition( clussphctr );
								sphrend.SetRadius( clussphrad );
								sphrend.SetRadius( clussphrad );
							}

							////////////////////////////////////////////////
							// lighting sphere test
							////////////////////////////////////////////////

							ork::CVector3 ctr = ork::CVector4(Model->GetBoundingCenter()).Transform(matw);
							for( size_t il=0; il<inuml; il++ )
							{
								ork::lev2::Light* plight = light_manager->mLightsInFrustum[il];
								OrkAssert( plight );
								bool baf = plight->AffectsSphere( sph2.mCenter, sph2.mRadius );
								if( baf )
								{
									lmask.AddLight( plight );
								}
							}
						}
					}

					if( btest )
					{
						lev2::CModelRenderable& renderable = renderer->QueueModel();

						//if(mEngineParamFloats[0] < 1.0f && mEngineParamFloats[0] > 0.0f)
						//	orkprintf("mEngineParamFloats[0] = %g\n", mEngineParamFloats[0]);

						for(int i = 0; i < kMaxEngineParamFloats; i++)
							renderable.SetEngineParamFloat(i, mEngineParamFloats[i]);

						renderable.SetModelInst( mModelInst );
						renderable.SetObject( GetOwner() );
						renderable.SetMesh( & mesh );
						renderable.SetSubMesh( & submesh );
						renderable.SetCluster( & cluster );
						renderable.SetModColor(renderer->GetTarget()->RefModColor());
						renderable.SetMatrix(matw);
						renderable.SetLightMask( lmask );
						renderable.SetScale( mfScale );
						renderable.SetRotate( mRotate );
						renderable.SetOffset( mOffset );
						renderable.SetWorldPose( pworldpose );

						size_t umat = size_t(material);
						u32 imtla = (umat&0xff);
						u32 imtlb = ((umat>>8)&0xff);
						u32 imtlc = ((umat>>16)&0xff);
						u32 imtld = ((umat>>24)&0xff);
						u32 imtl = (imtla+imtlb+imtlc+imtld)&0xff;

						int isortpass = (material->GetRenderQueueSortingData().miSortingPass+16)&0xff;
						int isortoffs = material->GetRenderQueueSortingData().miSortingOffset;

						int isortkey = (isortpass<<24)|(isortoffs<<16)|imtl;

						renderable.SetSortKey( isortkey );
						//orkprintf( " ModelDrawable::QueueToRenderer() rable<%p> \n", & renderable );


						inumacc++;
					}
					else
					{
						inumrej++;
					}
				}
			}
		}
	}

	//orkprintf( "numacc %d numrej %d\n", inumacc, inumrej );
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
CallbackDrawable::CallbackDrawable( Entity *pent )
	: Drawable( pent )
	, mSortKey( 4 )
	, mCallback( 0 )
	, mBufferCallback( 0 )
	, mDataDestroyer( 0 )
{
}
///////////////////////////////////////////////////////////////////////////////
CallbackDrawable::~CallbackDrawable()
{
	if( mDataDestroyer )
	{
		mDataDestroyer->Destroy();
	}
	mDataDestroyer = 0;
	mCallback = 0;
}
///////////////////////////////////////////////////////////////////////////////
// 
///////////////////////////////////////////////////////////////////////////////
void CallbackDrawable::QueueToRenderer( const DrawableBufItem& item, lev2::Renderer* renderer ) const
{	lev2::CallbackRenderable& renderable = renderer->QueueCallback();
	CMatrix4 mtxW;
	GetDagNode()->GetTransformNode().GetMatrix(mtxW);
	renderable.SetMatrix( item.mMatrix );
	renderable.SetObject( GetOwner() );
	renderable.SetCallback( mCallback ); 
	renderable.SetSortKey( mSortKey );
	renderable.SetDrawableData( GetData() );
	renderable.SetUserData0( item.mUserData0 );
	renderable.SetUserData1( item.mUserData1 );
	renderable.SetModColor(renderer->GetTarget()->RefModColor());
	//renderer
}
///////////////////////////////////////////////////////////////////////////////
// Multithreaded Renderer DB 
///////////////////////////////////////////////////////////////////////////////
void CallbackDrawable::QueueToBuffer(DrawableBufLayer&buffer) const
{
	AssertOnOpQ2( UpdateSerialOpQ() );
	if( GetDagNode() )
	{
		DrawableBufItem& cdb = buffer.Queue();
		GetDagNode()->GetTransformNode().GetMatrix(cdb.mMatrix);
		cdb.SetDrawable(this);
		cdb.mUserData0 = GetData();
		if( mBufferCallback )
		{
			mBufferCallback(cdb);
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////


}}
