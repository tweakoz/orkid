#!/usr/bin/env python3

from orkengine.core import Logger, vec3, ncui, CrcStringProxy
import time

# Create tokens proxy
tokens = CrcStringProxy()

logger = Logger.instance()
logger.enableNotCurses()
chan_test = logger.configureChannel("BUTTON_TEST", vec3(1, 0.5, 0), True)

uictx = ncui.context()
tabg = uictx.content

# Helper function to create test buttons
def create_button(text, colors=None):
    btn = ncui.Button()
    btn.text = text
    btn.width = 20
    btn.height = 3
    
    if colors:
        btn.normal_bg_color = colors.get('normal_bg', vec3(0.3, 0.3, 0.3))
        btn.normal_fg_color = colors.get('normal_fg', vec3(1, 1, 1))
        btn.hover_bg_color = colors.get('hover_bg', vec3(0.5, 0.5, 0.5))
        btn.hover_fg_color = colors.get('hover_fg', vec3(1, 1, 1))
        btn.pressed_bg_color = colors.get('pressed_bg', vec3(0.1, 0.1, 0.1))
        btn.pressed_fg_color = colors.get('pressed_fg', vec3(0.8, 0.8, 0.8))
    
    return btn

# Test 1: Basic Buttons with different colors
basic_pack = ncui.VerticalPack()

# Red button
red_btn = create_button("Red Button", {
    'normal_bg': vec3(0.5, 0.1, 0.1),
    'hover_bg': vec3(0.7, 0.2, 0.2),
    'pressed_bg': vec3(0.3, 0.05, 0.05)
})
#red_btn.on_click = lambda: chan_test.log("Red button clicked!")
basic_pack.addChild(red_btn)

# Green button  
green_btn = create_button("Green Button", {
    'normal_bg': vec3(0.1, 0.5, 0.1),
    'hover_bg': vec3(0.2, 0.7, 0.2),
    'pressed_bg': vec3(0.05, 0.3, 0.05)
})
green_btn.on_click = lambda: chan_test.log("Green button clicked!")
basic_pack.addChild(green_btn)

# Blue button
blue_btn = create_button("Blue Button", {
    'normal_bg': vec3(0.1, 0.1, 0.5),
    'hover_bg': vec3(0.2, 0.2, 0.7),
    'pressed_bg': vec3(0.05, 0.05, 0.3)
})
blue_btn.on_click = lambda: chan_test.log("Blue button clicked!")
basic_pack.addChild(blue_btn)

pur_btn = create_button("Blue Button", {
    'normal_bg': vec3(0.5, 0.1, 0.5),
    'hover_bg': vec3(0.7, 0.2, 0.7),
    'pressed_bg': vec3(0.3, 0.05, 0.3)
})
pur_btn.on_click = lambda: chan_test.log("Pur button clicked!")
basic_pack.addChild(pur_btn)

tabg.addTab("Basic", basic_pack)

# Test 2: Text Alignment
align_pack = ncui.VerticalPack()

# Left aligned
left_btn = create_button("Left Aligned")
left_btn.halign = tokens.left
left_btn.on_click = lambda: chan_test.log("Left aligned button clicked!")
align_pack.addChild(left_btn)

# Center aligned (default)
center_btn = create_button("Center Aligned")
center_btn.halign = tokens.center
center_btn.on_click = lambda: chan_test.log("Center aligned button clicked!")
align_pack.addChild(center_btn)

# Right aligned
right_btn = create_button("Right Aligned")
right_btn.halign = tokens.right
right_btn.on_click = lambda: chan_test.log("Right aligned button clicked!")
align_pack.addChild(right_btn)

tabg.addTab("Alignment", align_pack)

# Test 3: Vertical Alignment and Multi-line Text
valign_pack = ncui.VerticalPack()

# Top aligned multi-line
top_btn = create_button("Top\nAligned\nText")
top_btn.valign = tokens.top
top_btn.height = 5
top_btn.on_click = lambda: chan_test.log("Top aligned button clicked!")
valign_pack.addChild(top_btn)

# Center aligned multi-line
center_v_btn = create_button("Center\nAligned\nText")
center_v_btn.valign = tokens.center
center_v_btn.height = 5
center_v_btn.on_click = lambda: chan_test.log("Center (vertical) aligned button clicked!")
valign_pack.addChild(center_v_btn)

# Bottom aligned multi-line
bottom_btn = create_button("Bottom\nAligned\nText")
bottom_btn.valign = tokens.bottom
bottom_btn.height = 5
bottom_btn.on_click = lambda: chan_test.log("Bottom aligned button clicked!")
valign_pack.addChild(bottom_btn)

tabg.addTab("VAlign", valign_pack)

# Test 4: Different Sizes and States
sizes_pack = ncui.VerticalPack()

# Small button
small_btn = create_button("Small")
small_btn.width = 10
small_btn.height = 1
small_btn.on_click = lambda: chan_test.log("Small button clicked!")
sizes_pack.addChild(small_btn)

# Normal button
normal_btn = create_button("Normal Size")
normal_btn.width = 15
normal_btn.height = 3
normal_btn.on_click = lambda: chan_test.log("Normal button clicked!")
sizes_pack.addChild(normal_btn)

# Large button
large_btn = create_button("Large Button")
large_btn.width = 25
large_btn.height = 5
large_btn.on_click = lambda: chan_test.log("Large button clicked!")
sizes_pack.addChild(large_btn)

# Disabled button
disabled_btn = create_button("Disabled Button", {
    'disabled_bg': vec3(0.2, 0.2, 0.2),
    'disabled_fg': vec3(0.5, 0.5, 0.5)
})
disabled_btn.enabled = False
disabled_btn.on_click = lambda: chan_test.log("This should not appear!")
sizes_pack.addChild(disabled_btn)

tabg.addTab("Sizes", sizes_pack)

# Test 5: Interactive Example - Counter
counter_pack = ncui.VerticalPack()

# Counter state
counter_value = [0]

# Counter display (using a button as display)
counter_display = create_button(f"Count: {counter_value[0]}", {
    'normal_bg': vec3(0.2, 0.2, 0.2),
    'normal_fg': vec3(1, 1, 0)
})
counter_display.enabled = False
counter_pack.addChild(counter_display)

# Increment button
def increment():
    counter_value[0] += 1
    counter_display.text = f"Count: {counter_value[0]}"
    chan_test.log(f"Counter incremented to {counter_value[0]}")

inc_btn = create_button("Increment (+)")
inc_btn.normal_bg_color = vec3(0.1, 0.4, 0.1)
inc_btn.hover_bg_color = vec3(0.2, 0.6, 0.2)
inc_btn.on_click = increment
counter_pack.addChild(inc_btn)

# Decrement button
def decrement():
    counter_value[0] -= 1
    counter_display.text = f"Count: {counter_value[0]}"
    chan_test.log(f"Counter decremented to {counter_value[0]}")

dec_btn = create_button("Decrement (-)")
dec_btn.normal_bg_color = vec3(0.4, 0.1, 0.1)
dec_btn.hover_bg_color = vec3(0.6, 0.2, 0.2)
dec_btn.on_click = decrement
counter_pack.addChild(dec_btn)

# Reset button
def reset():
    counter_value[0] = 0
    counter_display.text = f"Count: {counter_value[0]}"
    chan_test.log("Counter reset to 0")

reset_btn = create_button("Reset")
reset_btn.normal_bg_color = vec3(0.3, 0.3, 0.1)
reset_btn.hover_bg_color = vec3(0.5, 0.5, 0.2)
reset_btn.on_click = reset
counter_pack.addChild(reset_btn)

tabg.addTab("Counter", counter_pack)

# Test 6: Layout Integration
layout_test = ncui.HorizSplit()
layout_test.split_position = 0.5

# Left side: Vertical pack of buttons
left_vpack = ncui.VerticalPack()
for i in range(5):
    btn = create_button(f"Button {i+1}")
    btn.on_click = lambda i=i: chan_test.log(f"Button {i+1} in left panel clicked!")
    left_vpack.addChild(btn)

# Right side: Horizontal pack of buttons
right_hpack = ncui.HorizontalPack()
for i in range(3):
    btn = create_button(f"H{i+1}")
    btn.width = 12
    btn.on_click = lambda i=i: chan_test.log(f"Horizontal button {i+1} clicked!")
    right_hpack.addChild(btn)

layout_test.left = left_vpack
layout_test.right = right_hpack

tabg.addTab("Layout", layout_test)

# Log initial test information
chan_test.log("Button Widget Test Suite")
chan_test.log("- Basic: Colored buttons with click callbacks")
chan_test.log("- Alignment: Horizontal text alignment")
chan_test.log("- VAlign: Vertical text alignment with multi-line text")
chan_test.log("- Sizes: Different button sizes and disabled state")
chan_test.log("- Counter: Interactive counter example")
chan_test.log("- Layout: Buttons integrated with layout widgets")
chan_test.log("Click buttons to test functionality!")

# Status updates
chan_test.status("Buttons", "6 test categories")
chan_test.status("Features", "colors, alignment, callbacks")
chan_test.status("Interaction", "mouse and keyboard")



uictx.waitForExit()

# release all locals (namely the lambdas)
for name in list(locals().keys()):
    if not name.startswith('_'):
        del locals()[name]
         
