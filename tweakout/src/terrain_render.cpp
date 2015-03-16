////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2014, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <orktool/qtui/qtui_tool.h>
#include <ork/math/plane.h>
#include <ork/math/misc_math.h>
#include <ork/math/basicfilters.h>
#include <orktool/filter/gfx/meshutil/meshutil.h>
#include <ork/lev2/gfx/renderer.h>
#include <ork/lev2/gfx/gfxmaterial_test.h>
#include <ork/gfx/camera.h>
#include <ork/lev2/input/input.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/kernel/prop.h>
///////////////////////////////////////////////////////////////////////////////
#include <ork/reflect/RegisterProperty.h>
#include <ork/reflect/IObjectPropertyType.hpp>
#include <ork/reflect/DirectObjectPropertyType.hpp>
#include <ork/reflect/enum_serializer.h>
#include <pkg/ent/editor/editor.h>
#include <ork/dataflow/scheduler.h>
///////////////////////////////////////////////////////////////////////////////
#include <pkg/ent/entity.h>
#include <pkg/ent/heightmap.h>
#include "terrain_editor.h"
#include "terrain_synth.h"
///////////////////////////////////////////////////////////////////////////////
#if 0

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace terrain {
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

sheightfield_iface_editor::sheightfield_iface_editor( const TerrainSynth& hf )
	: mhf(hf)
	, mColorTexture( 0 )
{
}

///////////////////////////////////////////////////////////////////////////////

void sheightfield_iface_editor::Render( lev2::RenderContextInstData& rcid,
										lev2::GfxTarget* targ,
										const lev2::CallbackRenderable* pren )
{
	//const sheightfield_iface_editor* shie = (const sheightfield_iface_editor*) pren->GetUserData();
	//shie->FastRender( rcid.GetRenderer(), *pren, rcid );
}


///////////////////////////////////////////////////////////////////////////////

void hmap_hfield_module::LockVisMap() const
{
	//mDefDataBlock.mHeightMap.GetLock().Lock();
	//mVisMutex.Lock();
	//mWorkerLev2Target = pt;
	//int igsize = mDefDataBlock.mHeightMap.GetGridSize();
}
void hmap_hfield_module::UnLockVisMap() const
{
	//mVisMutex.UnLock();
	//mDefDataBlock.mHeightMap.GetLock().UnLock();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void hmap_hfield_module::ComputeGeometry( )
{
	#if 0
	orkprintf( "ComputingGeometry\n" );
	lev2::GfxTarget* ptarg = HeightMapGPGPUComputeBuffer().GetContext();

	mVisLock.Lock();
	const int igl = HeightMapData().GetGridSize();

	if( igl >= 32 )
	{
		const int kblksize = igl>>5;
		const int kblksizep1 = (kblksize+1);
		const int igldiv = igl/kblksize;

		const int knumqua = kblksize*kblksize;
		const int knumtri = knumqua<<1;

		// ((31*31)*2)*3
			
		const float fsize = HeightMapData().GetWorldSize();

		////////////////////////////////////////////
		// get/compute the index buffer			
		////////////////////////////////////////////
		
		lev2::StaticIndexBuffer<U16>* indexbuffer = 0;

		orkmap<int,lev2::StaticIndexBuffer<U16>*>::const_iterator iti = idxbufmap.find( igl );

		if( iti == idxbufmap.end() ) // init index buffer for this terrain size
		{
			indexbuffer = OrkNew lev2::StaticIndexBuffer<U16>( knumtri*3 );
			idxbufmap[ igl ] = indexbuffer;

			U16 *pidx = (U16*) ptarg->IdxBuf_Lock( *indexbuffer );
		
			int idxbi = 0;
			for( int ixa=0; ixa<kblksize; ixa++ )
			{	int ixb=ixa+1;
				
				for( int iza=0; iza<kblksize; iza++ )
				{	
					int izb=iza+1;
					
					int idx_aa = ixa+(iza*kblksizep1);
					int idx_ba = ixb+(iza*kblksizep1);
					int idx_bb = ixb+(izb*kblksizep1);
					int idx_ab = ixa+(izb*kblksizep1);
					
					pidx[idxbi++] = U16(idx_aa);
					pidx[idxbi++] = U16(idx_ba);
					pidx[idxbi++] = U16(idx_bb);

					pidx[idxbi++] = U16(idx_aa);
					pidx[idxbi++] = U16(idx_bb);
					pidx[idxbi++] = U16(idx_ab);				
				}
			}
			
			ptarg->IdxBuf_UnLock( *indexbuffer );
		}
		else
		{
			indexbuffer = iti->second;
		}

		////////////////////////////////////////////
		// find/create vertexbuffers			
		////////////////////////////////////////////

		TerVtxBuffersType* vertexbuffers = 0;

		orkmap<int,TerVtxBuffersType*>::const_iterator itv = vtxbufmap.find(igl);

		if( itv == vtxbufmap.end() ) // init index buffer for this terrain size
		{
			vertexbuffers = OrkNew TerVtxBuffersType;
			vtxbufmap[ igl ] = vertexbuffers;

			for( int ibx=0; ibx<igldiv; ibx++ )
			{
				for( int ibz=0; ibz<igldiv; ibz++ )
				{
					lev2::StaticVertexBuffer<lev2::SVtxV12C4T8>* vbuf
						= OrkNew lev2::StaticVertexBuffer<lev2::SVtxV12C4T8>
							( (kblksizep1*kblksizep1), 0, lev2::EPRIM_POINTS );

					//ptarg->VtxBuf_Init( *vbuf );

					vertexbuffers->push_back( vbuf );
				}
			}
		}
		else
		{
			vertexbuffers = itv->second;
		}

		////////////////////////////////////////////
		// compute vertexbuffers
		////////////////////////////////////////////

		lev2::SVtxV12C4T8 vtx;

		int ivbindex = 0;

		for( int ibx=0; ibx<igldiv; ibx++ )
		{
			int ixbas = ibx*kblksize;
			float fxbas = float(ibx)/float(igldiv);
			for( int ibz=0; ibz<igldiv; ibz++ )
			{
				int izbas = ibz*kblksize;
				float fzbas = float(ibz)/float(igldiv);

				lev2::CVtxBuffer<lev2::SVtxV12C4T8>* vbuf = (*vertexbuffers)[ ivbindex ];

				vbuf->Reset();
				///////////////////////////////////////
				ptarg->VtxBuf_Lock( *vbuf );
				///////////////////////////////////////
				{
					for( int ix=0; ix<(kblksize+1); ix++ )
					{
						int ixv = ixbas+ix;
						int iadX = (ixv>=igl) ? (igl-1) : ixv;
						float fx = float(ixv)/float(igl);
						float fu = float(ixv)/float(igl-1);

						for( int iz=0; iz<(kblksize+1); iz++ )
						{
							int izv = izbas+iz;
							int iadZ = (izv>=igl) ? (igl-1) : izv;
							float fz = float(izv)/float(igl);

							float fv = float(izv)/float(igl-1);

							float fheight = mDefDataBlock.mHeightMap.GetHeight(iadX,iadZ);
							const CVector3& clr = Color( iadX, iadZ );
							vtx.miX = (fx-0.5f)*fsize; 
							vtx.miY = fheight;
							vtx.miZ = (fz-0.5f)*fsize; 
							vtx.muColor = clr.GetARGBU32();
							vtx.mfU = fu+(1.0f/4096.0f);
							vtx.mfV = fv+(1.0f/4096.0f);
							vbuf->AddVertex( vtx ); 
						}
					}
				}
				///////////////////////////////////////
				ptarg->VtxBuf_UnLock( *vbuf );
				///////////////////////////////////////

				ivbindex++;
			}
		}
	}
	mVisLock.UnLock();
	#endif
}

///////////////////////////////////////////////////////////////////////////////

void sheightfield_iface_editor::FastRender( const lev2::Renderer* renderer,
											const lev2::CallbackRenderable& rable,
											const lev2::RenderContextInstData& rcidata ) const
{
	#if 0
	lev2::GfxTarget* ptarg = renderer->GetTarget();
	//const sheightfield_iface_editor* shie = (sheightfield_iface_editor*) rable.GetUserData();
	const TerrainSynth& sh = mhf;

	sh.LockVisMap(ptarg);
	{
		const int igl = sh.GetTargetModule().HeightMapData().GetGridSize();
		if( igl>=32 )
		{
			const orkmap<int,TerVtxBuffersType*>& vtxbuffers = sh.GetTargetModule().VertexBuffers();
			const orkmap<int,lev2::StaticIndexBuffer<U16>*>& idxbuffers = sh.GetTargetModule().IndexBuffers();

			const orkmap<int,lev2::StaticIndexBuffer<U16>*>::const_iterator iti = idxbuffers.find( igl );
			const orkmap<int,TerVtxBuffersType*>::const_iterator itv = vtxbuffers.find( igl );

			if( (iti!=idxbuffers.end()) && (itv!=vtxbuffers.end() ) )
			{
				lev2::StaticIndexBuffer<U16>* indexbuffer = iti->second;
				TerVtxBuffersType* vbsptr = itv->second;
				TerVtxBuffersType& vertexbuffers = *vbsptr;

				///////////////////////////////////////////////////////////////////
				// render
				///////////////////////////////////////////////////////////////////

				static lev2::GfxMaterial3DSolid TerMat( ptarg );

				lev2::CTexture* ColorTex = this->mColorTexture;

				TerMat.SetColorMode( ptarg->IsPickState() 
										? lev2::GfxMaterial3DSolid::EMODE_MOD_COLOR
										: (ColorTex==0)
											? lev2::GfxMaterial3DSolid::EMODE_VERTEX_COLOR
											: lev2::GfxMaterial3DSolid::EMODE_TEXVERTEX_COLOR );

				TerMat.SetTexture( ColorTex );

				CColor4 ObjColor;
				ObjColor.SetRGBAU32( reinterpret_cast<U32>( rable.GetObject() ) );


				ptarg->BindMaterial( & TerMat );

				int ivbidx = 0;
				CMatrix4 wmat; //rable.GetTransformNode()->GetTransform()->GetMatrix();

				ptarg->PushModColor( ptarg->IsPickState() 
					? ObjColor
					: CColor4::White()
				);
				ptarg->PushMMatrix(wmat);
				{
					int inumvb = vertexbuffers.size();

					for( int ivb=0; ivb<inumvb; ivb++ )
					{
						ptarg->DrawIndexedPrimitive( *(vertexbuffers[ivb]), *indexbuffer, lev2::EPRIM_TRIANGLES );
					}
				}
				ptarg->PopModColor();	
				ptarg->PopMMatrix();
			}
		}
	}
	sh.UnLockVisMap();

#endif
}

/////////////////////////////////////////////////////////////////////////////
}}
/////////////////////////////////////////////////////////////////////////////
#endif