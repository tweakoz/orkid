#include <iostream>
#include <vector>
#include <algorithm>

namespace ork {
class ScramblerUint16 {
public:
    ScramblerUint16(int seedval=0) {

        std::seed_seq seed{seedval};

        printf("initializing scrambler\n");
        // Initialize the lookup table with the identity mapping
        for (int i = 0; i < 65536; ++i) {
            lookupTable[i] = i;
        }

        // Shuffle the lookup table to create a random permutation
        std::vector<uint16_t> indices(65536);
        std::iota(indices.begin(), indices.end(), 0);
        std::shuffle(indices.begin(), indices.end(), std::default_random_engine(seed));

        for (int i = 0; i < 65536; ++i) {
            lookupTable[i] = indices[i];
            lookupTable2[indices[i]] = i;
        }
        printf("scrambler initialized....\n");
    }

    uint16_t scramble(uint16_t value) {
        return lookupTable[value];
    }

    uint16_t unscramble(uint16_t scrambledValue) {
        return lookupTable2[scrambledValue];
    }

private:
    std::unordered_map<uint16_t, uint16_t> lookupTable;
    std::unordered_map<uint16_t, uint16_t> lookupTable2;
};

} //namespace ork {
