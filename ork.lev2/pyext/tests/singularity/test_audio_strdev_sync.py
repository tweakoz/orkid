#!/usr/bin/env ork.python
"""
Test for StrAudioDevice (Stream Audio Device)
Tests both SYNC (deterministic/non-realtime) and ASYNC (realtime) modes
Tests audio generation, capture, and extraction
"""

import sys, os, time, json
from pathlib import Path
from orkengine.core import *
from orkengine.lev2 import *

os.environ["ORKID_AUDIO_IOCLASS"] = "STREAM"

################################################################################

class StrAudioTestApp(object):

    def __init__(self):
        super().__init__()

        # Create EzApp with audio synth enabled in offscreen mode
        self.ezapp = OrkEzApp.create(
            self,
            enable_audio=True,
            enable_audio_output=True,
            enable_audio_synth=True,
            audio_stream_sync=True,
            enable_graphics=True,
            offscreen=True,
            freerun=False,
            target_ups = 60.0,
            target_fps = 60.0,
            width=640,
            height=480
        )

        self.str_audio = None
        self.current_test = 0
        self.updcount = 0
        self.rencount = 0

    ##############################################

    def onGpuInit(self, ctx):
        print("="*60)
        print("onGpuInit called - Audio should now be initialized")
        print("="*60)

        # Get the audio device (should be StrAudioDevice)
        self.str_audio = self.ezapp.audio_device
        synth = self.ezapp.audio_synth

        print(f"Audio device: {self.str_audio}")
        print(f"Audio device type: {type(self.str_audio)}")
        print(f"Audio synth: {synth}")

        assert(self.str_audio is not None)
        assert(synth is not None)

        #print(f"Device mode: {self.str_audio.mode}")
        print("✅ Audio system initialized successfully")
    
    ##############################################

    def onUpdate(self,updinfo):
      #print("onUpdate called")
      self.updcount += 1

    ##############################################
            
    def onGpuUpdate(self, ctx):
      # Called after each frame is rendered
      # In SYNC mode, this should be called in lockstep with audio processing
      #print("onGpuUpdate called")
      pass

    ##############################################

    def onGpuPostFrame(self, ctx):
      self.rencount += 1
      if(self.updcount%1200)==0:
        tsr = self.ezapp.total_samples_rendered
        tss = tsr/48000.0
        print(f"onUpdate count: {self.updcount} render count: {self.rencount} tss={tss:.3f} tsr={tsr}")
      pass

################################################################################

app = StrAudioTestApp()
app.ezapp.mainThreadLoop(on_iter=lambda : False)
