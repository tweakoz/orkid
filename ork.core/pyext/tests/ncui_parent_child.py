#!/usr/bin/env python3

from orkengine.core import Logger, vec3, ncui, CrcStringProxy
import time

# Create tokens proxy
tokens = CrcStringProxy()

logger = Logger.instance()
logger.enableNotCurses()
chan_test = logger.configureChannel("PARENT_CHILD_TEST", vec3(0, 1, 1), True)

uictx = ncui.context()

# Test parent-child relationships
main_split = ncui.HorizSplit()
main_split.split_position = 0.5

# Right side: another split
right_split = ncui.VerticalSplit()
right_split.split_position = 0.6

# Top part of right split
top_box = ncui.Box()
top_box.text = "Top Box"
top_box.bgcolor = vec3(0.2, 0.2, 0.4)
top_box.fgcolor = vec3(1, 1, 1)

# Bottom part of right split
bottom_pack = ncui.HorizontalPack()

for i in range(2):
    box = ncui.Box()
    box.text = f"Box {i+1}"
    box.bgcolor = vec3(0.1 + i * 0.2, 0.3, 0.1 + i * 0.2)
    box.fgcolor = vec3(1, 1, 1)
    box.width = 10
    box.height = 3
    bottom_pack.addChild(box)

# Left side pack
left_pack = ncui.VerticalPack()

# Set up the hierarchy
right_split.setTop(top_box)
right_split.setBottom(bottom_pack)
main_split.setLeft(left_pack)
main_split.setRight(right_split)

# Test hierarchy walking
def walk_hierarchy(widget):
    chan_test.log("Walking widget hierarchy:")
    chan_test.log(f"Widget: {widget.name}")
    
    # Test parent access
    parent = widget.getParent()
    if parent:
        chan_test.log(f"  Parent: {parent.name}")
    else:
        chan_test.log("  No parent")
    
    # Test root access
    root = widget.getRoot()
    if root:
        chan_test.log(f"  Root: {root.name}")
    else:
        chan_test.log("  No root")
    
    # Test ancestors
    ancestors = widget.getAncestors()
    chan_test.log(f"  Ancestors: {len(ancestors)}")
    for i, ancestor in enumerate(ancestors):
        chan_test.log(f"    {i}: {ancestor.name}")
    
    # Test isChildOf
    if parent:
        is_child = widget.isChildOf(parent)
        chan_test.log(f"  isChildOf parent: {is_child}")
        
        if root and root != parent:
            is_child_of_root = widget.isChildOf(root)
            chan_test.log(f"  isChildOf root: {is_child_of_root}")

# Add a test button for hierarchy walking
test_btn = ncui.Button()
test_btn.text = "Walk Hierarchy"
test_btn.width = 20
test_btn.height = 2
test_btn.normal_bg_color = vec3(0.3, 0.1, 0.3)
test_btn.on_click = lambda: walk_hierarchy(test_btn)

# Python bindings handle parent automatically
left_pack.addChild(test_btn)

#tabg.addTab("Test", test_pack)
splitV = ncui.VerticalSplit()
splitV.split_position = 0.4  # 60% for logger, 40% for debug
logger_tabs = uictx.swapContent(splitV)
splitV.top = logger_tabs
splitV.bottom = main_split

# Status updates
chan_test.log("Parent-Child Relationship Test")
chan_test.log("- Click buttons to test parent relationships")
chan_test.log("- Use 'Walk Hierarchy' to see complete structure")
chan_test.log("- Each widget should have proper parent pointers")

chan_test.log("Widget Complex hierarchy created")
chan_test.log("Relationships: Parent pointers set")
chan_test.log("Tests: Click buttons to test")

uictx.waitForExit()  # Wait for user to exit
for name in list(locals().keys()):
    if not name.startswith('_'):
        del locals()[name]
