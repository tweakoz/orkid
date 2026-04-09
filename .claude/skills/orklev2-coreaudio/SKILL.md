---
name: orklev2-coreaudio
description: Answer questions about orkid's CoreAudio backend (macOS), AudioUnit graph, HAL input/output callbacks, AuContext, CoreAudioDevice, stereo fragment pools, and how CoreAudio drives the Singularity synth engine. Use when the user asks about CoreAudio, AudioUnit, or macOS audio output.
user-invocable: false
---

# Orkid CoreAudio Backend Reference

When answering questions about the CoreAudio backend in orkid, consult the files below. All under `ork.lev2/`.

## Key Files

| Component | File |
|-----------|------|
| AudioDevice Base | `inc/ork/lev2/aud/audiodevice.h` |
| CoreAudioDevice | `src/aud/coreaudio/CoreAudioDevice.h` |
| CoreAudioDevice Impl | `src/aud/coreaudio/CoreAudioDevice.cpp` |
| AuContext (AudioUnit) | `src/aud/coreaudio/au.h`, `au.cpp` |
| I/O Callbacks | `src/aud/coreaudio/au_io.cpp` |
| Config Flags | `inc/ork/lev2/config.h` (ENABLE_CORE_AUDIO) |
| Factory | `src/aud/audiodevice.cpp` |

## Architecture

```
CoreAudioDevice
  └─ AuContext
       ├─ AUGraph (AudioUnit graph)
       │   ├─ _inputUnit (HAL input)
       │   └─ _outputUnit (HAL output)
       ├─ _inputProc() → LayerFragment → _inputQueue
       └─ _outputProc() ← StereoFragment ← _outputQueue
                              ↑
                    Singularity synth.compute()
```

## CoreAudioDevice

- `_the_synth` — Singularity synth instance
- `_aucontext` — AudioUnit context managing the graph
- `_inputDevList` / `_outputDevList` — enumerated CoreAudio devices
- `_actual_input_channels` — resolved input channel count

### Startup Flow
1. Get device info for input/output
2. Create Singularity synth instance
3. Create AuContext
4. `_aucontext->Init()` → build AudioUnit graph
5. `_aucontext->Start()` → begin audio processing

## AuContext

| Member | Description |
|--------|-------------|
| `_graph` | AUGraph (AudioUnit processing graph) |
| `_inputUnit` / `_outputUnit` | HAL AudioUnits |
| `_inputBuffer` | AudioBufferList for input |
| `_outputPool` / `_inputPool` | Object pools for audio fragments |
| `_outputQueue` / `_inputQueue` | MpMcBoundedQueue (4096 entries) |

### Graph Configurations
- `setupGraphForInputOnly()` — input-only HAL unit
- `setupGraphForOutputOnly()` — output-only HAL unit
- `setupGraphForIO()` — bidirectional I/O

## Callbacks (au_io.cpp)

### Input Callback (`_inputProc`)
1. `AudioUnitRender()` — pull data from HAL
2. De-interleave into `LayerFragment`
3. Push to `_inputQueue`
4. Delivered as `AudioInputChunk` to consumer

### Output Callback (`_outputProc`)
1. Timing synchronization with input device (sample timestamps)
2. Pop `StereoFragment` from `_outputQueue`
3. Interleave L/R channels into output buffer
4. Return buffer to pool

## Configuration

- **Sample Rate:** 48000 Hz (hardcoded)
- **Buffer Size:** `desired_framesize` (default 1024, configurable via env)
- **Format:** Float32 (CoreAudio native)
- **Channels:** Stereo output, mono/stereo input

## Connection to Singularity

The synth thread calls `synth->compute(numframes, inputbuffer)` which writes to `synth->_obuf._leftBuffer` / `_obuf._rightBuffer`. The output callback reads these into `StereoFragment` buffers queued for the HAL output.

## How to Answer

1. For CoreAudioDevice: check `CoreAudioDevice.h` and `.cpp`
2. For AudioUnit graph: check `au.h` and `au.cpp`
3. For I/O callbacks: check `au_io.cpp`
4. For base AudioDevice interface: check `audiodevice.h`
5. For Singularity integration: consult **orklev2-audio** skill
