#include <ork/lev2/gfx/meshutil/terclipmap.h>

namespace ork::meshutil::terclipmap {

Generator::Generator(parameters_ptr_t params)
    : _params(params) {
}

void Generator::generateClipmaps() {
  for (int level = 0; level < _params->levels; ++level) {
    generateLevel(level);
  }
}

void Generator::generateLevel(int level) {
  int levelScale            = _params->scale * (1 << level);
  int levelSize             = _params->gridSize * levelScale;
  vertex_vect_t vertices = generateGrid(levelSize, levelScale);
  index_vect_t indices   = generateIndices(_params->gridSize);
}

vertex_vect_t Generator::generateGrid(int levelSize, int levelScale) {
  vertex_vect_t vertices;
  for (int x = 0; x <= levelSize; x += levelScale) {
    for (int z = 0; z <= levelSize; z += levelScale) {
      vertices.push_back(glm::vec3(x, 0, z));
    }
  }
  return vertices;
}

index_vect_t Generator::generateIndices(int gridSize) {
  index_vect_t indices;
  for (int x = 0; x < gridSize; ++x) {
    for (int z = 0; z < gridSize; ++z) {
      int start = x * (gridSize + 1) + z;
      indices.push_back(start);
      indices.push_back(start + 1);
      indices.push_back(start + gridSize + 1);

      indices.push_back(start + 1);
      indices.push_back(start + gridSize + 2);
      indices.push_back(start + gridSize + 1);
    }
  }
  return indices;
}

} // namespace ork::meshutil