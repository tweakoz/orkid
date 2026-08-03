---
name: orkcore-logging
description: Answer questions about orkid's logging system, log channels, backends (stdout/file/HTML/HTTP), performance metrics, and Python logging integration. Use when the user asks about logging, log channels, or debug output.
user-invocable: false
---

# Orkid Logging System Reference

When answering questions about logging in orkid, consult these files.

## Key Files

| Component | Location |
|-----------|----------|
| Logger Header | `ork.core/inc/ork/util/logger.h` |
| Logger Impl | `ork.core/src/util/logger.cpp` |
| Stdout Backend | `ork.core/src/util/logger_std.cpp` |
| File Backend | `ork.core/src/util/logger_file.cpp` |
| HTML Backend | `ork.core/src/util/logger_html.cpp` |
| HTTP Backend | `ork.core/src/util/logger_http.cpp` |
| Python Bindings | `ork.core/pyext/pyext_logger.cpp` |

## Usage

```python
from orkengine import core

# Get logger singleton
logger = core.Logger.instance()

# Get or create channel
chan = logger.getChannel("MYAPP")
chan = logger.configureChannel("MYAPP", core.fvec3(0.5, 0.8, 1.0), True)

# Log messages
chan.log("message")
chan.warn("warning")      # Goes to stderr (red)
chan.error("error")       # Goes to stderr (red)
chan.status("sub", "msg") # Status with subchannel label

# Multi-part messages
chan.log_begin("start")
chan.log_continue("middle")
chan.log_end()

# Enable/disable
chan.enabled = True
```

## C++ Pattern

```cpp
// Typical: module-level channel
static logchannel_ptr_t logchan_mymod = logger()->getChannel("MYMOD");

logchan_mymod->log("Processing %d items", count);
logchan_mymod->warn("Unexpected state");
```

## Performance Metrics

```python
# Push immediate value
chan.perfItem("fps", 60.0)

# Pull value via lambda (sampled at perf_interval)
chan.perfItem("frame_time", lambda: get_frame_time())

# Configure sampling rate
chan.perf_interval = 0.001  # 1000 Hz sampling
chan.status_interval = 8.0  # Status dedup interval
```

## Backends

| Backend | Env Config | Purpose |
|---------|-----------|---------|
| Stdout | `ORKID_LOGGER_BACKEND=STDOUT` | Console (default) |
| File | `ORKID_LOGGER_BACKEND=FILE` | Async file write |
| HTML | `ORKID_LOGGER_BACKEND=HTML` | Interactive HTML log |
| HTTP | `ORKID_LOGGER_BACKEND=HTTP` | Live streaming via ZMQ+SSE |
| Fork | `ORKID_LOGGER_BACKEND=[STDOUT,FILE]` | Multiple backends |

File options: `ORKID_LOGGER_FILE_ANSI=1`, `ORKID_LOGGER_FILE_FLUSH_MS=100`

## Channel Filtering (Environment)

```bash
ORKID_LOGCHAN_0=1         # Enable all ('0' = wildcard)
ORKID_LOGCHAN_CATALOG=1   # Enable CATALOG channel
ORKID_LOGCHAN_GPU0=0      # Disable GPU* channels
ORKID_LOG_DISABLE=1       # Disable all logging
```

## How to Answer

1. For API: read `logger.h` for channel methods
2. For backends: check `logger_*.cpp` implementations
3. No log levels — use `log()` vs `warn()` vs `error()` for severity
4. Channels use plain `std::string` names, not CrcStrings
