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
  auto timestamp_t =
      py::class_<TimeStamp, timestamp_ptr_t>(singmodule, "TimeStamp")
          .def(py::init<>())
          .def(py::init<int, int, int>())
          .def_property(
              "measures",
              [](timestamp_ptr_t ts) { return ts->_measures; },
              [](timestamp_ptr_t ts, int val) { ts->_measures = val; })
          .def_property(
              "beats", [](timestamp_ptr_t ts) { return ts->_beats; }, [](timestamp_ptr_t ts, int val) { ts->_beats = val; })
          .def_property(
              "clocks", [](timestamp_ptr_t ts) { return ts->_clocks; }, [](timestamp_ptr_t ts, int val) { ts->_clocks = val; })
          .def("clone", [](timestamp_ptr_t ts) -> timestamp_ptr_t { return ts->clone(); })
          // implement + and - operators in terms of add and sub
          .def("__add__", [](timestamp_ptr_t ts, timestamp_ptr_t duration) -> timestamp_ptr_t { return ts->add(duration); })
          .def("__sub__", [](timestamp_ptr_t ts, timestamp_ptr_t duration) -> timestamp_ptr_t { return ts->sub(duration); })
          .def("__repr__", [](timestamp_ptr_t ts) -> std::string {
            std::ostringstream oss;
            oss << "TimeStamp( M: " << ts->_measures << ", B: " << ts->_beats << ", C: " << ts->_clocks << ")";
            return oss.str();
          });
  type_codec->registerStdCodec<timestamp_ptr_t>(timestamp_t);
  /////////////////////////////////////////////////////////////////////////////////
  auto timebase_t =
      py::class_<TimeBase, timebase_ptr_t>(singmodule, "TimeBase")
          .def(py::init<>())
          .def("clone", [](timebase_ptr_t tbase) -> timebase_ptr_t { return tbase->clone(); })
          .def("time", [](timebase_ptr_t tbase, timestamp_ptr_t ts) -> float { return tbase->time(ts); })
          .def("timeToTimeStamp", [](timebase_ptr_t tbase, float time) -> timestamp_ptr_t { return tbase->timeToTimeStamp(time); })
          .def("reduce", [](timebase_ptr_t tbase, timestamp_ptr_t ts) -> timestamp_ptr_t { return tbase->reduceTimeStamp(ts); })
          .def("accumBaseTimes", [](timebase_ptr_t tbase) -> float { return tbase->accumBaseTimes(); })
          .def_property(
              "numerator",
              [](timebase_ptr_t tbase) { return tbase->_numerator; },
              [](timebase_ptr_t tbase, int val) { tbase->_numerator = val; })
          .def_property(
              "denominator",
              [](timebase_ptr_t tbase) { return tbase->_denominator; },
              [](timebase_ptr_t tbase, int val) { tbase->_denominator = val; })
          .def_property(
              "tempo",
              [](timebase_ptr_t tbase) { return tbase->_tempo; },
              [](timebase_ptr_t tbase, float val) { tbase->_tempo = val; })
          .def_property(
              "basetime",
              [](timebase_ptr_t tbase) { return tbase->_basetime; },
              [](timebase_ptr_t tbase, float val) { tbase->_basetime = val; })
          .def_property(
              "duration",
              [](timebase_ptr_t tbase) { return tbase->_duration; },
              [](timebase_ptr_t tbase, float val) { tbase->_duration = val; })
          .def_property(
              "ppq", [](timebase_ptr_t tbase) { return tbase->_ppq; }, [](timebase_ptr_t tbase, int val) { tbase->_ppq = val; })
          .def_property("parent", [](timebase_ptr_t tbase) { return tbase->_parent; }, [](timebase_ptr_t tbase, timebase_ptr_t val) {
            tbase->_parent = val;
          })
          .def("__repr__", [](timebase_ptr_t tbase) -> std::string {
            std::ostringstream oss;
            oss << "TimeBase( sig: " << tbase->_numerator << "/" << tbase->_denominator << ", BPM: " << tbase->_tempo
                << ", PPQ: " << tbase->_ppq << " )";
            return oss.str();
          });
  type_codec->registerStdCodec<timebase_ptr_t>(timebase_t);
  /////////////////////////////////////////////////////////////////////////////////
  auto event_t = py::class_<Event, event_ptr_t>(singmodule, "Event")
                     .def(py::init<>())
                     .def_property(
                         "timestamp",
                         [](event_ptr_t evt) { return evt->_timestamp; },
                         [](event_ptr_t evt, timestamp_ptr_t val) { evt->_timestamp = val; })
                     .def_property(
                         "duration",
                         [](event_ptr_t evt) { return evt->_duration; },
                         [](event_ptr_t evt, timestamp_ptr_t val) { evt->_duration = val; })
                     .def_property(
                         "note", [](event_ptr_t evt) { return evt->_note; }, [](event_ptr_t evt, int val) { evt->_note = val; })
                     .def_property(
                         "vel", [](event_ptr_t evt) { return evt->_vel; }, [](event_ptr_t& evt, int val) { evt->_vel = val; });
  type_codec->registerStdCodec<event_ptr_t>(event_t);
  /////////////////////////////////////////////////////////////////////////////////
  auto clip_t =
      py::class_<Clip, clip_ptr_t>(singmodule, "Clip")
          .def_property(
              "name", [](clip_ptr_t clip) { return clip->_name; }, [](clip_ptr_t clip, std::string val) { clip->_name = val; })
          .def_property(
              "duration",
              [](clip_ptr_t clip) { return clip->_duration; },
              [](clip_ptr_t clip, timestamp_ptr_t val) { clip->_duration = val; });
  //.def_property("events", [](clip_ptr_t clip) { return clip->_events; }, [](clip_ptr_t clip, const std::vector<event_ptr_t>& val)
  //{ clip->_events = val; }); .def("addNote", [](clip_ptr_t& clip, int meas, int sixteenth, int clocks, int note, int vel, int dur)
  //{ clip->addNote(meas, sixteenth, clocks, note, vel, dur); });
  type_codec->registerStdCodec<clip_ptr_t>(clip_t);
  /////////////////////////////////////////////////////////////////////////////////
  using eventclip_ptr_t = std::shared_ptr<EventClip>;
  auto eventclip_t =
      py::class_<EventClip, Clip, eventclip_ptr_t>(singmodule, "EventClip")
          .def(
              "createNoteEvent",
              [](const eventclip_ptr_t& clip, timestamp_ptr_t ts, timestamp_ptr_t dur, int note, int vel) -> event_ptr_t {
                return clip->createNoteEvent(ts, dur, note, vel);
              })
          .def("__repr__", [](eventclip_ptr_t clip) -> std::string {
            std::ostringstream oss;
            auto dur = clip->_duration;
            oss << "EventClip( name: " << clip->_name << ", duration: "
                << "( M: " << dur->_measures << ", B: " << dur->_beats << ", C: " << dur->_clocks << ")" //
                << ", event_count: " << clip->_events.size() << " )";
            return oss.str();
          });
  type_codec->registerStdCodec<eventclip_ptr_t>(eventclip_t);
  /////////////////////////////////////////////////////////////////////////////////
  using fouronfloorclip_ptr_t = std::shared_ptr<FourOnFloorClip>;
  auto fouronfloorclip_t       = py::class_<FourOnFloorClip, Clip, fouronfloorclip_ptr_t>(singmodule, "FourOnFloorClip")
                            .def(
                                "__repr__",
                                [](fouronfloorclip_ptr_t clip) -> std::string {
                                  std::ostringstream oss;
                                  auto dur = clip->_duration;
                                  oss << "FourOnFloorClip( name: " << clip->_name << ", duration: "
                                      << "( M: " << dur->_measures << ", B: " << dur->_beats << ", C: " << dur->_clocks << ") )";
                                  return oss.str();
                                })
                            .def_property(
                                "note",
                                [](fouronfloorclip_ptr_t clip) { return clip->_note; },
                                [](fouronfloorclip_ptr_t clip, int val) { clip->_note = val; })
                            .def_property(
                                "vel",
                                [](fouronfloorclip_ptr_t clip) { return clip->_vel; },
                                [](fouronfloorclip_ptr_t clip, int val) { clip->_vel = val; });
  type_codec->registerStdCodec<fouronfloorclip_ptr_t>(fouronfloorclip_t);
  /////////////////////////////////////////////////////////////////////////////////
  auto track_t = py::class_<Track, track_ptr_t>(singmodule, "Track")
                     .def(py::init<>())
                     .def(
                         "createEventClipAtTimeStamp",
                         [](const track_ptr_t& track, std::string name, timestamp_ptr_t ts, timestamp_ptr_t dur) {
                           return track->createEventClipAtTimeStamp(name, ts, dur);
                         })
                     .def(
                         "createFourOnFloorClipAtTimeStamp",
                         [](const track_ptr_t& track, std::string name, timestamp_ptr_t ts, timestamp_ptr_t dur) {
                           return track->createFourOnFloorClipAtTimeStamp(name, ts, dur);
                         })
                     .def_property(
                         "clips",
                         [](const track_ptr_t& track) { return track->_clips_by_timestamp; },
                         [](track_ptr_t& track, const clipmap_t& val) { track->_clips_by_timestamp = val; })
                     .def_property(
                         "program",
                         [](const track_ptr_t& track) { return track->_program; },
                         [](track_ptr_t& track, prgdata_constptr_t val) { track->_program = val; })
                     .def_property(
                         "outputbus",[&](const track_ptr_t& track) { return track->_outbus; },
                         [](track_ptr_t& track, outbus_ptr_t val) { track->_outbus = val; })
                     .def("__repr__", [](track_ptr_t track) -> std::string {
                       std::ostringstream oss;
                       oss << "Track( clip_count: " << track->_clips_by_timestamp.size() << " )";
                       return oss.str();
                     });
  type_codec->registerStdCodec<track_ptr_t>(track_t);
  /////////////////////////////////////////////////////////////////////////////////
  auto sequence_t =
      py::class_<Sequence, sequence_ptr_t>(singmodule, "Sequence")
          .def(py::init<std::string>())
          .def("enqueue", [](sequence_ptr_t& seq, prgdata_constptr_t program) { seq->enqueue(program); })
          .def("createTrack", [](sequence_ptr_t& seq, std::string name) -> track_ptr_t { return seq->createTrack(name); })
          .def_property(
              "timebase",
              [](sequence_ptr_t seq) { return seq->_timebase; },
              [](sequence_ptr_t seq, timebase_ptr_t val) { seq->_timebase = val; })
          .def(
              "setTimeBaseForTime",
              [](sequence_ptr_t seq, float time, timebase_ptr_t tb) { 
                auto it = seq->_timebases.find(time);
                OrkAssert(it==seq->_timebases.end());
                seq->_timebases[time] = tb;
           })
          .def("__repr__", [](sequence_ptr_t seq) -> std::string {
            std::ostringstream oss;
            oss << "Sequence( track_count: " << seq->_tracks.size() << " )";
            return oss.str();
          });
  type_codec->registerStdCodec<sequence_ptr_t>(sequence_t);
  /////////////////////////////////////////////////////////////////////////////////
  auto sequencepb_t = py::class_<SequencePlayback, sequenceplayback_ptr_t>(singmodule, "SequencePlayback")
                          .def("__repr__", [](sequenceplayback_ptr_t seqpb) -> std::string {
                            std::ostringstream oss;
                            oss << "SequencePlayback( sequence: " << seqpb->_sequence->_name << " )";
                            return oss.str();
                          });
  type_codec->registerStdCodec<sequenceplayback_ptr_t>(sequencepb_t);
  /////////////////////////////////////////////////////////////////////////////////
  auto sequencer_t = py::class_<Sequencer, sequencer_ptr_t>(singmodule, "Sequencer")
                         .def("playSequence", [](sequencer_ptr_t& sequencer, sequence_ptr_t sequence, float timeoffset) -> sequenceplayback_ptr_t { //
                           return sequencer->playSequence(sequence,timeoffset);
                         });
  type_codec->registerStdCodec<sequencer_ptr_t>(sequencer_t);
}
///////////////////////////////////////////////////////////////////////////////
} // namespace ork::audio::singularity
///////////////////////////////////////////////////////////////////////////////
