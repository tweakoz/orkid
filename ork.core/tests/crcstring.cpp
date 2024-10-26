////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <utpp/UnitTest++.h>
#include <ork/util/crc.h>

using namespace ork;

TEST(crcstring_1) {
  CrcString crc1("crc1");
  CrcString crc2("crc2");

  CHECK_EQUAL(crc1.hashed(), "crc1"_crcu);
  CHECK_EQUAL(crc2.hashed(), "crc2"_crcu);

  printf("crc1<0x%llx>\n", crc1.hashed());
  printf("crc2<0x%llx>\n", crc2.hashed());
}
