////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/lev2/editor/manip.h>
#include <ork/lev2/gfx/gfxanim.h>
#include <ork/reflect/properties/registerX.inl>


////////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::editor {
////////////////////////////////////////////////////////////////////////////////


void ManipulatorInterface::describeX(class_t* clazz) {

}

///////////////////////////////////////////////////////////////////////////////

bool JointManipulatorInterface::supportsTranslation() const {
  return true;
}
bool JointManipulatorInterface::supportsRotation() const {
  return true;
}
bool JointManipulatorInterface::supportsScaling() const {
  return true;
}

///////////////////////////////////////////////////////////////////////////////

void JointManipulatorInterface::_onBeginTranslation(ui::event_constptr_t EV) {
  OrkAssert(false);
}
void JointManipulatorInterface::_onUpdateTranslation(ui::event_constptr_t EV) {
  OrkAssert(false);
}
void JointManipulatorInterface::_onEndTranslation(ui::event_constptr_t EV) {
  OrkAssert(false);
}

///////////////////////////////////////////////////////////////////////////////

void JointManipulatorInterface::_onBeginRotation(ui::event_constptr_t EV) {
  OrkAssert(false);
}
void JointManipulatorInterface::_onUpdateRotation(ui::event_constptr_t EV) {
  OrkAssert(false);
}
void JointManipulatorInterface::_onEndRotation(ui::event_constptr_t EV) {
  OrkAssert(false);
}

///////////////////////////////////////////////////////////////////////////////

void JointManipulatorInterface::_onBeginScaling(ui::event_constptr_t EV) {
  OrkAssert(false);
}
void JointManipulatorInterface::_onUpdateScaling(ui::event_constptr_t EV) {
  OrkAssert(false);
}
void JointManipulatorInterface::_onEndScaling(ui::event_constptr_t EV) {
  OrkAssert(false);
}

////////////////////////////////////////////////////////////////////////////////
} //namespace ork::lev2::editor {
////////////////////////////////////////////////////////////////////////////////

ImplementReflectionX(ork::lev2::editor::ManipulatorInterface, "ManipulatorInterface");
