#!/usr/bin/env ork.python
################################################################################
# Test for StrAudioDevice (Stream Audio Device)
# Tests both SYNC (deterministic/non-realtime) and ASYNC (realtime) modes
# Tests audio generation, capture, and extraction
################################################################################

import sys, os, time, json, math, signal
from pathlib import Path
from orkengine.core import vec2, vec3, vec4, mtx4, quat
from orkengine import lev2

os.environ["ORKID_AUDIO_IOCLASS"] = "STREAM"

################################################################################

class StrAudioTestApp(object):

    def __init__(self):
        super().__init__()
        self.freerun = False
        # Create EzApp with audio synth enabled in offscreen mode
        self.ezapp = lev2.OrkEzApp.create(
            self,
            enable_audio=True,
            enable_audio_output=True,
            enable_audio_synth=True,
            audio_stream_sync=True,
            enable_graphics=True,
            offscreen=not self.freerun,
            freerun=self.freerun,
            movie_output_path="/tmp/str_audio_test_movie.mp4",
            target_ups = 60.0,
            target_fps = 60.0,
            width=640,
            height=480
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
        self.capture_set = []
    
    ##############################################

    def onUpdate(self,updinfo):
      #print("onUpdate called")
      self.updcount += 1
      self.abstime = updinfo.absolutetime

    ##############################################
            
    def onGpuUpdate(self, ctx):
      # Called before each frame is rendered
      speed = 0.1
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
      fbi = ctx.FBI
      rtg = fbi.main_RTG

      for item in self.capture_set:
        if item.is_ready:
          #print("Capture complete")
          self.capture_set.remove(item)

      if rtg is not None:
        if not self.freerun:
          rtb = rtg.buffer(0)
          file_index = self.rencount%120
          capture_future = fbi.captureToFile(rtb, f"/tmp/capx_{file_index}.png")
          #print(f"Captured frame {self.rencount} to /tmp/capx_{self.rencount}.png")
          self.capture_set.append(capture_future)

      self.rencount += 1
      if(self.updcount%1200)==0:
        tsr = self.ezapp.total_samples_rendered
        tss = tsr/48000.0
        print(f"onUpdate count: {self.updcount} render count: {self.rencount} tss={tss:.3f} tsr={tsr}")

      # Exit after 600 frames
      if self.rencount >= 600:
        print(f"Reached {self.rencount} frames, sending SIGINT...")
        os.kill(os.getpid(), signal.SIGINT)

      #if not self.freerun:
      #  time.sleep(0.1) # simulate some cpu work

################################################################################

app = StrAudioTestApp()
app.ezapp.mainThreadLoop(on_iter=lambda : False)
