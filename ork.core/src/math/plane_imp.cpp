////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 


#include <ork/pch.h>

#include <ork/math/plane.h>
#include <ork/math/plane.hpp>
//#include <ork/math/gjk.h>
#include <ork/math/cmatrix4.h>
#include <ork/math/frustum.h>
#include <ork/file/chunkfile.h>
#include <ork/file/chunkfile.inl>

namespace ork {


template< typename T>
bool Plane<T>::PlaneIntersect( const Plane<T>& oth, Vector3<T>& outpos, Vector3<T>& outdir )
{
    outdir = GetNormal().Cross( oth.GetNormal() );
    T num = outdir.MagSquared();
	Vector3<T> c1 = (GetD()*oth.GetNormal()) + (oth.GetD()*GetNormal());
	outpos = c1.Cross( outdir ) * T(1.0)/num;
    return true; 
}

///////////////////////////////////////////////////////////////////////////////

template<> float Plane<float>::Abs( float in )
{
	return fabs( in );
}
template<> float Plane<float>::Epsilon()
{
	return Float::Epsilon();
}

///////////////////////////////////////////////////////////////////////////////

template<> double Plane<double>::Abs( double in )
{
	return fabs( in );
}
template<> double Plane<double>::Epsilon()
{
	return double(Float::Epsilon())*0.0001;
}

///////////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////


}

template class ork::Plane<float>;		// explicit template instantiation
template class ork::Plane<double>;		// explicit template instantiation

//template class ork::chunkfile::Reader<ork::lev2::CollisionLoadAllocator>;

