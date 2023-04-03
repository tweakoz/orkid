////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/math/plane.hpp>
#include <ork/lev2/gfx/meshutil/submesh.h>
#include <ork/lev2/gfx/meshutil/meshutil.h>
#include <deque>

///////////////////////////////////////////////////////////////////////////////
namespace ork::meshutil {
///////////////////////////////////////////////////////////////////////////////

submesh_ptr_t submeshFromFrustum(const Frustum& frustum, bool projective_rect_uv){
  auto rval =std::make_shared<submesh>();

  auto dp_frustum = ffrustum_to_dfrustum(frustum);

    auto addq      = [&](dvec3 vtxa, dvec3 vtxb, dvec3 vtxc, dvec3 vtxd, dvec4 col) {
      auto normal = (vtxb - vtxa).crossWith(vtxc - vtxa).normalized();

      auto uva = dvec2(0,0);
      auto uvb = dvec2(1,0);
      auto uvc = dvec2(1,1);
      auto uvd = dvec2(0,1);

      if(projective_rect_uv){
        
        // https://www.reedbeta.com/blog/quadrilateral-interpolation-part-1/
        
        dray3 rac(vtxa,(vtxc-vtxa));
        dvec3 intersection;
        bool intersects = rac.intersectSegment(dlineseg3(vtxb,vtxd),intersection);
        OrkAssert(intersects);

        float da = (intersection-vtxa).length();
        float db = (intersection-vtxb).length();
        float dc = (intersection-vtxc).length();
        float dd = (intersection-vtxd).length();

        auto uvqa = dvec3(uva.x,uva.y,1) * ((da+dc)/dc);
        auto uvqb = dvec3(uvb.x,uvb.y,1) * ((db+dd)/dd);
        auto uvqc = dvec3(uvc.x,uvc.y,1) * ((dc+da)/da);
        auto uvqd = dvec3(uvd.x,uvd.y,1) * ((dd+db)/db);

        rval->addQuad(
          dvec3_to_fvec3(vtxa), 
          dvec3_to_fvec3(vtxb), dvec3_to_fvec3(vtxc), dvec3_to_fvec3(vtxd),
          dvec3_to_fvec3(normal), dvec3_to_fvec3(normal), dvec3_to_fvec3(normal), dvec3_to_fvec3(normal),
          dvec3_to_fvec3(uvqa), dvec3_to_fvec3(uvqb), dvec3_to_fvec3(uvqc), dvec3_to_fvec3(uvqd),
          dvec2_to_fvec2(uva), dvec2_to_fvec2(uvb), dvec2_to_fvec2(uvc), dvec2_to_fvec2(uvd),
          dvec4_to_fvec4(col));

      }
      else{
        rval->addQuad(
          dvec3_to_fvec3(vtxa), 
          dvec3_to_fvec3(vtxb), dvec3_to_fvec3(vtxc), dvec3_to_fvec3(vtxd),
          dvec3_to_fvec3(normal), dvec3_to_fvec3(normal), dvec3_to_fvec3(normal), dvec3_to_fvec3(normal),
          dvec2_to_fvec2(uva), dvec2_to_fvec2(uvb), dvec2_to_fvec2(uvc), dvec2_to_fvec2(uvd),
          dvec4_to_fvec4(col));
      }

    };

    const auto& NC = dp_frustum.mNearCorners;
    const auto& FC = dp_frustum.mFarCorners;
    auto NTL = NC[0];
    auto FTL = FC[0];
    auto NTR = NC[1];
    auto FTR = FC[1];
    auto NBR = NC[2];
    auto FBR = FC[2];
    auto NBL = NC[3];
    auto FBL = FC[3];

    dvec4 color = dvec4(1,1,1,1);

    addq(NBL, NBR, NTR, NTL, color);
    addq(FBL, FTL, FTR, FBR, color);

    addq(NTL, NTR, FTR, FTL, color);
    addq(NBR, NBL, FBL, FBR, color);

    addq(NTL, FTL, FBL, NBL, color);
    addq(NBR, FBR, FTR, NTR, color);
  return rval;
}


///////////////////////////////////////////////////////////////////////////////
} //namespace ork::meshutil {
///////////////////////////////////////////////////////////////////////////////

