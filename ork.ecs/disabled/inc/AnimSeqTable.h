////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/kernel/orklut.h>

#include <pkg/ent/entity.h>

namespace ork::ent {

/// An animation sequence event table can be used to store events that should be triggered based on a generic keyframe.
///
/// Typically used with animations (SimpleAnimatable), but can also be used with things like camera and cinematic animations.
///
/// Example events: Sound effects, particle effects, collision sphere activation/deactivation.
class AnimSeqTable : public ork::Object {
  RttiDeclareConcrete(AnimSeqTable, ork::Object);

public:
  AnimSeqTable();
  ~AnimSeqTable();

  /// Adds an event to be triggered at the given keyframe
  void AddEvent(float keyframe, ork::event::Event* event);

  void CopyEvents(const AnimSeqTable* pase);

  /// Notifies all registered listeners of any events that occurred within the (previousKeyFrame, currentKeyFrame] interval.
  ///
  /// Note: for triggering events on frame 0.0, pass -1.0 for previousKeyFrame and 0.0 for currentKeyFrame.
  /// @pre previousKeyFrame < currentKeyFrame
  void NotifyListener(float previousKeyFrame, float currentKeyFrame, ork::Object* listener);

  const ork::orklut<float, ork::event::Event*>& GetEventTable() const { return mEventTable; }
  ork::orklut<float, ork::event::Event*>& GetEventTable() { return mEventTable; }

protected:
  ork::orklut<float, ork::event::Event*> mEventTable;
};

} // namespace ork::ent
