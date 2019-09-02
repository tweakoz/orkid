////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 


#ifndef _MATH_POLYGON_H
#define _MATH_POLYGON_H

#include <ork/math/cvector2.h>

///////////////////////////////////////////////////////////////////////////////

namespace ork { namespace meshutil {

///////////////////////////////////////////////////////////////////////////////

template <typename comp> class component_pool;
template <typename vtx> class edge;
template <typename vtx> class poly;
template <typename vtx> class mesh;

///////////////////////////////////////////////////////////////////////////////

class MeshComponentIndex
{
	int miComponentIndex;

public:

	static const int kinvalidcomponent = -1;

	void SetIndex( int idx ) { miComponentIndex=idx; }
	int GetIndex() const { return miComponentIndex; }

};

///////////////////////////////////////////////////////////////////////////////

class mesh_component
{
	int					miComponentIndex;

public:

	void SetComponentIndex( int idx ) { miComponentIndex=idx; }
	int GetComponentIndex() const { return miComponentIndex; }
};

///////////////////////////////////////////////////////////////////////////////

template <typename comp>
class component_pool //: public TRttiBase< component_pool<comp>, CObject >
{
	std::set<comp>			mComponentSet;
	orkvector<comp*>		mComponentIndexVect;

	/////////////////////////////////////////

public:

	component_pool<comp>();
	static void ClassInit();

	const comp& MergeComponent( const comp & c );
	inline int GetNu_components( void ) const { return int(mComponentSet.size()); }
	inline const comp& GetComponent( int index ) const { return *(mComponentIndexVect[index]); }
};

///////////////////////////////////////////////////////////////////////////////

template <typename vtx> 
class edge : public mesh_component
{
	const vtx*			mVertexA;
	const vtx*			mVertexB;
	const poly<vtx>*	mPolyL;
	const poly<vtx>*	mPolyR;

public:

	edge();
	edge( const vtx* iva, const vtx* ivb, const poly<vtx>* pl, const poly<vtx>*	pr );
	void ConnectToPoly( const poly<vtx>* p );
	
	int GetNumConnectedPolys() const;
	const poly<vtx>* GetLPoly() const;
	const poly<vtx>* GetRPoly() const;

	bool operator < (const edge<vtx> &other) const;

	const vtx* GetVertexA() const { return mVertexA; }
	const vtx* GetVertexB() const { return mVertexB; }

};


///////////////////////////////////////////////////////////////////////////////

template <typename vtx>
class poly : public mesh_component
{
	orkvector< const vtx* >	mVertices;
	int							miComponentIndex;

public:

	int GetNumSides( void ) const { return int( mVertices.size() ); }

	poly();
	const vtx& GetVertex( int idx ) const;
	void AddVertex( const vtx& v );
	
	bool operator < (const poly<vtx> &other) const;

	void SetComponentIndex( int idx ) { miComponentIndex=idx; }

	//vtx ComputeCenter( const vertexpool<vtx> &vpool ) const;
	//float ComputeEdgeLength( const vertexpool<vertex> &vpool, const CMatrix4 & MatRange, int iedge ) const;
	//float ComputeArea( const vertexpool &vpool, const CMatrix4 & MatRange ) const;
	//U64 HashIndices( void ) const;
};

///////////////////////////////////////////////////////////////////////////////

template <typename vtx>
struct mesh //: public TRttiBase< mesh<vtx>, CObject >
{
	component_pool< vtx >			mVertices;
	component_pool< poly<vtx> >		mPolys;
	component_pool< edge<vtx> >		mEdges;
	//CObject&						mRef;

public:

	static void ClassInit();

	mesh();

	const vtx& MergeVertex( const vtx& v );
	const poly<vtx>& MergePoly( const poly<vtx>& p );
	const edge<vtx>& MergeEdge( const edge<vtx>& e );

	inline int GetNumPolys( void ) const { return mPolys.GetNu_components(); }
	inline int GetNumEdges( void ) const { return mEdges.GetNu_components(); }
	inline int GetNumVertices( void ) const { return mVertices.GetNu_components(); }

	inline const vtx& GetVertex( int idx ) const { return mVertices.GetComponent(idx); }
	inline const poly<vtx>& GetPoly( int idx ) const { return mPolys.GetComponent(idx); }
	inline const edge<vtx>& GetEdge( int idx ) const { return mEdges.GetComponent(idx); }

};

///////////////////////////////////////////////////////////////////////////////

template <typename T> class circle
{
	typedef TVector2<T>	point_type;

    point_type		mCenter;
    T				mRadius;

public:

    circle();
    circle(const point_type& c, T r);

    const point_type& center() { return mCenter; }
    T radius() { return mRadius; }
    
	void Set(const point_type& c, T r );
    bool IsPointInside(const point_type& p ) const; // is a point inside the circle
    void CircumscribeFromTriangle( const point_type& p1, const point_type& p2, const point_type& p3);  // compute from 3 points
};

///////////////////////////////////////////////////////////////////////////////

template <typename meshadaptor> class DelaunayTriangulator
{
	//typedef typename meshadaptor::edgetype edgetype;

	meshadaptor&			mMeshAdaptor;
	int						mifacebindindex;
	int						miedgebindindex;

public:

	DelaunayTriangulator( meshadaptor& TheMeshAdaptor );

	void Triangulate();
	void FindClosestNeighbours( int& v0, int& v1 ) const;
	void CompleteTriangle();
	int FindEdge( int v0, int v1 ) const;
	int AddEdge( int v0, int v1, int il, int ir );
	void MergeLeftFace( int iedge, int v0, int v1 );
};

///////////////////////////////////////////////////////////////////////////////

} }

#endif
