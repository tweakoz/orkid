////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 


#include <ork/pch.h>
#include <ork/math/polygon.h>
#include <ork/util/crc64.h>

///////////////////////////////////////////////////////////////////////////////

#if 0
namespace ork {
namespace meshutil {

///////////////////////////////////////////////////////////////////////////////

//template <typename comp>
//const component_pool<comp> component_pool<comp>::EmptyPool;

///////////////////////////////////////////////////////////////////////////////

template <typename comp>
const comp& component_pool<comp>::MergeComponent( const comp& v )
{
	typename orkset<comp>::iterator it = mComponentSet.find(v);
	if( it != mComponentSet.end() ) // already in the set
	{
		return (*it);
	}
	else // inserting a component
	{
		std::pair<typename orkset<comp>::iterator,bool> pr = mComponentSet.insert(v);
	
		OrkAssert( pr.second == true );

		it = pr.first;

		mComponentIndexVect.push_back(0);

		int index = 0;

		for( typename orkset<comp>::iterator it2=mComponentSet.begin(); it2!=mComponentSet.end(); it2++ )
		{
			it2->SetComponentIndex(index);
			mComponentIndexVect[index] = &(*it2);
			index++;
		}
		//it = mComponentSet.find(v);
	}
	return *it;
}

///////////////////////////////////////////////////////////////////////////////

template <typename vtx>
edge<vtx>::edge()
	: mVertexA( 0 )
	, mVertexB( 0 )
	, mPolyL( 0 )
	, mPolyR( 0 )
{

}

template <typename vtx>
edge<vtx>::edge( const vtx* iva, const vtx* ivb, const poly<vtx>* pl, const poly<vtx>* pr )
	: mVertexA( iva )
	, mVertexB( ivb )
	, mPolyL( pl )
	, mPolyR( pr )
{
	
}

template <typename vtx>
bool edge<vtx>::operator < (const edge<vtx> &other) const
{
	struct CrcData
	{
		const vtx*			mVertexA;
		const vtx*			mVertexB;
		//const poly<vtx>*	mPolyL;
		//const poly<vtx>*	mPolyR;
	};

	CrcData cdata0;
	CrcData cdata1;
	
	cdata0.mVertexA = mVertexA;
	cdata0.mVertexB = mVertexB;
	//cdata0.mPolyL = mPolyL;
	//cdata0.mPolyR = mPolyR;

	cdata1.mVertexA = other.mVertexA;
	cdata1.mVertexB = other.mVertexB;
	//cdata1.mPolyL = other.mPolyL;
	//cdata1.mPolyR = other.mPolyR;

	boost::Crc64 crc64A = boost::crc64( (const void *) & cdata0, sizeof(cdata0) );
	boost::Crc64 crc64B = boost::crc64( (const void *) & cdata1, sizeof(cdata1) );
	return (crc64A<crc64B);
	
}

template <typename vtx>
const poly<vtx>* edge<vtx>::GetLPoly() const { return mPolyL; }

template <typename vtx>
const poly<vtx>* edge<vtx>::GetRPoly() const { return mPolyR; }

///////////////////////////////////////////////////////////////////////////////

template <typename vtx>
bool poly<vtx>::operator < (const poly<vtx> &other) const
{
	boost::Crc64 crc64A = boost::crc64( (const void *) & mVertices, sizeof(mVertices) );
	boost::Crc64 crc64B = boost::crc64( (const void *) & mVertices, sizeof(mVertices) );
	return (crc64A.crc0<crc64B.crc0);
}


///////////////////////////////////////////////////////////////////////////////

template <typename vtx>
const vtx& mesh<vtx>::MergeVertex( const vtx& v )
{
	return mVertices.MergeComponent( v );
}

template <typename vtx>
const poly<vtx>& mesh<vtx>::MergePoly( const poly<vtx>& p )
{
	return mPolys.MergeComponent( p );
}

template <typename vtx>
const edge<vtx>& mesh<vtx>::MergeEdge( const edge<vtx>& e )
{
	return mEdges.MergeComponent( e );
}

///////////////////////////////////////////////////////////////////////////////

template <typename T>
circle<T>::circle()
	: mCenter()
	, mRadius()
{

}

template <typename T>
circle<T>::circle(const point_type& c, T r)
	: mCenter(c)
	, mRadius(r)
{

}

template <typename T>
void circle<T>::set(const point_type& c, T r )
{
}

template <typename T>
bool circle<T>::IsPointInside(const point_type& p ) const // is a point inside the circle
{
	return ((mCenter-p).MagSquared() < (mRadius * mRadius));
}

template <typename T>
void circle<T>::CircumscribeFromTriangle( const point_type& p1, const point_type& p2, const point_type& p3)  // compute from 3 points
{
	point_type edge0 = (p2-p1);
	point_type edge1 = (p3-p1);

	T cp = edge0.PerpDot(edge1);

	if (cp != T(0.0))
	{
		T p1Sq = p1.x * p1.x + p1.y * p1.y;
		T p2Sq = p2.x * p2.x + p2.y * p2.y;
		T p3Sq = p3.x * p3.x + p3.y * p3.y;
		T num = p1Sq*(p2.y - p3.y) + p2Sq*(p3.y - p1.y) + p3Sq*(p1.y - p2.y);
		T cx = num / (T(2.0f) * cp);
		  num = p1Sq*(p3.x - p2.x) + p2Sq*(p1.x - p3.x) + p3Sq*(p2.x - p1.x);
		T cy = num / (T(2.0f) * cp);
		mCenter.setX(cx);
		mCenter.setY(cy);
	}
    mRadius = (mCenter-p1).Mag();
}

///////////////////////////////////////////////////////////////////////////////

template <typename meshadaptor>
DelaunayTriangulator<meshadaptor>::DelaunayTriangulator( meshadaptor& TheMeshAdaptor )
	: mMeshAdaptor( TheMeshAdaptor )
{

}

///////////////////////////////////////////////////////////////////////////////

template <typename vtx>
mesh<vtx>::mesh()
//	: mRef( mVertices )
{

}

/*template <typename vtx>
void mesh<vtx>::ClassInit()
{
	addProperty( new CObjectReferenceProp(	"Vertices",
												CProp::EFLAG_NONE,
												PROP_OFFSET(mesh<vtx>, mVertices),
												component_pool< vtx >::GetClassNameStatic() ) );
	
}*/

template <typename comp>
component_pool<comp>::component_pool()
{
}

template <typename comp>
void component_pool<comp>::ClassInit()
{
}

} }

#endif
