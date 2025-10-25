from orkengine.core import CrcString

###############################################################################
# ComponentizedApplication
#  an 'application level ECS'
#
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################

class ComponentizedApplication(object):

  def __init__(self):
    self.app_components = {}
    self.components_sorted = []
    self.absolutetime = 0.0

  ##############################################
  # add an application component
  # name : string name of component
  # component_clazz : class of component to instantiate
  # kwargs : keyword args to pass to component constructor
  # return : component instance
  # components are stored in a dict by name
  # components are also stored in a sorted execution list
  ##############################################

  def addComponent(self,name,component_clazz,**kwargs):
    component = component_clazz(**kwargs)
    self.app_components[name] = component
    keys = self.app_components.keys()
    keys_sorted = sorted(keys)
    self.components_sorted = [self.app_components[key] for key in keys_sorted]
    component.app = self
    return component 

  ##############################################
  # get components by class
  # component_clazz : class of component to find
  # return : list of component instances matching class
  ##################################################

  def findComponentsByClass(self,component_clazz):
    components = []
    for component in self.components_sorted:
      if isinstance(component,component_clazz):
        components.append(component)
    return components

  ##################################################
  # get component by name
  # name : string name of component
  # return : component instance or None
  ##################################################

  def findComponentByName(self,name):
    return self.app_components.get(name,None)

  #########
  # application broadcast handlers
  #########

  def onAppInit(self,initdata):
    # invoked on main thread when the application is initialized
    # immediately before the main loop starts
    for component in self.components_sorted:
      component.onAppInit(self,initdata)
    for component in self.components_sorted:
      component.onAppLink(self,initdata)
  
  def onAppExit(self):
    # invoked on main thread when the application is exiting
    # immediately after main loop ends
    for component in self.components_sorted:
      component.onAppExit()

  #########
  # audio / synth broadcast handlers
  #########

  def onAudioInit(self,audiodev):
    # invoked on audio thread when the audio device is initialized
    # immediately before audio processing starts
    # onAudioInit is called before onSynthInit
    # onAudioInit is called before onGpuInit
    for component in self.components_sorted:
      component.onAudioInit(audiodev)
    for component in self.components_sorted:
      component.onAudioLink(audiodev)
      
  def onSynthInit(self,synth):
    # invoked on audio thread when the synth is initialized
    # immediately before audio processing starts
    for component in self.components_sorted:
      component.onSynthInit(synth)
    for component in self.components_sorted:
      component.onSynthLink(synth)

  #########
  # GPU / renderer broadcast handlers
  #########

  def onGpuInit(self,ctx):
    # invoked on main thread when the GPU context is initialized
    # immediately before the main loop starts
    for component in self.components_sorted:
      component.onGpuInit(ctx)
    for component in self.components_sorted:
      component.onGpuLink(ctx)
      
  def onGpuExit(self,ctx):
    # invoked on main thread when the GPU context is exiting
    # immediately after the main loop ends
    for component in self.components_sorted:
      component.onGpuExit(ctx)
      
  def onGpuUpdate(self,ctx):
    # invoked on main thread each frame to update GPU resources
    # immediately before pre-frame
    for component in self.components_sorted:
      component.onGpuUpdate(ctx)

  def onGpuPreFrame(self,ctx):
    # invoked on main thread each frame before rendering
    # immediately before rendering
    for component in self.components_sorted:
      component.onGpuPreFrame(ctx)

  def onGpuPostFrame(self,ctx):
    # invoked on main thread each frame after rendering
    # immediately after rendering
    for component in self.components_sorted:
      component.onGpuPostFrame(ctx)
      
  #########
  # simulation / update thread broadcast handlers
  #########

  def onUpdateInit(self):
    # invoked on update thread when the update loop is initialized
    # immediately before the update loop starts
    for component in self.components_sorted:
      component.onUpdateInit()
    for component in self.components_sorted:
      component.onUpdateLink()
      
  def onUpdate(self,updinfo):
    # invoked on update thread each update loop iteration

    self.absolutetime = updinfo.absolutetime

    for component in self.components_sorted:
      component.onUpdate(updinfo) 

  def onUpdateExit(self):
    # invoked on update thread when the update loop is exiting
    # immediately after the update loop ends
    for component in self.components_sorted:
      component.onUpdateExit()

      
  ##################################################
  # notify : notify all components of an event
  ##################################################

  def notify(self,eventid: CrcString,**kwargs):
    for component in self.components_sorted:
      component.onNotify(eventid,**kwargs)

################################################################################
# ApplicationComponent
#  superclass for application components
#  uses template method pattern
#  application calls onXXXX methods
#  subclasses override _onXXXX methods
################################################################################

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

  def onAppLink(self,app,initdata):
    self.app = app
    self.initdata = initdata
    self._onAppLink(app,initdata)

  def _onAppLink(self,app,initdata):
    pass

  ##############################################

  def onAppExit(self):
    self._onAppExit

  def _onAppExit(self):
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

  def onUpdateLink(self):
    self._onUpdateLink()

  def _onUpdateLink(self):
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

  def onAudioLink(self,audiodev):
    self._onAudioLink(audiodev)

  def _onAudioLink(self,audiodev):
    pass

  ##############################################

  def onSynthInit(self,synth):
    self._onSynthInit(synth)
    
  def _onSynthInit(self,synth):
    pass

  def onSynthLink(self,synth):
    self._onSynthLink(synth)
    
  def _onSynthLink(self,synth):
    pass

  ##############################################

  def onGpuInit(self,ctx):
    self._onGpuInit(ctx)

  def _onGpuInit(self,ctx):
    pass

  def onGpuLink(self,ctx):
    self._onGpuLink(ctx)

  def _onGpuLink(self,ctx):
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
