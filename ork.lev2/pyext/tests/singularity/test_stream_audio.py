#!/usr/bin/env ork.python
"""
Test for StrAudioDevice (Stream Audio Device)
Tests both SYNC (deterministic/non-realtime) and ASYNC (realtime) modes
Tests audio generation, capture, and extraction
"""

import sys, os, time, json
from pathlib import Path
from orkengine.core import *
from orkengine.lev2 import *

os.environ["ORKID_AUDIO_IOCLASS"] = "STREAM"

################################################################################

class StrAudioTestApp(object):

    def __init__(self):
        super().__init__()


        # Create EzApp with audio synth enabled in offscreen mode
        self.ezapp = OrkEzApp.create(
            self,
            enable_audio=True,
            enable_audio_output=True,
            enable_audio_synth=True,
            enable_graphics=True,
            offscreen=True,
            width=640,
            height=480
        )

        self.str_audio = None
        self.test_results = []
        self.current_test = 0

    ##############################################

    def onGpuInit(self, ctx):
        print("="*60)
        print("onGpuInit called - Audio should now be initialized")
        print("="*60)

        # Get the audio device (should be StrAudioDevice)
        self.str_audio = self.ezapp.audio_device
        synth = self.ezapp.audio_synth

        print(f"Audio device: {self.str_audio}")
        print(f"Audio device type: {type(self.str_audio)}")
        print(f"Audio synth: {synth}")

        if self.str_audio is None:
            print("ERROR: Failed to initialize STREAM audio device!")
            sys.exit(1)

        if synth is None:
            print("ERROR: Failed to initialize audio synth!")
            sys.exit(1)

        #print(f"Device mode: {self.str_audio.mode}")
        print("✅ Audio system initialized successfully")

    ##############################################

    def onGpuUpdate(self, ctx):
        # Run tests on first update
        if self.current_test == 0:
            self.run_tests()
            self.current_test = 1

    ##############################################

    def run_tests(self):
        """Run all audio tests"""
        print("\n" + "="*60)
        print("Running StrAudioDevice Tests")
        print("="*60)

        tests = [
            ("SYNC Basic", self.test_sync_mode_basic),
        ]

        passed = 0
        failed = 0

        for test_name, test_func in tests:
            try:
                if test_func():
                    passed += 1
                    print(f"✅ {test_name} PASSED")
                else:
                    failed += 1
                    print(f"❌ {test_name} FAILED")
            except Exception as e:
                failed += 1
                print(f"❌ {test_name} FAILED with exception: {e}")
                import traceback
                traceback.print_exc()

        print("\n" + "="*60)
        print("Test Results")
        print("="*60)
        print(f"Passed: {passed}/{len(tests)}")
        print(f"Failed: {failed}/{len(tests)}")
        print("="*60)

        # Exit after tests
        sys.exit(0 if failed == 0 else 1)

    ##############################################

    def test_sync_mode_basic(self):
        """Test SYNC mode: deterministic audio generation"""
        print("\n" + "="*60)
        print("TEST: SYNC Mode Basic Operation")
        print("="*60)

        # Should be in SYNC mode because we initialized with offscreen=True
        #print(f"Device mode: {self.str_audio.mode}")

        sample_rate = 48000

        # Generate audio for 1 frame at 60fps (16.67ms)
        dt = 1.0 / 60.0
        expected_samples = int(sample_rate / 60)  # Should be 800 samples

        print(f"Generating audio for dt={dt:.4f}s (expected {expected_samples} samples)...")
        self.str_audio.advanceTime(dt)

        # Check available samples
        available = self.str_audio.availableSamples()
        print(f"Available samples: {available}")

        if available != expected_samples:
            print(f"ERROR: Expected {expected_samples} samples, got {available}")
            return False

        # Extract samples
        print(f"Extracting {expected_samples} samples...")
        audio_frame = self.str_audio.extractSamples(expected_samples)

        print(f"Extracted: num_samples={audio_frame.num_samples}, SR={audio_frame.sample_rate}")
        print(f"           timestamp={audio_frame.timestamp:.4f}s")
        print(f"           left channel: {len(audio_frame.left)} samples")
        print(f"           right channel: {len(audio_frame.right)} samples")

        # Validate
        if audio_frame.num_samples != expected_samples:
            print(f"ERROR: num_samples mismatch")
            return False

        if len(audio_frame.left) != expected_samples:
            print(f"ERROR: left channel size mismatch")
            return False

        if len(audio_frame.right) != expected_samples:
            print(f"ERROR: right channel size mismatch")
            return False

        # Check that buffer is now empty
        available_after = self.str_audio.availableSamples()
        print(f"Available after extraction: {available_after}")

        if available_after != 0:
            print(f"ERROR: Buffer should be empty after extraction")
            return False

        return True

################################################################################

StrAudioTestApp().ezapp.mainThreadLoop()
