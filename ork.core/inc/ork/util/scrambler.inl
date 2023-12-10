#include <iostream>
#include <vector>
#include <algorithm>
#include <random>

namespace ork {
template <size_t KSIZE>
struct IndexScrambler {

  IndexScrambler(int seedval=0) {

    _scrambleLUT.resize(KSIZE);
    _unscrambleLUT.resize(KSIZE);

    std::seed_seq seed{seedval};

    printf("initializing scrambler\n");
    // Initialize the lookup table with the identity mapping
    for (int i = 0; i < KSIZE; ++i) {
      _scrambleLUT[i] = i;
    }

    // Shuffle the lookup table to create a random permutation
    std::vector<uint16_t> indices(KSIZE);
    std::iota(indices.begin(), indices.end(), 0);
    std::shuffle(indices.begin(), indices.end(), std::default_random_engine(seed));

    for (int i = 0; i < KSIZE; ++i) {
      _scrambleLUT[i] = indices[i];
      _unscrambleLUT[indices[i]] = i;
    }
    printf("scrambler initialized....\n");
    }

    uint16_t scramble(uint16_t value) {
      return _scrambleLUT[value];
    }

    uint16_t unscramble(uint16_t scrambledValue) {
      return _unscrambleLUT[scrambledValue];
    }

    std::vector< uint16_t> _scrambleLUT;
    std::vector< uint16_t> _unscrambleLUT;
};

using indexscrambler65k_ptr_t = std::shared_ptr<IndexScrambler<65536>>;

} //namespace ork {
