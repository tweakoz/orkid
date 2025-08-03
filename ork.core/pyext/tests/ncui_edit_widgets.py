#!/usr/bin/env python3
################################################################
# Orkid Media Engine
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License.
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################

from orkengine.core import Logger, vec3, ncui, CrcStringProxy
import time, threading

# Create tokens proxy for alignment
tokens = CrcStringProxy()

# Setup logging
logger = Logger.instance()
logger.enableNotCurses()
chan_test = logger.configureChannel("EDIT_TEST", vec3(0.5, 1, 0.5), True)

# Create context
uictx = ncui.context()

def create_form_row(label_text, edit_widget, value_display=None):
    row = ncui.HorizontalPack()
    row.height = 1  # Set height for the row
    # Create label
    label = ncui.Box()
    label.text = label_text
    label.width = 20
    label.height = 1
    label.bgcolor = vec3(0.3, 0.3, 0.3)
    label.fgcolor = vec3(1.0, 1.0, 1.0)
    label.halign = tokens.right
    label.valign = tokens.center
    
    # Add widgets to row
    row.addChild(label)
    row.addChild(edit_widget)
    
    if value_display:
        row.addChild(value_display)
    
    return row

main_layout = ncui.VerticalPack()
main_layout.spacing = True  # Ensure adequate height for all widgets
#main_layout.spacing = 1
title_box = ncui.Box()
title_box.text = "Edit Widget Demo - All Types"
title_box.width = 80
title_box.height = 1
title_box.bgcolor = vec3(0.2, 0.4, 0.6)
title_box.fgcolor = vec3(1.0, 1.0, 1.0)
title_box.halign = tokens.center
title_box.valign = tokens.center

int_display = ncui.Box()
int_display.width = 20
int_display.height = 1
int_display.bgcolor = vec3(0.1, 0.1, 0.1)
int_display.fgcolor = vec3(0.8, 0.8, 0.8)
#int_display.text = f"Value: {int_edit.getValue()}"
int_display.halign = tokens.left
int_display.valign = tokens.center

float_display = ncui.Box()
float_display.width = 20
float_display.height = 1
float_display.bgcolor = vec3(0.1, 0.1, 0.1)
float_display.fgcolor = vec3(0.8, 0.8, 0.8)
#float_display.text = f"Value: {float_edit.getValue():.2f}"
float_display.halign = tokens.left
float_display.valign = tokens.center

options_title = ncui.Box()
options_title.text = "Options:"
options_title.width = 60
options_title.height = 1
options_title.bgcolor = vec3(0.4, 0.4, 0.4)
options_title.fgcolor = vec3(1.0, 1.0, 1.0)
options_title.halign = tokens.left
options_title.valign = tokens.center

status_box = ncui.Box()
status_box.text = "Status: Ready for input - Click to focus, type to edit, Enter to commit"
status_box.width = 80
status_box.height = 1
status_box.bgcolor = vec3(0.1, 0.3, 0.1)
status_box.fgcolor = vec3(0.8, 1.0, 0.8)
status_box.halign = tokens.center
status_box.valign = tokens.center

# Validation examples section
validation_title = ncui.Box()
validation_title.text = "Validation Examples:"
validation_title.width = 60
validation_title.height = 1
validation_title.bgcolor = vec3(0.4, 0.2, 0.2)
validation_title.fgcolor = vec3(1.0, 1.0, 1.0)
validation_title.halign = tokens.center
validation_title.valign = tokens.center

text_edit = ncui.TextEdit()
text_edit.text = "Enter your name"
text_edit.width = 30
text_edit.height = 1

int_edit = ncui.IntEdit()
int_edit.width = 15
int_edit.height = 1
int_edit.min_value = 0
int_edit.max_value = 120
int_edit.setValue(25)

float_edit = ncui.FloatEdit()
float_edit.width = 15
float_edit.height = 1
float_edit.min_value = 0.0
float_edit.max_value = 3.0
float_edit.decimal_places = 2
float_edit.setValue(1.75)

strict_int = ncui.IntEdit()
strict_int.width = 10
strict_int.height = 1
strict_int.min_value = 1
strict_int.max_value = 10
strict_int.setValue(5)

strict_float = ncui.FloatEdit()
strict_float.width = 10
strict_float.height = 1
strict_float.min_value = 0.0
strict_float.max_value = 1.0
strict_float.decimal_places = 3
strict_float.setValue(0.5)

text_row = create_form_row("Full Name:", text_edit)
age_row = create_form_row("Age:", int_edit, int_display)
height_row = create_form_row("Height (m):", float_edit, float_display)
validation_row1 = create_form_row("Number (1-10):", strict_int)
validation_row2 = create_form_row("Ratio (0.0-1.0):", strict_float)

bool_hpack = ncui.HorizontalPack()
bool_hpack.height = 1  # Set height for the boolean options row

if True:
  bool_edit1 = ncui.BoolEdit()
  bool_edit1.width = 25
  bool_edit1.height = 1
  bool_edit1.label = "Subscribe newsletter"
  bool_edit1.value = True

  bool_edit2 = ncui.BoolEdit()
  bool_edit2.width = 22
  bool_edit2.height = 1
  bool_edit2.label = "Enable notifications"
  bool_edit2.value = False

  bool_edit3 = ncui.BoolEdit()
  bool_edit3.width = 20
  bool_edit3.height = 1
  bool_edit3.label = "Remember settings"
  bool_edit3.value = True

  bool_hpack.addChild(bool_edit1)
  bool_hpack.addChild(bool_edit2)
  bool_hpack.addChild(bool_edit3)

main_layout.addChild(title_box)
main_layout.addChild(text_row)
main_layout.addChild(age_row)
main_layout.addChild(height_row)
main_layout.addChild(options_title)
main_layout.addChild(bool_hpack)
main_layout.addChild(validation_title)
main_layout.addChild(validation_row1)
main_layout.addChild(validation_row2)


splitV = ncui.VerticalSplit()
splitV.split_position = 0.4  # 40% for logger, 60% for edit widgets
logger_tabs = uictx.swapContent(splitV)
splitV.top = logger_tabs
splitV.bottom = main_layout

# Log test information
chan_test.log("Edit Widget Test Suite - Complete Demo")
chan_test.log("All edit widgets on single page:")
chan_test.log("- TextEdit: Free-form text input with cursor navigation")
chan_test.log("- IntEdit: Integer input with range validation (0-120)")
chan_test.log("- FloatEdit: Float input with precision control (0.0-3.0, 2 decimals)")
chan_test.log("- BoolEdit: Checkbox-style boolean toggles")
chan_test.log("- Validation: Strict range examples (1-10, 0.0-1.0)")
chan_test.log("")
chan_test.log("Interaction:")
chan_test.log("Click any edit field to focus (blue border)")
chan_test.log("Type to edit text/numbers")
chan_test.log("• Use arrow keys for cursor navigation")
chan_test.log("• Space to toggle checkboxes")
chan_test.log("• Enter to commit and release focus")
chan_test.log("• Invalid inputs show red background")

#########################################################
# Create update thread to refresh value displays
#########################################################

keep_running = True
def update_displays():
  while keep_running:
    current_int = int_edit.getValue()
    current_float = float_edit.getValue()
    int_display.text = f"Value: {current_int}"
    float_display.text = f"Value: {current_float:.2f}"
    time.sleep(0.25)

update_thread = threading.Thread(target=update_displays)
update_thread.daemon = True   
update_thread.start()
uictx.waitForExit()
keep_running = False
update_thread.join()

#########################################################
# Cleanup
#########################################################

for name in list(locals().keys()):
    if not name.startswith('_'):
        del locals()[name]
