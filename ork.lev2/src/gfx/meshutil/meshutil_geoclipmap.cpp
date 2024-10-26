#include <ork/lev2/gfx/meshutil/geoclipmap.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork::meshutil {

extern double _default_quantization;

namespace geoclipmap {
///////////////////////////////////////////////////////////////////////////////

Generator::Generator(parameters_ptr_t params)
    : _params(params) {
}

///////////////////////////////////////////////////////////////////////////////

vertex_vect_t Quad::generateTriangleList() const {
  vertex_vect_t triangles;
  auto addTriangle = [&](const dvec3& v1, const dvec3& v2, const dvec3& v3) {
    triangles.push_back(v1);
    triangles.push_back(v2);
    triangles.push_back(v3);
  };
  auto center = (vertices[0] + vertices[1] + vertices[2] + vertices[3]) * 0.25f;

  switch (_type) {
    case TessellationType::INTERNAL: {
      addTriangle(vertices[0], vertices[1], center);
      addTriangle(vertices[1], vertices[2], center);
      addTriangle(vertices[2], vertices[3], center);
      addTriangle(vertices[3], vertices[0], center);
      break;
    }
    case TessellationType::TOP: {
      // must interface without tjunctions on
      // next levels BOTTOM
      // this levels INTERNAL
      // this levels TOP
      // this levels CORNER_TL and CORNER_TR
      addTriangle(vertices[0], vertices[1], center);
      addTriangle(vertices[1], vertices[2], center);
      break;
    }
    case TessellationType::BOTTOM: {
      // must interface without tjunctions on
      // next levels TOP
      // this levels INTERNAL
      // this levels BOTTOM
      // this levels CORNER_BL and CORNER_BR
      addTriangle(vertices[2], vertices[3], center);
      addTriangle(vertices[3], vertices[0], center);
      break;
    }
    case TessellationType::LEFT: {
      // must interface without tjunctions on
      // next levels RIGHT
      // this levels INTERNAL
      // this levels LEFT
      // this levels CORNER_TL and CORNER_BL
      addTriangle(vertices[3], vertices[0], center);
      addTriangle(vertices[0], vertices[1], center);
      break;
    }
    case TessellationType::RIGHT: {
      // must interface without tjunctions on
      // next levels LEFT
      // this levels INTERNAL
      // this levels RIGHT
      // this levels CORNER_TR and CORNER_BR
      addTriangle(vertices[1], vertices[2], center);
      addTriangle(vertices[2], vertices[3], center);
      break;
    }
    default:
      break;
  }

  return triangles;
}

///////////////////////////////////////////////////////////////////////////////

submesh_ptr_t Generator::generateClipmaps() {

  _default_quantization = 0.0;

  //_mesh = std::make_shared<Mesh>();
  _submesh = std::make_shared<submesh>();

  std::vector<level_ptr_t> levels;
  // for (int level = 0; level < _params->_levels; ++level) {
  for (int level = 0; level < _params->_levels; ++level) {
    auto l = generateLevel(level);
    levels.push_back(l);
  }

  printf("generating submesh...\n");

  int ilevel = 0;

  auto warp_square_to_circle = [&](const dvec3& p) -> dvec3 {
    double r = std::sqrt(p.x * p.x + p.z * p.z); // Current distance from origin
    if (r == 0.0)
      return p; // If the point is the origin, return it directly

    // The maximum component magnitude determines the scaling needed
    double maxComponent = std::max(std::abs(p.x), std::abs(p.z));
    double scale        = r / maxComponent; // This scaling factor adjusts the radius uniformly

    // Apply the scaling uniformly
    double newX = p.x / scale;
    double newZ = p.z / scale;

    // Return the transformed point with y unchanged
    return dvec3(newX, p.y, newZ);
  };

  for (auto level : levels) {

    printf("level<%d>\n", ilevel);
    // do something with the levels
    int num_tris = level->_vertices.size() / 3;

    vertex V0, V1, V2;
    for (int itri = 0; itri < num_tris; itri++) {
      int i0    = itri * 3 + 0;
      int i1    = itri * 3 + 1;
      int i2    = itri * 3 + 2;
      double fi = double(itri) / double(num_tris);

      if (_params->_circle) {
        V0.mPos = warp_square_to_circle(level->_vertices[i0]);
        V1.mPos = warp_square_to_circle(level->_vertices[i1]);
        V2.mPos = warp_square_to_circle(level->_vertices[i2]);
      } else {
        V0.mPos = level->_vertices[i0];
        V1.mPos = level->_vertices[i1];
        V2.mPos = level->_vertices[i2];
      }
      V0.mNrm                = dvec3(1, 0, 0);
      V1.mNrm                = dvec3(0, 1, 0);
      V2.mNrm                = dvec3(0, 0, 1);
      V0.mUV[0].mMapTexCoord = fvec2(0, 0);
      V1.mUV[0].mMapTexCoord = fvec2(1, 0);
      V2.mUV[0].mMapTexCoord = fvec2(1, 1);
      V0.mUV[0].mMapBiNormal = fvec3(fi, 0, 0);
      V1.mUV[0].mMapBiNormal = fvec3(0, fi + 1, 0);
      V2.mUV[0].mMapBiNormal = fvec3(0, 0, fi + 2);
      auto smv0              = _submesh->mergeVertex(V0);
      auto smv1              = _submesh->mergeVertex(V1);
      auto smv2              = _submesh->mergeVertex(V2);

      _submesh->mergeTriangle(smv0, smv1, smv2);
      if (0)
        printf(
            "tri<%d> v0<%f %f %f> v1<%f %f %f> v2<%f %f %f>\n",
            itri,
            V0.mPos.x,
            V0.mPos.y,
            V0.mPos.z,
            V1.mPos.x,
            V1.mPos.y,
            V1.mPos.z,
            V2.mPos.x,
            V2.mPos.y,
            V2.mPos.z);
    }
    ilevel++;
  }

  printf("merging submesh...\n");

  //_mesh->MergeSubMesh(*_submesh, "clipmappolys");

  // submeshWriteObj(*_submesh, file::Path("/Users/michael/clipmap.obj"));
  return _submesh;
}

///////////////////////////////////////////////////////////////////////////////

level_ptr_t Generator::generateLevel(int level) {
  level_ptr_t lvl = std::make_shared<Level>();
  auto ring       = generateRing(level); // Inner and outer quad count are the same
  for (auto segment : ring->_segments) {
    for (const auto& quad : segment.quads) {
      auto tris = quad.generateTriangleList();
      if (tris.size()) {
        lvl->_vertices.insert(lvl->_vertices.end(), tris.begin(), tris.end());
      }
    }
  }
  return lvl;
}

///////////////////////////////////////////////////////////////////////////////

ring_ptr_t Generator::generateRing(int level) {
  ring_ptr_t output     = std::make_shared<Ring>();
  double this_quad_size = _params->_baseQuadSize * pow(2, level);
  double prev_quad_size = this_quad_size * 0.5;
  double next_quad_size = this_quad_size * 2.0f;

  int DIM   = _params->_ringSize;
  int DIMD2 = DIM >> 1;
  int DIMD4 = DIM >> 2;

  if (level == 0) {

    // Special case for level 0
    // 8x8 quads
    // no inner, but has outer, with 7x7 internals
    // center at origin

    RingSegment segment;
    dvec3 v0, v1, v2, v3;
    Quad quad;
    for (int iy = 0; iy < DIM; iy++) {
      int iyb = iy - DIMD2;
      int iyt = iy - (DIMD2 - 1);
      for (int ix = 0; ix < DIM; ix++) {
        int ixl = ix - DIMD2;
        int ixr = ix - (DIMD2 - 1);
        v0.x    = double(ixl);
        v1.x    = double(ixr);
        v2.x    = double(ixr);
        v3.x    = double(ixl);
        v0.z    = double(iyb);
        v1.z    = double(iyb);
        v2.z    = double(iyt);
        v3.z    = double(iyt);

        quad.vertices[0] = v0 * this_quad_size;
        quad.vertices[1] = v1 * this_quad_size;
        quad.vertices[2] = v2 * this_quad_size;
        quad.vertices[3] = v3 * this_quad_size;
        quad._type       = TessellationType::INTERNAL;

        segment.quads.push_back(quad);
      }
    }
    output->_segments.push_back(segment);

    // with OUTER's on exterior

  } else {

    RingSegment segment;

    // inners should interface without tjunctions with the previous level's outers(internals)
    // and interface with this level's internals (which should be twice as big as the previous level's internals)

    auto do_skip = [&](int val) { return (val > (DIMD4 - 1) and val < (DIM - DIMD4)); };

    for (int iy = 0; iy < DIM; iy++) {
      for (int ix = 0; ix < DIM; ix++) {

        if (do_skip(ix) and do_skip(iy))
          continue;

        Quad quad;
        double X0, X1, Z0, Z1;
        Z0 = -DIMD2 * this_quad_size;
        Z1 = -DIMD2 * this_quad_size;
        Z0 += double(iy) * this_quad_size;
        Z1 += double(iy + 1) * this_quad_size;
        X0 = -DIMD2 * this_quad_size;
        X1 = -DIMD2 * this_quad_size;
        X0 += double(ix) * this_quad_size;
        X1 += double(ix + 1) * this_quad_size;
        quad.vertices[0] = dvec3(X0, 0.0f, Z0);
        quad.vertices[1] = dvec3(X1, 0.0f, Z0);
        quad.vertices[2] = dvec3(X1, 0.0f, Z1);
        quad.vertices[3] = dvec3(X0, 0.0f, Z1);
        quad._type       = TessellationType::INTERNAL;
        segment.quads.push_back(quad);
      }
    }

    output->_segments.push_back(segment);
  }
  return output;
}

///////////////////////////////////////////////////////////////////////////////

} // namespace geoclipmap
} // namespace ork::meshutil