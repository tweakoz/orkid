###############################################################################
# ComponentizedApplication
#  an 'application level ECS'
#
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
#
################################################################################
# Canonical Initialization, Update, and Exit Sequence
#
# This documents the complete lifecycle of an OrkEzApp application, including:
# - Exact sequence order of all callbacks
# - Which thread each callback runs on
# - GIL (Global Interpreter Lock) behavior for Python parallelism
# - Threading interactions and synchronization points
#
# Reference: ork.lev2/pyext/src/pyext_ezapp.cpp, ork.lev2/src/ezapp.cpp
#
################################################################################
# INITIALIZATION SEQUENCE
################################################################################
#
# 1. Application.__init__() [PYTHON MAIN THREAD, GIL HELD]
#    - Add components via addComponent()
#    - Set ezapp_args for window configuration
#    - User code setup
#
# 2. Application.createEzApp() [PYTHON MAIN THREAD, GIL HELD]
#    - Creates OrkEzApp with merged args (defaults + ezapp_args)
#    - Broadcasts to components: onEzAppCreated(app, ezapp)
#      * Components can set up early UI (overlays, etc) before enableUiDraw()
#    - Calls setRefreshPolicy(RefreshFastest, 0)
#    - Calls enableUiDraw()
#    - Calls app template method: _onEzAppCreated()
#      * Default implementation calls _onUiInit()
#    - App overrides _onUiInit() to set up UI widgets
#
# 3. ezapp.mainThreadLoop() [PYTHON RELEASES GIL HERE]
#    Python releases GIL for C++ engine main loop (py::gil_scoped_release)
#    All subsequent callbacks re-acquire GIL individually
#    This allows UPDATE THREAD to run Python code in parallel with MAIN THREAD
#
# 4. Application.onAppInit(initdata) [MAIN THREAD, GIL ACQUIRED]
#    - First C++ engine callback after GIL release
#    - Broadcasts to components: onAppInit(app, initdata)
#      * Components initialize backends, resources
#    - Calls onAppLink() [see next]
#
# 5. Application.onAppLink() [MAIN THREAD, GIL ACQUIRED]
#    - Broadcasts to components: onAppLink(app, initdata)
#      * Components connect to other components, configure channels
#    - Calls app template method: _onAppLink()
#      * App configures component channels, connections
#
# 6. Application.onGpuInit(ctx) [MAIN/GPU THREAD, GIL ACQUIRED]
#    - Called from CtxGLFW::_runloopBegin()
#    - GPU context made current: ctx->makeCurrentContext()
#    - FontMan::gpuInit() called before user callback
#    - Broadcasts to components: onGpuInit(app, ctx), onGpuLink(app, ctx)
#    - Calls app template method: _onGpuInit(ctx)
#    - CRITICAL: Must complete before onUpdateInit (enforced by engine)
#
# 7. Application.onSynthInit(synth) [MAIN THREAD, GIL ACQUIRED]
#    - Called during GPU init phase if synrh enabled
#    - Called before onAudioInit during audio initialization
#    - Synth instance created and ready
#
# 8. Application.onAudioInit(audiodev) [MAIN THREAD, GIL ACQUIRED]
#    - Called during GPU init phase if audio enabled
#    - Audio system bringup: audio::singularity::synth::bringUp()
#    - onSynthInit() called first (if set)
#    - Then onAudioInit() called
#    - Finally audiodevice->startup()
#
# 9. [AUDIO/SYNTH THREADS SPAWNED HERE]
#    Separate C++ threads for audio and syntheeizer, runs concurrently with other threads
#    Thread name: "macos: CoreAudioThread"
#
# 10. [UPDATE THREAD SPAWNED HERE]
#    Separate C++ thread for update loop, runs concurrently with main thread
#    Thread name: "update"
#
# 11. Application.onUpdateInit() [UPDATE THREAD, GIL ACQUIRED]
#     - First callback in update thread
#     - Called AFTER onGpuInit completes (guaranteed by engine)
#     - Broadcasts to components: onUpdateInit(), onUpdateLink()
#     - App state flag set: KAPPSTATEFLAG_UPDRUNNING
#
################################################################################
# MAIN LOOP (Running Concurrently)
################################################################################
#
# MAIN/GPU THREAD LOOP [MAIN THREAD]:
#   Per iteration of CtxGLFW::_runloopIter():
#
#   A. Application.onGpuUpdate(ctx) [MAIN/GPU THREAD, GIL ACQUIRED]
#      - Called each render iteration
#      - GPU frame counter incremented
#      - Use for GPU resource updates
#
#   B. Application.onGpuPreFrame(ctx) [MAIN/GPU THREAD, GIL ACQUIRED]
#      - Would run before frame rendering
#
#   C. Application.onGpuPostFrame(ctx) [MAIN/GPU THREAD, GIL ACQUIRED]
#      - Would run after frame rendering (for image capture, etc.)
#
# UPDATE THREAD LOOP [UPDATE THREAD]:
#   While not KAPPSTATEFLAG_JOINING:
#
#   D. Application.onUpdate(updinfo) [UPDATE THREAD, GIL ACQUIRED]
#      - Called per logical update frame
#      - May run multiple times per render frame (or vice versa)
#      - Two execution modes:
#        * Async/Freerunning: Runs at target UPS (updates per second)
#        * Sync/Lockstep: Runs at fixed virtual time (deterministic)
#      - Broadcasts to components: onUpdate(updinfo)
#      - Calls app template method: _onUpdate(updinfo)
#      - Parameters: updinfo contains dt, abstime, frame counter
#
# EVENT THREAD (Event-driven) [MAIN/EVENT THREAD]:
#
#   E. Application.onUiEvent(event) [MAIN/EVENT THREAD, GIL ACQUIRED]
#      - Not time-based, triggered by UI events
#      - Must return ui::HandlerResult
#
################################################################################
# GIL AND PARALLELISM
################################################################################
#
# Python GIL Behavior:
#   1. mainThreadLoop() releases GIL (py::gil_scoped_release)
#   2. Each callback individually acquires GIL (py::gil_scoped_acquire)
#   3. GIL released again when callback returns
#   4. joinUpdate() releases GIL (py::gil_scoped_release)
#
# Maximizing Parallelism:
#   - MAIN THREAD and UPDATE THREAD can run C++ code in parallel
#   - Each C++ thread acquires GIL only when executing Python callbacks
#   - Keep callbacks short or in C++ to maximize concurrent execution
#   - Use C++ queues (_mainq, _updq, _conq) for inter-thread communication
#
# Thread-GIL Interactions:
#   MAIN THREAD:
#     - Initially owns GIL (Python thread)
#     - Releases GIL for mainThreadLoop
#     - Re-acquires GIL for each callback
#     - Callbacks: onAppInit, onGpuInit, onGpuUpdate, onDraw, onGpuExit
#
#   UPDATE THREAD:
#     - Spawned from C++, never initially owns GIL
#     - Acquires GIL only for Python callbacks
#     - Releases immediately after callback
#     - Callbacks: onUpdateInit, onUpdate, onUpdateExit
#
#   GPU CONTEXT:
#     - Tracked via ThreadGfxContext RAII wrapper
#     - makeCurrentContext() called before GPU operations
#
# State Synchronization:
#   - App state flags (atomic): KAPPSTATEFLAG_UPDRUNNING, KAPPSTATEFLAG_JOINING
#   - Serial queues: _mainq (main), _updq (update), _rthreadq (render)
#   - Concurrent queue: _conq (thread-safe)
#
################################################################################
# EXIT SEQUENCE
################################################################################
#
# Exit triggered by: window close, signalExit(), or Ctrl-C
#
# 1. Application state flag set: KAPPSTATEFLAG_JOINING [MAIN THREAD]
#    - Signals UPDATE THREAD to stop looping
#
# 2. Application.onUpdateExit() [UPDATE THREAD, GIL ACQUIRED]
#    - Last callback in update thread
#    - Broadcasts to components: onUpdateExit()
#    - Called when KAPPSTATEFLAG_JOINING detected
#
# 3. Application.onAudioExit() [UPDATE THREAD, GIL ACQUIRED]
#    - Called after onUpdateExit in update thread context
#    - Audio system shutdown
#
# 4. Application.onSynthExit() [UPDATE THREAD, GIL ACQUIRED]
#    - Called during audio exit
#    - Synth cleanup
#
# 5. [UPDATE THREAD JOIN] [MAIN THREAD, GIL RELEASED]
#    Update thread joins back to main thread
#    joinUpdate() called with GIL released for C++ synchronization
#    CRITICAL: Ensures onUpdateExit completes before onGpuExit
#
# 6. Application.onGpuExit(ctx) [MAIN/GPU THREAD, GIL ACQUIRED]
#    - Called from CtxGLFW::_runloopEnd()
#    - Called AFTER update thread joins (guaranteed by engine)
#    - GPU context made current
#    - Broadcasts to components: onGpuExit(ctx)
#    - Calls app template method: _onGpuExit(ctx)
#    - glfwDestroyWindow() called after user callback
#
# 7. Application.onAppExit() [MAIN THREAD, GIL ACQUIRED]
#    - Last callback, after everything else shuts down
#    - mainThreadLoop() has completed
#    - Broadcasts to components: onAppExit()
#    - Final cleanup
#
# 8. [MAIN THREAD RETURNS] [PYTHON MAIN THREAD, GIL HELD]
#    GIL fully reacquired, control returns to Python
#    Python interpreter can safely exit
#
################################################################################
# CRITICAL SEQUENCING CONSTRAINTS
################################################################################
#
# These orderings are enforced by the C++ engine:
#
# 1. onGpuInit MUST complete before onUpdateInit
#    - Update thread spawn deferred until after GPU init
#
# 2. onUpdateExit MUST complete before onGpuExit
#    - joinUpdate() called before CtxGLFW::_runloopEnd()
#
# 3. Audio initialization order: onSynthInit -> onAudioInit -> startup()
#
# 4. Audio shutdown in update thread context (after onUpdateExit)
#
################################################################################
# THREAD EXECUTION SUMMARY
################################################################################
#
# MAIN THREAD (Process origin, Python relinquishes to C++, GPU context):
#   onAppInit, onGpuInit, onAudioInit, onSynthInit
#   [LOOP] onGpuUpdate, onDraw, onUiEvent
#   onGpuExit, onAppExit
#
# UPDATE THREAD (C++ spawned, simulation):
#   onUpdateInit
#   [LOOP] onUpdate
#   onUpdateExit, onAudioExit, onSynthExit
#
################################################################################

import signal 
from orkengine.core import CrcString

################################################################################
class ComponentizedApplication(object):

  def __init__(self):
    self.app_components = {}
    self.components_sorted = []
    self.absolutetime = 0.0
    self.ezapp = None # will be set later
    self.initdata = None # will be set in onAppInit
    self.ezapp_args = {} # kwargs for OrkEzApp.create()
    def onCtrlC(signum, frame):
      print("signalling EXIT to ezapp")
      self.ezapp.signalExit()

    signal.signal(signal.SIGINT, onCtrlC)

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

  ##################################################
  # create ezapp
  # Creates OrkEzApp with stored ezapp_args
  # Subclasses can override to customize creation
  ##################################################

  def createEzApp(self):
    # import here to avoid circular dependency
    from orkengine import lev2

    # Set reasonable defaults
    default_args = {
      'left': 100,
      'top': 100,
      'width': 1280,
      'height': 720,
      'enable_freerun_ups': True,
      'enable_freerun_fps': True
    }

    # Merge user args with defaults (user args take precedence)
    args = {**default_args, **self.ezapp_args}

    # Create ezapp
    self.ezapp = lev2.OrkEzApp.create(self, **args)

    # Broadcast to components (for early UI setup like overlays)
    for component in self.components_sorted:
      component.onEzAppCreated(self, self.ezapp)

    # Standard setup (refresh policy and UI draw)
    self.ezapp.setRefreshPolicy(lev2.RefreshFastest, 0)
    self.ezapp.topWidget.enableUiDraw()

    # Call template method for subclass UI initialization
    self._onEzAppCreated()

    return self.ezapp

  def _onEzAppCreated(self):
    """Template method called after ezapp is created and configured

    This is where apps should call _onUiInit() to set up their UI widgets.
    """
    # Call UI initialization template method
    self._onUiInit()

  def _onUiInit(self):
    """Template method for UI initialization - override in subclasses"""
    pass

  #########
  # application broadcast handlers
  #########

  def onAppInit(self,initdata):
    # invoked on main thread when the application is initialized
    # immediately before the main loop starts
    self.initdata = initdata
    for component in self.components_sorted:
      component.onAppInit(self,initdata)
    # after all components initialized, call onAppLink
    self.onAppLink()

  def onAppLink(self):
    # invoked after all components have been initialized
    # broadcast to components first, then call app-level template method
    for component in self.components_sorted:
      component.onAppLink(self,self.initdata)
    # call app-level template method for subclass override
    self._onAppLink()

  def _onAppLink(self):
    # template method for subclasses to override
    pass

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
    self._onGpuInit(ctx)    

  def _onGpuInit(self,ctx):
    pass 

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

    self._onUpdate(updinfo)

  def _onUpdate(self,updinfo):
    pass

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
    pass

  ##############################################

  def onEzAppCreated(self,app,ezapp):
    self.app = app
    self.ezapp = ezapp
    self._onEzAppCreated(app,ezapp)

  def _onEzAppCreated(self,app,ezapp):
    pass

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
