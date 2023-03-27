////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/lev2/ui/ged/ged.h>
#include <ork/lev2/ui/ged/ged_container.h>
#include <utpp/UnitTest++.h>
#include <ork/kernel/string/deco.inl>
#include <ork/util/hotkey.h>

using namespace ork;
using namespace ork::lev2;
using namespace ork::lev2::ged;

TEST(ged1) {

    auto model = ObjModel::createShared();
    auto container = GedContainer::createShared(model);
    auto edit_obj = std::make_shared<HotKey>();
    model->attach(edit_obj);

    model->dump("yo");
}
