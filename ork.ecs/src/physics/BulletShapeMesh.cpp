////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/kernel/orklut.hpp>

#include <ork/reflect/properties/registerX.inl>
#include <ork/reflect/properties/DirectTyped.hpp>
#include <ork/reflect/properties/DirectTypedMap.hpp>

#include <ork/math/basicfilters.h>

#include <ork/lev2/gfx/meshutil/meshutil.h>
#include <ork/lev2/gfx/meshutil/geometry.h> // street_spine artifact read (spine collider)
#include <ork/lev2/gfx/meshutil/submesh.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <rapidjson/document.h>
#include <ork/lev2/gfx/image.h>

#include <ork/ecs/scene.h>
#include <ork/ecs/entity.h>
#include <ork/ecs/entity.inl>

#include "bullet_impl.h"

ImplementReflectionX(ork::ecs::BulletShapeMeshData, "EcsBulletShapeMeshData");
ImplementReflectionX(ork::ecs::BulletShapeSpineData, "BulletShapeSpineData");

///////////////////////////////////////////////////////////////////////////////
namespace ork::ecs {
///////////////////////////////////////////////////////////////////////////////
static const bool USE_GIMPACT = false;
///////////////////////////////////////////////////////////////////////////////////////

void BulletShapeMeshData::describeX(object::ObjectClass* clazz) {
  clazz->directProperty("Scale", &BulletShapeMeshData::_scale);
  clazz->directProperty("MeshPath", &BulletShapeMeshData::_meshpath);

  // reflect::RegisterProperty("Model", &BulletShapeMeshData::GetModelAccessor, &BulletShapeMeshData::SetModelAccessor);
  // reflect::annotatePropertyForEditor<BulletShapeMeshData>("Model", "editor.class", "ged.factory.assetlist");
  // reflect::annotatePropertyForEditor<BulletShapeMeshData>("Model", "editor.assettype", "xgmodel");
  // reflect::annotatePropertyForEditor<BulletShapeMeshData>("Model", "editor.assetclass", "xgmodel");
}

///////////////////////////////////////////////////////////////////////////////

btTriangleIndexVertexArray* flatSubmeshToTriVertArray(meshutil::flatsubmesh_ptr_t flatsubmesh, const fvec3& scale, const fvec3& translation) {

  btTriangleIndexVertexArray* indexVertexArrays = new btTriangleIndexVertexArray;

  const auto& src_indices  = flatsubmesh->MergeTriIndices;

  btIndexedMesh btmesh;

  int inumindices  = src_indices.size();
  std::vector<uint32_t> Indices;

  for (int ii = 0; ii < inumindices; ii += 3) {

    uint32_t idx0 = src_indices[ii + 0];
    uint32_t idx1 = src_indices[ii + 1];
    uint32_t idx2 = src_indices[ii + 2];

    bool bOK = (idx0 != idx1) && (idx0 != idx2) && (idx1 != idx2);

    if (bOK) { // weed out degenerates
      Indices.push_back(idx0);
      Indices.push_back(idx1);
      Indices.push_back(idx2);
    }
  }

  OrkAssert((inumindices % 3) == 0);

  ////////////////////////////////////////////////////////
  // convert into flat array — sized to the KEPT indices. The old code allocated and
  // counted the ORIGINAL total while copying from the weeded vector: one degenerate
  // triangle => out-of-bounds reads => a garbage BVH TAIL (collision silently stops
  // partway through the mesh — the road-collider missing-arms bug, 2026-07-22).
  ////////////////////////////////////////////////////////

  const int nkept = int(Indices.size());
  if (nkept != inumindices)
    printf("flatSubmeshToTriVertArray: weeded %d degenerate tri(s) (%d -> %d indices)\n",
           (inumindices - nkept) / 3, inumindices, nkept);
  auto pnewU32 = new uint32_t[nkept];
  for (int i = 0; i < nkept; i++) {
    pnewU32[i] = Indices[i];
  }
  btmesh.m_triangleIndexBase   = (const uint8_t*)pnewU32;
  btmesh.m_triangleIndexStride = 3 * sizeof(uint32_t);
  btmesh.m_indexType           = PHY_INTEGER;
  btmesh.m_numTriangles        = nkept / 3;

  ////////////////////////////////////////////////////////

  lev2::EVtxStreamFormat efmt = flatsubmesh->evtxformat;
  OrkAssert(efmt==lev2::EVtxStreamFormat::V12N12B12T8C4);

  const auto& vertices = flatsubmesh->MergeVertsT8;
  int inumvertices = vertices.size();
  printf( "inumvertices<%d>\n", inumvertices );
  const int knfloats    = 9;
  btmesh.m_numVertices  = inumvertices;
  btScalar* pVERTS      = new btScalar[btmesh.m_numVertices * knfloats];
  btmesh.m_vertexBase   = (const unsigned char*)pVERTS;
  btmesh.m_vertexStride = knfloats * sizeof(btScalar);

  fvec3 DSC = scale;
  fvec3 DTR = translation;

  for (int i = 0; i < inumvertices; i++) {
    const auto& src_vtx = vertices[i];
    float fv                                    = src_vtx._uv.y;
    int j                                       = i * knfloats;
    pVERTS[j + 0]                               = src_vtx._position.x*DSC.x+DTR.x;
    pVERTS[j + 1]                               = src_vtx._position.y*DSC.y+DTR.y; 
    pVERTS[j + 2]                               = src_vtx._position.z*DSC.z+DTR.z;
    pVERTS[j + 3]                               = src_vtx._normal.x;
    pVERTS[j + 4]                               = src_vtx._normal.y;
    pVERTS[j + 5]                               = src_vtx._normal.z;
    pVERTS[j + 6]                               = src_vtx._binormal.x;
    pVERTS[j + 7]                               = src_vtx._binormal.y;
    pVERTS[j + 8]                               = src_vtx._binormal.z;
  }

  indexVertexArrays->addIndexedMesh(btmesh, PHY_INTEGER);

  return indexVertexArrays;
}

///////////////////////////////////////////////////////////////////////////////

btCollisionShape* flatSubmeshToBvhTriangleMeshShape(meshutil::flatsubmesh_ptr_t flatsubmesh,
                                                    const fvec3& scale, const fvec3& translation) {
  btTriangleIndexVertexArray* arrays = flatSubmeshToTriVertArray(flatsubmesh,scale,translation);

  btVector3 aabbMin, aabbMax;
  arrays->calculateAabbBruteForce(aabbMin, aabbMax);

  btBvhTriangleMeshShape* pshape = new btBvhTriangleMeshShape(
      arrays
      //    , /*useQuantizedAabbCompression=*/false, aabbMin, aabbMax);
      ,
      /*useQuantizedAabbCompression=*/true,
      aabbMin,
      aabbMax);
  // pshape->buildOptimizedBvh();

  pshape->getOptimizedBvh();

  printf("MeshAABB min<%f %f %f> max<%f %f %f>\n", //
         aabbMin.x(), aabbMin.y(), aabbMin.z(), //
         aabbMax.x(), aabbMax.y(), aabbMax.z());

  return pshape;
}

///////////////////////////////////////////////////////////////////////////////

btCompoundShape* meshToBvhTriangleCompoundShape(meshutil::flatsubmesh_ptr_t flatsubmesh, 
                                                decompxf_ptr_t xform,
                                                const fvec3& scale, const fvec3& translation ) {
  btCompoundShape* compoundShape = new btCompoundShape;
  btCollisionShape* shape        = flatSubmeshToBvhTriangleMeshShape(flatsubmesh,scale,translation);
  //xform->_uniformScale *= 2;
  xform = std::make_shared<DecompTransform>();
  auto mtx = xform->composed2();
  btTransform tr = orkmtx4tobtmtx4(mtx);
  compoundShape->addChildShape(tr, shape);
  return compoundShape;
}

///////////////////////////////////////////////////////////////////////////////////////

btCompoundShape* meshToBoxShape(::ork::meshutil::flatsubmesh_ptr_t mesh, decompxf_ptr_t xform) {
  auto aabox   = mesh->_aabox;
  auto xyz     = aabox.mMin;
  auto whd     = aabox.size();

  // Assumes center of box is at origin
  btCompoundShape* compoundShape = new btCompoundShape;
  //xform->_uniformScale *= 2;
  auto mtx = xform->composed2();
  btTransform tr = orkmtx4tobtmtx4(mtx);

  auto box_shape = new btBoxShape(btVector3(whd.x, whd.y, whd.z));

  compoundShape->addChildShape(tr, box_shape);

  return compoundShape;
}

///////////////////////////////////////////////////////////////////////////////

btSphereShape* meshToSphereShape(meshutil::flatsubmesh_ptr_t mesh, float fscale) {
  auto aabox   = mesh->_aabox;
  auto xyz     = aabox.mMin;
  auto whd     = aabox.size();
  auto sph     = Sphere(aabox);
  float radius = sph.mRadius;
  // Assumes center of sphere is at origin
  return new btSphereShape(radius * fscale);
}

///////////////////////////////////////////////////////////////////////////////////////

BulletShapeMeshData::BulletShapeMeshData() : _scale(1,1,1) {

    _shapeFactory._createShape = [=](const ShapeCreateData& data) -> BulletShapeBaseInst* {
      auto rval = new BulletShapeBaseInst(this);

      auto abs_path = _meshpath.toAbsolute();

      /////////////////////////////
      // todo : datablock based caching
      /////////////////////////////

      if( _flatmesh == nullptr ){
        if(_submesh){
          _flatmesh = std::make_shared<meshutil::FlatSubMesh>(*_submesh);
        }
        else{
          lev2::rendervar_strmap_t assetvars;
          _flatmesh = std::make_shared<meshutil::FlatSubMesh>(abs_path,assetvars);
        }
      }

      if (_flatmesh and _flatmesh->inumverts) {
        auto xform = data.mEntity->transform();
        rval->_collisionShape = meshToBvhTriangleCompoundShape(_flatmesh, xform, _scale, _translation);
        //rval->_collisionShape = meshToBoxShape(_flatmesh, xform);
      } else {
        float scale = _scale.magnitude();
        rval->_collisionShape = new btSphereShape(scale);
      }
      return rval;
    };
}

///////////////////////////////////////////////////////////////////////////////////////

BulletShapeMeshData::~BulletShapeMeshData() {
}

///////////////////////////////////////////////////////////////////////////////

#if 0


///////////////////////////////////////////////////////////////////////////////

btCollisionShape* XgmClusterToGimpactMeshShape(ork::lev2::xgmcluster_ptr_t xgmcluster, float fscale) {
  btCollisionShape* rval = 0;

  btTriangleIndexVertexArray* arrays = XgmClusterToTriVertArray(xgmcluster, fscale);

  btVector3 aabbMin, aabbMax;
  arrays->calculateAabbBruteForce(aabbMin, aabbMax);

  if (USE_GIMPACT) {
    /*btVector3 scale(fscale,fscale,fscale);
      btGImpactConvexDecompositionShape* pshape = new btGImpactConvexDecompositionShape(arrays,scale);
      pshape->updateBound();
      rval = pshape;*/
  } else {
    btGImpactMeshShape* pshape = new btGImpactMeshShape(arrays);
    rval                       = pshape;
  }
  //
  printf(
      "MeshAABB min<%f %f %f> max<%f %f %f>\n",
      aabbMin.x,
      aabbMin.y,
      aabbMin.z,
      aabbMax.x,
      aabbMax.y,
      aabbMax.z);

  return rval;
}

///////////////////////////////////////////////////////////////////////////////

btCompoundShape* meshToGimpactCompoundShape(const ork::lev2::XgmMesh* xgmmesh, float fscale) {
  btCompoundShape* compoundShape = new btCompoundShape;

  for (int sm = 0; sm < xgmmesh->numSubMeshes(); sm++) {
    const ork::lev2::XgmSubMesh* submesh = xgmmesh->subMesh(sm);
    for (int c = 0; c < submesh->GetNumClusters(); c++) {
      ork::lev2::xgmcluster_ptr_t xgmcluster = submesh->cluster(c);

      if (btCollisionShape* shape = XgmClusterToGimpactMeshShape(xgmcluster, fscale)) {
        btTransform tr;
        tr.setIdentity();
        compoundShape->addChildShape(tr, shape);
      }
    }
  }

  return compoundShape;
}

#endif

///////////////////////////////////////////////////////////////////////////////
// BulletShapeSpineData — the road WALKABLE RIBBON proxy (physics-proxy law).
// Reads the street_spine baked artifact (RouteSpine export) and sweeps a
// simplified surface: per segment a flat deck band (node width, lifted by
// _lift_m) + two shoulder bands crossfalling to the terrain-pinned outer chord
// (road_elev, unlifted — the O2 oracle holds road_elev == terrain at every
// node). Junction coverage v1 = the incident bands overlap across the apron
// (ring fans arrive with segment providers). ~6 tris/station — NEVER the
// render mesh.
///////////////////////////////////////////////////////////////////////////////

void BulletShapeSpineData::describeX(object::ObjectClass* clazz) {
  clazz->directProperty("spine_asset", &BulletShapeSpineData::_spine_asset);
  clazz->directProperty("ogeo_path", &BulletShapeSpineData::_ogeo_path);
  clazz->directProperty("shoulder_m", &BulletShapeSpineData::_shoulder_m);
  clazz->directProperty("lift_m", &BulletShapeSpineData::_lift_m);
  clazz->directProperty("ground_asset", &BulletShapeSpineData::_ground_asset);
}

BulletShapeSpineData::BulletShapeSpineData() {
  _shapeFactory._createShape = [=](const ShapeCreateData& data) -> BulletShapeBaseInst* {
    auto rval = new BulletShapeBaseInst(this);

    std::string path = _ogeo_path;
    if (path.empty()) {
      OrkAssert(not _spine_asset.empty());
      path = file::Path::expandPathString("<assetcache>/roads/" + _spine_asset + "/street_spine.ogeo");
    }
    if (not std::filesystem::exists(path)) {
      printf(
          "BulletShapeSpine: street_spine MISSING <%s> — the road graph must build with "
          "route_spine export_name set BEFORE this shape (declaration order = dependency "
          "order)\n",
          path.c_str());
      OrkAssert(false);
    }
    auto geo = meshutil::Geometry::readChunkfile(file::Path(path.c_str()));
    OrkAssert(geo);
    auto chP   = geo->_point.channelAs<fvec3>("P");
    auto chW   = geo->_point.channelAs<float>("width");
    auto chPar = geo->_point.channelAs<int>("parent");
    if (not chP or not chW or not chPar) {
      printf("BulletShapeSpine: <%s> missing P/width/parent channels (stale artifact? re-bake)\n", path.c_str());
      OrkAssert(false);
    }

    // GROUND PINNING: outer shoulder chords ride the ACTUAL terrain height — the same
    // baked EXR the terrain collider consumes. road_elev == terrain only ON the spine
    // (oracle O2); laterally it diverges, which left floating lips over low ground and
    // unreachable ribbons over fills (owner live-caught 2026-07-22). Sampling = the
    // scatter-parity convention: truncating nearest-texel, u = x/E + 0.5.
    std::vector<float> ground;
    int gdim   = 0;
    float gext = 0.0f;
    if (not _ground_asset.empty()) {
      std::string base  = file::Path::expandPathString("<assetcache>/terrain/" + _ground_asset);
      std::string hpath = base + "/height.exr";
      std::string mpath = base + "/" + _ground_asset + ".terrain.json";
      std::ifstream mf(mpath);
      if (not std::filesystem::exists(hpath) or not mf.good()) {
        printf(
            "BulletShapeSpine: ground_asset<%s> heightmap/manifest MISSING <%s> — the "
            "HeightField must materialize BEFORE this shape (declaration order = dependency order)\n",
            _ground_asset.c_str(), hpath.c_str());
        OrkAssert(false);
      }
      std::stringstream mstrm;
      mstrm << mf.rdbuf();
      rapidjson::Document doc;
      doc.Parse(mstrm.str().c_str());
      OrkAssert(not doc.HasParseError());
      gext     = doc["scale"]["extent_m"].GetFloat();
      auto img = lev2::Image::createFromFile(hpath.c_str());
      OrkAssert(img and img->_width > 0 and img->_bytesPerChannel == 4);
      gdim = int(img->_width);
      ground.resize(size_t(gdim) * gdim);
      for (int y = 0; y < gdim; y++)
        for (int x = 0; x < gdim; x++)
          ground[size_t(y) * gdim + x] = img->pixel32f(x, y)[0];
    }
    auto groundAt = [&](double x, double z, double fallback) -> double {
      if (ground.empty())
        return fallback;
      double u = x / double(gext) + 0.5, v = z / double(gext) + 0.5;
      int xi = std::min(std::max(int(u * gdim), 0), gdim - 1);
      int yi = std::min(std::max(int(v * gdim), 0), gdim - 1);
      return double(ground[size_t(yi) * gdim + xi]);
    };

    const size_t N = chP->_data.size();
    // DIRECT triangle soup — deliberately NOT the pooling submesh/FlatSubMesh path:
    // bullet needs no vertex welding, and the shared vertex-pool hash has a FILED
    // world-scale aliasing bug (Vector3::hash 21-bit packing floods x/y beyond
    // +-524.3m — silently welded distant verts and dropped whole road arms as
    // "duplicate" quads, 2026-07-22). The collider owns its own arrays; the global
    // hash fix is a separate adjudicated slice.
    auto verts   = new std::vector<btScalar>();
    auto indices = new std::vector<uint32_t>(); // heap: must outlive the shape (bullet references them)
    auto addq = [&](dvec3 q0, dvec3 q1, dvec3 q2, dvec3 q3) {
      uint32_t base = uint32_t(verts->size() / 3);
      for (const auto& q : {q0, q1, q2, q3}) {
        verts->push_back(btScalar(q.x));
        verts->push_back(btScalar(q.y));
        verts->push_back(btScalar(q.z));
      }
      const uint32_t quad[6] = {base, base + 1, base + 2, base, base + 2, base + 3};
      indices->insert(indices->end(), quad, quad + 6);
    };
    int nsegs = 0;
    for (size_t i = 0; i < N; i++) {
      int p = chPar->_data[i];
      if (p < 0 or size_t(p) >= N)
        continue; // root
      dvec3 a(chP->_data[i].x, chP->_data[i].y, chP->_data[i].z);
      dvec3 b(chP->_data[size_t(p)].x, chP->_data[size_t(p)].y, chP->_data[size_t(p)].z);
      dvec3 d = b - a;
      d.y     = 0.0;
      double len = d.magnitude();
      if (len < 1e-6)
        continue;
      d              = d * (1.0 / len);
      dvec3 side(d.z, 0.0, -d.x); // horizontal perpendicular
      double half = 0.5 * double(chW->_data[i]);
      double sh   = double(_shoulder_m);
      dvec3 lift(0.0, double(_lift_m), 0.0);
      // deck band (lifted)
      addq(a - side * half + lift, b - side * half + lift, //
           b + side * half + lift, a + side * half + lift);
      // shoulder bands: inner edge at the lifted deck rail; OUTER chord pinned to the
      // sampled terrain (sunk 5cm so entry is a ramp, never a knife-edge lip)
      dvec3 aL = a - side * (half + sh);
      dvec3 bL = b - side * (half + sh);
      dvec3 aR = a + side * (half + sh);
      dvec3 bR = b + side * (half + sh);
      aL.y = groundAt(aL.x, aL.z, a.y) - 0.05;
      bL.y = groundAt(bL.x, bL.z, b.y) - 0.05;
      aR.y = groundAt(aR.x, aR.z, a.y) - 0.05;
      bR.y = groundAt(bR.x, bR.z, b.y) - 0.05;
      addq(a + side * half + lift, b + side * half + lift, bR, aR);
      addq(aL, bL, b - side * half + lift, a - side * half + lift);
      nsegs++;
    }
    printf("BulletShapeSpine: <%s> %d segments -> %zu verts %zu tris (deck+shoulders, lift %.2fm)\n",
           path.c_str(), nsegs, verts->size() / 3, indices->size() / 3, _lift_m);
    if (not indices->empty()) {
      btIndexedMesh btmesh;
      btmesh.m_numTriangles        = int(indices->size() / 3);
      btmesh.m_triangleIndexBase   = (const uint8_t*)indices->data();
      btmesh.m_triangleIndexStride = 3 * sizeof(uint32_t);
      btmesh.m_indexType           = PHY_INTEGER;
      btmesh.m_numVertices         = int(verts->size() / 3);
      btmesh.m_vertexBase          = (const uint8_t*)verts->data();
      btmesh.m_vertexStride        = 3 * sizeof(btScalar);
      auto arrays = new btTriangleIndexVertexArray;
      arrays->addIndexedMesh(btmesh, PHY_INTEGER);
      btVector3 aabbMin, aabbMax;
      arrays->calculateAabbBruteForce(aabbMin, aabbMax);
      auto trishape = new btBvhTriangleMeshShape(arrays, /*useQuantizedAabbCompression=*/true, aabbMin, aabbMax);
      auto compound = new btCompoundShape;
      compound->addChildShape(btTransform::getIdentity(), trishape);
      rval->_collisionShape = compound;
      printf("BulletShapeSpine: AABB min<%.1f %.1f %.1f> max<%.1f %.1f %.1f>\n",
             aabbMin.x(), aabbMin.y(), aabbMin.z(), aabbMax.x(), aabbMax.y(), aabbMax.z());
      // WALK-GATE ORACLE (spec gate): probe every spine node through the BUILT shape —
      // a small AABB query at (x, road_elev+lift, z) must hit >=1 ribbon triangle.
      {
        struct CountCB : public btTriangleCallback {
          int _count = 0;
          void processTriangle(btVector3*, int, int) override { _count++; }
        };
        int misses = 0;
        for (size_t i = 0; i < N; i++) {
          btVector3 c(chP->_data[i].x, chP->_data[i].y + float(_lift_m), chP->_data[i].z);
          btVector3 ext(1.0f, 3.0f, 1.0f);
          CountCB cb;
          trishape->processAllTriangles(&cb, c - ext, c + ext);
          if (cb._count == 0) {
            misses++;
            if (misses <= 4)
              printf("BulletShapeSpine: ORACLE MISS node[%zu] at (%.1f, %.1f, %.1f)\n", i, c.x(), c.y(), c.z());
          }
        }
        printf("BulletShapeSpine: walk-gate oracle %s — %d/%d nodes covered\n",
               misses ? "FAIL" : "PASS", int(N) - misses, int(N));
      }
    } else {
      printf("BulletShapeSpine: <%s> EMPTY spine — collider degenerates to a point sphere\n", path.c_str());
      rval->_collisionShape = new btSphereShape(0.01f);
    }
    return rval;
  };
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::ecs
///////////////////////////////////////////////////////////////////////////////
