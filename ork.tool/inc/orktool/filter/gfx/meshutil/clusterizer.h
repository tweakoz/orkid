
////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

///////////////////////////////////////////////////////////////////////////////

#include <ork/util/crc.h>
#include <ork/util/crc64.h>
#include <orktool/filter/filter.h>
#include <ork/math/cvector3.h>
#include <ork/math/cvector4.h>
#include <ork/math/box.h>
#include <algorithm>
#include <ork/kernel/Array.h>

#include <ork/lev2/gfx/gfxenv_enum.h>
#include <ork/lev2/gfx/gfxvtxbuf.h>
#include <ork/lev2/gfx/gfxmaterial.h>
#include <unordered_map>
#include <orktool/filter/gfx/meshutil/meshutil.h>

namespace ork::tool {

///////////////////////////////////////////////////////////////////////////////

struct XgmClusterTri
{
	MeshUtil::vertex Vertex[3];
};

///////////////////////////////////////////////////////////////////////////////

struct XgmClusterBuilder //: public ork::Object
{
//	RttiDeclareAbstract(XgmClusterBuilder,ork::Object);
//public:

	ork::MeshUtil::submesh			mSubMesh;
	lev2::VertexBufferBase*			mpVertexBuffer;
	//////////////////////////////////////////////////
	XgmClusterBuilder();
	virtual ~XgmClusterBuilder();
	//////////////////////////////////////////////////
	virtual bool AddTriangle( const XgmClusterTri& Triangle ) = 0;
	virtual void BuildVertexBuffer( const MeshUtil::ToolMaterialGroup& matgroup ) = 0;
	//////////////////////////////////////////////////
	void Dump( void );
	///////////////////////////////////////////////////////////////////
	// Build Vertex Buffers
	///////////////////////////////////////////////////////////////////

};

///////////////////////////////////////////////////////////////////////////////

struct XgmSkinnedClusterBuilder : public XgmClusterBuilder
{
//	RttiDeclareAbstract(XgmSkinnedClusterBuilder,XgmClusterBuilder);
//public:
	/////////////////////////////////////////////////
	const orkmap<std::string,int>& RefBoneRegMap() const { return mmBoneRegMap; }

	bool AddTriangle( const XgmClusterTri& Triangle ) final;
    void BuildVertexBuffer( const MeshUtil::ToolMaterialGroup& matgroup ) final; // virtual

	int FindNewBoneIndex( const std::string& BoneName );
	void BuildVertexBuffer_V12N12T8I4W4();
	void BuildVertexBuffer_V12N12B12T8I4W4();
	void BuildVertexBuffer_V12N6I1T4();

	orkmap<std::string,int>			mmBoneRegMap;
};

///////////////////////////////////////////////////////////////////////////////

class XgmRigidClusterBuilder : public XgmClusterBuilder
{
	//RttiDeclareAbstract(XgmRigidClusterBuilder,XgmClusterBuilder);
	/////////////////////////////////////////////////
	bool AddTriangle( const XgmClusterTri& Triangle ) final;
    void BuildVertexBuffer( const MeshUtil::ToolMaterialGroup& matgroup ) final;

	void BuildVertexBuffer_V12N6C2T4();
	void BuildVertexBuffer_V12N12B12T8C4();
	void BuildVertexBuffer_V12N12T16C4();
	void BuildVertexBuffer_V12N12B12T16();
};

///////////////////////////////////////////////////////////////////////////////

struct XgmClusterizer
{
	///////////////////////////////////////////////////////
	XgmClusterizer();
	virtual ~XgmClusterizer();
	///////////////////////////////////////////////////////
	virtual bool AddTriangle( const XgmClusterTri& Triangle, const MeshUtil::ToolMaterialGroup* cmg ) = 0;
	virtual void Begin() {}
	virtual void End() {}
	///////////////////////////////////////////////////////
	size_t GetNumClusters() const { return ClusterVect.size(); }
	XgmClusterBuilder* GetCluster(int idx) const { return ClusterVect[idx]; }

	orkvector< XgmClusterBuilder* > ClusterVect;
	///////////////////////////////////////////////////////
};

///////////////////////////////////////////////////////////////////////////////

struct XgmClusterizerDiced : public XgmClusterizer
{
	///////////////////////////////////////////////////////
	XgmClusterizerDiced();
	virtual ~XgmClusterizerDiced();
	///////////////////////////////////////////////////////
	bool AddTriangle( const XgmClusterTri& Triangle, const MeshUtil::ToolMaterialGroup* cmg );
	void Begin(); // virtual
	void End(); // virtual
	///////////////////////////////////////////////////////

	ork::MeshUtil::submesh mPreDicedMesh;
};

///////////////////////////////////////////////////////////////////////////////

struct XgmClusterizerStd : public XgmClusterizer
{
	XgmClusterizerStd();
	virtual ~XgmClusterizerStd();
	bool AddTriangle( const XgmClusterTri& Triangle, const MeshUtil::ToolMaterialGroup* cmg );
};

} // namespace ork:::ool {
