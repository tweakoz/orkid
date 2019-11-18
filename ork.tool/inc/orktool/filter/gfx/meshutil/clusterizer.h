
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

namespace ork::MeshUtil {

///////////////////////////////////////////////////////////////////////////////

struct XgmClusterTri
{
	vertex _vertex[3];
};

///////////////////////////////////////////////////////////////////////////////

struct XgmClusterBuilder
{
	submesh			                _submesh;
	lev2::VertexBufferBase*			_vertexBuffer;
	//////////////////////////////////////////////////
	XgmClusterBuilder();
	virtual ~XgmClusterBuilder();
	//////////////////////////////////////////////////
	virtual bool AddTriangle( const XgmClusterTri& Triangle ) = 0;
	virtual void BuildVertexBuffer( const ToolMaterialGroup& matgroup ) = 0;
	//////////////////////////////////////////////////
	void Dump( void );
	///////////////////////////////////////////////////////////////////
	// Build Vertex Buffers
	///////////////////////////////////////////////////////////////////

};

///////////////////////////////////////////////////////////////////////////////

struct XgmSkinnedClusterBuilder : public XgmClusterBuilder
{
	/////////////////////////////////////////////////
	const orkmap<std::string,int>& RefBoneRegMap() const { return _boneRegisterMap; }

	bool AddTriangle( const XgmClusterTri& Triangle ) final;
    void BuildVertexBuffer( const ToolMaterialGroup& matgroup ) final; // virtual

	int FindNewBoneIndex( const std::string& BoneName );
	void BuildVertexBuffer_V12N12T8I4W4();
	void BuildVertexBuffer_V12N12B12T8I4W4();
	void BuildVertexBuffer_V12N6I1T4();

	orkmap<std::string,int>			_boneRegisterMap;
};

///////////////////////////////////////////////////////////////////////////////

class XgmRigidClusterBuilder : public XgmClusterBuilder
{
	/////////////////////////////////////////////////
	bool AddTriangle( const XgmClusterTri& Triangle ) final;
    void BuildVertexBuffer( const ToolMaterialGroup& matgroup ) final;

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
	virtual bool AddTriangle( const XgmClusterTri& Triangle, const ToolMaterialGroup* cmg ) = 0;
	virtual void Begin() {}
	virtual void End() {}
	///////////////////////////////////////////////////////
	size_t GetNumClusters() const { return _clusters.size(); }
	XgmClusterBuilder* GetCluster(int idx) const { return _clusters[idx]; }

	orkvector< XgmClusterBuilder* > _clusters;
	///////////////////////////////////////////////////////
};

///////////////////////////////////////////////////////////////////////////////

struct XgmClusterizerDiced : public XgmClusterizer
{
	///////////////////////////////////////////////////////
	XgmClusterizerDiced();
	virtual ~XgmClusterizerDiced();
	///////////////////////////////////////////////////////
	bool AddTriangle( const XgmClusterTri& Triangle, const ToolMaterialGroup* cmg );
	void Begin(); // virtual
	void End(); // virtual
	///////////////////////////////////////////////////////

	submesh _preDicedMesh;
};

///////////////////////////////////////////////////////////////////////////////

struct XgmClusterizerStd : public XgmClusterizer
{
	XgmClusterizerStd();
	virtual ~XgmClusterizerStd();
	bool AddTriangle( const XgmClusterTri& Triangle, const ToolMaterialGroup* cmg );
};

} // namespace ork:::ool {
