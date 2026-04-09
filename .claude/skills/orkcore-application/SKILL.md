---
name: orkcore-application
description: Answer questions about orkid's application framework, AppInitData, ComponentizedApplication, application lifecycle, path expanders, environment setup, and the Python app pattern. Use when the user asks about app init, components, lifecycle callbacks, or environment variables.
user-invocable: false
---

# Orkid Application Framework Reference

When answering questions about application structure in orkid, consult these files.

## Key Files

| Component | Location |
|-----------|----------|
| Application (C++) | `ork.core/inc/ork/application/application.h` |
| Application Impl | `ork.core/src/application/application.cpp` |
| Python Bindings | `ork.core/pyext/pyext_application.cpp` |
| ComponentizedApplication | `obt.project/scripts/ork/app/application.py` |
| Logger UI Component | `obt.project/scripts/ork/app/loggerui.py` |
| Frame Profiler | `obt.project/scripts/ork/app/frame_profiler.py` |

## Architecture Overview

### AppInitData
Singleton struct (`appinitdata()`) centralizing initialization config:
- Command-line parsing (Boost.ProgramOptions)
- Window config: width, height, fullscreen, MSAA/SSAA
- Audio config: device names, channels, framesize, stream sync
- Target UPS/FPS rates
- Pre/post init operation queues

### Path Expanders (set during init)
```
<assetcache>  → <stage>/assetcache
<staging>     → staging directory root
<ork_data>    → orkid data directory
<ork_ecsscenes> → ork.data/ecsscenes
<ork_envmaps> → staging/envmaps
<ork_testdata> → ork.data/tests
```

### URI Contexts
```
src://     → ork.data/src
data://    → ork.data
lev2://    → ork.data/platform_lev2
orkid://   → orkid root
ork_core://→ ork.core source
ork_lev2://→ ork.lev2 source
```

### Environment Variables
- `ORKID_ASSET_MANIFEST_DIRS` — colon-separated manifest paths
- `ORKID_AUDIO_INPUT_DEVICE` / `ORKID_AUDIO_OUTPUT_DEVICE`
- `ORKID_AUDIO_FRAMESIZE` — buffer size (default 1024)
- `ORKID_AUDIO_STREAM_SYNC` — audio/graphics sync
- `ORKID_DISABLE_ALWAYS_ON_TOP` — window flag
- `ORKID_LOGGER_BACKEND` — logger selection

### ComponentizedApplication (Python)
High-level Python app framework with ECS-like component pattern:

```python
class MyApp(ComponentizedApplication):
  def __init__(self):
    super().__init__()
    self.logger = self.addComponent("logger", loggerui.LoggerUIComponent, ...)
    self.createEzApp(width=1280, height=720)

  def _onUiInit(self):
    lg = self.ezapp.topLayoutGroup
    # Build UI here

  def _onGpuInit(self, ctx):
    # GPU resources here

  def _onUpdate(self, updinfo):
    # Per-frame logic here

app = MyApp()
app.ezapp.mainThreadLoop()
```

### Lifecycle Callbacks (in order)
1. `onAppInit(initdata)` → `_onAppLink()`
2. `onGpuInit(ctx)` → components init → `_onGpuInit()` → components link → `_onGpuLink()`
3. `onUpdateInit()` → `_onUpdateInit()` → `_onUpdateLink()`
4. `onUpdate(updinfo)` → `_onUpdate(updinfo)` (per frame)
5. `onUpdateExit()` → `_onUpdateExit()`
6. `onGpuExit(ctx)` → `_onGpuExit()`
7. `onAppExit()` → `_onAppExit()`

### ApplicationComponent Base
```python
class MyComponent(ApplicationComponent):
  def _onGpuInit(self, ctx): ...
  def _onGpuLink(self, ctx): ...
  def _onUpdate(self, updinfo): ...
```

### Thread Model
- **Main Thread**: Python origin, owns GIL initially, releases for C++ engine
- **GPU Thread**: Same as main (GPU context bound to main)
- **Update Thread**: Spawned from C++, acquires GIL only for Python callbacks
- **Audio Thread**: Spawned during audio init

## How to Answer

1. For app structure: check `application.py` for Python pattern, `application.h` for C++
2. For path setup: check `application.cpp` StdFileSystemInitalizer
3. For lifecycle order: follow the callback chain in ComponentizedApplication
4. For components: subclass `ApplicationComponent`, override `_onXXXX` methods
