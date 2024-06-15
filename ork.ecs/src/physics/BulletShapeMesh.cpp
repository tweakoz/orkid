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

#include <ork/ecs/scene.h>
#include <ork/ecs/entity.h>
#include <ork/ecs/entity.inl>

#include "bullet_impl.h"

ImplementReflectionX(ork::ecs::BulletShapeMeshData, "EcsBulletShapeMeshData");

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

btTriangleIndexVertexArray* flatSubmeshToTriVertArray(meshutil::flatsubmesh_ptr_t flatsubmesh,BulletShapeMeshData* data) {

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
  // convert into flat array
  ////////////////////////////////////////////////////////

  auto pnewU32 = new uint32_t[inumindices];
  for (int i = 0; i < inumindices; i++) {
    pnewU32[i] = Indices[i];
  }
  btmesh.m_triangleIndexBase   = (const uint8_t*)pnewU32;
  btmesh.m_triangleIndexStride = 3 * sizeof(uint32_t);
  btmesh.m_indexType           = PHY_INTEGER;
  btmesh.m_numTriangles        = inumindices / 3;

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

  fvec3 DSC = data->_scale;
  fvec3 DTR = data->_translation;

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
                                                    BulletShapeMeshData* data) {
  btTriangleIndexVertexArray* arrays = flatSubmeshToTriVertArray(flatsubmesh,data);

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
                                                BulletShapeMeshData* data ) {
  btCompoundShape* compoundShape = new btCompoundShape;
  btCollisionShape* shape        = flatSubmeshToBvhTriangleMeshShape(flatsubmesh,data);
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
        lev2::rendervar_strmap_t assetvars;
        _flatmesh = std::make_shared<meshutil::FlatSubMesh>(abs_path,assetvars);
      }

      if (_flatmesh and _flatmesh->inumverts) {
        auto xform = data.mEntity->transform();
        rval->_collisionShape = meshToBvhTriangleCompoundShape(_flatmesh, xform,this);
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
} // namespace ork::ecs
///////////////////////////////////////////////////////////////////////////////
