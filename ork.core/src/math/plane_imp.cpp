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
#include <ork/file/chunkfile.hpp>

namespace ork {


template< typename T>
bool TPlane<T>::PlaneIntersect( const TPlane<T>& oth, TVector3<T>& outpos, TVector3<T>& outdir )
{
    outdir = GetNormal().Cross( oth.GetNormal() );
    T num = outdir.MagSquared();
	TVector3<T> c1 = (GetD()*oth.GetNormal()) + (oth.GetD()*GetNormal());
	outpos = c1.Cross( outdir ) * T(1.0)/num;
    return true; 
}

///////////////////////////////////////////////////////////////////////////////

template<> float TPlane<float>::Abs( float in )
{
	return CFloat::Abs( in );
}
template<> float TPlane<float>::Epsilon()
{
	return CFloat::Epsilon();
}

///////////////////////////////////////////////////////////////////////////////

template<> double TPlane<double>::Abs( double in )
{
	return std::abs( in );
}
template<> double TPlane<double>::Epsilon()
{
	return double(CFloat::Epsilon())*0.0001;
}

///////////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////


}

template class ork::TPlane<float>;		// explicit template instantiation
template class ork::TPlane<double>;		// explicit template instantiation

//template class ork::chunkfile::Reader<ork::lev2::CollisionLoadAllocator>;

