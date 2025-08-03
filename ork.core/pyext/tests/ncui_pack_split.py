#!/usr/bin/env python3

from orkengine.core import Logger, vec3, ncui, CrcStringProxy
import time

# Create tokens proxy
tokens = CrcStringProxy()

logger = Logger.instance()
logger.enableNotCurses()
chan_test = logger.configureChannel("PACK_SPLIT", vec3(0, 1, 1), True)

uictx = ncui.context()
tabg = uictx.content

# Helper function to create colored boxes for testing
def create_test_box(text, bgcolor, fgcolor=vec3(1, 1, 1)):
    box = ncui.Box()
    box.text = text
    box.bgcolor = bgcolor
    box.fgcolor = fgcolor
    box.halign = tokens.center
    box.valign = tokens.center
    return box

# Test 1: VerticalPack - Multiple boxes stacked vertically
vpack = ncui.VerticalPack()

box1 = create_test_box("Box 1", vec3(0.3, 0.1, 0.1))
box2 = create_test_box("Box 2", vec3(0.1, 0.3, 0.1))
box3 = create_test_box("Box 3", vec3(0.1, 0.1, 0.3))

# Set different heights for boxes
box1.height = 4
box2.height = 4
box3.height = 4

vpack.addChild(box1)
vpack.addChild(box2)
vpack.addChild(box3)

tabg.addTab("VPack", vpack)

# Test 2: HorizontalPack - Multiple boxes side by side
hpack = ncui.HorizontalPack()

box4 = create_test_box("Left\nBox", vec3(0.3, 0.3, 0.1))
box5 = create_test_box("Center\nBox", vec3(0.3, 0.1, 0.3))
box6 = create_test_box("Right\nBox", vec3(0.1, 0.3, 0.3))

# Set different widths for boxes
box4.width = 16
box5.width = 16
box6.width = 16

hpack.addChild(box4)
hpack.addChild(box5)
hpack.addChild(box6)

tabg.addTab("HPack", hpack)

# Test 3: HorizSplit - Left/Right split with different content
hsplit = ncui.HorizSplit()
hsplit.split_position = 0.3
#hsplit.split_color = vec3(1, 1, 0)  # Yellow separator

# Left side: TextLines with scrollable content
left_text = ncui.TextLines()
left_text.color = vec3(0, 1, 0)
left_text.bg_color = vec3(0.1, 0.1, 0.1)
for i in range(20):
    left_text.addLine(f"Left line {i+1}")

# Right side: Nested VerticalPack
right_vpack = ncui.VerticalPack()
right_box1 = create_test_box("Right Top", vec3(0.2, 0.2, 0.4))
right_box2 = create_test_box("Right Bottom", vec3(0.4, 0.2, 0.2))
right_box1.height = 8
right_box2.height = 6
right_vpack.addChild(right_box1)
right_vpack.addChild(right_box2)

hsplit.left = left_text
hsplit.right = right_vpack

tabg.addTab("HSplit", hsplit)

# Test 4: VerticalSplit - Top/Bottom split
vsplit = ncui.VerticalSplit()
vsplit.split_position = 0.4
#vsplit.split_color = vec3(1, 0, 1)  # Magenta separator

# Top side: HorizontalPack
top_hpack = ncui.HorizontalPack()
for i in range(4):
    small_box = create_test_box(f"T{i+1}", vec3(0.1 + i*0.1, 0.3, 0.2))
    small_box.width = 8
    top_hpack.addChild(small_box)

# Bottom side: Large centered box
bottom_box = create_test_box("Bottom Area\nLarge Content\nMultiple Lines", vec3(0.2, 0.4, 0.2))

vsplit.top = top_hpack
vsplit.bottom = bottom_box

tabg.addTab("VSplit", vsplit)

# Test 5: Complex Layout - Nested splits and packs
complex_layout = ncui.HorizSplit()
complex_layout.split_position = 0.5
#complex_layout.split_color = vec3(0.5, 0.5, 0.5)

# Left side: VerticalSplit
left_vsplit = ncui.VerticalSplit()
left_vsplit.split_position = 0.6
#left_vsplit.split_color = vec3(0.7, 0.3, 0.3)

# Top of left: HorizontalPack
left_top_hpack = ncui.HorizontalPack()
for i in range(3):
    mini_box = create_test_box(f"L{i+1}", vec3(0.4, 0.1*i, 0.4))
    mini_box.width = 10
    left_top_hpack.addChild(mini_box)

# Bottom of left: Single box
left_bottom_box = create_test_box("Left\nBottom", vec3(0.1, 0.4, 0.4))

left_vsplit.top = left_top_hpack
left_vsplit.bottom = left_bottom_box

# Right side: VerticalPack with mixed content
right_vpack_complex = ncui.VerticalPack()

# Add TextLines
right_text = ncui.TextLines()
right_text.color = vec3(1, 1, 0)
right_text.bg_color = vec3(0.2, 0.1, 0.2)
right_text.height = 6
for i in range(10):
    right_text.addLine(f"Right log {i+1}: Some message here")

# Add boxes with different alignments
aligned_box1 = create_test_box("Top Right", vec3(0.3, 0.3, 0.1))
aligned_box1.height = 3
aligned_box1.halign = tokens.right
aligned_box1.valign = tokens.top

aligned_box2 = create_test_box("Center", vec3(0.1, 0.3, 0.3))
aligned_box2.height = 4
aligned_box2.halign = tokens.center
aligned_box2.valign = tokens.center

right_vpack_complex.addChild(right_text)
right_vpack_complex.addChild(aligned_box1)
right_vpack_complex.addChild(aligned_box2)

complex_layout.left = left_vsplit
complex_layout.right = right_vpack_complex

tabg.addTab("Complex", complex_layout)

# Test 6: Dynamic Properties Test
props_test = ncui.VerticalPack()

# Create some test widgets
test_box = create_test_box("Test Widget", vec3(0.2, 0.2, 0.4))
test_box.height = 5

test_hpack = ncui.HorizontalPack()

for i in range(3):
    small = create_test_box(f"S{i}", vec3(0.1, 0.1, 0.1))
    small.width = 6
    test_hpack.addChild(small)

test_split = ncui.HorizSplit()
test_split.split_position = 0.7  # Uneven split
test_split.split_color = vec3(0, 1, 0)  # Green separator
test_split.left = create_test_box("70%", vec3(0.3, 0.1, 0.1))
test_split.right = create_test_box("30%", vec3(0.1, 0.1, 0.3))

props_test.addChild(test_box)
props_test.addChild(test_hpack)
props_test.addChild(test_split)

tabg.addTab("Props", props_test)

# Log test information
chan_test.log("Pack & Split Widget Test - Comprehensive layout testing")
chan_test.log(f"VerticalPack children: {len(vpack.children)}")
chan_test.log(f"HorizontalPack children: {len(hpack.children)}")
chan_test.log(f"HorizSplit position: {hsplit.split_position}")
chan_test.log(f"VerticalSplit position: {vsplit.split_position}")

# Status updates
chan_test.status("VPack", f"{len(vpack.children)} children")
chan_test.status("HSplit", f"pos={hsplit.split_position:.1f}")
chan_test.status("VSplit", f"pos={vsplit.split_position:.1f}")
chan_test.status("Complex", "nested layout")

uictx.waitForExit()  # Wait for user to exit
for name in list(locals().keys()):
    if not name.startswith('_'):
        del locals()[name]
