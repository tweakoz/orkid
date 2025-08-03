#!/usr/bin/env python3

from orkengine.core import Logger, vec3, ncui
import time

# Enable normal NotCurses logging (this creates TabGroup as root content)
logger = Logger.instance()
logger.enableNotCurses()
chan_debug = logger.configureChannel("COMBO_DEBUG", vec3(1, 1, 0), True)

# Get UI context - at this point logger TabGroup is the root content
uictx = ncui.context()

# Create debug content section
debug_pack = ncui.VerticalPack()

# Add title box
title_box = ncui.Box()
title_box.bgcolor = vec3(0.1, 0.1, 0.3)
title_box.fgcolor = vec3(1, 1, 1)
title_box.text = "ComboBox Debug Section"
title_box.width = 40
title_box.height = 2
debug_pack.addChild(title_box)

# Create a simple ComboBox for testing
combo = ncui.ComboBox()
combo.items = ["Item 1", "Item 2", "Item 3", "Item 4", "Item 5"]
combo.width = 25
combo.height = 1
combo.selected_index = 0

# Add callback to log selection events with detailed position info
def on_selection_changed(index, value):
    chan_debug.log(f"=== SELECTION EVENT ===")
    chan_debug.log(f"Selected: index={index}, value='{value}'")
    chan_debug.log(f"Combo position: x={combo.x}, y={combo.y}")
    chan_debug.log(f"Combo size: w={combo.width}, h={combo.height}")
    chan_debug.log(f"======================")

combo.on_selection_changed = on_selection_changed
debug_pack.addChild(combo)

# Create a button to log current positions
debug_button = ncui.Button()
debug_button.text = "Log Positions"
debug_button.width = 20
debug_button.height = 2

def log_current_positions():
    chan_debug.log("=== CURRENT POSITIONS ===")
    chan_debug.log(f"Combo: x={combo.x}, y={combo.y}, w={combo.width}, h={combo.height}")
    chan_debug.log(f"Title: x={title_box.x}, y={title_box.y}, w={title_box.width}, h={title_box.height}")
    chan_debug.log(f"Debug Pack: x={debug_pack.x}, y={debug_pack.y}, w={debug_pack.width}, h={debug_pack.height}")
    chan_debug.log(f"Context size: w={uictx.width}, h={uictx.height}")
    chan_debug.log(f"Expected dropdown Y: {combo.y + combo.height}")
    chan_debug.log(f"========================")

debug_button.on_click = log_current_positions
debug_pack.addChild(debug_button)

# Add a test area indicator
test_area = ncui.Box()
test_area.bgcolor = vec3(0.1, 0.2, 0.1)
test_area.fgcolor = vec3(1, 1, 1)
test_area.text = "Test Area - Dropdown should appear HERE when ComboBox is clicked"
test_area.width = 50
test_area.height = 2
debug_pack.addChild(test_area)

# Now create the vertical split and rearrange the UI
split = ncui.VerticalSplit()
split.split_position = 0.6  # 60% for logger, 40% for debug

# Swap the root content: remove logger TabGroup, install VerticalSplit
logger_tabs = uictx.swapContent(split)

# Arrange the split content
split.top = logger_tabs     # Logger tabs on top
split.bottom = debug_pack   # Debug content on bottom

# Log initial setup
chan_debug.log("=== SPLIT LAYOUT CREATED ===")
chan_debug.log("Logger tabs are now in the TOP section")
chan_debug.log("Debug content is in the BOTTOM section")
chan_debug.log("Split position: 60% top, 40% bottom")

# Wait a moment for layout to settle
time.sleep(0.1)

# Log final positions after split layout
chan_debug.log("=== AFTER SPLIT LAYOUT ===")
chan_debug.log(f"Split: x={split.x}, y={split.y}, w={split.width}, h={split.height}")
chan_debug.log(f"Debug pack: x={debug_pack.x}, y={debug_pack.y}, w={debug_pack.width}, h={debug_pack.height}")
chan_debug.log(f"Combo final: x={combo.x}, y={combo.y}, w={combo.width}, h={combo.height}")

# Instructions
chan_debug.log("=== TEST INSTRUCTIONS ===")
chan_debug.log("1. You should see logger tabs in the TOP section")
chan_debug.log("2. Debug content should be in the BOTTOM section")  
chan_debug.log("3. Click 'Log Positions' to verify coordinates")
chan_debug.log("4. Click ComboBox - dropdown should appear in debug section")
chan_debug.log("5. Try selecting items and verify correct selection")
chan_debug.log("========================")

# Keep running
time.sleep(60) 