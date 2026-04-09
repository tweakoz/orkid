---
name: orklev2-pipewire
description: Answer questions about orkid's PipeWire audio backend (Linux), SPA buffer processing, pw_stream callback, CPU load tracking, and how PipeWire drives the Singularity synth. Use when the user asks about PipeWire or modern Linux audio.
user-invocable: false
---

# Orkid PipeWire Backend Reference

When answering questions about the PipeWire audio backend in orkid, consult the files below. All under `ork.lev2/`.

## Key Files

| Component | File |
|-----------|------|
| AudioDevice Base | `inc/ork/lev2/aud/audiodevice.h` |
| AudioDevicePipeWire | `src/aud/pipewire/audiodevice_pipewire.h` |
| Implementation | `src/aud/pipewire/audiodevice_pipewire.cpp` |
| Config Flags | `inc/ork/lev2/config.h` (ENABLE_PIPEWIRE, Linux only) |

## Architecture

PipeWire stream callback model:

```
PipeWire Main Loop Thread (on_pw_process)
  ├─ Dequeue SPA buffer from pw_stream
  ├─ synth->compute(num_frames, input_buffer)
  ├─ Interleave L/R floats → output buffer
  ├─ Compute CPU load percentage
  └─ Re-queue buffer
```

## PrivateImplementation

| Member | Description |
|--------|-------------|
| `_synth` | Singularity synth instance |
| `_audioThread` / `_synthThread` | Processing threads |
| `_pipewire_stream` | `pw_stream*` handle |
| `_pipewire_main_loop` | `pw_main_loop*` |
| `_spa_buffer` | SPA buffer for output |
| `_input_buffer` | Float input buffer (2048 samples) |
| `_timer` | CPU load timing |

## Process Callback (`on_pw_process`)

1. Dequeue buffer from PipeWire stream
2. Calculate frames: `num_frames = maxsize / stride`
3. `synth->compute(num_frames, _input_buffer)`
4. Interleave `_obuf._leftBuffer` / `_rightBuffer` → output
5. Compute CPU load as percentage
6. Re-queue buffer

## Configuration

| Parameter | Value |
|-----------|-------|
| Format | Interleaved float (32-bit) |
| Channels | 2 (stereo) |
| Sample Rate | 48000 Hz |
| Frames/Buffer | 256 (`DESIRED_NUMFRAMES`) |

## How to Answer

1. For PipeWire device: check `audiodevice_pipewire.h` and `.cpp`
2. For base interface: check `audiodevice.h`
3. For Singularity integration: consult **orklev2-audio** skill
