////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <utpp/UnitTest++.h>
#include <ork/object/Object.inl>
#include "reflectionclasses.inl"


using namespace ork;


TEST(OrkInterface1) {

    auto subject = std::make_shared<InterfaceTest>();

    auto iface = subject->queryInterface<TheTestInterface>("TheTestInterface");
    OrkAssert(iface);
    iface->doSomethingWith(subject.get());

}

