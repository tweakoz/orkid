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
# List available GLFW monitors
# Monitor names can be used with fullscreen_monitor= kwarg
# on OrkEzApp.create() and createSecondaryWindow()
################################################################

if __name__ == "__main__":
    monitors = lev2.enumerateGlfwMonitors()

    if not monitors:
        print(deco.red("No monitors found."))
    else:
        # Column widths
        NAME_W = 30
        RES_W = 14
        HZ_W = 6
        POS_W = 12
        SIZE_W = 14
        SCALE_W = 8

        print()
        print(deco.yellow("MONITORS"))
        header = (deco.white(f"{'Name':<{NAME_W}}") + " " +
                  deco.cyan(f"{'Resolution':<{RES_W}}") + " " +
                  deco.green(f"{'Hz':<{HZ_W}}") + " " +
                  deco.magenta(f"{'Position':<{POS_W}}") + " " +
                  deco.white(f"{'Size (mm)':<{SIZE_W}}") + " " +
                  deco.cyan(f"{'Scale':<{SCALE_W}}") + " " +
                  deco.yellow("Flags"))
        print(header)
        print(deco.white("-" * (NAME_W + RES_W + HZ_W + POS_W + SIZE_W + SCALE_W + 14)))

        for mon in monitors:
            res = f"{mon.width}x{mon.height}"
            pos = f"{mon.x},{mon.y}"
            size = f"{mon.physical_width_mm}x{mon.physical_height_mm}"
            hidpi = mon.content_scale_x > 1.0 or mon.content_scale_y > 1.0
            scale = f"{mon.content_scale_x}x"
            flags = []
            if mon.primary:
                flags.append("primary")
            if hidpi:
                flags.append("hidpi")
            flags_str = " ".join(flags)

            row = (deco.white(f"{mon.name:<{NAME_W}}") + " " +
                   deco.cyan(f"{res:<{RES_W}}") + " " +
                   deco.green(f"{mon.refresh_rate:<{HZ_W}}") + " " +
                   deco.magenta(f"{pos:<{POS_W}}") + " " +
                   deco.white(f"{size:<{SIZE_W}}") + " " +
                   deco.cyan(f"{scale:<{SCALE_W}}") + " " +
                   deco.yellow(flags_str))
            print(row)

        print()
        print(deco.yellow("Usage:"))
        print(deco.white("  Primary window:   ") + deco.val("OrkEzApp.create(..., fullscreen_monitor=\"<name>\")"))
        print(deco.white("  Secondary window: ") + deco.val("createSecondaryWindow(..., fullscreen_monitor=\"<name>\")"))
        print()
