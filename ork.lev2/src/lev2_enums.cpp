////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////


#include <ork/pch.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/gfxenv_enum.h>
#include <ork/reflect/enum_serializer.inl>
#include <ork/reflect/properties/DirectEnum.inl>

/////////////////////////////////////////////////////////////////////////////////////////////////..

using msaaenum_t = ork::lev2::MsaaSamples;
using blending_t = ork::lev2::Blending;

/////////////////////////////////////////////////////////////////////////////////////////////////..

DeclareEnumSerializer(msaaenum_t);
BeginEnumRegistration(msaaenum_t);
    RegisterEnum(msaaenum_t, MSAA_1X);
    RegisterEnum(msaaenum_t, MSAA_4X);
    RegisterEnum(msaaenum_t, MSAA_8X);
    RegisterEnum(msaaenum_t, MSAA_9X);
    RegisterEnum(msaaenum_t, MSAA_16X);
    RegisterEnum(msaaenum_t, MSAA_25X);
    RegisterEnum(msaaenum_t, MSAA_36X);
EndEnumRegistration();
ImplementEnumSerializer(msaaenum_t);
template<> class ork::reflect::DirectEnum<msaaenum_t>;

/////////////////////////////////////////////////////////////////////////////////////////////////..

DeclareEnumSerializer(blending_t);
BeginEnumRegistration(blending_t);
    RegisterEnum(blending_t, OFF);
    RegisterEnum(blending_t, PREMA);
    RegisterEnum(blending_t, ALPHA);
    RegisterEnum(blending_t, DSTALPHA);
    RegisterEnum(blending_t, ADDITIVE);
    RegisterEnum(blending_t, ALPHA_ADDITIVE);
    RegisterEnum(blending_t, SUBTRACTIVE);
    RegisterEnum(blending_t, ALPHA_SUBTRACTIVE);
    RegisterEnum(blending_t, MODULATE);
EndEnumRegistration();
ImplementEnumSerializer(blending_t);
template<> class ork::reflect::DirectEnum<blending_t>;

/////////////////////////////////////////////////////////////////////////////////////////////////..

namespace ork::lev2{
    void registerEnums(){
        InvokeEnumRegistration(msaaenum_t);
        InvokeEnumRegistration(blending_t);
    }
}
