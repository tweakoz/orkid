# Movie I/O Technical Design Document

## Overview

The Orkid Media Engine provides a multi-backend architecture for video playback and encoding. Platform-specific hardware acceleration is available alongside cross-platform FFmpeg fallback.

## Backend Architecture

```
MovieBackendImpl (abstract base)
├── FFmpegBackend       - Cross-platform CPU decode
├── VideoToolboxBackend - macOS hardware decode
├── VAAPIBackend        - Linux AMD/Intel hardware decode
└── NVDECBackend        - Linux NVIDIA hardware decode
```

**Files:**
- `ork.lev2/inc/ork/lev2/gfx/util/movie.inl` - Interface definitions
- `ork.lev2/src/gfx/misc/movie_playback.cpp` - Dispatcher
- `ork.lev2/src/gfx/misc/movie_playback_ffmpeg.cpp` - FFmpeg backend
- `ork.lev2/src/gfx/misc/movie_playback_videotoolbox.mm` - VideoToolbox backend

---

## 1. FFmpeg Playback Backend

### Pipeline

```
AVFormatContext (container)
    └─ AVPacket (compressed packet)
        └─ AVFrame (decoded YUV)
            └─ sws_scale (YUV→RGB)
                └─ Image (CPU buffer)
                    └─ Frame Queue
```

### Threading Model

```
Decode Thread
├─ Reads packets from file
├─ Decodes video/audio frames
├─ YUV→RGB conversion (software)
└─ Pushes to frame queue

Render Thread
├─ Polls ImageProvider
├─ Calculates frame drift
├─ Skips frames if behind
└─ Returns current image
```

### Frame Queue

```cpp
std::deque<image_ptr_t> _frame_queue;  // Protected by mutex
size_t _max_queue_size = 30;
std::mutex _queue_mutex;
std::condition_variable _queue_cv;
```

### Timing and Drift Correction

```cpp
// Calculate expected frame based on elapsed time
auto elapsed = now - _playback_start;
int64_t expected_frame = int64_t(elapsed_seconds * _fps);

// Skip old frames to catch up
while (!_frame_queue.empty() && _current_frame_index < expected_frame) {
  _current_image = _frame_queue.front();
  _frame_queue.pop_front();
  _current_frame_index++;
}
```

### Performance Characteristics

| Aspect | Details |
|--------|---------|
| CPU Load | ~30% single-core @ 1080p30 H.264 |
| Memory | Queue size × frame size (6.2MB @ 1080p) |
| Latency | Variable (1-100ms decode, codec-dependent) |
| Scaling | O(width × height) for color conversion |

### Strengths
- Cross-platform (Linux, macOS, Windows)
- Supports any FFmpeg codec
- Predictable decode order

### Limitations
- Software color conversion (CPU intensive)
- No GPU-direct output
- Memory grows if render thread slow

---

## 2. VideoToolbox Playback Backend

### Pipeline

```
AVAssetReader (container)
    └─ CMSampleBuffer (compressed)
        └─ VTDecompressionSession (hardware decode)
            └─ CVPixelBuffer (decoded, IOSurface-backed)
                └─ GpuExternalSurface wrapper
                    └─ VkImage (via MoltenVK)
```

### Threading Model

```
Decode Thread
├─ Feeds CMSampleBuffers to VTDecompressionSession
├─ FPS-based pacing (sleep to match decode rate)
└─ Handles looping via asset reader reset

Decompression Callback (async, from AVFoundation)
├─ Receives CVPixelBuffer (unpredictable order due to B-frames)
├─ Retains CVPixelBuffer (extends lifetime)
├─ Wraps in GpuExternalSurface
├─ Inserts into PTS-sorted buffer
└─ Reuses VkImage if IOSurfaceID seen before

Render Thread
├─ Polls TextureProvider
├─ Drains frames by PTS order
├─ Releases old CVPixelBuffer (deferred)
└─ Returns texture for sampling
```

### Buffer Management

**IOSurface Pool Reuse:**
```cpp
std::unordered_map<IOSurfaceID, iosurfaceteximpl_ptr_t> _iosurface_to_impl;
```
- VideoToolbox pools 2-3 IOSurfaces (double/triple buffer)
- Stable IOSurfaceID used as key (not pointer)
- Single VkImage created per unique IOSurface, then reused

**PTS Reordering Buffer:**
```cpp
struct PendingFrame {
  iosurfaceteximpl_ptr_t iosurface_impl;
  double pts;                    // Presentation timestamp
  CVPixelBufferRef pixel_buffer; // Retained reference
};

std::set<PendingFrame> _pending_frames;  // Auto-sorted by PTS
```
- B-frame codecs decode out of order: [I,B,P] may have PTS [0,2,1]
- Set sorts by PTS, providing correct temporal order

**CVPixelBuffer Lifecycle:**
1. VTDecompressionSession decodes into pooled IOSurface
2. Callback retains CVPixelBuffer (increments use count)
3. Stored in PendingFrame for later display
4. On frame display, previous CVPixelBuffer released
5. VideoToolbox recycles IOSurface when use count → 0

### Texture Integration

```cpp
// In decompression callback
handle->surface = vulkan::createGpuSurfaceFromCVPixelBuffer(pixelBuffer);

// In currentTexture()
tex->_impl_2.set<iosurfaceteximpl_ptr_t>(new_impl);
```

- Zero-copy: MoltenVK maps IOSurface directly to VkImage
- Single persistent Texture object, dynamic impl swapping
- No CPU→GPU copy; GPU-direct path

### Performance Characteristics

| Aspect | Details |
|--------|---------|
| CPU Load | 3-5% @ 4K60 on Apple Silicon |
| Memory | Pool size × frame size (2-3 surfaces) |
| Latency | 2-frame buffer for B-frame reordering |
| Decode | Hardware video engine (very fast) |

### Strengths
- Hardware-accelerated decode
- GPU-direct output (zero-copy)
- Minimal memory allocation churn

### Limitations
- macOS only
- Frame reordering buffer adds latency
- Asset reader reset on loop (expensive)
- Audio not yet implemented

---

## 3. FFmpeg Movie Capture/Encoding

### Pipeline

```
Render Thread
├─ GPU capture → CaptureBuffer (async readback)
├─ Audio samples from synth
└─ Queue frame + audio for encoding

Encoding Thread
├─ Wait for GPU readback completion
├─ RGB→YUV conversion (sws_scale)
├─ FFmpeg video encoder (H.264/H.265)
├─ FFmpeg audio encoder (AAC)
└─ Mux to container (MP4/MKV)
```

### Threading Model

```
Render Thread
├─ Queues GPU capture futures
├─ Queues audio sample counts
└─ Backpressure if queue full

Audio Thread (OS-managed)
└─ Generates audio samples

Encoding Thread
├─ Monitors GPU capture completion
├─ Monitors audio sample availability
├─ Symmetric decision: encode whichever behind
└─ Writes muxed packets to file
```

### Frame Queue

```cpp
struct CapturedMovieFrame {
  captureasync_ptr_t capture_future;   // GPU readback status
  capturebuffer_ptr_t capture_buffer;  // RGBA image
  int frame_number;                    // For PTS
  int expected_audio_samples;          // Samples to extract
};

std::deque<CapturedMovieFrame> _frame_queue;
size_t _max_queue_size = 120;
```

### A/V Synchronization

**Symmetric PTS Comparison:**
```cpp
double video_pts = frame_number / fps;
double audio_pts = total_audio_samples / sample_rate;

bool should_video = can_video && (!can_audio || video_pts <= audio_pts);
bool should_audio = can_audio && (!can_video || audio_pts < video_pts);
```

- Encodes whichever stream is behind
- Ensures properly interleaved output
- Handles codec frame size constraints (e.g., AAC needs 1024 samples)

### Conversion Pipeline

```
GPU RenderTarget
    └─ Async Readback (CaptureBuffer)
        └─ RGBA CPU buffer
            └─ sws_scale (RGB→YUV420P)
                └─ FFmpeg encoder
                    └─ Muxer (MP4/MKV)
```

### Performance Characteristics

| Aspect | Details |
|--------|---------|
| Video Encode | 50-80% CPU (software H.264 @ 1080p30) |
| Video Encode | 5% CPU (hardware NVENC) |
| Audio Encode | ~1% CPU (AAC) |
| Sync Overhead | 5-10% over raw encoding |

---

## 4. Audio Handling

### Playback Audio

```cpp
struct MovieAudioFrame {
  std::vector<float> _samples;  // Interleaved float
  int _sample_rate;             // 48000 Hz typical
  int _channels;                // 1, 2, or 6
  double _pts;
};

// Callback interface
void setAudioCallback(audio_callback_t cb);
```

- Decoded via FFmpeg audio decoder
- Routed to singularity synth via streaming oscillator
- Ringbuffer manages resampling/buffering

### Capture Audio

```cpp
struct AudioFrameCapture {
  std::vector<float> _left;   // Left channel
  std::vector<float> _right;  // Right channel
  int _num_samples;           // Per codec (1024 for AAC)
};
```

- Samples from audio synthesis system
- Resampled if needed (44.1kHz → 48kHz)
- Chunked to codec frame size

---

## 5. Comparison Matrix

| Feature | FFmpeg Playback | VideoToolbox | FFmpeg Capture |
|---------|-----------------|--------------|----------------|
| Platform | Cross-platform | macOS only | Cross-platform |
| Decode | Software | Hardware | N/A |
| Output | CPU Image | GPU Texture | File (MP4/MKV) |
| Copy | YUV→RGB (CPU) | Zero-copy | RGB→YUV (CPU) |
| Threading | Decode + Render | Decode + Callback + Render | Render + Encode |
| A/V Sync | Frame skip | PTS buffer | Symmetric PTS |

---

## 6. Integration Points

### Texture System
- `ETextureSource::MOVIE` marks movie-sourced textures
- VideoToolbox uses `GpuExternalSurface` for GPU-direct import
- FFmpeg requires separate CPU→GPU upload

### Audio System
- Audio callbacks route to singularity synth
- Streaming oscillator program for playback
- Capture pulls from synthesis output

### Render Loop
- ImageProvider (FFmpeg) / TextureProvider (VideoToolbox)
- Polled each frame; handles all threading internally
- Returns nullptr if frame not ready

---

## 7. Design Patterns

### Strategy Pattern
- `MoviePlaybackContext` dispatches to selected backend
- Runtime backend selection via enum
- Factory functions create concrete implementations

### Producer-Consumer
- Decode thread produces → queue
- Render thread consumes from queue
- Condition variables coordinate

### Adapter Pattern
- ImageProvider/TextureProvider wrap backends
- Render loop polls without knowing backend details
- Lambda-based providers for flexibility

### PTS Synchronization
- Both playback and capture use presentation timestamps
- Enables temporal alignment of streams
- Handles B-frame reordering transparently
