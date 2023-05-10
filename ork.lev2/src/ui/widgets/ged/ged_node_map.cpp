////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/lev2/ui/ged/ged.h>
#include <ork/lev2/ui/ged/ged_node.h>
#include <ork/lev2/ui/ged/ged_skin.h>
#include <ork/lev2/ui/ged/ged_container.h>
#include <ork/kernel/core_interface.h>
#include <ork/lev2/gfx/dbgfontman.h>
#include <ork/reflect/properties/registerX.inl>

ImplementReflectionX(ork::lev2::ged::GedMapNode, "GedMapNode");

////////////////////////////////////////////////////////////////
namespace ork::lev2::ged {
////////////////////////////////////////////////////////////////

void GedMapNode::describeX(class_t* clazz) {
}

void GedMapNode::FocusItem(const PropTypeString& key) {
}

bool GedMapNode::IsKeyPresent(const KeyDecoName& pkey) const {
  return false;
}
void GedMapNode::AddKey(const KeyDecoName& pkey) {
}

void GedMapNode::OnMouseDoubleClicked(ui::event_constptr_t ev) {
}

void GedMapNode::CheckVis() {
}
void GedMapNode::DoDraw(Context* pTARG) {
}

void GedMapNode::AddItem(ui::event_constptr_t ev) {
}
void GedMapNode::RemoveItem(ui::event_constptr_t ev) {
}
void GedMapNode::MoveItem(ui::event_constptr_t ev) {
}
void GedMapNode::DuplicateItem(ui::event_constptr_t ev) {
}
void GedMapNode::ImportItem(ui::event_constptr_t ev) {
}
void GedMapNode::ExportItem(ui::event_constptr_t ev) {
}

} // namespace ork::lev2::ged
