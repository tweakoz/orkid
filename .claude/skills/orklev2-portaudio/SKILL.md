---
name: orklev2-portaudio
description: Answer questions about orkid's PortAudio backend (cross-platform), callback-based audio, device selection by short ID, int16 input / float output, CPU load tracking, and how PortAudio drives the Singularity synth. Use when the user asks about PortAudio or cross-platform audio.
user-invocable: false
---

# Orkid PortAudio Backend Reference

When answering questions about the PortAudio audio backend in orkid, consult the files below. All under `ork.lev2/`.

## Key Files

| Component | File |
|-----------|------|
| AudioDevice Base | `inc/ork/lev2/aud/audiodevice.h` |
| AudioDevicePa | `src/aud/portaudio/audiodevice_pa.h` |
| Implementation | `src/aud/portaudio/audiodevice_pa.cpp` |
| Config Flags | `inc/ork/lev2/config.h` (ENABLE_PORTAUDIO) |

## Architecture

Single-callback model — PortAudio calls `patestCallback()` on its audio thread:

```
PortAudio Thread (patestCallback)
  ├─ Input: int16_t* → convert to float → AudioInputChunk → _input_handler
  ├─ synth->compute(numframes, inputbuffer)
  └─ Output: synth->_obuf._leftBuffer/_rightBuffer → interleave → float* output
```

## AudioDevicePa

- `_the_synth` — Singularity synth instance
- `PaImpl._stream` — PortAudio stream handle
- `PaImpl._input_override` / `_output_override` — device index overrides
- `PaImpl._actual_input_channels` — resolved channel count

## Callback (`patestCallback`)

**Input Processing:**
1. Check `_input_handler` callback
2. Convert int16 → float, handle stereo→mono mixing
3. Call handler with `AudioInputChunk`

**Output Processing:**
1. `synth->compute(framesPerBuffer, inputbuffer)`
2. Read `synth->_obuf._leftBuffer` / `_rightBuffer`
3. Interleave to float output buffer
4. Track CPU load: `Pa_GetStreamCpuLoad()`

## Configuration

| Parameter | Value |
|-----------|-------|
| Input Format | int16_t (paInt16) |
| Output Format | float (paFloat32) |
| Sample Rate | From `getSampleRate()` |
| Frames/Buffer | 128 (macOS), 256 (Linux) |

## Device Selection

- Resolves 4-char base36 short IDs to full device names
- Enumerates all PortAudio devices via `Pa_GetDeviceCount()`
- Matches by name and channel requirements
- `Pa_OpenStream()` with selected devices → `Pa_StartStream()`

## Platform Notes

- **macOS:** Default backend when CoreAudio not selected; 128 frame buffer for lower latency
- **Linux:** Available alongside ALSA and PipeWire; 256 frame buffer

## How to Answer

1. For PortAudio device: check `audiodevice_pa.h` and `.cpp`
2. For base interface: check `audiodevice.h`
3. For Singularity integration: consult **orklev2-audio** skill
