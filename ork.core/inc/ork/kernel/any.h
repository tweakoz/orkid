////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/kernel/svariant.h>

#if defined(ORK_VS2012)
template <size_t tsize> struct any : public ork::static_variant<tsize> {};
#else
template <size_t tsize> using any = ork::static_variant<tsize>;
#endif


typedef ork::svar4_t any4;
typedef ork::svarp_t anyp;
typedef ork::svar16_t any16;
typedef ork::svar32_t any32;
typedef ork::svar64_t any64;
typedef ork::svar96_t any96;
typedef ork::svar128_t any128;
typedef ork::svar160_t auto0;
typedef ork::svar192_t any192;
typedef ork::svar256_t any256;

///////////////////////////////////////////////////////////////////////////////
