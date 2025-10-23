#!/usr/bin/env ork.python
################################################################################
# Test for StrAudioDevice (Stream Audio Device)
# Tests both SYNC (deterministic/non-realtime) and ASYNC (realtime) modes
# Tests audio generation, capture, and extraction
################################################################################

import sys, os, time, json, math, signal, argparse
from pathlib import Path
from orkengine.core import vec2, vec3, vec4, mtx4, quat
from orkengine import lev2

os.environ["ORKID_LOG_ALWAYSFLUSH"] = "1"

parser = argparse.ArgumentParser()
parser.add_argument('--freerun', '-f', action='store_true', help='Enable freerun mode (async)')
args = parser.parse_args()

################################################################################

class StrAudioTestApp(object):

    def __init__(self):
        super().__init__()

        self.freerun = args.freerun
        self.FPS = 60.0 # frames per second
        self.LEN = 30.0  # seconds
        self.NUMFRAMES = int(self.FPS * self.LEN)
        self.NUMFRAMESP1 = self.NUMFRAMES + 1
        
        ########################################
        # lockstep mode ?, use STREAM audio device (for movie capture)
        ########################################

        if not self.freerun:          
          os.environ["ORKID_AUDIO_IOCLASS"] = "STREAM"

        ########################################
        # Create EzApp with audio synth enabled 
        # if lockstep, use offscreen mode
        ########################################

        self.ezapp = lev2.OrkEzApp.create(
            self,
            enable_audio=True,
            enable_audio_output=True,
            enable_audio_synth=True,
            audio_stream_sync=True,
            enable_graphics=True,
            offscreen=not self.freerun,
            freerun=self.freerun,
            target_ups = self.FPS,
            target_fps = self.FPS,
            width=1920,
            height=1080
        )

        ########################################
        # simple UI setup
        ########################################

        self.ezapp.topWidget.enableUiDraw()
        lg_group = self.ezapp.topLayoutGroup
        lg_group.clearColorGuide = vec4(0.8,0.6,0.2,1)
        self.griditems = lg_group.makeGrid(
          width=2,
          height=2,
          margin = 4,
          uiclass = lev2.ui.Box,
          args = ["label",vec4(0.1,0.1,0.3,1)],
        )
        lg_group.margin = 4
        
        ########################################

        self.str_audio = None
        self.current_test = 0
        self.updcount = 0
        self.rencount = 0
        self.abstime = 0.0
        
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
      self.abstime = updinfo.absolutetime

    ##############################################
            
    def onGpuUpdate(self, ctx):
      # Called before each frame is rendered
      speed = 1.0
      r0 = math.sin(speed*self.abstime*1.0*math.pi)*0.5 + 0.5
      g0 = math.sin(speed*self.abstime*1.31*math.pi)*0.5 + 0.5
      b0 = math.sin(speed*self.abstime*1.51*math.pi)*0.5 + 0.5
      r1 = math.sin(speed*self.abstime*1.71*math.pi)*0.5 + 0.5
      g1 = math.sin(speed*self.abstime*1.91*math.pi)*0.5 + 0.5
      b1 = math.sin(speed*self.abstime*2.11*math.pi)*0.5 + 0.5
      r2 = math.sin(speed*self.abstime*2.31*math.pi)*0.5 + 0.5
      g2 = math.sin(speed*self.abstime*2.51*math.pi)*0.5 + 0.5
      b2 = math.sin(speed*self.abstime*2.71*math.pi)*0.5 + 0.5
      r3 = math.sin(speed*self.abstime*2.91*math.pi)*0.5 + 0.5
      g3 = math.sin(speed*self.abstime*3.11*math.pi)*0.5 + 0.5
      b3 = math.sin(speed*self.abstime*3.31*math.pi)*0.5 + 0.5
      self.griditems[0].widget.color = vec4(r0,g0,b0,1)
      self.griditems[1].widget.color = vec4(r1,g1,b1,1)
      self.griditems[2].widget.color = vec4(r2,g2,b2,1)
      self.griditems[3].widget.color = vec4(r3,g3,b3,1)

    ##############################################

    def onGpuPostFrame(self, ctx):
      self.rencount += 1
      if self.freerun == False:
        match self.rencount:
          case 1:
            self.mcc = self.ezapp.enableMovieRecording( output_path="/tmp/str_audio_test_movie.mp4",
                                                        fps=self.FPS,
                                                        max_queue_size=180 )
          case self.NUMFRAMES:
            self.ezapp.finishMovieRecording()
          case self.NUMFRAMESP1:
            self.ezapp.signalExit()

################################################################################

app = StrAudioTestApp()
app.ezapp.mainThreadLoop(on_iter=lambda : False)
