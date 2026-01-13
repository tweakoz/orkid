#!/usr/bin/env ork.python
################################################################
# Orkid Media Engine
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License.
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################

"""
Basic EzApp creation test - no main loop execution.

This test verifies that OrkEzApp can be created and has expected properties.

Usage:
  ./test_ezapp_creation.py
"""

import sys
from orkengine import core
from orkengine import lev2

results = []

def test(name, condition):
    results.append(condition)
    print(f"  {'PASS' if condition else 'FAIL'}: {name}")

################################################################
# Minimal app class for EzApp.create
################################################################
class MinimalApp:
    pass

################################################################

def main():
    print("Testing EzApp Creation (No Main Loop)")
    print("=" * 50)

    # Create EzApp with offscreen mode
    print("\n[Creating lev2.OrkEzApp in offscreen mode]")
    app_obj = MinimalApp()
    ezapp = lev2.OrkEzApp.create(app_obj, width=64, height=64, offscreen=True)

    print("\n[Basic Properties]")
    test("EzApp created successfully", ezapp is not None)
    test("mainwin is available", ezapp.mainwin is not None)
    test("topWidget is available", ezapp.topWidget is not None)
    test("topLayoutGroup is available", ezapp.topLayoutGroup is not None)
    test("uicontext is available", ezapp.uicontext is not None)
    test("vars is available", ezapp.vars is not None)

    print("\n[AppInitData Access]")
    appinit = ezapp._appinit
    test("_appinit available", appinit is not None)
    test("_appinit has misc_varmap", hasattr(appinit, 'misc_varmap'))

    print("\n[Audio Properties (disabled by default)]")
    test("audio_device is None (audio disabled)", ezapp.audio_device is None)
    test("audio_synth is None (synth disabled)", ezapp.audio_synth is None)

    # Summary
    passed, failed = results.count(True), results.count(False)
    print(f"\n{'='*50}")
    print(f"Results: {passed} passed, {failed} failed")
    return 0 if failed == 0 else 1

if __name__ == '__main__':
    sys.exit(main())
