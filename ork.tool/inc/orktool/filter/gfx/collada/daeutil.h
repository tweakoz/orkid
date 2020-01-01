////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

class DaeDataSource
{
	int							miStride;
	int							miCount;
	//const float*				mData;
	uint32*						mpIndices;
	const FCDGeometrySource*	mSource;
	FCDGeometryPolygons *		mMatGroup;

public:

	DaeDataSource( const FCDGeometrySource* pcolladasrc=0, FCDGeometryPolygons* MatGroup=0 );
	void Bind( const FCDGeometrySource * pcolladasrc, FCDGeometryPolygons * MatGroup );
	int GetSourceIndex( int ifacevertbase, int ivertinface ) const;
	size_t GetDataSize() const;
	float GetData( int idx ) const;
	int GetStride() const;
};

bool ParseColladaMaterialBindings( FCDocument& daedoc, ork::MeshUtil::material_semanticmap_t& MatSemMap );
