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
# Device names or short IDs can be used with ORKID_AUDIO_INPUT_DEVICE
# and ORKID_AUDIO_OUTPUT_DEVICE environment variables
################################################################

def print_table(title, devices, is_input):
    if not devices:
        return

    # Column widths
    ID_W = 6
    CH_W = 5
    SR_W = 8
    NAME_W = 50

    # Header
    print(deco.yellow(title))
    header = (deco.white(f"{'ID':<{ID_W}}") + " " +
              deco.cyan(f"{'Ch':<{CH_W}}") + " " +
              deco.green(f"{'SR':<{SR_W}}") + " " +
              deco.yellow("Name"))
    print(header)
    print(deco.white("-" * (ID_W + CH_W + SR_W + NAME_W + 3)))

    # Rows
    for dev in devices:
        short_id = dev.input_short_id if is_input else dev.output_short_id
        channels = dev.max_input_channels if is_input else dev.max_output_channels
        sr = int(dev.sample_rate)

        row = (deco.white(f"{short_id:<{ID_W}}") + " " +
               deco.cyan(f"{channels:<{CH_W}}") + " " +
               deco.green(f"{sr:<{SR_W}}") + " " +
               deco.yellow(dev.name[:NAME_W]))
        print(row)
    print()

if __name__ == "__main__":
    devices = lev2.enumerateAudioDevices()

    if not devices:
        print(deco.red("No audio devices found."))
    else:
        print()
        input_devices = [d for d in devices if d.max_input_channels > 0]
        output_devices = [d for d in devices if d.max_output_channels > 0]

        print_table("INPUT DEVICES", input_devices, is_input=True)
        print_table("OUTPUT DEVICES", output_devices, is_input=False)

        print(deco.yellow("Usage:"))
        print(deco.white("  export ") + deco.key("ORKID_AUDIO_INPUT_DEVICE") + deco.white("=") + deco.val("\"<short_id or name>\""))
        print(deco.white("  export ") + deco.key("ORKID_AUDIO_OUTPUT_DEVICE") + deco.white("=") + deco.val("\"<short_id or name>\""))
        print()
        print(deco.white("  Examples (use IDs from table above):"))
        print(deco.white("    export ") + deco.key("ORKID_AUDIO_INPUT_DEVICE") + deco.white("=") + deco.val("\"G6PQ\""))
        print(deco.white("    export ") + deco.key("ORKID_AUDIO_OUTPUT_DEVICE") + deco.white("=") + deco.val("\"H40R\""))
