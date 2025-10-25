from orkengine.core import CrcString

###############################################################################

class ComponentizedApplication(object):

  def __init__(self):
    self.app_components = {}
    self.components_sorted = []

  ##############################################

  def addComponent(self,name,component_clazz,**kwargs):
    component = component_clazz(**kwargs)
    self.app_components[name] = component
    keys = self.app_components.keys()
    keys_sorted = sorted(keys)
    self.components_sorted = [self.app_components[key] for key in keys_sorted]
    component.app = self
    return component 

  ##################################################

  def findComponentByClass(self,component_clazz):
    for component in self.components_sorted:
      if isinstance(component,component_clazz):
        return component
    return None

  ##################################################

  def findComponentByName(self,name):
    return self.app_components.get(name,None)

  ##############################################
  # broadcast handlers
  ##############################################

  def onAppInit(self,initdata):
    for component in self.components_sorted:
      component.onAppInit(self,initdata)
  
  def onAppExit(self):
    for component in self.components_sorted:
      component.onAppExit()

  def onGpuInit(self,ctx):
    for component in self.components_sorted:
      component.onGpuInit(ctx)
      
  def onGpuExit(self,ctx):
    for component in self.components_sorted:
      component.onGpuExit(ctx)
      
  def onGpuUpdate(self,ctx):
    for component in self.components_sorted:
      component.onGpuUpdate(ctx)

  def onGpuPreFrame(self,ctx):
    for component in self.components_sorted:
      component.onGpuPreFrame(ctx)

  def onGpuPostFrame(self,ctx):
    for component in self.components_sorted:
      component.onGpuPostFrame(ctx)
      
  def onAudioInit(self,audiodev):
    for component in self.components_sorted:
      component.onAudioInit(audiodev)
      
  def onSynthInit(self,synth):
    for component in self.components_sorted:
      component.onSynthInit(synth)

  def onUpdateInit(self):
    for component in self.components_sorted:
      component.onUpdateInit()
      
  def onUpdateExit(self):
    for component in self.components_sorted:
      component.onUpdateExit()

  def onUpdate(self,updinfo):
    for component in self.components_sorted:
      component.onUpdate(updinfo) 
      
  ##################################################
  # notify : notify all components of an event
  ##################################################

  def notify(self,eventid: CrcString,**kwargs):
    for component in self.components_sorted:
      component.onNotify(eventid,**kwargs)

###############################################################################

class ApplicationComponent(object):
  def __init__(self):
    def onCtrlC(signum, frame):
      print("signalling EXIT to ezapp")
      self.ezapp.signalExit()

    signal.signal(signal.SIGINT, onCtrlC)

  ##############################################

  def onAppInit(self,app,initdata):
    self.app = app
    self.initdata = initdata
    self._onAppInit(app,initdata)

  def _onAppInit(self,app,initdata):
    pass

  ##############################################

  def onAppExit(self):
    self._onAppExit

  def _onAppExit(self):
    pass

  ##############################################

  def onLink(self):
    # where component link to other components
    self._onLink()
  def _onLink(self):
    pass

  ##############################################
  def onStart(self,app):
    # where component link to other components
    pass

  ##############################################

  def onUpdateInit(self):
    self._onUpdateInit()

  def _onUpdateInit(self):
    pass


  ##############################################

  def onUpdate(self,updinfo):
    self._onUpdate(updinfo)

  def _onUpdate(self,updinfo):
    pass

  ##############################################

  def onUpdateExit(self):
    self._onUpdateExit()

  def _onUpdateExit(self):
    pass


  ##############################################

  def onAudioInit(self,audiodev):
    self._onAudioInit(audiodev)

  def _onAudioInit(self,audiodev):
    pass

  ##############################################

  def onSynthInit(self,synth):
    self._onSynthInit(synth)
    
  def _onSynthInit(self,synth):
    pass

  ##############################################

  def onGpuInit(self,ctx):
    self._onGpuInit(ctx)

  def _onGpuInit(self,ctx):
    pass

  ##############################################

  def onGpuPreFrame(self,ctx):
    self._onGpuPreFrame(ctx)

  def _onGpuPreFrame(self,ctx):
    pass

  ##############################################

  def onGpuPostFrame(self,ctx):
    self._onGpuPostFrame(ctx)

  def _onGpuPostFrame(self,ctx):
    pass

  ##############################################

  def onGpuUpdate(self,ctx):
    self._onGpuUpdate(ctx)

  def _onGpuUpdate(self,ctx):
    pass

  ##############################################

  def onGpuExit(self,ctx):
    self._onGpuExit(ctx)

  def _onGpuExit(self,ctx):
    pass

  ##############################################

  def onNotify(self, eventid: str, **kwargs):
    self._onNotify(eventid, **kwargs)
    
  def _onNotify(self, eventid: str, **kwargs):
    pass
