#!/usr/bin/env python3

from orkengine.core import Logger, vec3, ncui, CrcStringProxy
import time

# Create tokens proxy
tokens = CrcStringProxy()

logger = Logger.instance()
logger.enableNotCurses()
chan_test = logger.configureChannel("_TEST", vec3(1,0,1), True)

uictx = ncui.context()
tabg = uictx.content

# Test 1: Top Left (default)
box1 = ncui.Box()
box1.bgcolor = vec3(0.2, 0.0, 0.2)
box1.fgcolor = vec3(1, 1, 1)
box1.text = "Top Left\nDefault"
box1.halign = tokens.left
box1.valign = tokens.top
tabg.addTab("TL", box1)

# Test 2: Center Center
box2 = ncui.Box()
box2.bgcolor = vec3(0.0, 0.2, 0.2)
box2.fgcolor = vec3(1, 1, 1)
box2.text = "Center\nMulti-line\nText"
box2.halign = tokens.center
box2.valign = tokens.center
tabg.addTab("CC", box2)

# Test 3: Bottom Right
box3 = ncui.Box()
box3.bgcolor = vec3(0.2, 0.2, 0.0)
box3.fgcolor = vec3(1, 1, 1)
box3.text = "Bottom Right\nAligned"
box3.halign = tokens.right
box3.valign = tokens.bottom
tabg.addTab("BR", box3)

# Test 4: Mixed alignments
box4 = ncui.Box()
box4.bgcolor = vec3(0.1, 0.2, 0.1)
box4.fgcolor = vec3(1, 1, 1)
box4.text = "Left\nCenter\nVertical"
box4.halign = tokens.left
box4.valign = tokens.center
tabg.addTab("LC", box4)

# Test 5: Single line center
box5 = ncui.Box()
box5.bgcolor = vec3(0.2, 0.1, 0.2)
box5.fgcolor = vec3(1, 1, 1)
box5.text = "Single Line Center"
box5.halign = tokens.center
box5.valign = tokens.center
tabg.addTab("SLC", box5)

# Test 6: Property readback and copying
box6 = ncui.Box()
box6.bgcolor = vec3(0.1, 0.1, 0.2)
box6.fgcolor = vec3(1, 1, 1)
box6.text = "Copied alignment"
box6.halign = box2.halign  # Copy alignment from box2
box6.valign = box2.valign  # Copy alignment from box2
tabg.addTab("CPY", box6)

# Test invalid alignment (should default to left/top)
box7 = ncui.Box()
box7.bgcolor = vec3(0.1, 0.1, 0.1)
box7.fgcolor = vec3(1, 0, 0)
box7.text = "Invalid alignment"
box7.halign = tokens.invalid_horizontal  # Should default to left
box7.valign = tokens.invalid_vertical    # Should default to top
tabg.addTab("INV", box7)

# Log the initial test message
chan_test.log("Text justification test - switch between tabs to see different alignments")

uictx.waitForExit()
