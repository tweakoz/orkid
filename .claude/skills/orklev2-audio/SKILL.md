---
name: orklev2-audio
description: Answer questions about orkid's Singularity synthesizer engine, DSP processing, oscillators, filters, envelopes, modulation, sampling, FM synthesis, audio output, and program/bank structure. Use when the user asks about audio, synth, DSP, or sound.
user-invocable: false
---

# Orkid Singularity Audio/Synth Engine Reference

When answering questions about audio or the synthesizer in orkid, consult the files below. All under `ork.lev2/`.

## Key Files

| Component | Header |
|-----------|--------|
| Synth Engine | `inc/ork/lev2/aud/singularity/synth.h` |
| Layer (Voice) | `inc/ork/lev2/aud/singularity/layer.h` |
| DSP Blocks/Pipeline | `inc/ork/lev2/aud/singularity/dspblocks.h` |
| Program/Bank Data | `inc/ork/lev2/aud/singularity/synthdata.h` |
| Oscillators | `inc/ork/lev2/aud/singularity/alg_oscil.h` |
| Filters | `inc/ork/lev2/aud/singularity/alg_filters.h` |
| Amplitude/Gain | `inc/ork/lev2/aud/singularity/alg_amp.h` |
| Nonlinear/Distortion | `inc/ork/lev2/aud/singularity/alg_nonlin.h` |
| Envelopes | `inc/ork/lev2/aud/singularity/envelope.h` |
| Controllers/LFO | `inc/ork/lev2/aud/singularity/controller.h` |
| Modulation Routing | `inc/ork/lev2/aud/singularity/modulation.h` |
| Sampler | `inc/ork/lev2/aud/singularity/sampler.h` |
| Wavetable | `inc/ork/lev2/aud/singularity/wavetable.h` |
| FM Operators | `inc/ork/lev2/aud/singularity/fmosc.h` |
| TX81Z Emulation | `inc/ork/lev2/aud/singularity/tx81z.h` |
| CZ1 Phase Distortion | `inc/ork/lev2/aud/singularity/cz1.h` |
| Delays | `inc/ork/lev2/aud/singularity/delays.h` |
| DSP Buffer | `inc/ork/lev2/aud/singularity/dspbuffer.h` |
| Key On/Off | `inc/ork/lev2/aud/singularity/konoff.h` |
| Types | `inc/ork/lev2/aud/singularity/krztypes.h` |
| Audio Device | `inc/ork/lev2/aud/audiodevice.h` |

## Architecture Overview

### Execution Model
- Voice-based polyphonic synthesis (up to 512 voices)
- Control rate: 1500 Hz (32 frames @ 48kHz)
- Sample rate: 48 kHz
- Per-voice DSP pipeline with stages in series

### Signal Flow
```
Voice -> Layer -> Algorithm (multi-stage)
  -> DspStage 0 [Block0, Block1, ...] -> DspStage 1 -> ... -> OutputBus
    -> Insert Effects -> Master EQ -> Audio Device
```

### Core Classes
- `synth` — main engine, voice allocation, output buses
- `Layer` — runtime voice instance, holds DSP state
- `LayerData` — voice config (algorithm, controllers, keymap, pitch)
- `Alg` / `AlgData` — algorithm container (up to 16 stages)
- `DspStage` / `DspStageData` — stack of up to 32 DSP blocks
- `DspBlock` / `DspBlockData` — individual DSP unit (abstract base)

### Program/Bank Structure
```
BankData
+-- programs (int -> ProgramData)
+-- keymaps (int -> KeyMapData)
+-- multisamples (int -> MultiSampleData)

ProgramData
+-- name, tags, monophonic, portamento
+-- _layerdatas[] (multiple layers per program)
    +-- LayerData
        +-- _algdata (DSP algorithm)
        +-- _ctrlBlock (controllers)
        +-- _kmpBlock (keymap for samples)
        +-- loKey/hiKey, loVel/hiVel (range)
        +-- output bus routing
```

### Oscillator Types
- SINE, SAW, SQUARE (band-limited with PolyBLEP)
- SINEPLUS, SAWPLUS, SWPLUSSHP (harmonically enriched)
- SHAPEMODOSC, PLUSSHAPEMODOSC (shape modulation)
- SYNCM / SYNCS (oscillator sync master/slave)
- SAMPLER (multi-sample playback with keymaps)
- TX81Z FM operators (4-op Yamaha emulation)
- CZ1 phase distortion

### Filter Types
- `TrapSVF` — state variable (LPF/HPF/BPF/Notch, trap-rule)
- `BiQuad` — 2nd-order IIR
- `OnePoleLoPass` / `OnePoleHiPass` — 1st order
- `ParaOne` — parametric EQ with shelving
- DSP blocks: LP2RES, LOPAS2, LPGATE, BANDPASS_FILT, NOTCH_FILT, etc.

### Envelope Types
- `RateLevelEnvData` — ADSR-style with segment times/levels
- `AsrData` — Attack-Sustain-Release
- `YmEnvData` — Yamaha: AR/D1/S/D2
- `TX81ZEnvData` — TX81Z with polynomial rate scaling

### Modulation System
```
DspParamData
+-- _coarse (base value)
+-- _fine (fine tuning)
+-- _keyTrack (keyboard follow)
+-- _velTrack (velocity follow)
+-- _mods (BlockModulationData)
    +-- _src1 (primary: LFO, envelope, etc.)
    +-- _src2 (depth modulation)
```

Custom evaluators: pitch, frequency, amplitude, or lambda-based.

### Key On/Off
```cpp
synth::keyOn(note, vel, program, keyonmods)
  -> Layer::keyOn(note, vel, layerdata, outputbus)
    -> Alg::keyOn() -> all DspBlocks, Controllers
```

## How to Answer

1. For DSP blocks: check `dspblocks.h` for base class, `alg_*.h` for specific types
2. For modulation: read `modulation.h` for routing, `controller.h` for LFO/envelope sources
3. For sample playback: read `sampler.h` for KeyMapData, MultiSampleData, SampleOscillator
4. For program structure: read `synthdata.h` for BankData/ProgramData hierarchy
