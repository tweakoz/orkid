# Orkid Audio Synthesizer (Singularity)

---

### Summary

Orkid's synthesizer is loosely inspired by Kurzeil VAST, in that it supports full digital modular synthesis. 

---

### Features

  - General purpose digital modular synthesizer
  - Can emulate a few hardware synths
    - Kurzweil K2000 (including .krz file import)
    - Soundfonts (.sf2 file import)
    - Casio CZx (including sysex bank import)
    - Yamaha Tx81z (including sysex bank import)
  - Emulations are converted to singularity modular format
    - this allows synth 'models' to be mixed freely.
  - A variety of DSP modules are included
    - Phase Modulation Oscillators (Tx81z style)
    - Phase Distortion Oscillators (CZ style)
    - A variety of analog emulation oscillators with/without antialiasing covering the usual, Sin, Saw, Pulse, Square, Triangle, with synchronization support.
    - Sample playback oscillators with loop, reverseloop, pingpong loop, etc..
    - Noise Oscillators
    - Nonlinear Operators (Waveshapers, Wrap, etc..)
    - Filters - Variety of Lowpass, BandPass, HighPass, Notch, Allpass filters.
    - Envelope Generators - variety of EG types.
    - Amplifier operators, MonoIO, Mono-StereoIO, Ring Modulators, Splitters, etc.. 
  - All DSPgraphs can be layered or split.
  - Flexible modulation routing (can apply to all modules that have modulation inputs)
    - LFO's, EG's, Sample/Hold, FUNS, etc..
    - Arbitrary modulation expressions supported.
    - Can modulate from c++ or python lambda functions.
  - Effects Section
    - delays
    - flangers
    - chorus
    - FDN reverb
    - EQ
    - distortion
    - Can link up individual effects into FX chains/graphs
  - Flexible MixBus architecture 
    - not limited to a single stereo bus, have as many as you need and can afford.
  - Output Devices
    - PortAudio
    - ALSA
    - PipeWire
    - NULL (for debug)


  ### Concepts

  - DataObjects (objects used to construct instance objects, immutable from audio thread)
    - DspBlockData     
    - DspStageData
    - AlgData
    - KmpBlockData
    - BlockModulationData
    - DspParamData
    - ControlBlockData
    - ControllerData
    - LayerData
    - ProgramData
    - BankData 
    - SynthData
    - KeyMap

  - InstanceObjects (objects used in audio processing, derived from data objects)
    - DspBlock
    - DspStage
    - Alg
    - IoMask
    - DspParam
    - ControlBlockInst
    - ControllerInst
    - Layer
    - Synth
    - DspBuffer
    - outputBuffer
    - KeyOnInfo
    - KeyOnModifiers
    - OutputBus

