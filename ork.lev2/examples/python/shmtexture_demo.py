#!/usr/bin/env ork.python
################################################################
# Orkid Media Engine
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License.
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################
#
# ShmTexture Demo - Shared Memory Texture Producer/Consumer
#
# Usage:
#   python shmtexture_demo.py --producer          # Run producer only (writes frames)
#   python shmtexture_demo.py --consumer          # Run consumer only (reads and displays)
#   python shmtexture_demo.py --both              # Run both in same process
#
# The producer generates animated test patterns.
# The consumer displays them via Vulkan.
#
################################################################

import argparse
import time
import numpy as np
import threading
from collections import deque
from orkengine.core import vec4, CrcStringProxy
from orkengine import lev2
from ork.app import application

################################################################################
# Rolling average FPS tracker
################################################################################

class FpsTracker:
    def __init__(self, window_seconds=3.0):
        self.window_seconds = window_seconds
        self.timestamps = deque()

    def tick(self):
        now = time.time()
        self.timestamps.append(now)
        # Remove timestamps outside the window
        cutoff = now - self.window_seconds
        while self.timestamps and self.timestamps[0] < cutoff:
            self.timestamps.popleft()

    def fps(self):
        if len(self.timestamps) < 2:
            return 0.0
        elapsed = self.timestamps[-1] - self.timestamps[0]
        if elapsed <= 0:
            return 0.0
        return (len(self.timestamps) - 1) / elapsed

tokens = CrcStringProxy()

################################################################################
# Producer: generates animated test patterns (headless, no window)
################################################################################

class ShmProducer:
    def __init__(self, name="demo", width=640, height=480):
        self.name = name
        self.width = width
        self.height = height
        self.producer = lev2.ShmTexProducer.create(name, width, height, tokens.RGBA8)
        self.frame_count = 0
        self.start_time = time.time()
        self.fps_tracker = FpsTracker(window_seconds=3.0)
        print(f"[Producer] Created SHM producer '{name}' {width}x{height}")

    def write_frame(self):
        # Get write buffer
        pixels, buffer_idx = self.producer.beginWrite()

        # Generate animated test pattern
        t = time.time() - self.start_time

        # Create gradient with animated wave
        y_coords = np.arange(self.height).reshape(-1, 1)
        x_coords = np.arange(self.width).reshape(1, -1)

        # Animated color channels
        r = ((x_coords / self.width * 255 + t * 50) % 256).astype(np.uint8)
        g = ((y_coords / self.height * 255 + t * 30) % 256).astype(np.uint8)
        b = ((np.sin(x_coords * 0.02 + t * 2) * 127 + 128 +
              np.sin(y_coords * 0.02 + t * 3) * 64)).astype(np.uint8)

        # Broadcast to full image
        r = np.broadcast_to(r, (self.height, self.width))
        g = np.broadcast_to(g, (self.height, self.width))
        b = np.broadcast_to(b, (self.height, self.width))

        # Copy to shared memory buffer
        pixels[:, :, 0] = r
        pixels[:, :, 1] = g
        pixels[:, :, 2] = b
        pixels[:, :, 3] = 255

        # Submit frame with timestamp
        timestamp_ns = int(time.time() * 1e9)
        self.producer.endWrite(timestamp_ns)

        self.frame_count += 1
        self.fps_tracker.tick()

        if self.frame_count % 60 == 0:
            fps = self.fps_tracker.fps()
            print(f"[Producer] Frame {self.frame_count}, {fps:.1f} fps, dropped: {self.producer.framesDropped}")

    def run(self, target_fps=60):
        """Run producer loop at target FPS"""
        print(f"[Producer] Starting at {target_fps} fps (Ctrl+C to stop)")
        frame_time = 1.0 / target_fps
        sleep_compensation = 0.0

        try:
            while True:
                frame_start = time.time()
                self.write_frame()

                # Sleep with dynamic compensation
                elapsed = time.time() - frame_start
                sleep_time = frame_time - elapsed - sleep_compensation
                if sleep_time > 0:
                    time.sleep(sleep_time)

                # Measure actual frame time and adjust compensation
                actual_frame_time = time.time() - frame_start
                error = actual_frame_time - frame_time
                sleep_compensation += error * 0.1  # smooth convergence
        except KeyboardInterrupt:
            print(f"\n[Producer] Stopped after {self.frame_count} frames")

################################################################################
# ShmTexture App: displays consumer, optionally runs producer thread
################################################################################

class ShmTextureApp(application.ComponentizedApplication):

    def __init__(self, shm_name="demo", run_producer=False, tex_width=640, tex_height=480):
        super().__init__()
        self.shm_name = shm_name
        self.run_producer = run_producer
        self.tex_width = tex_width
        self.tex_height = tex_height
        self.producer = None
        self.consumer = None
        self.imageview = None
        self.producer_thread = None
        self.running = True
        self.frame_count = 0
        self.start_time = time.time()
        self.fps_tracker = FpsTracker(window_seconds=3.0)

        self.ezapp = self.createEzApp(name="ShmTextureDemo",
                                      width=800,
                                      height=600,
                                      fullscreen=False,
                                      enable_audio=False)
        self.ezapp.setRefreshPolicy(lev2.RefreshFastest, 0)
        self.ezapp.topWidget.enableUiDraw()

        # If running producer, create it now (before consumer tries to connect)
        if self.run_producer:
            self.producer = lev2.ShmTexProducer.create(self.shm_name, self.tex_width, self.tex_height, tokens.RGBA8)
            print(f"[App] Producer created: {self.tex_width}x{self.tex_height}")

        # Try to connect consumer
        try:
            self.consumer = lev2.ShmTexConsumer.create(self.shm_name)
            print(f"[App] Consumer connected: {self.consumer.width}x{self.consumer.height}")
        except Exception as e:
            print(f"[App] Consumer failed to connect (producer not running?): {e}")
            self.consumer = None

    def _onUiInit(self):
        """Initialize UI layout with single ImageView"""
        lg_group = self.ezapp.topLayoutGroup
        lg_group.margin = 4

        # Create single ImageView using 1x1 grid
        griditems = lg_group.makeGrid(
            width=1,
            height=1,
            margin=4,
            uiclass=lev2.ui.ImageView,
            args=["shmtex_view", vec4(0.2, 0.2, 0.2, 1.0)])
        self.imageview = griditems[0].widget
        self.imageview.maintain_aspect_ratio = True

    def producer_loop(self):
        """Producer thread function"""
        print(f"[Producer Thread] Starting")
        frame_time = 1.0 / 60.0
        frame_num = 0

        while self.running:
            frame_start = time.time()

            # Get write buffer
            pixels, buffer_idx = self.producer.beginWrite()

            # Generate test pattern
            t = time.time() - self.start_time

            y_coords = np.arange(self.tex_height).reshape(-1, 1)
            x_coords = np.arange(self.tex_width).reshape(1, -1)

            r = ((x_coords / self.tex_width * 255 + t * 50) % 256).astype(np.uint8)
            g = ((y_coords / self.tex_height * 255 + t * 30) % 256).astype(np.uint8)
            b = ((np.sin(x_coords * 0.02 + t * 2) * 127 + 128)).astype(np.uint8)

            r = np.broadcast_to(r, (self.tex_height, self.tex_width))
            g = np.broadcast_to(g, (self.tex_height, self.tex_width))
            b = np.broadcast_to(b, (self.tex_height, self.tex_width))

            pixels[:, :, 0] = r
            pixels[:, :, 1] = g
            pixels[:, :, 2] = b
            pixels[:, :, 3] = 255

            timestamp_ns = int(time.time() * 1e9)
            self.producer.endWrite(timestamp_ns)

            frame_num += 1

            # Maintain ~60 fps
            elapsed = time.time() - frame_start
            sleep_time = frame_time - elapsed
            if sleep_time > 0:
                time.sleep(sleep_time)

        print(f"[Producer Thread] Stopped after {frame_num} frames")

    def _onGpuInit(self, ctx):
        """Initialize GPU resources"""
        # Start producer thread if running in combined mode
        if self.run_producer and self.producer:
            self.producer_thread = threading.Thread(target=self.producer_loop, daemon=True)
            self.producer_thread.start()
            print(f"[App] Producer thread started")

    def onGpuUpdate(self, ctx):
        """GPU update - fetch new frames from SHM"""
        if not self.consumer:
            # Try to reconnect
            try:
                self.consumer = lev2.ShmTexConsumer.create(self.shm_name)
                print(f"[App] Consumer connected: {self.consumer.width}x{self.consumer.height}")
            except:
                return

        # Update texture from SHM
        if self.consumer.update(ctx):
            tex = self.consumer.texture
            # Set the texture on the imageview
            self.imageview.texture = tex
            self.frame_count += 1
            self.fps_tracker.tick()

            if self.frame_count % 60 == 0:
                fps = self.fps_tracker.fps()
                latency = self.consumer.averageLatencyMs
                print(f"[App] Frame {self.frame_count}, {fps:.1f} fps, latency: {latency:.2f}ms")

    def _onExit(self):
        """Cleanup on exit"""
        print("[App] Shutting down...")
        self.running = False
        if self.producer_thread:
            self.producer_thread.join(timeout=1.0)

################################################################################
# Main
################################################################################

def main():
    parser = argparse.ArgumentParser(description="ShmTexture Demo - Shared Memory Texture Transfer")

    mode_group = parser.add_mutually_exclusive_group(required=True)
    mode_group.add_argument("--producer", action="store_true", help="Run producer only (generates frames)")
    mode_group.add_argument("--consumer", action="store_true", help="Run consumer only (displays frames)")
    mode_group.add_argument("--both", action="store_true", help="Run both producer and consumer")

    parser.add_argument("--name", default="shmtex_demo", help="Shared memory name (default: shmtex_demo)")
    parser.add_argument("--width", type=int, default=640, help="Frame width (default: 640)")
    parser.add_argument("--height", type=int, default=480, help="Frame height (default: 480)")
    parser.add_argument("--fps", type=int, default=60, help="Target FPS for producer (default: 60)")

    args = parser.parse_args()

    if args.producer:
        # Headless producer mode - no window needed
        producer = ShmProducer(args.name, args.width, args.height)
        producer.run(target_fps=args.fps)

    elif args.consumer:
        # Consumer only mode
        app = ShmTextureApp(shm_name=args.name, run_producer=False)
        app.ezapp.mainThreadLoop()

    elif args.both:
        # Combined producer + consumer mode
        app = ShmTextureApp(shm_name=args.name, run_producer=True,
                           tex_width=args.width, tex_height=args.height)
        app.ezapp.mainThreadLoop()

if __name__ == "__main__":
    main()
