#!/usr/bin/env ork.python
################################################################
# Orkid Media Engine
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License.
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################

from orkengine import core
from orkengine import lev2
import obt.deco

deco = obt.deco.Deco()

################################################################
# List available audio devices
# Device names can be used with ORKID_AUDIO_INPUT_DEVICE and
# ORKID_AUDIO_OUTPUT_DEVICE environment variables
################################################################

if __name__ == "__main__":
    devices = lev2.enumerateAudioDevices()

    if not devices:
        print(deco.red("No audio devices found."))
    else:
        print(deco.yellow("Available Audio Devices") + "\n")

        input_devices = [d for d in devices if d.max_input_channels > 0]
        output_devices = [d for d in devices if d.max_output_channels > 0]

        if input_devices:
            print(deco.magenta("INPUT DEVICES") + deco.white(" (for ORKID_AUDIO_INPUT_DEVICE):"))
            for dev in input_devices:
                print(deco.white("  └── ") + deco.cyan(dev.name))
                print(deco.white("       ") + deco.key("channels: ") + deco.val(str(dev.max_input_channels)) +
                      deco.white(", ") + deco.key("sample_rate: ") + deco.val(str(dev.default_sample_rate)))
            print()

        if output_devices:
            print(deco.magenta("OUTPUT DEVICES") + deco.white(" (for ORKID_AUDIO_OUTPUT_DEVICE):"))
            for dev in output_devices:
                print(deco.white("  └── ") + deco.cyan(dev.name))
                print(deco.white("       ") + deco.key("channels: ") + deco.val(str(dev.max_output_channels)) +
                      deco.white(", ") + deco.key("sample_rate: ") + deco.val(str(dev.default_sample_rate)))
            print()

        print(deco.yellow("Usage:"))
        print(deco.white("  export ") + deco.key("ORKID_AUDIO_INPUT_DEVICE") + deco.white("=") + deco.val("\"<device_name>\""))
        print(deco.white("  export ") + deco.key("ORKID_AUDIO_OUTPUT_DEVICE") + deco.white("=") + deco.val("\"<device_name>\""))
