////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
//////////////////////////////////////////////////////////////// 


#pragma once

namespace ork {
float pol2rect_x(float ang, float rad);
float pol2rect_y(float ang, float rad);
float rect2pol_ang(float x, float y);
float rect2pol_angr(float x, float y);
float rect2pol_rad(float x, float y);
};