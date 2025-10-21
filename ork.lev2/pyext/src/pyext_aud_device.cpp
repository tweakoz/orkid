////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "pyext.h"
#include <ork/lev2/aud/audiodevice.h>
#include <ork/lev2/aud/stream/audiodevice_stream.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
    void pyinit_aud_device(py::module& lev2_module) {
    /////////////////////////////////////////////////////////////////////////////////
    auto type_codec = python::pb11_typecodec_t::instance();
    /////////////////////////////////////////////////////////////////////////////////
    auto auddev_t = py::class_<AudioDevice, audiodevice_ptr_t>(lev2_module, "AudioDevice"); //
    type_codec->registerStdCodec<audiodevice_ptr_t>(auddev_t);
    /////////////////////////////////////////////////////////////////////////////////
    auto audinpsrc_t = py::class_<AudioInputChunkSource, audioinputchunk_source_ptr_t>(lev2_module, "AudioInputChunkSource"); //
    type_codec->registerStdCodec<audioinputchunk_source_ptr_t>(audinpsrc_t);
    /////////////////////////////////////////////////////////////////////////////////
    auto straudinpsrc_t = py::class_<StreamingAudioInputChunkSource, AudioInputChunkSource, audiostreaminginputchunk_source_ptr_t>(lev2_module, "StreamingAudioInputChunkSource"); //
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
