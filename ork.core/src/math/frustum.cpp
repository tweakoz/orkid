////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
//////////////////////////////////////////////////////////////// 


#include <ork/pch.h>
#include <ork/math/cvector2.h>
#include <ork/math/line.h>
#include <ork/math/plane.h>
#include <ork/math/frustum.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork {
///////////////////////////////////////////////////////////////////////////////

ffrustum dfrustum_to_ffrustum(const dfrustum& dvec){
    ffrustum rval;
    rval._nearPlane = dplane3_to_fplane3(dvec._nearPlane);
    rval._farPlane = dplane3_to_fplane3(dvec._farPlane);
    rval._leftPlane = dplane3_to_fplane3(dvec._leftPlane);
    rval._rightPlane = dplane3_to_fplane3(dvec._rightPlane);
    rval._topPlane = dplane3_to_fplane3(dvec._topPlane);
    rval._bottomPlane = dplane3_to_fplane3(dvec._bottomPlane);
    for( int i=0; i<4; i++ ){
        rval.mNearCorners[i] = dvec3_to_fvec3(dvec.mNearCorners[i]);
        rval.mFarCorners[i] = dvec3_to_fvec3(dvec.mFarCorners[i]);
    }
    rval.mCenter = dvec3_to_fvec3(dvec.mCenter);
    rval.mXNormal = dvec3_to_fvec3(dvec.mXNormal);
    rval.mYNormal = dvec3_to_fvec3(dvec.mYNormal);
    rval.mZNormal = dvec3_to_fvec3(dvec.mZNormal);
    return rval;
}
dfrustum ffrustum_to_dfrustum(const ffrustum& dvec){
    dfrustum rval;
    rval._nearPlane = fplane3_to_dplane3(dvec._nearPlane);
    rval._farPlane = fplane3_to_dplane3(dvec._farPlane);
    rval._leftPlane = fplane3_to_dplane3(dvec._leftPlane);
    rval._rightPlane = fplane3_to_dplane3(dvec._rightPlane);
    rval._topPlane = fplane3_to_dplane3(dvec._topPlane);
    rval._bottomPlane = fplane3_to_dplane3(dvec._bottomPlane);
    for( int i=0; i<4; i++ ){
        rval.mNearCorners[i] = fvec3_to_dvec3(dvec.mNearCorners[i]);
        rval.mFarCorners[i] = fvec3_to_dvec3(dvec.mFarCorners[i]);
    }
    rval.mCenter = fvec3_to_dvec3(dvec.mCenter);
    rval.mXNormal = fvec3_to_dvec3(dvec.mXNormal);
    rval.mYNormal = fvec3_to_dvec3(dvec.mYNormal);
    rval.mZNormal = fvec3_to_dvec3(dvec.mZNormal);
    return rval;
}

///////////////////////////////////////////////////////////////////////////////
}
///////////////////////////////////////////////////////////////////////////////

