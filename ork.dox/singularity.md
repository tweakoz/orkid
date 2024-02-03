# Orkid Audio Synthesizer (Singularity)

---

### Summary

Orkid's synthesizer is loosely inspired by Kurzeil VAST, in that it supports full digital modular synthesis. 

---

### Features

  - General purpose digital modular synthesizer
  - Can emulate a few hardware synths
    - [Kurzweil K2000 (including .krz file import)](https://www.youtube.com/watch?v=NcKFVNdrGdM)
    - Soundfonts (.sf2 file import)
    - Casio CZx (including sysex bank import)
    - Yamaha Tx81z (including sysex bank import)
  - Emulations are converted to singularity native modular format, This facilitates freform combination of synthesis models.
  - A variety of DSP modules are included
    - Phase Modulation Oscillators (Tx81z style)
    - Phase Distortion Oscillators (CZ style)
    - A variety of virtual analog oscillators with/without antialiasing covering the usual, Sin, Saw, Pulse, Square, Triangle, with synchronization support.
    - Sample playback oscillators with loop, reverseloop, pingpong loop, etc..
    - Noise Oscillators
    - TODO: Granular Synthesis, Speech Synthesis, Physical Modeling.
    - Nonlinear Operators (Waveshapers, Wrap, etc..)
    - Filters - Variety of Lowpass, BandPass, HighPass, Notch, Allpass filters.
    - Envelope Generators - variety of EG types.
    - Amplifier operators, MonoIO, Mono-StereoIO, 2D Panning, Ring Modulators, Splitters, etc.. 
  - All DSPgraphs can be layered or split.
  - Flexible modulation routing (can apply to all modules that have modulation inputs)
    - LFO's, EG's, Sample/Hold, FUNS, etc..
    - Arbitrary modulation expressions supported.
    - Can modulate from c++ or python lambda functions.
  - Effects Section
    - delays (static and variable)
    - flangers
    - chorus
    - FDN4/FDN8 reverb
    - Shelf/Parametric EQ
    - Distortion
    - Can link up individual effects into FX chains/graphs
  - Flexible MixBus architecture 
    - not limited to a single stereo bus, have as many as you need and can afford.
  - Output Devices
    - PortAudio
    - ALSA
    - PipeWire
    - NULL (for debug)
  - lev2 UI support for things like Oscilloscopes/Spectrum Analyzers/Control Signal Plotting for debugging.
  - Accessible from C++ and Python bindings.
  - Sequencer : [YoutubeVideo](https://www.youtube.com/watch?v=W2OUjiMKmN4)
  - 2D Surround : [YoutubeVideo](https://www.youtube.com/watch?v=vxivKOwVijI)

  ### Architecture

  ![Singularity Architecture:1](Singul.png)

  ### Definitions of data objects (live performance mutable objects)

 Data objects are used to construct instance objects.
 Data objects are immutable from audio thread.
 They are typically created/modified by user from UI or c++/python code

  - SynthData : (subclassed by import parsers) contains zero or more BankData's 
  - BankData : contains zero or more ProgramData's
  - ProgramData : contains zero or more LayerData
  - LayerData : contains an AlgData, ControlBlockData
  - AlgData : contains zero or more DspStageData's
  - DspStageData : contains zero or more DspBlockData's
  - DspBlockData : (subclassed) base class for all dsp block data objects
    - A few examples: PITCH_DATA, SAMPLER_DATA, SINE_DATA, SAW_DATA, SQUARE_DATA, SHAPEMODOSC_DATA, BANDPASS_FILT_DATA
    - has zero or more DspParamData which can dynamically alter the operation of the DspBlock (analogy: a gpu shader parameter)
  - DspParamData : basic parameter for a DspBlock
  - KmpBlockData : used by sampler - contains Keymap data
  - BlockModulationData : contains 2 ControllerData (sources), a ControllerData for Controller2 depth modulation and an "Evaluator"
  - Evaluator : c++ lambda function which combines controllers into a single value via some computation
  - ControlBlockData : contains zero or more ControllerData's
  - ControllerData : (subclassed) base class for controller data objects
     - A few examples: LfoData, FunData, RateLevelEnvData, NatEnvWrapperData, AsrData, YmEnvData, TX81ZEnvData
  - KeyMap : contains multisample layout with key, velocity splits, etc..

  ### Definitions of instance objects (live performance mutable objects)

  - Synth : collection of all layers/busses that get mixed to final outputs.
  - OutputBus : set of DSP channels which sum layers which are assigned to it. 
  - DspBuffer : set of parallel (n-channel) waveform buffers used for passing signals between DSP opertations in a stage.
  - outputBuffer
  - KeyOnInfo
  - KeyOnModifiers
  - Layer : A single "voice", generating a sound using an Algorithm, despite being a single "voice" - it can still within itself have multiple sound generation components. Layers are also used for Effects processing, they could either be an effect per voice, or non-keyed effect layers can be attached to output busses.
  - Algorithm : A stack of DspStages that implement a sound generation method, comprised of 1 or more DspStage's.
  - IoConfig : Input Output topology description for a DspStage (num inputs, num outputs)
  - DspStage : typically a Directed Acyclic Graph (DAG) of DspBlocks conforming to an IoConfig, with 1 or more inputs, 0 or more middle DspBlocks, and 1 or more output DspBlocks. Cycles can be permitted in some cases.
  - DspBlocks : Implementation of a specific DSP technique - can be sources, modifiers, or sinks. eg. PM Oscillator, Sample Playback Oscillator, Filter, Mono Output Amp, DelayLine, PitchShifter, Reverb, Chorus, etc.. Can have 0 or more DspParam's
  - DspParam : a parameter of a DspBlock that can be set, evaluated, modulated, etc..
  - BlockModulation : A set of up to 2 modulation sources (controller) that modulate a DspParam, coupled with an "evalutor" which handles block specific semantics.
  - ControlBlockInst
  - ControllerInst : a realtime data source routed into a BlockModulation, eg. LFO, EG, FUN, Keyboard Input, MIDI CC, custom lambda etc..
  - FUN : K2000 style function generator (supports functions like "a+b", "a-b", "(a+b)/2", "a*b", "quantize b to a", "lowpass(f=a,b)", "warp1(a,b)", etc..



