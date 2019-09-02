////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <orktool/orktool_pch.h>
#include <ork/math/plane.h>
#include <orktool/filter/gfx/meshutil/meshutil.h>
#if ! defined(IX)
#include <IL/il.h>

namespace devil { void InitDevIL(); }

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace MeshUtil {
///////////////////////////////////////////////////////////////////////////////

void TexToVtx( const tokenlist& options )
{
	ork::tool::FilterOptMap	OptionsMap;
	OptionsMap.SetDefault("-inobj", "textovtx_in.obj");
	OptionsMap.SetDefault( "-outobj", "textovtx_out.dae");
	OptionsMap.SetDefault( "-intex", "textovtx_in.tga");
	OptionsMap.SetOptions( options );

	MeshUtil::toolmesh inmesh;

	std::string ttv_in = OptionsMap.GetOption( "-inobj" )->GetValue();
	std::string ttv_out = OptionsMap.GetOption( "-outobj" )->GetValue();
	std::string ttv_texin = OptionsMap.GetOption( "-intex" )->GetValue();

	inmesh.ReadAuto( ttv_in.c_str() );

	devil::InitDevIL();

	///////////////////////////////////////////////////////////
	// Load Texture Data
	///////////////////////////////////////////////////////////

	ILuint ImageILObject;
	ilGenImages(1, &ImageILObject);
	ilBindImage(ImageILObject);
	ilEnable( IL_ORIGIN_SET );
	ILboolean OriginOK = ilOriginFunc( IL_ORIGIN_UPPER_LEFT );
	bool bv = ilLoadImage( (const ILstring) ttv_texin.c_str() );
	ilActiveImage( 0 );

	ILenum Error = ilGetError(); 

	if( Error != 0 )
	{
		return;
	}
	ILuint Count = ilGetInteger(IL_NUM_IMAGES);
	ILuint Width = ilGetInteger(IL_IMAGE_WIDTH);
	ILuint Height = ilGetInteger(IL_IMAGE_HEIGHT);
	ILuint Depth = ilGetInteger(IL_IMAGE_DEPTH);
	ILuint BPP = ilGetInteger(IL_IMAGE_BPP);
	ILuint datasize = ilGetInteger(IL_IMAGE_SIZE_OF_DATA);
	ILuint dxt_format = ilGetInteger( IL_DXTC_DATA_FORMAT );
	bool IsDXTC = ( dxt_format != IL_DXT_NO_COMP );
	ILubyte* Data = ilGetData();

	orkprintf( "<DEVIL> [%s] [count %d] [W%d] [H%d] [D%d] [Data %08x] [BPP %d] Error %d\n", ttv_texin.c_str(), Count, Width, Height, Depth, Data, BPP, Error );
	orkprintf( "<DEVIL> [datasize %08x]\n", datasize );
	
	///////////////////////////////////////////////////////////
	// fill in mesh connectivity
	// all we care about is data (faces,vertices) connected at pyhsical positions 
	//  disregarding other attributes - so 2 vertices with the same pos and different uv's, colors or normals
	//   will be merged into one (just as far as the connectivity is concerned)
	///////////////////////////////////////////////////////////

	orkmap<U64, orkset<int> >	VertexPosToVtxPoolIndices;
	orkmap<U64, orkset<int> >	VertexPosToFacePoolIndices;
	orkmap<U64, CColor3 >		VertexColorMap;

#if 0
	int inumf = inmesh.GetNumPolys(3);

	for( int iface=0; iface<inumf; iface++ )
	{
		const poly& inpoly = inmesh.RefPoly(iface);
		int inumv = inpoly.GetNumSides();
		if( inumv == 3 )
		{
			for( int iv=0; iv<inumv; iv++ )
			{
				int idx = inpoly.GetVertexID(iv);
				const vertex& invtx = inmesh.RefVertexPool().GetVertex(idx);
				const fvec3& pos = invtx.mPos;
				boost::Crc64 crc64 = boost::crc64( (const void *) & pos, sizeof( pos ) );
				VertexPosToVtxPoolIndices[ crc64.crc0 ].insert( idx );
				VertexPosToFacePoolIndices[ crc64.crc0 ].insert( iface );
			}
		}
	}

	///////////////////////////////////////////////////////////
	// for every vertex position
	//   average surrounding texels on all connected faces (to the percentage (ie 1/2 way) point on each)
	///////////////////////////////////////////////////////////

	for( orkmap<U64, orkset<int> >::const_iterator 
			itv=VertexPosToVtxPoolIndices.begin();
			itv!=VertexPosToVtxPoolIndices.end();
			itv++ )
	{
		orkmap<U64, orkset<int> >::const_iterator itf = VertexPosToFacePoolIndices.find( itv->first );

		const orkset<int>& vertexset = itv->second; // Set of all verts at given position
		const orkset<int>& faceset = itf->second;	// Set of all faces connected to given position

		const U64 key = itf->first;

		CColor3 AvgVertexColor(0.0f,0.0f,0.0f);
		int		NumVertexSamples = 0;

		///////////////////////////////////////////////////
		// calc total surface area for surrounding faces
		///////////////////////////////////////////////////

		float ftotsurfacearea = 0.0f;

		for( orkset<int>::const_iterator itface=faceset.begin(); itface!=faceset.end(); itface++ )
		{
			int ifaceindex = *itface;
			const poly& inpoly = inmesh.RefPoly(ifaceindex);

			if( inpoly.GetNumSides() == 3 )
			{
				const vertex& invtxA = inmesh.RefVertexPool().GetVertex(inpoly.GetVertexID(0));
				const vertex& invtxB = inmesh.RefVertexPool().GetVertex(inpoly.GetVertexID(1));
				const vertex& invtxC = inmesh.RefVertexPool().GetVertex(inpoly.GetVertexID(2));
				float fsurfarea = (invtxB.mPos-invtxA.mPos).Mag()*(invtxC.mPos-invtxA.mPos).Mag()*0.5f;
				ftotsurfacearea += fsurfarea;
			}
		}

		///////////////////////////////////////////////////
		// calc vertex color (weighted average of sampled texture on surrounding faces)
		///////////////////////////////////////////////////

		for( orkset<int>::const_iterator itface=faceset.begin(); itface!=faceset.end(); itface++ )
		{
			int ifaceindex = *itface;

			const poly& inpoly = inmesh.RefPoly(ifaceindex);
			int inumv = inpoly.GetNumSides();

			if( inumv == 3 )
			{
				int ifv_this = -1;

				const vertex& invtxA = inmesh.RefVertexPool().GetVertex(inpoly.GetVertexID(0));
				const vertex& invtxB = inmesh.RefVertexPool().GetVertex(inpoly.GetVertexID(1));
				const vertex& invtxC = inmesh.RefVertexPool().GetVertex(inpoly.GetVertexID(2));
				const fvec3& posA = invtxA.mPos;
				const fvec3& posB = invtxB.mPos;
				const fvec3& posC = invtxC.mPos;
				boost::Crc64 crc64A = boost::crc64( (const void *) & posA, sizeof( posA ) );
				boost::Crc64 crc64B = boost::crc64( (const void *) & posB, sizeof( posB ) );
				boost::Crc64 crc64C = boost::crc64( (const void *) & posC, sizeof( posC ) );

				OrkAssert( crc64A.crc0 != crc64B.crc0 );	// make sure we have no zero area faces
				OrkAssert( crc64A.crc0 != crc64C.crc0 );	// make sure we have no zero area faces
				OrkAssert( crc64B.crc0 != crc64C.crc0 );	// make sure we have no zero area faces

				float fsurfarea = (invtxB.mPos-invtxA.mPos).Mag()*(invtxC.mPos-invtxA.mPos).Mag()*0.5f;

				///////////////////////////////////////////////////
				// find the triangle pivot sweep vertex
				///////////////////////////////////////////////////

				int idx_this = (key==crc64A.crc0) ? 0 : (key==crc64B.crc0) ? 1 : (key==crc64C.crc0) ? 2 : -1;

				OrkAssert( idx_this != -1 );	// make sure we found a match

				/////////////////////////////////////
				// if we got here
				//  the we have the necessary data with which to 
				//	compute the vertex color from the texture
				//	we have:
				//		the texture itself
				//		all connected vertices and their UV's
				/////////////////////////////////////

				const int knumsamplesperface = 256;
				const float kfaceweight = 1.0f/float(knumsamplesperface);

				fvec3 AvgFaceColor(0.0f,0.0f,0.0f);
				for( int isamp=0; isamp<knumsamplesperface; isamp++ )
				{
					float fu = float(rand()%0xfff)/float(4096.0f);	// 0 .. 1
					float fv = float(rand()%0xfff)/float(4096.0f);	// 0 .. 1

					////////////////////////////////////////////
					// compute s and t from u and v

					const vertex* psrcA = 0;
					const vertex* psrcB = 0;
					const vertex* psrcC = 0;

					switch( idx_this )
					{
						case 0:		// invtxA is center
						{	
							psrcA = & invtxA;
							psrcB = & invtxB;
							psrcC = & invtxC;
							break;
						}
						case 1:		// invtxB is center
						{	
							psrcA = & invtxB;
							psrcB = & invtxC;
							psrcC = & invtxA;
							break; 
						}
						case 2:		// invtxC is center
						{	
							psrcA = & invtxC;
							psrcB = & invtxA;
							psrcC = & invtxB;
							break;
						}
					}
					
					const fvec2& vstA = psrcA->mUV[0].mMapTexCoord;
					const fvec2& vstB = psrcB->mUV[0].mMapTexCoord;
					const fvec2& vstC = psrcC->mUV[0].mMapTexCoord;

					fvec2 vst_AB = (vstB-vstA);
					fvec2 vst_AC = (vstC-vstA);

					fvec2 vst_S; vst_S.Lerp( vst_AB, vst_AC, fu );

					fvec2 vst_ST; vst_ST.Lerp( vstA, (vstA+vst_S), (fv*0.5f) );

					float fs = vst_ST.GetX();
					float ft = vst_ST.GetY();

					////////////////////////////////////////////

					int i_s = int( (fs)*float(Width-1) );
					int i_t = int( (ft)*float(Height-1) );
					
					int ipix = (i_t*Width)+i_s;

					CColor3 pixcolor;

					switch( BPP )
					{
						case 4:
						{
							U8* pixbase = (U8*) Data+(ipix*4);
							U8 red = pixbase[2];
							U8 grn = pixbase[1];
							U8 blu = pixbase[0];

							pixcolor.SetX( float(red)/255.0f );
							pixcolor.SetY( float(grn)/255.0f );
							pixcolor.SetZ( float(blu)/255.0f );
							break;
						}
						case 3:
						{
							U8* pixbase = (U8*) Data+(ipix*3);
							U8 red = pixbase[2];
							U8 grn = pixbase[1];
							U8 blu = pixbase[0];

							pixcolor.SetX( float(red)/255.0f );
							pixcolor.SetY( float(grn)/255.0f );
							pixcolor.SetZ( float(blu)/255.0f );
							break;
						}
						default:
							OrkAssert( false );
						
					}

					AvgFaceColor += (pixcolor*kfaceweight);
				}
				AvgVertexColor += (AvgFaceColor*fsurfarea);
			}
		}

		AvgVertexColor = AvgVertexColor*(1.0f/ftotsurfacearea);

		VertexColorMap[ key ] = AvgVertexColor;
	}
			
	//////////////////////////////////////////////////
	// now we have computed all vertex colors, make a new mesh
	//////////////////////////////////////////////////
	MeshUtil::toolmesh outmesh;
	//////////////////////////////////////////////////

	int idxcache[3];
	for( int iface=0; iface<inumf; iface++ )
	{
		const poly& intri = inmesh.RefPoly(iface);
		int inumsides = intri.GetNumSides();
		OrkAssert( inumsides==3 );
		for( int iv=0; iv<inumsides; iv++ )
		{
			int idx = intri.GetVertexID(iv);
			idxcache[ (iv%3) ] = idx;
			if( 2 == (iv%3) )
			{
				vertex tv0 = inmesh.RefVertexPool().GetVertex( idxcache[0] );
				vertex tv1 = inmesh.RefVertexPool().GetVertex( idxcache[1] );
				vertex tv2 = inmesh.RefVertexPool().GetVertex( idxcache[2] );
				const fvec3& posA = tv0.mPos;
				const fvec3& posB = tv1.mPos;
				const fvec3& posC = tv2.mPos;
				boost::Crc64 crc64A = boost::crc64( (const void *) & posA, sizeof( posA ) );
				boost::Crc64 crc64B = boost::crc64( (const void *) & posB, sizeof( posB ) );
				boost::Crc64 crc64C = boost::crc64( (const void *) & posC, sizeof( posC ) );

				CColor3 clrA = VertexColorMap[ crc64A.crc0 ];
				CColor3 clrB = VertexColorMap[ crc64B.crc0 ];
				CColor3 clrC = VertexColorMap[ crc64C.crc0 ];

				tv0.mCol[0] = fvec4(clrA);
				tv1.mCol[0] = fvec4(clrB);
				tv2.mCol[0] = fvec4(clrC);

				int imapped0 = outmesh.MergeVertex( tv0 );
				int imapped1 = outmesh.MergeVertex( tv1 );
				int imapped2 = outmesh.MergeVertex( tv2 );

				poly toolpoly( imapped0, imapped1, imapped2 );
				outmesh.MergePoly( toolpoly );
			}
		}
	}

	//////////////////////////////////////////////////
	outmesh.WriteAuto( ttv_out.c_str() );
	//////////////////////////////////////////////////

	#endif
}

///////////////////////////////////////////////////////////////////////////////
}}
///////////////////////////////////////////////////////////////////////////////
#endif
