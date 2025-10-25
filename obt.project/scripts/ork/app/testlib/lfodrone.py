import time 
from ork.singularity import testlib
from ork.app.application import ApplicationComponent

##############################################

class LfoDroneComponent(ApplicationComponent):

  def __init__(self, notes=[12,24,36,48,60], fx_prog = "ShifterChorus"):
    self.notes = notes
    self.fx_prog = fx_prog
    self.voices = []

  ##############################################

  def _onSynthInit(self,synth):

    testlib.bindSynthToApp(synth,                    # synth instance
                           self,                     # app instance
                           initial_gain=-12.0,       # initial gain in dB
                           main_fx=self.fx_prog)     # main bus effect

    self.waveprog = testlib.WaveformsProgram()
    P = self.waveprog.program
    synth.programbus.uiprogram = P
    mods = None

    for note in self.notes:
      voice = synth.keyOn(note,127,P,mods)
      time.sleep(0.1)
      self.voices.append(voice)