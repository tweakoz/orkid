////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "pyext.h"
#include <ork/lev2/aud/audiodevice.h>
#include <ork/lev2/aud/singularity/keyon_prof.h>
#include <ork/lev2/aud/stream/audiodevice_stream.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
    void pyinit_aud_device(py::module& lev2_module) {
    /////////////////////////////////////////////////////////////////////////////////
    auto type_codec = python::pb11_typecodec_t::instance();
    /////////////////////////////////////////////////////////////////////////////////
    // AudioDeviceInfo
    /////////////////////////////////////////////////////////////////////////////////
    auto auddevinfo_t = py::class_<AudioDeviceInfo, audiodeviceinfo_ptr_t>(lev2_module, "AudioDeviceInfo")
        .def_readonly("name", &AudioDeviceInfo::_name)
        .def_readonly("input_short_id", &AudioDeviceInfo::_input_short_id)
        .def_readonly("output_short_id", &AudioDeviceInfo::_output_short_id)
        .def_readonly("max_input_channels", &AudioDeviceInfo::_max_input_channels)
        .def_readonly("max_output_channels", &AudioDeviceInfo::_max_output_channels)
        .def_readonly("sample_rate", &AudioDeviceInfo::_sample_rate)
        .def_readonly("device_index", &AudioDeviceInfo::_device_index)
        .def_property_readonly("supported_input_rates", [](audiodeviceinfo_ptr_t info) -> py::list {
          py::list result;
          for (auto r : info->_supported_input_rates) {
            result.append(r);
          }
          return result;
        })
        .def_property_readonly("supported_output_rates", [](audiodeviceinfo_ptr_t info) -> py::list {
          py::list result;
          for (auto r : info->_supported_output_rates) {
            result.append(r);
          }
          return result;
        })
        .def("__repr__", [](audiodeviceinfo_ptr_t info) -> std::string {
          return FormatString("AudioDeviceInfo(name='%s', in=%d, out=%d, sr=%g)",
                              info->_name.c_str(),
                              info->_max_input_channels,
                              info->_max_output_channels,
                              info->_sample_rate);
        });
    type_codec->registerStdCodec<audiodeviceinfo_ptr_t>(auddevinfo_t);
    /////////////////////////////////////////////////////////////////////////////////
    // enumerateAudioDevices
    /////////////////////////////////////////////////////////////////////////////////
    lev2_module.def("enumerateAudioDevices", []() -> py::list {
      // Release the GIL while probing host audio devices — Pa_Initialize /
      // Pa_IsFormatSupported can block for tens/hundreds of ms on Linux.
      audiodeviceinfo_list_t devices;
      {
        py::gil_scoped_release _release;
        devices = enumerateAudioDevices();
      }
      py::list result;
      for (const auto& dev : devices) {
        result.append(dev);
      }
      return result;
    });
    lev2_module.def("findAudioDeviceByShortId", [](const std::string& short_id) -> audiodeviceinfo_ptr_t {
      return findAudioDeviceByShortId(short_id);
    }, py::arg("short_id"));
    /////////////////////////////////////////////////////////////////////////////////
    // realtime device telemetry — the xrun counter the audio callback publishes.
    // always live (not gated on ORKID_PA_DIAG); zero on devices that never run a
    // host callback (STREAM/NULL).
    /////////////////////////////////////////////////////////////////////////////////
    lev2_module.def("audioDiagCounters", []() -> py::dict {
      const auto& ctrs = audioDiagCounters();
      py::dict rval;
      rval["underflows"]        = ctrs._underflows.load(std::memory_order_relaxed);
      rval["callbacks"]         = ctrs._callbacks.load(std::memory_order_relaxed);
      rval["frames_per_buffer"] = ctrs._frames_per_buffer.load(std::memory_order_relaxed);
      rval["sample_rate"]       = ctrs._sample_rate.load(std::memory_order_relaxed);
      return rval;
    });
    /////////////////////////////////////////////////////////////////////////////////
    // note-on phase attribution (ORKID_KEYON_PROF). prints the table and CLEARS the
    // accumulators. the realtime path emits it from the device's diag window, which a
    // consumer-pumped STREAM run never reaches — so the table is only readable off a
    // hardware device without this hook. no-op unless the env gate was set at load.
    /////////////////////////////////////////////////////////////////////////////////
    lev2_module.def("audioKeyOnProfReport", []() {
      ::ork::audio::singularity::keyonProfReport();
      fflush(stdout);
    });
    /////////////////////////////////////////////////////////////////////////////////
    auto auddev_t = py::class_<AudioDevice, audiodevice_ptr_t>(lev2_module, "AudioDevice"); //
    type_codec->registerStdCodec<audiodevice_ptr_t>(auddev_t);
    /////////////////////////////////////////////////////////////////////////////////
    auto audinpsrc_t = py::class_<AudioInputChunkSource, audioinputchunk_source_ptr_t>(lev2_module, "AudioInputChunkSource"); //
    type_codec->registerStdCodec<audioinputchunk_source_ptr_t>(audinpsrc_t);
    /////////////////////////////////////////////////////////////////////////////////
    auto straudinpsrc_t = py::class_<StreamingAudioInputChunkSource, AudioInputChunkSource, audiostreaminginputchunk_source_ptr_t>(lev2_module, "StreamingAudioInputChunkSource")
        .def_property_readonly("current_playback_timestamp", [](audiostreaminginputchunk_source_ptr_t src) -> double {
          return src->_current_playback_timestamp.load(std::memory_order_relaxed);
        });
    type_codec->registerStdCodec<audiostreaminginputchunk_source_ptr_t>(straudinpsrc_t);
    /////////////////////////////////////////////////////////////////////////////////
    // AudioFrameCapture
    /////////////////////////////////////////////////////////////////////////////////
    auto audioframecap_t = py::class_<AudioFrameCapture, audioframecapture_ptr_t>(lev2_module, "AudioFrameCapture")
        .def(py::init<>())
        .def_readonly("left", &AudioFrameCapture::_left)
        .def_readonly("right", &AudioFrameCapture::_right)
        .def_readonly("num_samples", &AudioFrameCapture::_num_samples)
        .def_readonly("sample_rate", &AudioFrameCapture::_sample_rate)
        .def_readonly("timestamp", &AudioFrameCapture::_timestamp);
    type_codec->registerStdCodec<audioframecapture_ptr_t>(audioframecap_t);
    /////////////////////////////////////////////////////////////////////////////////
    // StrAudioDevice
    /////////////////////////////////////////////////////////////////////////////////
    py::enum_<StrAudioDevice::Mode>(lev2_module, "StrAudioDeviceMode")
        .value("ASYNC_REALTIME", StrAudioDevice::Mode::ASYNC_REALTIME)
        .value("SYNC_NONREALTIME", StrAudioDevice::Mode::SYNC_NONREALTIME)
        .export_values();
       
    using strauddec_ptr_t = std::shared_ptr<StrAudioDevice>;
    auto straudiodev_t = py::class_<StrAudioDevice, AudioDevice, strauddec_ptr_t>(lev2_module, "StrAudioDevice")
        .def("advanceTime", &StrAudioDevice::advanceTime, py::arg("dt_seconds"))
        .def("extractSamples", &StrAudioDevice::extractSamples, py::arg("num_samples"))
        .def("availableSamples", &StrAudioDevice::availableSamples)
        .def("currentTime", &StrAudioDevice::currentTime)
        .def_readwrite("mode", &StrAudioDevice::_mode);
    type_codec->registerStdCodec<strauddec_ptr_t>(straudiodev_t);
    }
///////////////////////////////////////////////////////////////////////////////
} //namespace ork::lev2 {
