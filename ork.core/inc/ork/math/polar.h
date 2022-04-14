////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 


#pragma once

namespace ork {
float pol2rect_x(float ang, float rad);
float pol2rect_y(float ang, float rad);
float rect2pol_ang(float x, float y);
float rect2pol_angr(float x, float y);
float rect2pol_rad(float x, float y);
};