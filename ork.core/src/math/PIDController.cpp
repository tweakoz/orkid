////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
//////////////////////////////////////////////////////////////// 

#include<ork/pch.h>
#include<ork/math/PIDController.inl>

///////////////////////////////////////////////////////////////////////////////

template struct ork::PIDController<float>;
template struct ork::PIDController2<float>;
template struct ork::PIDController<double>;
template struct ork::PIDController2<double>;
