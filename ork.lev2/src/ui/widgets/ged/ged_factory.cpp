////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/lev2/ui/ged/ged.h>
#include <ork/lev2/ui/ged/ged_node.h>
#include <ork/kernel/core_interface.h>
#include <ork/rtti/RTTIX.inl>

// template class ork::object::Signal<void,ork::lev2::ged::ObjModel>;

namespace ork::lev2::ged {
///////////////////////////////////////////////////////////////////////////////

void GedFactory::describeX(object::ObjectClass* clazz) {

}

///////////////////////////////////////////////////////////////////////////////

geditemnode_ptr_t
GedFactory::createItemNode( ObjModel* mdl, //
                            const ConstString& Name, //
                            const reflect::ObjectProperty* prop, //
                            object_ptr_t obj) const { //
  return nullptr; //std::make_shared<GedLabelNode>(mdl, Name.c_str(), prop, obj);
}

///////////////////////////////////////////////////////////////////////////////
} //namespace ork::lev2::ged {

ImplementReflectionX(ork::lev2::ged::GedFactory, "GedFactory");
