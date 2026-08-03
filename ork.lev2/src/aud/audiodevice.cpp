////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/lev2/aud/audiodevice.h>
#include <ork/lev2/aud/audio_numa.h>
#include <ork/file/file.h>
#include <ork/file/path.h>
#include <ork/file/chunkfile.h>
#include <ork/kernel/string/StringBlock.h>
#include <ork/kernel/orklut.hpp>
#include <ork/kernel/fixedlut.hpp>
#include <ork/kernel/tempstring.h>
#include <ork/asset/FileAssetLoader.h>
#include <ork/asset/FileAssetNamer.h>
#include <ork/kernel/netpacket_serdes.inl>
#include <ork/kernel/environment.h>
#include <ork/math/audiomath.h>
#include <ork/reflect/properties/register.h>
#include <ork/util/logger.h>
#include <map>
#include <functional>
#include <algorithm>
#include <pthread.h>
#include <sched.h>
#include <cstring>
#include <cerrno>
#if defined(__linux__)
#include <sys/syscall.h>
#include <unistd.h>
#endif

///////////////////////////////////////////////////////////////////////////////

#include "null/audiodevice_null.h"
///////////////////////////////////////
#if defined(ENABLE_ALSA)
#include "alsa/audiodevice_alsa.h"
#endif
///////////////////////////////////////
#if defined(ENABLE_PORTAUDIO)
#include "portaudio/audiodevice_pa.h"
#endif
///////////////////////////////////////
#if defined(ENABLE_PIPEWIRE)
#include "pipewire/audiodevice_pipewire.h"
#endif
///////////////////////////////////////
#if defined(ENABLE_CORE_AUDIO)
#include "coreaudio/CoreAudioDevice.h"
#endif
///////////////////////////////////////
#include <ork/lev2/aud/stream/audiodevice_stream.h>
///////////////////////////////////////

bool gb_audio_filter = false;

using namespace ork::audiomath;

///////////////////////////////////////////////////////////////////////////////

template class ork::orklut<int, ork::PoolString>;

///////////////////////////////////////////////////////////////////////////////

namespace ork { namespace lev2 {

///////////////////////////////////////////////////////////////////////////////

AudioInputChunk::AudioInputChunk(size_t numch) 
 : _chunk_index(0)
 , _num_frames(0) {
 setNumChannels(numch);
}

///////////////////////////////////////////////////////////////////////////////

void AudioInputChunk::setNumChannels(size_t channel_count){
  _channels.resize(channel_count);
}

///////////////////////////////////////////////////////////////////////////////

static logchannel_ptr_t logchan_audiort = logger()->configureChannel("audio.RT", fvec3(1, 0.85, 0.35), true);

AudioDiagCounters& audioDiagCounters() {
  static AudioDiagCounters _counters;
  return _counters;
}

///////////////////////////////////////////////////////////////////////////////

static std::string _schedPolicyName(int policy) {
  switch (policy) {
    case SCHED_FIFO:
      return "SCHED_FIFO";
    case SCHED_RR:
      return "SCHED_RR";
    case SCHED_OTHER:
      return "SCHED_OTHER";
    default:
      return FormatString("SCHED_<%d>", policy);
  }
}

#if defined(__linux__)
static std::string _threadAffinityString() {
  cpu_set_t cpus;
  CPU_ZERO(&cpus);
  if (0 != pthread_getaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpus)) {
    return "unknown";
  }
  std::string result;
  int count = 0;
  for (int i = 0; i < CPU_SETSIZE; i++) {
    if (CPU_ISSET(i, &cpus)) {
      count++;
      if (count <= 8) {
        result += result.empty() ? FormatString("%d", i) : FormatString(",%d", i);
      }
    }
  }
  if (count > 8) {
    result += FormatString(",...(%d cpus)", count);
  }
  return result.empty() ? "none" : result;
}
#endif

///////////////////////////////////////////////////////////////////////////////
// elevateAudioThread — see audiodevice.h.
//
// linux: PortAudio's ALSA host api leaves its callback thread at SCHED_OTHER
//  unless PaAlsa_EnableRealtimeScheduling() was called, and even then it only
//  boosts to SCHED_FIFO priority 1 (pa_unix_util.c BoostPriority) — not enough
//  headroom for a 256-frame / 5.33ms deadline. So the engine elevates the
//  thread itself. This needs RLIMIT_RTPRIO >= priority (granted per-group in
//  /etc/security/limits.d) or CAP_SYS_NICE; without it pthread_setschedparam
//  returns EPERM and the thread stays SCHED_OTHER — running, deadline-fragile,
//  and LOUD about it.
// darwin: CoreAudio's HAL already schedules device IOProc threads under
//  THREAD_TIME_CONSTRAINT_POLICY (Apple TN2169 "Audio Latency"), which is
//  strictly stronger than anything pthread scheduling can request; SCHED_FIFO
//  must NOT be imposed on it. Threads we create ourselves on darwin would be
//  elevated with thread_policy_set(mach_thread_self(),
//  THREAD_TIME_CONSTRAINT_POLICY, ...) instead — the host-api callback thread
//  is not ours, so this path only reports.
//
// The logging here runs on the audio thread, but exactly once, on the first
// callback (stream-open, before the tour has any load) — never in steady state.
///////////////////////////////////////////////////////////////////////////////

void elevateAudioThread(const char* thread_name, int priority) {

  int entry_policy = 0;
  sched_param entry_param{};
  pthread_getschedparam(pthread_self(), &entry_policy, &entry_param);

#if defined(__linux__)

  // confine to the audio pools' node BEFORE the affinity is reported, so the
  //  one line below tells the truth about where this thread may run.
  audioNumaPinThreadToHomeNode(thread_name);

  const long tid       = syscall(SYS_gettid);
  const auto affinity  = _threadAffinityString();
  const int prio_min   = sched_get_priority_min(SCHED_FIFO);
  const int prio_max   = sched_get_priority_max(SCHED_FIFO);
  const int want_prio  = std::clamp(priority, prio_min, prio_max);

  sched_param want_param{};
  want_param.sched_priority = want_prio;
  const int rc              = pthread_setschedparam(pthread_self(), SCHED_FIFO, &want_param);

  int achieved_policy = 0;
  sched_param achieved_param{};
  pthread_getschedparam(pthread_self(), &achieved_policy, &achieved_param);

  if (0 == rc) {
    logchan_audiort->log(
        "thread<%s> tid<%ld> RT-ELEVATED: entry<%s prio %d> achieved<%s prio %d> affinity<%s>",
        thread_name,
        tid,
        _schedPolicyName(entry_policy).c_str(),
        entry_param.sched_priority,
        _schedPolicyName(achieved_policy).c_str(),
        achieved_param.sched_priority,
        affinity.c_str());
  } else {
    logerrchannel()->log(
        "thread<%s> tid<%ld> RT-ELEVATION DENIED: requested<SCHED_FIFO prio %d> rc<%d:%s> "
        "achieved<%s prio %d> affinity<%s> - audio runs at normal priority and WILL miss "
        "deadlines under load; grant realtime scheduling to this user "
        "(/etc/security/limits.d, e.g. '@audio - rtprio 95' + 'usermod -aG audio <user>')",
        thread_name,
        tid,
        want_prio,
        rc,
        strerror(rc),
        _schedPolicyName(achieved_policy).c_str(),
        achieved_param.sched_priority,
        affinity.c_str());
  }

#elif defined(__APPLE__)

  logchan_audiort->log(
      "thread<%s> RT-ELEVATION SKIPPED (darwin): achieved<%s prio %d> - the CoreAudio HAL "
      "already runs device IOProc threads under THREAD_TIME_CONSTRAINT_POLICY; requested "
      "band<%d> is advisory only on this platform",
      thread_name,
      _schedPolicyName(entry_policy).c_str(),
      entry_param.sched_priority,
      priority);

#else

  logerrchannel()->log(
      "thread<%s> RT-ELEVATION UNSUPPORTED on this platform: achieved<%s prio %d>",
      thread_name,
      _schedPolicyName(entry_policy).c_str(),
      entry_param.sched_priority);

#endif
}

///////////////////////////////////////////////////////////////////////////////

static audiodevice_ptr_t g_audio_device = nullptr;

struct AudioDevFactory {

  AudioDevFactory(appinitdata_wkptr_t aid){

    std::string default_device_type = "PORTAUDIO";
    #if defined(ENABLE_CORE_AUDIO)
    default_device_type = "COREAUDIO";
    #endif

    std::string device_type = default_device_type;
    if( auto appinitd = aid.lock() ){
      if( appinitd->_audio_ioclass != "default" ){
        device_type = appinitd->_audio_ioclass;
      }
    }

    #if defined(ENABLE_ALSA)
    if( device_type == "ALSA" ){
      _device = std::make_shared<AudioDeviceAlsa>(aid);
    }
#endif
#if defined(ENABLE_CORE_AUDIO)
    if( device_type == "COREAUDIO" ){
      _device = std::make_shared<ca::CoreAudioDevice>(aid);
    }
#endif
#if defined(ENABLE_PORTAUDIO)
    if( device_type == "PORTAUDIO" ){
      _device = std::make_shared<AudioDevicePa>(aid);
    }
#endif
#if defined(ENABLE_PIPEWIRE)
    if( device_type == "PIPEWIRE" ){
      _device = std::make_shared<pipewire::AudioDevicePipeWire>(aid);
    }
#endif
    // only a real device carries a hardware deadline; STREAM and NULL are
    //  pumped by their consumer, so the NUMA hardening (see audio_numa.h) must
    //  not tax them - nor perturb an offline render with a 2GB page migration.
    const bool is_realtime_device = (nullptr != _device);

    if( device_type == "STREAM" ){
      _device = std::make_shared<StrAudioDevice>(aid);
    }

    if(nullptr == _device ){
      _device = std::make_shared<AudioDeviceNULL>(aid);
    }
    //printf("AudioDevFactory: audio ioclass<%s>\n",device_type.c_str());

    // before the backend's startup() builds the synth: MCL_FUTURE is what makes
    //  this cover the dsp pools that do not exist yet.
    audioNumaSetRealtimeDevice(is_realtime_device);
    audioNumaLockMemory();

    g_audio_device = _device;
  }

  audiodevice_ptr_t _device;
};

using audiodevfactory_ptr_t = std::shared_ptr<AudioDevFactory>;

audiodevice_ptr_t AudioDevice::createInstance(appinitdata_wkptr_t aid) {
  AudioDevFactory devf(aid);
  return devf._device;
}

///////////////////////////////////////////////////////////////////////////////

audiodevice_ptr_t AudioDevice::createNullInstance(appinitdata_wkptr_t aid) {
  auto dev        = std::make_shared<AudioDeviceNULL>(aid);
  g_audio_device  = dev;
  return dev;
}

///////////////////////////////////////////////////////////////////////////////

audiodevice_ptr_t AudioDevice::getInstance() {
  return g_audio_device;
}

///////////////////////////////////////////////////////////////////////////////

AudioDevice::AudioDevice(appinitdata_wkptr_t appinitd)
  : _appinitdata(appinitd) {
  _vars = std::make_shared<varmap::VarMap>();
}

AudioDevice::~AudioDevice() {
}

void AudioDevice::startup() {}
void AudioDevice::shutdown() {}

///////////////////////////////////////////////////////////////////////////////

void StreamingAudioInputChunkSource::start() {

}
void StreamingAudioInputChunkSource::stop() {

}
lev2::audioinputchunk_ptr_t StreamingAudioInputChunkSource::getChunk() {
  lev2::audioinputchunk_ptr_t chunk = nullptr;
  _inputqueue.try_pop(chunk);
  return chunk;
}

///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// Platform-specific enumeration functions are defined in their respective files
// (coreaudio/, portaudio/, pipewire/, alsa/)
///////////////////////////////////////////////////////////////////////////////

#if defined(ENABLE_CORE_AUDIO)
audiodeviceinfo_list_t enumerateAudioDevices_coreaudio();
#endif
#if defined(ENABLE_PORTAUDIO)
audiodeviceinfo_list_t enumerateAudioDevices_portaudio();
#endif
#if defined(ENABLE_PIPEWIRE)
audiodeviceinfo_list_t enumerateAudioDevices_pipewire();
#endif
#if defined(ENABLE_ALSA)
audiodeviceinfo_list_t enumerateAudioDevices_alsa();
#endif

// Generate stable 4-char base36 hash from direction, name, sample rate, and channels
static std::string _makeShortIdHash(const std::string& direction, const std::string& name, double sr, int channels) {
  std::string input = direction + ":" + name + ":" + std::to_string(int(sr)) + ":" + std::to_string(channels);
  size_t h = std::hash<std::string>{}(input);

  static const char base36[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
  std::string hash;
  for (int i = 0; i < 4; i++) {
    hash += base36[h % 36];
    h /= 36;
  }
  return hash;
}

audiodeviceinfo_list_t enumerateAudioDevices() {
  audiodeviceinfo_list_t result;
#if defined(ENABLE_CORE_AUDIO)
  result = enumerateAudioDevices_coreaudio();
#elif defined(ENABLE_PORTAUDIO)
  result = enumerateAudioDevices_portaudio();
#elif defined(ENABLE_PIPEWIRE)
  result = enumerateAudioDevices_pipewire();
#elif defined(ENABLE_ALSA)
  result = enumerateAudioDevices_alsa();
#endif

  // Filter: inputs must have 1 or 2 channels, outputs must have 2 channels, SR must be 48000
  audiodeviceinfo_list_t filtered;
  for (auto& dev : result) {
    bool valid_input = (dev->_max_input_channels == 1 || dev->_max_input_channels == 2) &&
                       (int(dev->_sample_rate) == 48000);
    bool valid_output = (dev->_max_output_channels == 2) &&
                        (int(dev->_sample_rate) == 48000);

    if (valid_input || valid_output) {
      // Only keep channels that match the filter
      if (!valid_input) {
        dev->_max_input_channels = 0;
      }
      if (!valid_output) {
        dev->_max_output_channels = 0;
      }
      filtered.push_back(dev);
    }
  }
  result = filtered;

  // Assign short IDs based on stable hash (4-char only, no prefix)
  for (auto& dev : result) {
    if (dev->_max_input_channels > 0) {
      dev->_input_short_id = _makeShortIdHash("I", dev->_name, dev->_sample_rate, dev->_max_input_channels);
    }
    if (dev->_max_output_channels > 0) {
      dev->_output_short_id = _makeShortIdHash("O", dev->_name, dev->_sample_rate, dev->_max_output_channels);
    }
  }

  // Sort by short ID for stable ordering
  std::sort(result.begin(), result.end(), [](const auto& a, const auto& b) {
    std::string id_a = a->_input_short_id.empty() ? a->_output_short_id : a->_input_short_id;
    std::string id_b = b->_input_short_id.empty() ? b->_output_short_id : b->_input_short_id;
    return id_a < id_b;
  });

  return result;
}

///////////////////////////////////////////////////////////////////////////////

audiodeviceinfo_ptr_t findAudioDeviceByShortId(const std::string& short_id) {
  auto devices = enumerateAudioDevices();
  for (const auto& dev : devices) {
    if (dev->_input_short_id == short_id || dev->_output_short_id == short_id) {
      return dev;
    }
  }
  return nullptr;
}

///////////////////////////////////////////////////////////////////////////////

audiodeviceinfo_ptr_t retryAndDiagnoseShortId(const std::string& short_id, bool want_output) {
  // One fresh re-enumeration retry — covers a transiently busy device that has
  // since been released.
  auto devices = enumerateAudioDevices();
  for (const auto& dev : devices) {
    if (dev->_input_short_id == short_id || dev->_output_short_id == short_id) {
      return dev;
    }
  }

  // Still unresolved. Diagnose the "held by another process" case: a device
  // whose requested-direction id WOULD match, but whose requested direction is
  // currently zeroed (0 channels) while the other direction is live — the
  // signature of a device whose one side is held by PipeWire/JACK/etc. The
  // id-table assigns 2 channels for output, and 1 or 2 for input.
  const char* dir_tag                = want_output ? "O" : "I";
  const std::vector<int> probe_chans = want_output ? std::vector<int>{2} : std::vector<int>{2, 1};
  for (const auto& dev : devices) {
    int this_dir  = want_output ? dev->_max_output_channels : dev->_max_input_channels;
    int other_dir = want_output ? dev->_max_input_channels : dev->_max_output_channels;
    if (this_dir != 0 || other_dir == 0) {
      continue;
    }
    for (int ch : probe_chans) {
      if (_makeShortIdHash(dir_tag, dev->_name, dev->_sample_rate, ch) == short_id) {
        logerrchannel()->log(
            "audio device '%s' present but its %s side is unavailable (0 channels) "
            "while its %s side is live - possibly held by another process "
            "(e.g. PipeWire/JACK); short id '%s' cannot resolve this run",
            dev->_name.c_str(),
            want_output ? "output" : "input",
            want_output ? "input" : "output",
            short_id.c_str());
        return nullptr;
      }
    }
  }
  return nullptr;
}

///////////////////////////////////////////////////////////////////////////////

}} // namespace ork::lev2
