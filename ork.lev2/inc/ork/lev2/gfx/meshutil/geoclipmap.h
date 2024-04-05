#pragma once 
///////////////////////////////////////////////////////////////////////////////
#include <ork/lev2/gfx/meshutil/meshutil.h>
#include <ork/lev2/gfx/meshutil/submesh.h>
#include <ork/math/cvector2.h>
#include <ork/math/cvector3.h>
#include <vector>
#include <iostream>
///////////////////////////////////////////////////////////////////////////////

namespace ork::meshutil::geoclipmap {
///////////////////////////////////////////////////////////////////////////////

struct Parameters;
struct Generator;
struct Level;
struct Ring;

using parameters_ptr_t = std::shared_ptr<Parameters>;
using level_ptr_t = std::shared_ptr<Level>;
using vertex_vect_t = std::vector<dvec3>;
using ring_ptr_t = std::shared_ptr<Ring>;

///////////////////////////////////////////////////////////////////////////////

struct Parameters {
    int _levels = 1;
    int _scale = 1;
    int _gridSize = 256;
};

///////////////////////////////////////////////////////////////////////////////

enum class TessellationType {
    INTERNAL,     // For quads fully within the ring, not touching the ring's edge
    TOP,    // For quads along the inner edge of the ring
    BOTTOM, // For quads along the inner edge of the ring
    LEFT,   // For quads along the inner edge of the ring
    RIGHT,  // For quads along the inner edge of the ring
    CORNER_TL, // For quads at the corners of the inner edge
    CORNER_TR, // For quads at the corners of the inner edge
    CORNER_BL, // For quads at the corners of the inner edge
    CORNER_BR, // For quads at the corners of the inner edge
};

struct Quad {

    vertex_vect_t generateTriangleList() const; 
    dvec3 vertices[4];      // Corners of the quad
    TessellationType _type; // Type of tessellation required for this quad

};

struct RingSegment {
    std::vector<Quad> quads;     // All quads within this segment, possibly including multiple tessellation types
};

struct Ring {
    std::vector<RingSegment> _segments; // Segments making up this ring, could be used to separate inner/outer edges
    int level;                         // Level of this ring in the terrain
};

///////////////////////////////////////////////////////////////////////////////

struct Level {
    vertex_vect_t _vertices;
};

///////////////////////////////////////////////////////////////////////////////

struct Generator {


    Generator(parameters_ptr_t params);

    submesh_ptr_t generateClipmaps();
    ring_ptr_t generateRing(int level);
    level_ptr_t generateLevel(int level);

    parameters_ptr_t _params;
    submesh_ptr_t _submesh;
    mesh_ptr_t _mesh;
};

///////////////////////////////////////////////////////////////////////////////
} //namespace ork::meshutil {
