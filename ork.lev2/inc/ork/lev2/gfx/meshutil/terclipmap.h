#pragma once 

#include <ork/math/cvector3.h>
#include <vector>
#include <iostream>

namespace ork::meshutil::terclipmap {

struct Parameters;
struct Generator;

struct Parameters {
    int levels;
    int scale;
    int gridSize;
};

using parameters_ptr_t = std::shared_ptr<Parameters>;

using vertex_vect_t = std::vector<fvec3>;
using index_vect_t = std::vector<uint32_t>;

struct Generator {


    Generator(parameters_ptr_t params);

    void generateClipmaps();
    void generateLevel(int level);
    vertex_vect_t generateGrid(int levelSize, int levelScale);
    index_vect_t generateIndices(int gridSize);

    parameters_ptr_t _params;
};

} //namespace ork::meshutil {
