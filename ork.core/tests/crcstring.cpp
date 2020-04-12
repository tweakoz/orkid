#include <utpp/UnitTest++.h>
#include <ork/util/crc.h>

using namespace ork;

TEST(crcstring_1) {
  CrcString crc1("crc1");
  CrcString crc2("crc2");

  CHECK_EQUAL(crc1.hashed(), "crc1"_crcu);
  CHECK_EQUAL(crc2.hashed(), "crc2"_crcu);

  printf("crc1<0x%zx>\n", crc1.hashed());
  printf("crc2<0x%zx>\n", crc2.hashed());
}
