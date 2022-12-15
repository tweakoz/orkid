////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 

#include<ork/pch.h>
#include<ork/math/PIDController.inl>

///////////////////////////////////////////////////////////////////////////////

template struct ork::PIDController<float>;
template struct ork::PIDController2<float>;
template struct ork::PIDController<double>;
template struct ork::PIDController2<double>;
