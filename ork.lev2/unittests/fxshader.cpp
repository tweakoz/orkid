////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/lev2/gfx/shadman.h>
#include <utpp/UnitTest++.h>

using namespace ork;
using namespace ork::lev2;

TEST(test_storage_buffer_mapping) {

  std::vector<uint8_t> data;
  data.resize(1<<20);

  /////////////////////////////////////////////
  // fake compute interface mapping GPU memory
  /////////////////////////////////////////////

  FxShaderStorageBufferMapping mapping;
  mapping._mappedaddr = data.data();
  mapping._length = data.size();
  mapping._cursor = 0;
  mapping._offset = 0;

  /////////////////////////////////////////////
  // test alignment
  /////////////////////////////////////////////

/*
struct InputVertexSprite {
    vec4 pos;      // Offset 0, Size 16 bytes, Alignment 16 bytes
    vec4 lw;       // Offset 16, Size 16 bytes, Alignment 16 bytes
    vec4 vel;      // Offset 32, Size 16 bytes, Alignment 16 bytes
    vec4 age_rand; // Offset 48, Size 16 bytes, Alignment 16 bytes
}; // Total Size: 64 bytes

struct OutputVertexSprite {
    vec4 hposL;    // Offset 0, Size 16 bytes, Alignment 16 bytes
    vec4 hposR;    // Offset 16, Size 16 bytes, Alignment 16 bytes
    vec2 uv;       // Offset 32, Size 8 bytes, Alignment 8 bytes
    vec2 age_rand; // Offset 40, Size 8 bytes, Alignment 8 bytes
}; // Total Size: 48 bytes (std430 does not require padding to 64 bytes)
*/

  struct alignas(16) InputVertexSprite {
    fvec4 pos;      // 16
    fvec4 lw;       // 32
    fvec4 vel;      // 48
    fvec4 age_rand; // 64
};
  struct alignas(16) OutputVertexSprite {
    fvec4 hposL;    // 16
    fvec4 hposR;    // 32
    fvec2 uv;       // 40
    fvec2 age_rand; // 48
  };
  using input_vertices_t = InputVertexSprite[16384];
  using output_vertices_t = OutputVertexSprite[65536];

/*
layout(std430, binding = 0) buffer {
    int          num_vertices;      // Offset 0, Size 4 bytes, Alignment 4 bytes
    vec3         _padding1;         // Offset 4, Size 12 bytes, Alignment 16 bytes

    mat4         v_L;               // Offset 16, Size 64 bytes (4 x vec4), Alignment 16 bytes
    mat4         v_R;               // Offset 80, Size 64 bytes, Alignment 16 bytes
    mat4         mvp_L;             // Offset 144, Size 64 bytes, Alignment 16 bytes
    mat4         mvp_R;             // Offset 208, Size 64 bytes, Alignment 16 bytes

    vec3         obj_nrmz;          // Offset 272, Size 12 bytes, Alignment 16 bytes
    float        _padding2;         // Offset 284, Size 4 bytes, Alignment 4 bytes

    InputVertexSprite  inp_vertex[16384]; // Offset 288, Size 64 * 16384 = 1,048,576 bytes, Alignment 16 bytes
    OutputVertexSprite out_vertex[65536]; // Offset 1048864, Size 48 * 65536 = 3,145,728 bytes, Alignment 16 bytes
    // final offset: 4194592
} ssbo_compute;
*/

  CHECK(mapping._cursor == 0);
  mapping.make<int>(1);
  printf("cursor<%d>\n", int(mapping._cursor));
  CHECK(mapping._cursor == 4);
  mapping.make<fmtx4>();
  printf("cursor<%d>\n", int(mapping._cursor));
  CHECK(mapping._cursor == 80);
  mapping.make<fmtx4>();
  printf("cursor<%d>\n", int(mapping._cursor));
  CHECK(mapping._cursor == 144);
  mapping.make<fmtx4>();
  printf("cursor<%d>\n", int(mapping._cursor));
  CHECK(mapping._cursor == 208);
  mapping.make<fmtx4>();
  printf("cursor<%d>\n", int(mapping._cursor));
  CHECK(mapping._cursor == 272);
  mapping.make<fvec3>();
  printf("cursor<%d>\n", int(mapping._cursor));
  CHECK(mapping._cursor == 284);
  mapping.advance<input_vertices_t>();
  printf("cursor<%d>\n", int(mapping._cursor));
  CHECK(mapping._cursor == 1048864);
  mapping.advance<output_vertices_t>();
  printf("cursor<%d>\n", int(mapping._cursor));
  CHECK(mapping._cursor == 4194592);

/*
layout(std430, binding = 0) buffer {
    int          num_vertices;      // Offset 0, Size 4 bytes, Alignment 4 bytes

    mat4         v_L;               // Offset 16, Size 64 bytes, Alignment 16 bytes
    mat4         v_R;               // Offset 80, Size 64 bytes, Alignment 16 bytes
    mat4         mvp_L;             // Offset 144, Size 64 bytes, Alignment 16 bytes
    mat4         mvp_R;             // Offset 208, Size 64 bytes, Alignment 16 bytes

    vec3         bb_hori;           // Offset 272, Size 12 bytes, Alignment 16 bytes
    vec3         bb_vert;           // Offset 284, Size 12 bytes, Alignment 16 bytes

    InputVertexSprite  inp_vertex[16384]; 
    OutputVertexSprite out_vertex[65536]; 
*/

  mapping._cursor = 0; // reset cursor
  mapping.make<int>(1);
  printf("cursor<%d>\n", int(mapping._cursor));
  CHECK(mapping._cursor == 4);
  mapping.make<fmtx4>();
  printf("cursor<%d>\n", int(mapping._cursor));
  CHECK(mapping._cursor == 80);
  mapping.make<fmtx4>();
  printf("cursor<%d>\n", int(mapping._cursor));
  CHECK(mapping._cursor == 144);
  mapping.make<fmtx4>();
  printf("cursor<%d>\n", int(mapping._cursor));
  CHECK(mapping._cursor == 208);
  mapping.make<fmtx4>();
  printf("cursor<%d>\n", int(mapping._cursor));
  CHECK(mapping._cursor == 272);
  mapping.make<fvec3>();
  printf("cursor<%d>\n", int(mapping._cursor));
  CHECK(mapping._cursor == 284);
  mapping.advance<input_vertices_t>();
  printf("cursor<%d>\n", int(mapping._cursor));
  CHECK(mapping._cursor == 1048864);
  mapping.advance<output_vertices_t>();
  printf("cursor<%d>\n", int(mapping._cursor));
  CHECK(mapping._cursor == 4194592);


}
