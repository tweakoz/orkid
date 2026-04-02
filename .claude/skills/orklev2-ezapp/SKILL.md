---
name: orklev2-ezapp
description: Answer questions about orkid's EzApp framework, OrkEzApp creation, main loop, EzTopWidget, secondary windows, refresh policies, GPU/update thread lifecycle, and Python app patterns. Use when the user asks about creating apps, the main loop, windows, refresh, or the EzApp lifecycle.
user-invocable: false
---

# Orkid EzApp Framework Reference

When answering questions about EzApp or application creation in orkid, consult these files.

## Key Files

| Component | Location |
|-----------|----------|
| EzApp Header | `ork.lev2/inc/ork/lev2/ezapp.h` |
| EzApp Impl | `ork.lev2/src/ezapp.cpp` |
| EzTopWidget | `ork.lev2/src/ezapp_topwidget.cpp` |
| EzMainWin | `ork.lev2/src/ezapp_mainwin.cpp` |
| Secondary Windows | `ork.lev2/src/ez_secondary_win.cpp` |
| Python Bindings | `ork.lev2/pyext/src/pyext_ezapp.cpp` |
| CTXBASE (refresh) | `ork.lev2/inc/ork/lev2/gfx/ctxbase.h` |
| Tests | `ork.lev2/pyext/tests/application/test_ezapp_creation.py` |

## Architecture Overview

### OrkEzApp Creation (Python)
```python
from orkengine import lev2

class MyApp:
  def onGpuInit(self, ctx): ...
  def onUpdate(self, updinfo): ...
  def onDraw(self, draw_event): ...

app = MyApp()
ezapp = lev2.OrkEzApp.create(app,
  width=1280, height=720,
  fullscreen=False,
  enable_audio=True,
  use_subsystems=True,
  target_ups=60.0,
  target_fps=60.0)

ezapp.mainThreadLoop()  # Blocks until exit
```

### Creation kwargs
- **Window**: `name`, `left`, `top`, `width`, `height`, `fullscreen`, `fullscreen_monitor`, `offscreen`
- **Graphics**: `enable_graphics`, `msaa`, `ssaa`, `disable_mouse_cursor`, `fsmouse`
- **Audio**: `enable_audio`, `enable_audio_input/output`, `enable_audio_synth`, `audio_input/output_devname`, `audio_stream_sync`
- **Timing**: `freerun`, `target_ups`, `target_fps`, `enable_freerun_ups/fps`, `enable_lockstep_ups/fps`
- **Advanced**: `use_subsystems`, `drm_mode_id`, `movie_output_path`, `rcfd`

### Callback Detection (hasattr-based)
```python
onAppInit()                    # Before main loop
onAppExit()                    # After main loop
onGpuInit(ctx)                 # GPU context ready
onGpuExit(ctx)                 # GPU cleanup
onGpuUpdate(ctx)               # Per GPU frame
onGpuPreFrame(ctx)             # Before draw
onGpuPostFrame(ctx)            # After draw
onUpdateInit()                 # Update thread start
onUpdateExit()                 # Update thread end
onUpdate(update_data)          # Per-frame logic (update thread)
onDraw(draw_event)             # Render frame (main thread)
onUiEvent(event)               # UI event handler
onAudioInit(audiodevice)       # Audio device ready
onAudioExit()                  # Audio cleanup
onSynthInit(synth)             # Synth ready
onSynthExit()                  # Synth cleanup
```

### Two Init Modes
1. **Legacy Ad-Hoc** (`use_subsystems=False`): direct inline GPU/audio init
2. **HFSM Subsystem** (`use_subsystems=True`): dependency-driven via subsystem graph

### Main Loop Modes
- **Freerun**: wall-clock timing, adaptive wait, independent update/render rates
- **Lockstep**: fixed timestep, deterministic, frame-request sync between threads

### EzTopWidget
- Root UI widget (`ui::Group` subclass)
- Owns `_topLayoutGroup` (layout root for UI)
- `enableUiDraw()` — sets up compositing pipeline (call once in onGpuInit)
- Handles resize, draws UI context, manages movie capture

### Properties
```python
ezapp.topWidget          # EzTopWidget
ezapp.topLayoutGroup     # LayoutGroup (UI root)
ezapp.uicontext          # ui.Context
ezapp.mainwin            # EzMainWin
ezapp.audio_device       # AudioDevice (if enabled)
ezapp.audio_synth        # Synth (if enabled)
ezapp.vars               # VarMap for user data
ezapp.timescale          # Speed multiplier (float)
```

### Refresh Policies
```python
ezapp.setRefreshPolicy(lev2.EREFRESH_FASTEST, -1)     # As fast as possible
ezapp.setRefreshPolicy(lev2.EREFRESH_WHENDIRTY, -1)   # Only when dirty
ezapp.setRefreshPolicy(lev2.EREFRESH_FIXEDFPS, 60)    # Fixed 60fps
```

### Secondary Windows
```python
popup = ezapp.createSecondaryWindow(
  width=800, height=600, x=200, y=150,
  title="Tool Window", decorated=True, resizable=True, floating=True)

uic = popup.ui_context
root = lev2.ui.LayoutGroup.create("popup_lg")
root.setRect(0, 0, popup.width, popup.height)
uic.top = root

# Build UI in root layout group...

popup.onClosed = lambda: print("Window closed")
popup.requestClose()  # Programmatic close
```

Config options: `width`, `height`, `x`, `y`, `title`, `decorated`, `resizable`, `floating`, `transparent`, `focusOnShow`

Popup shortcut:
```python
popup = ezapp.createPopupWindow(x, y, w, h, transparent=True)
```

### Granular Loop Control
```python
ezapp.mainThreadBegin()
while not done:
    ezapp.mainThreadIter()
ezapp.mainThreadEnd()
```

### Movie Recording
```python
settings = lev2.MovieCaptureSettings()
settings.output_path = "/tmp/output.mp4"
ezapp.enableMovieRecording(settings)
# ... run ...
ezapp.finishMovieRecording()
```

### Thread Model
- **Main Thread**: GPU context, rendering, UI events
- **Update Thread**: simulation logic (`onUpdate`)
- **Audio Thread**: audio processing (if enabled)
- GIL released during `mainThreadLoop()`, acquired per-callback

### Monitor Enumeration
```python
monitors = lev2.enumerateGlfwMonitors()
for mon in monitors:
    print(mon.name, mon.width, mon.height, mon.refresh_rate, mon.primary)
```

## How to Answer

1. For app creation: check `pyext_ezapp.cpp` for kwargs handling
2. For lifecycle order: trace callback registration in `pyext_ezapp.cpp`
3. For secondary windows: read `ez_secondary_win.cpp`
4. For refresh policies: check `ctxbase.h` ERefreshPolicy enum
5. For main loop internals: read `ezapp.cpp` mainThreadLoop methods
