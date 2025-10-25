from ork.app.application import ApplicationComponent

class MovieCaptureComponent(ApplicationComponent):

  def __init__(self,
               OUTPATH="/tmp/testmov.mp4",
               LEN=5,
               FPS=30,
               NUMFRAMES=30,
               PRESET="medium"):
    self.rencount = 0
    self.freerun = False
    self.output_path = OUTPATH
    self.fps = FPS
    self.numframes = NUMFRAMES
    self.numframesp1 = NUMFRAMES+1
    self.preset = PRESET

  ###############################################

  def _onAppInit(self,app,initdata):
    self.ezapp = app.ezapp

  ###############################################
    
  def _onGpuPostFrame(self, ctx):
    self.rencount += 1
    enable_movie = not self.freerun
    if self.freerun == False:
      self.np4 = self.numframes+4
      self.np5 = self.np4+1
      match self.rencount:
        case 4:
          if enable_movie:
            self.mcc = self.ezapp.enableMovieRecording( output_path=self.output_path,
                                                        preset=self.preset,
                                                        fps=self.fps,
                                                        max_queue_size=300, # how far ahead can renderer get ahead of encoder ?
                                                        audio_test_tone=False )
        case self.np4:
          if enable_movie:
            self.ezapp.finishMovieRecording()
        case self.np5:
          self.ezapp.signalExit()

