////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <iostream>
#include <utpp/UnitTest++.h>
#include <cmath>
#include <limits>
#include <ork/math/misc_math.h>
#include <ork/orkstd.h>
#include <ork/orkstd.h>
#include <klein/klein.hpp>
#include <ork/util/scrambler.inl>

using namespace ork;

TEST(scrambleru16) {
  using namespace ork;
  IndexScrambler<65536> scrambler(42);
  for (int i = 0; i < 65536; i++) {
    uint16_t scrambled = scrambler.scramble(i);
    uint16_t unscrambled = scrambler.unscramble(scrambled);
    printf( "i<%04x> scrambled<%04x> unscrambled<%04x>\n", i, scrambled, unscrambled);
    CHECK_EQUAL(i, unscrambled);
  }
}
