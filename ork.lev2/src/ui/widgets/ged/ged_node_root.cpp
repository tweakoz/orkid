////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/lev2/ui/ged/ged.h>
#include <ork/lev2/ui/ged/ged_node.h>
#include <ork/lev2/ui/ged/ged_skin.h>
#include <ork/kernel/core_interface.h>
#include <ork/reflect/properties/registerX.inl>

ImplementReflectionX(ork::lev2::ged::GedRootNode, "GedRootNode");

////////////////////////////////////////////////////////////////
namespace ork::lev2::ged {
////////////////////////////////////////////////////////////////

void GedRootNode::describeX(class_t* clazz) {
}

GedRootNode::GedRootNode(
    GedContainer* c,                       //
    const char* name,                    //
    const reflect::ObjectProperty* prop, //
    object_ptr_t obj)
    : GedItemNode(c, name, prop, obj) {
}

void GedRootNode::DoDraw(lev2::Context* pTARG){

}
void GedRootNode::Layout(int ix, int iy, int iw, int ih){
    GedItemNode::Layout(ix,iy,iw,ih);
}

////////////////////////////////////////////////////////////////
} // namespace ork::lev2::ged
////////////////////////////////////////////////////////////////
