///////////////////////////////////////////////////////////////////////////////
//
//
///////////////////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <pkg/ent/entity.h>
#include <pkg/ent/entity.hpp>
#include <pkg/ent/scene.h>
#include <pkg/ent/scene.hpp>
#include <pkg/ent/drawable.h>
#include <ork/reflect/RegisterProperty.h>
#include <ork/rtti/downcast.h>
#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/lev2/gfx/gfxprimitives.h>
#include <ork/lev2/gfx/gfxmaterial_test.h>
///////////////////////////////////////////////////////////////////////////////
#include <ork/reflect/AccessorObjectPropertyType.hpp>
///////////////////////////////////////////////////////////////////////////////
#include <pkg/ent/bullet.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/renderer.h>
#include <ork/util/endian.h>
#include <BulletCollision/CollisionShapes/btHeightfieldTerrainShape.h>
///////////////////////////////////////////////////////////////////////////////
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::BulletShapeHeightfieldData, "BulletShapeHeightfieldData");
///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace ent {

///////////////////////////////////////////////////////////////////////////////

struct hfield_geometry
{
	int mGridSize;
	lev2::GfxMaterial3DSolid* mTerrainMtl;
	orkmap<int,TerVtxBuffersType*>				vtxbufmap;
	sheightmap									mHeightMap;
	const BulletShapeHeightfieldData&			mHFD;
	ork::CVector3 								mAaBbMin;
	ork::CVector3 								mAaBbMax;
	bool 										mInitVIZ;
	btHeightfieldTerrainShape*					mTerrainShape;
	Entity* 									mEntity;

	hfield_geometry( const BulletShapeHeightfieldData& data );

	void init_visgeom(lev2::GfxTarget* ptarg);
	btHeightfieldTerrainShape* init_bullet_shape(const ShapeCreateData& data);
};

///////////////////////////////////////////////////////////////////////////////

hfield_geometry::hfield_geometry(const BulletShapeHeightfieldData& data)
	: mHFD(data)
	, mHeightMap(0,0)
	, mInitVIZ(true)
	, mTerrainShape(nullptr)
	, mEntity(nullptr)
	, mTerrainMtl(nullptr)
{

}

///////////////////////////////////////////////////////////////////////////////

btHeightfieldTerrainShape* hfield_geometry::init_bullet_shape(const ShapeCreateData& data)
{
	mEntity = data.mEntity;

	bool bloadok = mHeightMap.Load( mHFD.HeightMapPath() );

	if( false == bloadok )
		return nullptr;

	int idimx = mHeightMap.GetGridSizeX();
	int idimz = mHeightMap.GetGridSizeZ();

	float aspect = float(idimz)/float(idimx);
	const float kworldsizeX = mHFD.WorldSize();
	const float kworldsizeZ = kworldsizeX*aspect;

	mHeightMap.SetWorldSize( kworldsizeX, kworldsizeZ );
	mHeightMap.SetWorldHeight( mHFD.WorldHeight() );


	auto world_controller = data.mWorld;
	const BulletWorldControllerData& world_data = world_controller->GetWorldData();

	btVector3 grav = !world_data.GetGravity();

	//////////////////////////////////////////
	// hook it up to bullet if present
	//////////////////////////////////////////

	float ftoth = mHeightMap.GetMaxHeight()-mHeightMap.GetMinHeight();


	auto pdata = mHeightMap.GetHeightData();

	mTerrainShape = new btHeightfieldTerrainShape( idimx,idimz, //w,h
											   (void*)pdata, // data
											   ftoth,
											   1, // upAxis,
											   true, // usefloat heightDataType,
											   false ); // flipQuadEdges );

	mTerrainShape->setUseDiamondSubdivision(true);

	float fworldsizeX = mHeightMap.GetWorldSizeX();
	float fworldsizeZ = mHeightMap.GetWorldSizeZ();

	float scalex = fworldsizeX/float(idimx);
	float scalez = fworldsizeZ/float(idimz);
	float scaley = 1.0f;

	mTerrainShape->setLocalScaling( btVector3(scalex, mHeightMap.GetWorldHeight(), scalez ) );

	return mTerrainShape;
}

///////////////////////////////////////////////////////////////////////////////

void hfield_geometry::init_visgeom(lev2::GfxTarget* ptarg)
{
	if( false == mInitVIZ )
		return;

	mInitVIZ = false;

	auto sphmap = mHFD.GetSphereMap();
	auto sphmaptex = (sphmap!=nullptr) ? sphmap->GetTexture() : nullptr;

	mTerrainMtl = new lev2::GfxMaterial3DSolid( ptarg );
	mTerrainMtl->SetColorMode( lev2::GfxMaterial3DSolid::EMODE_VERTEX_COLOR );

	orkprintf( "ComputingGeometry\n" );

	vtxbufmap.clear();

	const int iglX = mHeightMap.GetGridSizeX();
	const int iglZ = mHeightMap.GetGridSizeZ();

	const float kworldsizeX = mHeightMap.GetWorldSizeX();
	const float kworldsizeZ = mHeightMap.GetWorldSizeZ();

	//const float fsize = kfw;

	auto bbctr = (mAaBbMin+mAaBbMax)*0.5f;
	auto bbdim = (mAaBbMax-mAaBbMin);

	printf( "IGLX<%d> IGLZ<%d> kworldsizeXZ<%f %f>\n", iglX, iglZ, kworldsizeX, kworldsizeZ );
	//printf( "bbmin<%f %f %f>\n", mAaBbMin.GetX(), mAaBbMin.GetY(), mAaBbMin.GetZ() );
	//printf( "bbmax<%f %f %f>\n", mAaBbMax.GetX(), mAaBbMax.GetY(), mAaBbMax.GetZ() );
	//printf( "bbctr<%f %f %f>\n", bbctr.GetX(), bbctr.GetY(), bbctr.GetZ() );
	//printf( "bbdim<%f %f %f>\n", bbdim.GetX(), bbdim.GetY(), bbdim.GetZ() );

	AABox aab;
	aab.BeginGrow();

	const int terrain_ngrids = iglX*iglZ;

	if( terrain_ngrids >= 1024 )
	{
		//const int knumqua = kblksizeX*kblksizeZ;
		//const int knumtri = knumqua<<1;

		int inumcol = iglX-1;
		int inumrow = iglZ-1;
		int inumpointsperrow = 2+inumcol*2+2; // 2 degenerates and 2 to start
		int inumvertstotal = inumpointsperrow*inumrow;

		////////////////////////////////////////////
		// find/create vertexbuffers			
		////////////////////////////////////////////

		TerVtxBuffersType* vertexbuffers = 0;

		orkmap<int,TerVtxBuffersType*>::const_iterator itv = vtxbufmap.find(terrain_ngrids);

		if( itv == vtxbufmap.end() ) // init index buffer for this terrain size
		{
			vertexbuffers = OrkNew TerVtxBuffersType;
			vtxbufmap[ terrain_ngrids ] = vertexbuffers;


			auto vbuf = new lev2::StaticVertexBuffer<lev2::SVtxV12C4T16>( inumvertstotal, 0, lev2::EPRIM_POINTS );
			vertexbuffers->push_back( vbuf );
		}
		else
		{
			vertexbuffers = itv->second;
		}

		//mDisplayLists.resize( vertexbuffers->size() );

		////////////////////////////////////////////
		// compute vertexbuffers
		////////////////////////////////////////////

		lev2::CVtxBuffer<lev2::SVtxV12C4T16>* vbuf = (*vertexbuffers)[ 0 ];
		vbuf->Reset();
		lev2::VtxWriter<ork::lev2::SVtxV12C4T16> vwriter;
		vwriter.Lock( ptarg, vbuf, inumvertstotal ); 

		auto l_submit = [&]( const CVector3& xyz, const CVector3& nrm, float fu, float fv )
		{
			auto vo = mHFD.GetVisualOffset();
			lev2::SVtxV12C4T16 vtx;
			vtx.miX = (vo+xyz).GetX(); 
			vtx.miY = (vo+xyz).GetY(); 
			vtx.miZ = (vo+xyz).GetZ(); 

			float fu2 = 0.5f+(nrm.GetX()*0.5);
    		float fv2 = 0.5f+(nrm.GetY()*0.5);

			vtx.muColor = (CVector3(0.25f,0.25f,0.25f)+nrm*0.25f).GetVtxColorAsU32();

			vtx.mfU = fu2;//+(1.0f/4096.0f);
			vtx.mfV = fv2;//+(1.0f/4096.0f);
			//printf( "addvertex<%f %f %f>\n", vtx.miX, vtx.miY, vtx.miZ );
			vwriter.AddVertex( vtx ); 
		};

		for( int irow=0; irow<inumrow; irow++ )
		{
			int iza = irow;
			int izb = irow+1;

			float fva = float(irow)/float(inumrow);
			float fvb = float(irow+1)/float(inumrow);

			for( int icol=0; icol<inumcol; icol++ )
			{
				int ixa = icol;
				int ixb = icol+1;

				float fua = float(icol)/float(inumcol);
				float fub = float(icol+1)/float(inumcol);

				auto xyz_xaza = mHeightMap.XYZ(ixa,iza);
				auto xyz_xazb = mHeightMap.XYZ(ixa,izb);

				aab.Grow( xyz_xaza );
				aab.Grow( xyz_xazb );

				auto nrm_xaza = mHeightMap.ComputeNormal(ixa,iza);
				auto nrm_xazb = mHeightMap.ComputeNormal(ixa,izb);

				if( icol==0 ) // leading degen
				{
					l_submit( xyz_xazb, nrm_xazb, fua, fvb );
				}

				l_submit( xyz_xazb, nrm_xazb, fua, fvb );
				l_submit( xyz_xaza, nrm_xaza, fua, fva );

				if( icol==(inumcol-1) ) // trailing degen
				{
					//auto xyz_xbza = mHeightMap.XYZ(ixb,iza);
					//auto xyz_xbzb = mHeightMap.XYZ(ixb,izb);
					//aab.Grow( xyz_xbza );
					//aab.Grow( xyz_xbzb );
					//auto nrm_xbza = (vnq+mHeightMap.ComputeNormal(ixb,iza)*0.25f).GetVtxColorAsU32();
					//auto nrm_xbzb = (vnq+mHeightMap.ComputeNormal(ixb,izb)*0.25f).GetVtxColorAsU32();

					l_submit( xyz_xaza, nrm_xaza, fua, fva );
				}

			}
		}
		vwriter.UnLock(ptarg);
	}
	aab.EndGrow();
	auto geomin = aab.Min();
	auto geomax = aab.Max();
	auto geosiz = aab.GetSize();
	printf( "geomin<%f %f %f>\n", geomin.GetX(), geomin.GetY(), geomin.GetZ() );
	printf( "geomax<%f %f %f>\n", geomax.GetX(), geomax.GetY(), geomax.GetZ() );
	printf( "geosiz<%f %f %f>\n", geosiz.GetX(), geosiz.GetY(), geosiz.GetZ() );
	//mGeomDirty = false;	
}
///////////////////////////////////////////////////////////////////////////////

void FastRender(	const lev2::RenderContextInstData& rcidata,
					hfield_geometry* htri )
{
	const lev2::Renderer* renderer = rcidata.GetRenderer();
	const ent::Entity* pent = htri->mEntity;
	//const DagNode& dagn = pent->GetEntData().GetDagNode();
	//const auto& xf = dagn.GetTransformNode().GetTransform();
	const auto& hfd = htri->mHFD;
	auto sphmap = hfd.GetSphereMap();

	lev2::GfxTarget* ptarg = renderer->GetTarget();

	auto framedata = ptarg->GetRenderContextFrameData();

	const CMatrix4& PMTX = framedata->GetCameraCalcCtx().mPMatrix;
	const CMatrix4& VMTX = framedata->GetCameraCalcCtx().mVMatrix;
	const auto MMTX = pent->GetEffectiveMatrix();

	bool bpick = ptarg->FBI()->IsPickState();

	if( bpick )
	{
		orkprintf( "yo\n" );
		return ;
	}
	//////////////////////////
	htri->init_visgeom(ptarg);
	//////////////////////////
	

	ptarg->MTXI()->PushPMatrix(PMTX);
	ptarg->MTXI()->PushVMatrix(VMTX);
	ptarg->MTXI()->PushMMatrix(MMTX);
	{
		const int iglX = htri->mHeightMap.GetGridSizeX();
		const int iglZ = htri->mHeightMap.GetGridSizeZ();

		const int terrain_ngrids = iglX*iglZ;

		if( terrain_ngrids>=1024 )
		{
			const auto& vb_map = htri->vtxbufmap;
			const auto& itv = vb_map.find( terrain_ngrids );

			if( (itv!=vb_map.end() ) )
			{
				//auto indexbuffer = iti->second;
				auto vbsptr = itv->second;
				auto& vertexbuffers = *vbsptr;

				///////////////////////////////////////////////////////////////////
				// render
				///////////////////////////////////////////////////////////////////

				//auto tex2 = htrc.GetColorTexture2();

				lev2::Texture* ColorTex = nullptr;
				if( sphmap && sphmap->GetTexture() )
					ColorTex = sphmap->GetTexture();

				auto std_mode = (ColorTex!=nullptr) 
							  ? lev2::GfxMaterial3DSolid::EMODE_TEXVERTEX_COLOR
							  : lev2::GfxMaterial3DSolid::EMODE_VERTEX_COLOR;

				htri->mTerrainMtl->SetColorMode( bpick ? lev2::GfxMaterial3DSolid::EMODE_MOD_COLOR : std_mode );
				
				htri->mTerrainMtl->SetTexture( ColorTex );

				ptarg->PushMaterial( htri->mTerrainMtl );
				int ivbidx = 0;

				CColor4 ObjColor;
				//ObjColor.SetRGBAU32( (uint32_t) & pent->GetEntData() );

				ptarg->PushModColor( bpick ? ObjColor : CColor4::White() );

				{
					int inumvb = vertexbuffers.size();

					int inumpasses = htri->mTerrainMtl->BeginBlock(ptarg,rcidata);
					bool bDRAW = htri->mTerrainMtl->BeginPass( ptarg,0 );
					if( bDRAW )
					{
						for( int ivb=0; ivb<inumvb; ivb++ )
						{
							auto vertex_buf = vertexbuffers[ivb];

							//const lev2::DisplayList& dlist = DisplayLists[ivb];
							//ptarg->DisplayListDrawEML( dlist );
							//ptarg->GBI()->DrawIndexedPrimitiveEML( *vertex_buf, *indexbuffer, lev2::EPRIM_TRIANGLES );
							ptarg->GBI()->DrawPrimitiveEML( *vertex_buf, lev2::EPRIM_TRIANGLESTRIP );
						}
						htri->mTerrainMtl->EndPass( ptarg );
						htri->mTerrainMtl->EndBlock( ptarg );
					}
				}
				ptarg->PopModColor();	
				ptarg->PopMaterial();
			}
		}
	}
	ptarg->MTXI()->PopMMatrix();
	ptarg->MTXI()->PopVMatrix();
	ptarg->MTXI()->PopPMatrix();
	//htrc.UnLockVisMap();
}

///////////////////////////////////////////////////////////////////////////////

static void RenderHeightfield(	lev2::RenderContextInstData& rcid,
			 					lev2::GfxTarget* targ,
			 					const lev2::CallbackRenderable* pren )
{
	auto data = pren->GetDrawableDataA().Get<hfield_geometry*>();
	if( data )
		FastRender( rcid, data );
}

///////////////////////////////////////////////////////////////////////////////

void BulletShapeHeightfieldData::Describe()
{
	reflect::RegisterProperty( "HeightMap", & BulletShapeHeightfieldData::GetHeightMapName, & BulletShapeHeightfieldData::SetHeightMapName );
	reflect::RegisterProperty( "WorldHeight", & BulletShapeHeightfieldData::mWorldHeight );
	reflect::RegisterProperty( "WorldSize", & BulletShapeHeightfieldData::mWorldSize );
	reflect::RegisterProperty( "SphericalLightMap", & BulletShapeHeightfieldData::GetTextureAccessor, & BulletShapeHeightfieldData::SetTextureAccessor );
	reflect::RegisterProperty( "VisualOffset", &BulletShapeHeightfieldData::mVisualOffset );

	reflect::AnnotatePropertyForEditor<BulletShapeHeightfieldData>( "HeightMap", "editor.class", "ged.factory.filelist" );
	reflect::AnnotatePropertyForEditor<BulletShapeHeightfieldData>( "HeightMap", "editor.filetype", "png" );

	reflect::AnnotatePropertyForEditor< BulletShapeHeightfieldData >(	"WorldHeight", "editor.range.min", "1.0f" );
	reflect::AnnotatePropertyForEditor< BulletShapeHeightfieldData >(	"WorldHeight", "editor.range.max", "20000.0" );

	reflect::AnnotatePropertyForEditor< BulletShapeHeightfieldData >(	"WorldSize", "editor.range.min", "1.0f" );
	reflect::AnnotatePropertyForEditor< BulletShapeHeightfieldData >(	"WorldSize", "editor.range.max", "20000.0" );

	ork::reflect::AnnotatePropertyForEditor<BulletShapeHeightfieldData>("SphericalLightMap", "editor.class", "ged.factory.assetlist" );
	ork::reflect::AnnotatePropertyForEditor<BulletShapeHeightfieldData>("SphericalLightMap", "editor.assettype", "lev2tex" );
	ork::reflect::AnnotatePropertyForEditor<BulletShapeHeightfieldData>("SphericalLightMap", "editor.assetclass", "lev2tex");

}

///////////////////////////////////////////////////////////////////////////////
void BulletShapeHeightfieldData::SetTextureAccessor( ork::rtti::ICastable* const & tex)
{	mSphereLightMap = tex ? ork::rtti::autocast( tex ) : 0;
}
///////////////////////////////////////////////////////////////////////////////
void BulletShapeHeightfieldData::GetTextureAccessor( ork::rtti::ICastable* & tex) const
{	tex = mSphereLightMap;
}

///////////////////////////////////////////////////////////////////////////////

BulletShapeHeightfieldData::BulletShapeHeightfieldData()
	: mHeightMapName( "none" )
	, mWorldHeight( 1000.0f )
	, mWorldSize( 1000.0f )
	, mSphereLightMap(nullptr)
{
	mShapeFactory = [=](const ShapeCreateData& data) -> BulletShapeBaseInst*
	{
		auto rval = new BulletShapeBaseInst;
		auto geo = new hfield_geometry( *this );
		rval->mCollisionShape = geo->init_bullet_shape(data);

		//////////////////////////////////////////
		#if 1//DRAWTHREADS
		auto pdrw = OrkNew ent::CallbackDrawable( data.mEntity );
		data.mEntity->AddDrawable( AddPooledLiteral("Default"),pdrw );
		
		pdrw->SetRenderCallback( RenderHeightfield );
		pdrw->SetOwner( & data.mEntity->GetEntData() );
		pdrw->SetUserDataA( (hfield_geometry*) geo );
		pdrw->SetSortKey(1000);
		#endif	

		return rval;
	};
}

///////////////////////////////////////////////////////////////////////////////

BulletShapeHeightfieldData::~BulletShapeHeightfieldData()
{

}

///////////////////////////////////////////////////////////////////////////////

void BulletShapeHeightfieldData::SetHeightMapName( file::Path const & lmap )
{
	mHeightMapName = lmap;
	//mbDirty = true;
}

///////////////////////////////////////////////////////////////////////////////

void BulletShapeHeightfieldData::GetHeightMapName( file::Path & lmap ) const
{
	lmap = mHeightMapName;
}

///////////////////////////////////////////////////////////////////////////////
}}
///////////////////////////////////////////////////////////////////////////////
