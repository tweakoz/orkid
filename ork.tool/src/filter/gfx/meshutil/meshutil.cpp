////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <orktool/orktool_pch.h>
#include <ork/math/plane.h>
#include <orktool/filter/gfx/meshutil/meshutil.h>
#include <ork/kernel/orklut.hpp>

template class ork::orklut<std::string,ork::MeshUtil::submesh*>;

namespace ork { namespace MeshUtil {

const vertexpool vertexpool::EmptyPool;

/////////////////////////////////////////////////////////////////////////

submesh::submesh(const vertexpool& vpool) 
	: mvpool(vpool)
	, mfSurfaceArea(0)
	, mbMergeEdges(true)
{	
	//mMergedPolys.reserve(32<<10);
	//if( mbMergeEdges )
	{
		//mEdges.reserve(32<<10);
	}
	for( int i=0; i<kmaxsidesperpoly; i++ )
	{	mPolyTypeCounter[i] = 0;
	}
}

submesh::~submesh()
{
	static size_t gc1 = 0;
	static size_t gc2 = 0;
	static size_t gc3 = 0;
	static size_t gc4 = 0;
	static size_t gc5 = 0;
	static size_t gc6 = 0;

	size_t ic1 = mpolyhashmap.size();
	size_t ic2 = mEdgeMap.size();
	size_t ic3 = mvpool.VertexPoolMap.size();
	size_t ic4 = mvpool.VertexPool.size();
	size_t ic5 = mEdges.size();
	size_t ic6 = mMergedPolys.size();
	gc1 += ic1;
	gc2 += ic2;
	gc3 += ic3;
	gc4 += ic4;
	gc5 += ic5;
	gc6 += ic6;
	size_t is1 = sizeof(std::pair<U64,int>);
	size_t is2 = sizeof(std::pair<int,int>);
	size_t is3 = is1;
	size_t is4 = sizeof(vertex);
	size_t is5 = sizeof(edge);
	size_t is6 = sizeof(poly);

	//orkprintf( "///////////////////////////////////\n" );
	//orkprintf( "polyhash cnt<%d:%d> tot<%d:%d>\n", ic1,ic1*is1, gc1,gc1*is1 );
	//orkprintf( "polys cnt<%d:%d> tot<%d:%d>\n", ic6,ic6*is6, gc6,gc6*is6 );
	//orkprintf( "edgemap cnt<%d:%d> tot<%d:%d>\n", ic2,ic2*is2, gc2,gc2*is2 );
	//orkprintf( "edges cnt<%d:%d> tot<%d:%d>\n", ic5,ic5*is5, gc5,gc5*is5 );
	//orkprintf( "vpoolmap cnt<%d:%d> tot<%d:%d>\n", ic3,ic3*is3, gc3,gc3*is3 );
	//orkprintf( "vpool cnt<%d:%d> tot<%d:%d>\n", ic4,ic4*is4, gc4,gc4*is4 );
	//orkprintf( "///////////////////////////////////\n" );
}

/////////////////////////////////////////////////////////////////////////

toolmesh::toolmesh()
	: mbMergeEdges(true)
{
}

///////////////////////////////////////////////////////////////////////////////

toolmesh::~toolmesh()
{
	int icnt = int(mPolyGroupLut.size());
	for( int i=0; i<icnt; i++ )
	{
		submesh* psub  = mPolyGroupLut.GetIterAtIndex(i)->second;

		if( psub )
		{
			delete psub;
		}
	}
}

///////////////////////////////////////////////////////////////////////////////

void toolmesh::Prune()
{
	orkset<std::string> SubsToPrune;
	for( orklut<std::string, submesh*>::const_iterator it=mPolyGroupLut.begin(); it!=mPolyGroupLut.end(); it++ )
	{	const submesh& src_grp = *it->second;
		const std::string& name = it->first;
		int inump = src_grp.GetNumPolys();
		if( 0 == inump )
		{
			SubsToPrune.insert(name);
		}
	}
	while(SubsToPrune.empty()==false)
	{
		const std::string& name = *SubsToPrune.begin();
		RemoveSubMesh( name );
		SubsToPrune.erase(SubsToPrune.begin());
	}
}

///////////////////////////////////////////////////////////////////////////////

void toolmesh::Dump( const std::string& comment ) const
{	if(1) return;//

	static int icnt=0;
	std::string fname = CreateFormattedString("tmeshout%d.txt", icnt );
	icnt++;
	FILE* fout = fopen(fname.c_str(),"wt");
	fprintf( fout, "////////////////////////////////////////////////\n" );
	fprintf( fout, "////////////////////////////////////////////////\n" );
	fprintf( fout, "// toolmesh dump<%s>\n", comment.c_str() );
	fprintf( fout, "////////////////////////////////////////////////\n" );
	fprintf( fout, "////////////////////////////////////////////////\n" );
	for(	orkmap<std::string,std::string>::const_iterator 
			itm=mShadingGroupToMaterialMap.begin();
			itm!=mShadingGroupToMaterialMap.end();
			itm++ )
	{	const std::string& key = itm->first;
		const std::string& val = itm->second;
		fprintf( fout, "// toolmesh::shadinggroup<%s> material<%s>\n", key.c_str(), val.c_str() );
	}
	fprintf( fout, "////////////////////////////////////////////////\n" );
	for(	orkmap<std::string,std::string>::const_iterator 
			itm=mAnnotations.begin();
			itm!=mAnnotations.end();
			itm++ )
	{	const std::string& key = itm->first;
		const std::string& val = itm->second;
		fprintf( fout, "// toolmesh::annokey<%s> annoval<%s>\n", key.c_str(), val.c_str() );
	}
	fprintf( fout, "////////////////////////////////////////////////\n" );
	for( orklut<std::string, submesh*>::const_iterator it=mPolyGroupLut.begin(); it!=mPolyGroupLut.end(); it++ )
	{	const submesh& src_grp = *it->second;
		const std::string& name = it->first;
		int inump = src_grp.GetNumPolys();
		fprintf( fout, "// toolmesh::polygroup<%s> numpolys<%d>\n", name.c_str(), inump );
		const submesh::AnnotationMap& subannos = src_grp.RefAnnotations();
		for(	orkmap<std::string,std::string>::const_iterator 
				itm=subannos.begin();
				itm!=subannos.end();
				itm++ )
		{	const std::string& key = itm->first;
			const std::string& val = itm->second;
			fprintf( fout, "//		submesh::annokey<%s> annoval<%s>\n", key.c_str(), val.c_str() );
		}
		orkset<const AnnoMap*>	annosets;
		for( int ip=0; ip<inump; ip++ )
		{	const poly& ply = src_grp.RefPoly(ip);
			const AnnoMap* amap = ply.GetAnnoMap();
			if(amap) annosets.insert(amap);
		}
		for( orkset<const AnnoMap*>::const_iterator it2=annosets.begin(); it2!=annosets.end(); it2++ )
		{	const AnnoMap* amapp = (*it2);
			const orkmap<std::string,std::string>& amap	= amapp->mAnnotations;
			for( orkmap<std::string,std::string>::const_iterator it3=amap.begin(); it3!=amap.end(); it3++ )
			{	const std::string& key = it3->first;
				const std::string& val = it3->second;
				fprintf( fout, "//			submeshpoly::annokey<%s> annoval<%s>\n", key.c_str(), val.c_str() );
				
			}
		}
		for( int ip=0; ip<inump; ip++ )
		{
			fprintf( fout, "poly<%d> <", ip );
			const poly& ply = src_grp.RefPoly(ip);
			int inumv = ply.GetNumSides();
			for( int iv=0; iv<inumv; iv++ )
			{	
				int i0 = iv;
				int i1 = (iv+1)%inumv;
				int iv0 = ply.GetVertexID(i0);
				int iv1 = ply.GetVertexID(i1);
				edge Edge(iv0,iv1);
				u64 ue = src_grp.GetEdgeBetween(iv0,iv1);
				u32 ue0 = u32(ue&0xffffffff);
				u32 ue1 = u32((ue>>32)&0xffffffff);

				fprintf( fout, "%d:%08x%08x ", ply.GetVertexID(iv), ue0, ue1 );
			}
			fprintf( fout, ">\n" );
		}




	}
	fprintf( fout, "////////////////////////////////////////////////\n" );
	fclose(fout);
}

///////////////////////////////////////////////////////////////////////////////

void toolmesh::CopyMaterialsFromToolMesh( const toolmesh& from )
{	mMaterialsByShadingGroup = from.mMaterialsByShadingGroup;
	mMaterialsByName = from.mMaterialsByName;
	mShadingGroupToMaterialMap = from.mShadingGroupToMaterialMap;
}

///////////////////////////////////////////////////////////////////////////////

void toolmesh::SetAnnotation( const char* annokey, const char* annoval )
{	std::string aval = "";
	if( annoval != 0 ) aval = annoval;
	mAnnotations[ std::string(annokey) ] = aval;
}
void submesh::SetAnnotation( const char* annokey, const char* annoval )
{	std::string aval = "";
	if( annoval != 0 ) aval = annoval;
	mAnnotations[ std::string(annokey) ] = aval;
}


///////////////////////////////////////////////////////////////////////////////

const char* toolmesh::GetAnnotation( const char* annokey ) const
{	static const char* defret( "" );
	orkmap<std::string,std::string>::const_iterator it = mAnnotations.find( std::string(annokey) );
	if( it != mAnnotations.end() )
	{	return (*it).second.c_str();
	}
	return defret;
}
const char* submesh::GetAnnotation( const char* annokey ) const
{	static const char* defret( "" );
	orkmap<std::string,std::string>::const_iterator it = mAnnotations.find( std::string(annokey) );
	if( it != mAnnotations.end() )
	{	return (*it).second.c_str();
	}
	return defret;
}

///////////////////////////////////////////////////////////////////////////////

void submesh::SplitOnAnno( toolmesh& out, const std::string& annokey ) const
{	int inumpolys = (int) mMergedPolys.size();
	for( int ip=0; ip<inumpolys; ip++ )
	{	const poly& ply = mMergedPolys[ ip ];
		std::string pgroup = ply.GetAnnotation( annokey );
		if( pgroup!="" )
		{	submesh& sub = out.MergeSubMesh(pgroup.c_str());
			int inumpv = ply.GetNumSides();
			poly NewPoly;
			NewPoly.miNumSides = inumpv;
			for( int iv=0; iv<inumpv; iv++ )
			{	int ivi = ply.GetVertexID(iv);
				const vertex& vtx = RefVertexPool().GetVertex( ivi );
				int inewvi = sub.MergeVertex( vtx );
				NewPoly.miVertices[iv] = inewvi;
			}
			NewPoly.SetAnnoMap(ply.GetAnnoMap());
			sub.MergePoly(NewPoly);
		}
	}
}


///////////////////////////////////////////////////////////////////////////////

void submesh::MergeAnnos( const AnnotationMap& mrgannos, bool boverwrite )
{
	for( AnnotationMap::const_iterator it=mrgannos.begin(); it!=mrgannos.end(); it++ )
	{
		const std::string& key = it->first;
		const std::string& val = it->second;

		AnnotationMap::iterator itf=mAnnotations.find(key);
		if( itf == mAnnotations.end() )
		{
			mAnnotations[key]=val;
		}
		else if( boverwrite )
		{
			itf->second = val;
		}
	}
}

///////////////////////////////////////////////////////////////////////////////

void submesh::SplitOnAnno( toolmesh& out, const std::string& prefix, const std::string& annokey ) const
{	int inumpolys = (int) mMergedPolys.size();
	AnnotationMap merge_annos;
	merge_annos["SplitPrefix"] = prefix;
	for( int ip=0; ip<inumpolys; ip++ )
	{	const poly& ply = mMergedPolys[ ip ];
		std::string pgroup = ply.GetAnnotation( annokey );
		std::string merged_name = prefix+std::string("_")+pgroup;
		if( pgroup!="" )
		{	submesh& sub = out.MergeSubMesh(merged_name.c_str(),merge_annos);
			int inumpv = ply.GetNumSides();
			poly NewPoly;
			NewPoly.miNumSides = inumpv;
			for( int iv=0; iv<inumpv; iv++ )
			{	int ivi = ply.GetVertexID(iv);
				const vertex& vtx = RefVertexPool().GetVertex( ivi );
				int inewvi = sub.MergeVertex( vtx );
				NewPoly.miVertices[iv] = inewvi;
			}
			NewPoly.SetAnnoMap(ply.GetAnnoMap());
			sub.MergePoly(NewPoly);
		}
	}
}

///////////////////////////////////////////////////////////////////////////////

void submesh::SplitOnAnno( toolmesh& out, const std::string& annokey, const AnnotationMap& mrgannos ) const
{	int inumpolys = (int) mMergedPolys.size();
	for( int ip=0; ip<inumpolys; ip++ )
	{	const poly& ply = mMergedPolys[ ip ];
		std::string pgroup = ply.GetAnnotation( annokey );
		if( pgroup!="" )
		{	submesh& sub = out.MergeSubMesh(pgroup.c_str());
			sub.MergeAnnos( mrgannos, true );
			int inumpv = ply.GetNumSides();
			poly NewPoly;
			NewPoly.miNumSides = inumpv;
			for( int iv=0; iv<inumpv; iv++ )
			{	int ivi = ply.GetVertexID(iv);
				const vertex& vtx = RefVertexPool().GetVertex( ivi );
				int inewvi = sub.MergeVertex( vtx );
				NewPoly.miVertices[iv] = inewvi;
			}
			NewPoly.SetAnnoMap(ply.GetAnnoMap());
			sub.MergePoly(NewPoly);
		}
	}
}

///////////////////////////////////////////////////////////////////////////////

void submesh::ImportPolyAnnotations( const annopolylut& apl )
{	int inumpolys = (int) mMergedPolys.size();
	for( int ip=0; ip<inumpolys; ip++ )
	{	poly& ply = mMergedPolys[ ip ];
		const AnnoMap* amap = apl.Find( *this, ply );
		if( amap )
		{	ply.SetAnnoMap( amap );
		}
	}
}	

///////////////////////////////////////////////////////////////////////////////

void submesh::ExportPolyAnnotations( annopolylut& apl ) const
{	int inumpolys = (int) mMergedPolys.size();
	for( int ip=0; ip<inumpolys; ip++ )
	{	const poly& ply = mMergedPolys[ ip ];
		U64 uhash = apl.HashItem(*this,ply);
		const AnnoMap* amap = ply.GetAnnoMap();
		apl.mAnnoMap[uhash] = amap;
	}
}

///////////////////////////////////////////////////////////////////////////////

const AABox& submesh::GetAABox() const 
{	if( mAABoxDirty )
	{	mAABox.BeginGrow();
		int inumvtx = (int) RefVertexPool().GetNumVertices();
		for( int i=0; i<inumvtx; i++ )
		{	const vertex& v = RefVertexPool().GetVertex(i);
			mAABox.Grow(v.mPos);
		}
		mAABox.EndGrow();
		mAABoxDirty = false;
	}
	return mAABox;
}

///////////////////////////////////////////////////////////////////////////////

AABox toolmesh::GetAABox() const 
{	AABox outp;
	outp.BeginGrow();
	int inumsub = (int) mPolyGroupLut.size();
	for( int is=0; is<inumsub; is++ )
	{	const submesh& sub = *mPolyGroupLut.GetItemAtIndex(is).second;
		outp.Grow( sub.GetAABox().Min() );
		outp.Grow( sub.GetAABox().Max() );
	}
	outp.EndGrow();
	return outp;
}

///////////////////////////////////////////////////////////////////////////////

const edge& submesh::RefEdge( U64 edgekey ) const 
{	OrkAssert( edgekey != poly::Inv );
	HashU64IntMap::const_iterator it = mEdgeMap.find( edgekey );
	OrkAssert( it != mEdgeMap.end() );
	int index = it->second;
	OrkAssert( index < int(mEdges.size()) );
	return mEdges[ index ];
}

///////////////////////////////////////////////////////////////////////////////

int submesh::MergeVertex( const vertex & vtx, int idx )
{	mAABoxDirty = true;
	return mvpool.MergeVertex(vtx,idx);
}

///////////////////////////////////////////////////////////////////////////////

poly & submesh::RefPoly( int i )
{	OrkAssert( orkvector<int>::size_type(i) < mMergedPolys.size() );
	return mMergedPolys[i];
}

///////////////////////////////////////////////////////////////////////////////

const poly & submesh::RefPoly( int i ) const
{	OrkAssert( orkvector<int>::size_type(i) < mMergedPolys.size() );
	return mMergedPolys[i];
}

///////////////////////////////////////////////////////////////////////////////

const orkvector<poly>& submesh::RefPolys() const
{	return mMergedPolys;
}

///////////////////////////////////////////////////////////////////////////////

const orklut<std::string, submesh*>& toolmesh::RefSubMeshLut() const
{	return mPolyGroupLut;
}

///////////////////////////////////////////////////////////////////////////////

/*const orkvector<submesh *> &toolmesh::RefPolyGroupByPolyIndex() const
{
	return mPolyGroupByPolyIndex;
}*/

/////////////////////////////////////////////////////////////////////////

void submesh::FindNSidedPolys( orkvector<int>& output, int inumsides ) const
{	int inump = (int) mMergedPolys.size();
	for( int i=0; i<inump; i++ )
	{	const poly & ply = RefPoly(i);
		if( ply.GetNumSides() == inumsides )
		{	output.push_back(i);
		}
	}
}

///////////////////////////////////////////////////////////////////////////////

int	submesh::GetNumPolys( int inumsides ) const
{	int iret = 0;
	if( 0 == inumsides )
	{	iret = (int) mMergedPolys.size();
	}
	else
	{	OrkAssert( inumsides < kmaxsidesperpoly );
		iret = mPolyTypeCounter[inumsides];
	}
	return iret;
}

///////////////////////////////////////////////////////////////////////////////

const submesh * toolmesh::FindSubMeshFromMaterialName( const std::string& materialname ) const
{	for( orkmap<std::string,std::string>::const_iterator it=mShadingGroupToMaterialMap.begin(); it!=mShadingGroupToMaterialMap.end(); it++ )
	{	if( materialname == it->second )
		{	const std::string& shgrpname = it->first;
			orklut<std::string, submesh*>::const_iterator itpg=mPolyGroupLut.find(shgrpname);
			if( itpg != mPolyGroupLut.end() )
			{	return itpg->second;
			}
		}
	}
	return 0;
}

///////////////////////////////////////////////////////////////////////////////

submesh * toolmesh::FindSubMeshFromMaterialName( const std::string& materialname )
{	for( orkmap<std::string,std::string>::const_iterator it=mShadingGroupToMaterialMap.begin(); it!=mShadingGroupToMaterialMap.end(); it++ )
	{	if( materialname == it->second )
		{	const std::string& shgrpname = it->first;
			orklut<std::string, submesh*>::iterator itpg=mPolyGroupLut.find(shgrpname);
			if( itpg != mPolyGroupLut.end() )
			{	return itpg->second;
			}
		}
	}
	return 0;
}

///////////////////////////////////////////////////////////////////////////////

const submesh* toolmesh::FindSubMesh(const std::string& polygroupname ) const
{	orklut<std::string,submesh*>::const_iterator it=mPolyGroupLut.find(polygroupname);
	return (it==mPolyGroupLut.end()) ? 0 : it->second;
}

///////////////////////////////////////////////////////////////////////////////

submesh* toolmesh::FindSubMesh(const std::string& polygroupname )
{	orklut<std::string,submesh*>::iterator it=mPolyGroupLut.find(polygroupname);
	return (it==mPolyGroupLut.end()) ? 0 : it->second;
}

///////////////////////////////////////////////////////////////////////////////

void submesh::GetEdges( const poly& ply, orkvector<edge>& Edges ) const
{	int icnt = 0;
	int icntf = 0;
	for( int is=0; is<ply.GetNumSides(); is++ )
	{	U64 ue = ply.mEdges[is];
		HashU64IntMap::const_iterator it = mEdgeMap.find( ue );
		if( it != mEdgeMap.end() )
		{	int ie = it->second;
			Edges.push_back( mEdges[ie] );
			icntf++;
		}
		icnt++;
	}
}

///////////////////////////////////////////////////////////////////////////////

void submesh::GetAdjacentPolys( int ply, orkset<int>& output ) const
{	orkvector<edge> edges;
	GetEdges(RefPoly(ply), edges);
	for (orkvector<edge>::const_iterator edgeIter = edges.begin(); edgeIter != edges.end(); edgeIter++)
	{	orkset<int> connectedPolys;
		GetConnectedPolys(*edgeIter,connectedPolys);
		for( orkset<int>::const_iterator it2=connectedPolys.begin(); it2!=connectedPolys.end(); it2++ )
		{	int  ic = *it2;
			if( ic != ply )
			{	output.insert(connectedPolys.begin(), connectedPolys.end());
			}
		}
	}
}

///////////////////////////////////////////////////////////////////////////////

const U64 submesh::GetEdgeBetween( int aind, int bind) const
{	const poly& a = RefPoly(aind);
	const poly& b = RefPoly(bind);
	for(int eaind = 0 ; eaind < a.miNumSides ; eaind++)
		for(int ebind = 0 ; ebind < b.miNumSides ; ebind++)
			if (a.mEdges[eaind] == b.mEdges[ebind])
				return a.mEdges[eaind];
	return poly::Inv;
}

///////////////////////////////////////////////////////////////////////////////

void submesh::GetConnectedPolys( const edge & ed, orkset<int>& output ) const
{	U64 keyA = ed.GetHashKey();
	HashU64IntMap::const_iterator itfind = mEdgeMap.find( keyA );
	if( itfind != mEdgeMap.end() )
	{	int ie = itfind->second;
		const edge & edfound = 	mEdges[ ie ];
		int inump = edfound.GetNumConnectedPolys();
		for( int ip=0; ip<inump; ip++ )
		{	int ipi = edfound.GetConnectedPoly( ip );
			output.insert( ipi );
		}
	}
}

///////////////////////////////////////////////////////////////////////////////

void toolmesh::SetRangeTransform( const CVector4 & vscale, const CVector4 & vtrans )
{	CMatrix4 MatS, MatT;
	MatS.Scale( vscale.GetX(), vscale.GetY(), vscale.GetZ() );
	MatT.SetTranslation( vtrans.GetX(), vtrans.GetY(), vtrans.GetZ() );
	mMatRange = MatS*MatT;
}

///////////////////////////////////////////////////////////////////////////////

void toolmesh::RemoveSubMesh( const std::string& pgroup )
{	orklut<std::string, submesh*>::iterator it = mPolyGroupLut.find( pgroup );
	if( it != mPolyGroupLut.end() )
	{
		submesh* psub = it->second;	
		mPolyGroupLut.erase(it);
		delete psub;
	}
}

///////////////////////////////////////////////////////////////////////////////

static const std::string gnomatch("");
const std::string& AnnoMap::GetAnnotation( const std::string& annoname ) const
{	orkmap<std::string,std::string>::const_iterator it = mAnnotations.find(annoname);
	if( it!=mAnnotations.end() )
	{	return it->second;
	}
	return gnomatch;
}

///////////////////////////////////////////////////////////////////////////////

U64 annopolyposlut::HashItem( const submesh& tmesh, const poly& ply ) const
{	boost::Crc64 crc64;
	crc64_init(crc64);
	int inumpv = ply.GetNumSides();
	for( int iv=0; iv<inumpv; iv++ )
	{	int ivi = ply.GetVertexID(iv);
		const vertex& vtx = tmesh.RefVertexPool().GetVertex( ivi );
		crc64_compute(crc64, &vtx.mPos, sizeof(vtx.mPos));
		crc64_compute(crc64, &vtx.mNrm, sizeof(vtx.mNrm));
	}	
	crc64_fin(crc64);
	return crc64.crc0;
}

///////////////////////////////////////////////////////////////////////////////

const AnnoMap* annopolylut::Find( const submesh& tmesh, const poly& ply ) const
{	const AnnoMap* rval = 0;
	U64 uhash = HashItem(tmesh,ply);
	orkmap<U64,const AnnoMap*>::const_iterator it = mAnnoMap.find(uhash);
	if( it != mAnnoMap.end() )
	{	rval = it->second;
	}
	return rval;
}

///////////////////////////////////////////////////////////////////////////////
/*
void SubMesh::GenIndexBuffers( void ) 
{
	int inumvtx = RefVertexPool().VertexPool.size();

	orkvector<int> TrianglePolyIndices;
	orkvector<int> QuadPolyIndices;

	FindNSidedPolys( TrianglePolyIndices, 3 );
	FindNSidedPolys( QuadPolyIndices, 4 );

	int inumtri( TrianglePolyIndices.size() );
	int inumquad( QuadPolyIndices.size() );

	mpBaseTriangleIndices = new U16[ inumtri*3 ];
	mpBaseQuadIndices = new U16[ inumquad*4 ];

	for( int itri=0; itri<inumtri; itri++ )
	{
		int iti = TrianglePolyIndices[itri];

		const poly & tri = RefPoly( iti );

		int i0 = tri.miVertices[0];
		int i1 = tri.miVertices[1];
		int i2 = tri.miVertices[2];

		OrkAssert( i0<inumvtx );
		OrkAssert( i1<inumvtx );
		OrkAssert( i2<inumvtx );

		mpBaseTriangleIndices[ (itri*3)+0 ] = U16(i0);
		mpBaseTriangleIndices[ (itri*3)+1 ] = U16(i1);
		mpBaseTriangleIndices[ (itri*3)+2 ] = U16(i2);
	}

	for( int iqua=0; iqua<inumquad; iqua++ )
	{
		int iqi = QuadPolyIndices[iqua];

		const poly & qu = RefPoly( iqi );

		int i0 = qu.miVertices[0];
		int i1 = qu.miVertices[1];
		int i2 = qu.miVertices[2];
		int i3 = qu.miVertices[3];

		OrkAssert( i0<inumvtx );
		OrkAssert( i1<inumvtx );
		OrkAssert( i2<inumvtx );
		OrkAssert( i3<inumvtx );

		mpBaseQuadIndices[ (iqua*4)+0 ] = U16(i0);
		mpBaseQuadIndices[ (iqua*4)+1 ] = U16(i1);
		mpBaseQuadIndices[ (iqua*4)+2 ] = U16(i2);
		mpBaseQuadIndices[ (iqua*4)+3 ] = U16(i3);
	}

}*/

///////////////////////////////////////////////////////////////////////////////

} } // namespace ork : MeshUtil
