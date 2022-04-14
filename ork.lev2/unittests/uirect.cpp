////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/lev2/ui/ui.h>
#include <utpp/UnitTest++.h>

///////////////////////////////////////////////////////////////////////////////
TEST(uirect_move) {
  ork::ui::Rect r(0, 0, 100, 100);
  auto r2 = r;
  r2.moveTop(75);
  CHECK(r2._y == 75);
  CHECK(r2._h == 100);

  r2 = r;
  r2.moveBottom(25);
  CHECK(r2._y == -74);
  CHECK(r2._h == 100);

  r2 = r;
  r2.moveLeft(75);
  CHECK(r2._x == 75);
  CHECK(r2._w == 100);

  r2 = r;
  r2.moveRight(75);
  CHECK(r2._x == -24);
  CHECK(r2._w == 100);

  r2 = r;
  r2.moveCenter(100, 100);
  CHECK(r2._x == 50);
  CHECK(r2._y == 50);
  CHECK(r2.x2() == 149);
  CHECK(r2.y2() == 149);
  CHECK(r2._w == 100);
  CHECK(r2._h == 100);
}
///////////////////////////////////////////////////////////////////////////////
TEST(uirect_set) {
  ork::ui::Rect r(0, 0, 100, 100);
  auto r2 = r;
  r2.setTop(75);
  CHECK(r2._y == 75);
  CHECK(r2._h == 25);

  r2 = r;
  r2.setLeft(75);
  CHECK(r2._x == 75);
  CHECK(r2._w == 25);

  r2 = r;
  r2.setBottom(25);
  CHECK(r2._y == 0);
  CHECK(r2.y2() == 25);
  CHECK(r2._h == 26);

  r2 = r;
  r2.setRight(25);
  CHECK(r2._x == 0);
  CHECK(r2.x2() == 25);
  CHECK(r2._w == 26);
}

///////////////////////////////////////////////////////////////////////////////
TEST(uirect_testpoint) {
  ork::ui::Rect r(0, 0, 100, 100);

  CHECK(r.isPointInside(0, 0));
  CHECK(r.isPointInside(50, 50));
  CHECK(r.isPointInside(99, 99));

  CHECK(not r.isPointInside(-1, -1));
  CHECK(not r.isPointInside(100, 100));
  CHECK(not r.isPointInside(101, 101));
}
