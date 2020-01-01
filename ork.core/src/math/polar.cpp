////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 


#include <ork/pch.h>
#include <ork/math/polar.h>
#include <ork/math/misc_math.h>
#include <cmath>


namespace ork {
///////////////////////////////////////////////////////////////////////////////

float pol2rect_x(float ang, float rad) {
  float x = rad * cosf(ang);
  return x;
}

///////////////////////////////////////////////////////////////////////////////

float pol2rect_y(float ang, float rad) {
  float y = rad * sinf(ang);
  return y;
}

///////////////////////////////////////////////////////////////////////////////

float rect2pol_ang(float x, float y) {
  float ang = 0.0f;

  // AXIS
  if (x == 0.0f) {
    if (y == 0.0f)
      ang = 0.0f;
    else if (y > 0.0f)
      ang = PI_DIV_2;
    else if (y < 0.0f)
      ang = (3.0f * PI_DIV_2);
  } else if (y == 0.0f) {
    if (x < 0.0f)
      ang = PI;
    else if (x > 0.0f)
      ang = 0.0f;
  }

  // Q0 ( bottom right )
  else if ((x > 0.0f) && (y > 0.0f)) {
    ang = atanf(y / x);
  }
  // Q1 ( bottom left )
  else if ((x < 0.0f) && (y > 0.0f)) {
    ang = PI + atanf(y / x);
  }
  // Q2 ( top left )
  else if ((x < 0.0f) && (y < 0.0f)) {
    ang = PI + atanf(y / x);
  }
  // Q3 ( top right )
  else if ((x > 0.0f) && (y < 0.0f)) {
    ang = atanf(y / x);
  }

  return ang;
}

///////////////////////////////////////////////////////////////////////////////

float rect2pol_angr(float x, float y) {
  float ang = 0.0f;

  // AXIS
  if (x == 0.0f) {
    if (y == 0.0f)
      ang = 0.0f;
    else if (y > 0.0f)
      ang = PI_DIV_2;
    else if (y < 0.0f)
      ang = (3.0f * PI_DIV_2);
  } else if (y == 0.0f) {
    if (x < 0.0f)
      ang = PI;
    else if (x > 0.0f)
      ang = 0.0f;
  }

  // Q0 ( bottom right )
  else if ((x > 0.0f) && (y > 0.0f)) {
    ang = atanf(y / x);
  }
  // Q1 ( bottom left )
  else if ((x < 0.0f) && (y > 0.0f)) {
    ang = PI + atanf(y / x);
  }
  // Q2 ( top left )
  else if ((x < 0.0f) && (y < 0.0f)) {
    ang = PI + atanf(y / x);
  }
  // Q3 ( top right )
  else if ((x > 0.0f) && (y < 0.0f)) {
    ang = atanf(y / x);
  }

  return ang;
}

///////////////////////////////////////////////////////////////////////////////

float rect2pol_rad(float x, float y) {
  float rad = sqrtf((x * x) + (y * y));
  return rad;
}

}