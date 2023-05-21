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


using msaaenum_t = ork::lev2::MsaaSamples;

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

namespace ork::lev2{

    void registerEnums(){
        InvokeEnumRegistration(msaaenum_t);
    }
}

template<> class ork::reflect::DirectEnum<ork::lev2::MsaaSamples>;
