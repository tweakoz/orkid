////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "pyext.h"
#include <ork/lev2/aud/audiodevice.h>
#include <ork/lev2/aud/singularity/cz1.h>
#include <ork/lev2/aud/singularity/krzdata.h>
#include <ork/lev2/aud/singularity/tx81z.h>
#include <ork/lev2/aud/singularity/fxgen.h>
#include <ork/lev2/aud/singularity/hud.h>
#include <ork/lev2/ui/widget.h>
#include <ork/lev2/ui/group.h>
#include <ork/lev2/ui/surface.h>
#include <ork/lev2/ui/viewport.h>
#include <ork/lev2/ui/layoutgroup.inl>
#include <ork/lev2/ui/anchor.h>
#include <ork/lev2/ui/box.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork::audio::singularity {
///////////////////////////////////////////////////////////////////////////////
using namespace ork::lev2;
///////////////////////////////////////////////////////////////////////////////
void pyinit_aud_singularity_sequencer(py::module& singmodule) {
  /////////////////////////////////////////////////////////////////////////////////
  auto type_codec = python::TypeCodec::instance();
  /////////////////////////////////////////////////////////////////////////////////
  auto event_t = py::class_<Event, event_ptr_t>(singmodule, "Event")
    .def(py::init<>())
    .def_property("time", [](event_ptr_t evt) { return evt->_time; }, [](event_ptr_t evt, float val) { evt->_time = val; })
    .def_property("dur", [](event_ptr_t evt) { return evt->_dur; }, [](event_ptr_t evt, float val) { evt->_dur = val; })
    .def_property("note", [](event_ptr_t evt) { return evt->_note; }, [](event_ptr_t evt, int val) { evt->_note = val; })
    .def_property("vel", [](event_ptr_t evt) { return evt->_vel; }, [](event_ptr_t& evt, int val) { evt->_vel = val; });
  type_codec->registerStdCodec<event_ptr_t>(event_t);
  /////////////////////////////////////////////////////////////////////////////////
  auto clip_t = py::class_<Clip, clip_ptr_t>(singmodule, "Clip")
    .def(py::init<>());
    //.def_property("events", [](clip_ptr_t clip) { return clip->_events; }, [](clip_ptr_t clip, const std::vector<event_ptr_t>& val) { clip->_events = val; });
    //.def("addNote", [](clip_ptr_t& clip, int meas, int sixteenth, int clocks, int note, int vel, int dur) { clip->addNote(meas, sixteenth, clocks, note, vel, dur); });
  type_codec->registerStdCodec<clip_ptr_t>(clip_t);
  /////////////////////////////////////////////////////////////////////////////////
  auto track_t = py::class_<Track, track_ptr_t>(singmodule, "Track")
    .def(py::init<>())
    .def_property("clips", [](const track_ptr_t& track) { return track->_clips_by_timestamp; }, [](track_ptr_t& track, const clipmap_t& val) { track->_clips_by_timestamp = val; });
  type_codec->registerStdCodec<track_ptr_t>(track_t);
  /////////////////////////////////////////////////////////////////////////////////
  auto sequence_t = py::class_<Sequence, sequence_ptr_t>(singmodule, "Sequence")
    .def(py::init<>())
    .def("enqueue", [](sequence_ptr_t& seq, prgdata_constptr_t program) { seq->enqueue(program); });
  type_codec->registerStdCodec<sequence_ptr_t>(sequence_t);
  /////////////////////////////////////////////////////////////////////////////////
  auto sequencer_t = py::class_<Sequencer, sequencer_ptr_t>(singmodule, "Sequencer");
  type_codec->registerStdCodec<sequencer_ptr_t>(sequencer_t);
}
///////////////////////////////////////////////////////////////////////////////
} //namespace ork::audio::singularity {
///////////////////////////////////////////////////////////////////////////////
