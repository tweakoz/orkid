#include <ork/lev2/gfx/meshutil/terclipmap.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork::meshutil::terclipmap {
///////////////////////////////////////////////////////////////////////////////

Generator::Generator(parameters_ptr_t params)
    : _params(params) {
}

///////////////////////////////////////////////////////////////////////////////

vertex_vect_t Quad::generateTriangleList() const {
  vertex_vect_t triangles;
  auto addTriangle = [&](const fvec3& v1, const fvec3& v2, const fvec3& v3) {
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
  }

  return triangles;
}

///////////////////////////////////////////////////////////////////////////////

mesh_ptr_t Generator::generateClipmaps() {

  auto mesh = std::make_shared<Mesh>();
  auto subm = std::make_shared<submesh>();

  std::vector<level_ptr_t> levels;
  for (int level = 0; level < _params->_levels; ++level) {
    auto l = generateLevel(level);
    levels.push_back(l);
  }

  printf("generating submesh...\n");

  int ilevel = 0;

  for (auto level : levels) {

    printf("level<%d>\n", ilevel);
    // do something with the levels
    int num_tris = level->_vertices.size() / 3;

    vertex V0, V1, V2;
    for (int itri = 0; itri < num_tris; itri++) {
      int i0    = itri * 3 + 0;
      int i1    = itri * 3 + 1;
      int i2    = itri * 3 + 2;
      V0.mPos   = fvec3_to_dvec3(level->_vertices[i0]);
      V1.mPos   = fvec3_to_dvec3(level->_vertices[i1]);
      V2.mPos   = fvec3_to_dvec3(level->_vertices[i2]);
      V0.mNrm   = dvec3(1, 0, 0);
      V1.mNrm   = dvec3(0, 1, 0);
      V2.mNrm   = dvec3(0, 0, 1);
      auto smv0 = subm->mergeVertex(V0);
      auto smv1 = subm->mergeVertex(V1);
      auto smv2 = subm->mergeVertex(V2);

      subm->mergeTriangle(smv0, smv1, smv2);
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

  mesh->MergeSubMesh(*subm, "clipmappolys");

  return mesh;
}

///////////////////////////////////////////////////////////////////////////////

level_ptr_t Generator::generateLevel(int level) {
  level_ptr_t lvl = std::make_shared<Level>();
  auto ring       = generateRing(level); // Inner and outer quad count are the same
  for (auto segment : ring->_segments) {
    for (auto quad : segment.quads) {
      auto tris = quad.generateTriangleList();
      lvl->_vertices.insert(lvl->_vertices.end(), tris.begin(), tris.end());
    }
  }
  return lvl;
}

///////////////////////////////////////////////////////////////////////////////

constexpr static float base_quad_size = 1.0f;

int compute_anchor(int base, int level) { // recursive
  // compute anchor_right sequence
  // 0 4 12 28 60 124 252 508
  // 0 2 6 14 30 62 126 254
  // given level
  if (level == 0) {
    return 0;
  } else {
    return base + 2 * compute_anchor(base, level - 1);
  }
}
///////////////////////////////////////////////////////////////////////////////

ring_ptr_t Generator::generateRing(int level) {
  ring_ptr_t output    = std::make_shared<Ring>();
  float this_quad_size = base_quad_size * pow(2, level);
  float prev_quad_size = this_quad_size * 0.5;
  float next_quad_size = this_quad_size * 2.0f;

  if (level == 0) {

    // Special case for level 0
    // 8x8 quads
    // no inner, but has outer, with 7x7 internals
    // center at origin

    RingSegment segment;
    fvec3 v0, v1, v2, v3;
    Quad quad;
    for (int iy = 0; iy < 8; iy++) {
      int iyb = iy - 4;
      int iyt = iy - 3;
      for (int ix = 0; ix < 8; ix++) {
        int ixl = ix - 4;
        int ixr = ix - 3;
        v0.x    = float(ixl);
        v1.x    = float(ixr);
        v2.x    = float(ixr);
        v3.x    = float(ixl);
        v0.z    = float(iyb);
        v1.z    = float(iyb);
        v2.z    = float(iyt);
        v3.z    = float(iyt);

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

    int anchor = compute_anchor(4, level);

    // inners should interface without tjunctions with the previous level's outers(internals)
    // and interface with this level's internals (which should be twice as big as the previous level's internals)

    // 8*3 = 24  ()
    // 24*3 = 72
    // 72*3 = 216
    // 24/2 = 12
    // 12*3 = 36
    // 12*2
    // 14*4
    // 16*8

    int DIM   = (12 + (level - 1) * 2);

    float DIMD2 = float(DIM)*0.5;

    for (int iy = 0; iy < DIM; iy++) {

      bool yskip = (iy > 3 and iy < (DIM - 4));

      for (int ix = 0; ix < DIM; ix++) {

        bool xskip = (ix > 3 and ix < (DIM - 4));

        if (yskip and xskip)
          continue;

        Quad quad;
        float X0, X1, Z0, Z1;
        Z0 = -DIMD2 * this_quad_size;
        Z1 = -DIMD2 * this_quad_size;
        Z0 += float(iy) * this_quad_size;
        Z1 += float(iy + 1) * this_quad_size;
        X0 = -DIMD2 * this_quad_size;
        X1 = -DIMD2 * this_quad_size;
        X0 += float(ix) * this_quad_size;
        X1 += float(ix + 1) * this_quad_size;
        quad.vertices[0] = fvec3(X0, 0.0f, Z0);
        quad.vertices[1] = fvec3(X1, 0.0f, Z0);
        quad.vertices[2] = fvec3(X1, 0.0f, Z1);
        quad.vertices[3] = fvec3(X0, 0.0f, Z1);
        quad._type       = TessellationType::INTERNAL;
        segment.quads.push_back(quad);
      }
    }

    output->_segments.push_back(segment);
  }
  return output;
}

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::meshutil::terclipmap