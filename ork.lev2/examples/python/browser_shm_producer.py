#!/usr/bin/env ork.python
################################################################
# Orkid Media Engine
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License.
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################
#
# Browser SHM Producer - Renders web pages to shared memory texture
#
# Usage:
#   ./browser_shm_producer.py --url https://example.com
#   ./browser_shm_producer.py --url https://example.com --name mybrowser --width 1920 --height 1080
#
# Then run consumer:
#   ./shmtexture_demo.py --consumer --name mybrowser
#
################################################################

import argparse
import time
from io import BytesIO
from collections import deque

from PIL import Image
from playwright.sync_api import sync_playwright

from orkengine.core import CrcStringProxy
from orkengine import lev2

tokens = CrcStringProxy()

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

################################################################################
# Browser SHM Producer
################################################################################

class BrowserShmProducer:
    def __init__(self, url, name="shmtex_demo", width=2560, height=1440, target_fps=60):
        self.url = url
        self.name = name
        self.width = width
        self.height = height
        self.target_fps = target_fps
        self.frame_count = 0
        self.fps_tracker = FpsTracker(window_seconds=3.0)

        # Create SHM producer (RGBA8 format)
        self.producer = lev2.ShmTexProducer.create(name, width, height, tokens.RGBA8)
        print(f"[Browser] Created SHM producer '{name}' {width}x{height}")

    def run(self):
        print(f"[Browser] Starting at {self.target_fps} fps (Ctrl+C to stop)")
        print(f"[Browser] URL: {self.url}")
        frame_time = 1.0 / self.target_fps
        sleep_compensation = 0.0

        with sync_playwright() as p:
            browser = p.chromium.launch()
            page = browser.new_page(viewport={'width': self.width, 'height': self.height})
            page.goto(self.url)
            print(f"[Browser] Page loaded: {page.title()}")

            try:
                while True:
                    frame_start = time.time()

                    # Capture screenshot as JPEG (much faster than PNG)
                    jpg_bytes = page.screenshot(type='jpeg', quality=85)

                    # Decode JPEG to RGBA
                    img = Image.open(BytesIO(jpg_bytes))
                    if img.mode != 'RGBA':
                        img = img.convert('RGBA')

                    # Write to SHM
                    pixels, buffer_idx = self.producer.beginWrite()
                    pixels[:] = img
                    timestamp_ns = int(time.time() * 1e9)
                    self.producer.endWrite(timestamp_ns)

                    self.frame_count += 1
                    self.fps_tracker.tick()

                    if self.frame_count % 30 == 0:
                        fps = self.fps_tracker.fps()
                        print(f"[Browser] Frame {self.frame_count}, {fps:.1f} fps")

                    # Sleep with dynamic compensation
                    elapsed = time.time() - frame_start
                    sleep_time = frame_time - elapsed - sleep_compensation
                    if sleep_time > 0:
                        time.sleep(sleep_time)

                    actual_frame_time = time.time() - frame_start
                    error = actual_frame_time - frame_time
                    sleep_compensation += error * 0.1

            except KeyboardInterrupt:
                print(f"\n[Browser] Stopped after {self.frame_count} frames")
            finally:
                browser.close()

################################################################################
# Main
################################################################################

def main():
    parser = argparse.ArgumentParser(description="Browser SHM Producer - Render web pages to shared memory")
    parser.add_argument("--url", required=True, help="URL to render")
    parser.add_argument("--name", default="shmtex_demo", help="Shared memory name (default: shmtex_demo)")
    parser.add_argument("--width", type=int, default=1920, help="Viewport width (default: 1280)")
    parser.add_argument("--height", type=int, default=1080, help="Viewport height (default: 720)")
    parser.add_argument("--fps", type=int, default=30, help="Target FPS (default: 30)")

    args = parser.parse_args()

    producer = BrowserShmProducer(
        url=args.url,
        name=args.name,
        width=args.width,
        height=args.height,
        target_fps=args.fps
    )
    producer.run()

if __name__ == "__main__":
    main()
