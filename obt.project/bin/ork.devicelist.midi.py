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
# List available MIDI devices
################################################################

if __name__ == "__main__":
    print(deco.yellow("Available MIDI Devices") + "\n")

    # MIDI Inputs
    inp_ctx = lev2.midi.InputContext()
    inputs = inp_ctx.inputs

    if inputs:
        print(deco.magenta("MIDI INPUT DEVICES:"))
        for name, index in sorted(inputs.items(), key=lambda x: x[1]):
            print(deco.white("  └── ") + deco.white("[") + deco.val(str(index)) + deco.white("] ") + deco.cyan(name))
        print()
    else:
        print(deco.red("No MIDI input devices found."))
        print()

    # MIDI Outputs
    out_ctx = lev2.midi.OutputContext()
    outputs = out_ctx.outputs

    if outputs:
        print(deco.magenta("MIDI OUTPUT DEVICES:"))
        for name, index in sorted(outputs.items(), key=lambda x: x[1]):
            print(deco.white("  └── ") + deco.white("[") + deco.val(str(index)) + deco.white("] ") + deco.cyan(name))
        print()
    else:
        print(deco.red("No MIDI output devices found."))
        print()
