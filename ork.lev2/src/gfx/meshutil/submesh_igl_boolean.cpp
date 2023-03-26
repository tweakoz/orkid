////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/lev2/config.h>
#if defined(ENABLE_IGL)

#include <ork/kernel/orklut.hpp>
#include <ork/math/plane.h>
#include <ork/lev2/gfx/meshutil/submesh.h>
#include <ork/lev2/gfx/meshutil/igl.h>
#include <iostream>

#include <Eigen/Core>

#include <igl/copyleft/cgal/mesh_boolean.h>
#include <igl/copyleft/cgal/piecewise_constant_winding_number.h>

namespace ork::meshutil {

bool IglMesh::piecewiseConstantWindingNumber() const {
  return igl::copyleft::cgal::piecewise_constant_winding_number(_verts,_faces); //
} 

//////////////////////////////////////////////////////////////////////////////
bool IglMesh::booleanOf(iglmesh_constptr_t a, BooleanOperation operation, iglmesh_constptr_t b ){

  igl::MeshBooleanType _igltype;

  switch(operation){
    case BooleanOperation::PLUS:
    _igltype = igl::MESH_BOOLEAN_TYPE_UNION;
      break;
    case BooleanOperation::INTERSECTION:
    _igltype = igl::MESH_BOOLEAN_TYPE_INTERSECT;
      break;
    case BooleanOperation::MINUS:
    _igltype = igl::MESH_BOOLEAN_TYPE_MINUS;
      break;
    case BooleanOperation::XOR:
    _igltype = igl::MESH_BOOLEAN_TYPE_XOR;
      break;
    case BooleanOperation::RESOLVE:
    _igltype = igl::MESH_BOOLEAN_TYPE_RESOLVE;
      break;
    default:
      OrkAssert(false);
      break;
  }

  Eigen::VectorXi J,I;

  igl::copyleft::cgal::mesh_boolean( //
    a->_verts, a->_faces,  //
    b->_verts, b->_faces, //
    _igltype,
    _verts,
    _faces,
    J
    );

  return false;
}


//////////////////////////////////////////////////////////////////////////////
} //namespace ork::meshutil {

#endif // #if defined(ENABLE_IGL)
