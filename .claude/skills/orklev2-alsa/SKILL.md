---
name: orklev2-alsa
description: Answer questions about orkid's ALSA audio backend (Linux), dual-thread architecture (ALSA thread + synth thread), PCM device configuration, multi-buffer producer/consumer queues, int16 interleaving, and how ALSA drives the Singularity synth. Use when the user asks about ALSA or Linux audio output.
user-invocable: false
---

# Orkid ALSA Backend Reference

When answering questions about the ALSA audio backend in orkid, consult the files below. All under `ork.lev2/`.

## Key Files

| Component | File |
|-----------|------|
| AudioDevice Base | `inc/ork/lev2/aud/audiodevice.h` |
| AudioDeviceAlsa | `src/aud/alsa/audiodevice_alsa.h` |
| Implementation | `src/aud/alsa/audiodevice_alsa.cpp` |
| Config Flags | `inc/ork/lev2/config.h` (ENABLE_ALSA, Linux only) |

## Architecture

Dual-thread design with producer/consumer buffer queues:

```
Synth Thread                          ALSA Thread
───────────                           ───────────
pop empty buf ← _multibufProducer     pop filled buf ← _multibufConsumer
synth->compute()                      snd_pcm_writei(buf)
float→int16 convert                   handle XRUN (snd_pcm_prepare)
push filled buf → _multibufConsumer   push empty buf → _multibufProducer
```

## PrivateImplementation

| Member | Description |
|--------|-------------|
| `_synth` | Singularity synth instance |
| `_alsaThread` | Thread writing to PCM device |
| `_synthThread` | Thread running synth compute |
| `_float_buf` | DSP buffer (float) for synth output |
| `_numframes` | Buffer size (256 frames) |
| `_multibufProducer` | Queue of empty int16 buffers |
| `_multibufConsumer` | Queue of filled int16 buffers |
| `_execstate` | Atomic: 0=init, 1=running, 2=shutting_down, 3=stopped |

Buffer pool: 4 buffers (`KBUFFERCOUNT`) of 256 frames x 2 channels x int16.

## PCM Configuration

- **Device:** `"default"`
- **Format:** `SND_PCM_FORMAT_S16_LE` (signed 16-bit little-endian)
- **Channels:** 2 (stereo)
- **Sample Rate:** 48000 Hz
- **Access:** `SND_PCM_ACCESS_RW_INTERLEAVED`
- **Buffer Frames:** 256 (`DESIRED_NUMFRAMES`)

## Synth Thread

1. Pop empty buffer from producer queue
2. `synth->compute(numframes, float_buf)`
3. Read `synth->_obuf._leftBuffer` / `_rightBuffer`
4. Convert float → int16 with gain 16384.0
5. Interleave L/R into buffer
6. Push filled buffer to consumer queue

## ALSA Thread

1. Pop filled buffer from consumer queue
2. `snd_pcm_writei(pcm_handle, buf, numframes)`
3. Handle underrun (XRUN) with `snd_pcm_prepare()`
4. Return empty buffer to producer queue

## Device Enumeration

- Uses `snd_device_name_hint()` API
- Probes sample rates: 44100, 48000, 88200, 96000
- Filters for 1-2 input channels, 2 output channels, 48000 Hz

## How to Answer

1. For ALSA device: check `audiodevice_alsa.h` and `.cpp`
2. For base interface: check `audiodevice.h`
3. For Singularity integration: consult **orklev2-audio** skill
