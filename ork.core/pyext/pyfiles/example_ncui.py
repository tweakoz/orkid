#!/usr/bin/env python3

"""
Example usage of the Orkid Core NotCurses UI Python bindings.

This script demonstrates:
1. Basic widget usage
2. Layout management
3. Custom widget creation
4. Event handling and interaction
"""

import sys
import time
import threading
import ork.core as core
import ork.core.ncui as ncui
import ork.core.pyfiles.ncui_cleanup as ncui_cleanup

def basic_widgets_example():
    """Basic widget usage"""
    print("=== Basic Widgets Example ===")
    
    # Set up cleanup
    ncui_cleanup.setup()
    
    # Get UI context
    ui = ncui.Context.instance()
    
    # Create a simple text widget
    text_widget = ncui.TextLines()
    text_widget.setColor(core.fvec3(0, 1, 0))  # Green text
    text_widget.addLine("Hello, NotCurses UI!")
    text_widget.addLine("This is a text widget")
    text_widget.addLine("Press Ctrl+C to exit")
    
    # Set as root content
    ui.setContent(text_widget)
    
    # Add more lines periodically
    def add_lines():
        for i in range(10):
            time.sleep(2)
            text_widget.addLine(f"Line {i+4}")
    
    # Start background thread
    thread = threading.Thread(target=add_lines)
    thread.daemon = True
    thread.start()
    
    # Keep running until interrupted
    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        pass

def layout_example():
    """Layout management example"""
    print("=== Layout Management Example ===")
    
    ncui_cleanup.setup()
    ui = ncui.Context.instance()
    
    # Create multiple text widgets
    header = ncui.TextLines()
    header.setColor(core.fvec3(1, 1, 0))  # Yellow
    header.addLine("=== Header Widget ===")
    header.addLine("This is the header")
    
    content = ncui.TextLines()
    content.setColor(core.fvec3(0, 1, 1))  # Cyan
    content.addLine("Content area")
    content.addLine("This is where main content goes")
    
    footer = ncui.TextLines()
    footer.setColor(core.fvec3(1, 0, 1))  # Magenta
    footer.addLine("Footer: Press Ctrl+C to exit")
    
    # Create vertical layout
    layout = ncui.VerticalPack()
    layout.spacing = 1.0
    layout.addChild(header)
    layout.addChild(content)
    layout.addChild(footer)
    
    ui.setContent(layout)
    
    # Add content periodically
    def update_content():
        for i in range(20):
            time.sleep(1)
            content.addLine(f"Content line {i+3}")
    
    thread = threading.Thread(target=update_content)
    thread.daemon = True
    thread.start()
    
    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        pass

def tabs_example():
    """Tab group example"""
    print("=== Tab Group Example ===")
    
    ncui_cleanup.setup()
    ui = ncui.Context.instance()
    
    # Create tab group
    tabs = ncui.TabGroup()
    
    # Create content for each tab
    log_widget = ncui.TextLines()
    log_widget.setColor(core.fvec3(0, 1, 0))  # Green
    log_widget.addLine("Log messages:")
    
    status_widget = ncui.TextLines()
    status_widget.setColor(core.fvec3(0, 0, 1))  # Blue
    status_widget.addLine("System status:")
    status_widget.addLine("CPU: Normal")
    status_widget.addLine("Memory: 45%")
    
    debug_widget = ncui.TextLines()
    debug_widget.setColor(core.fvec3(1, 0, 0))  # Red
    debug_widget.addLine("Debug information:")
    debug_widget.addLine("Debug mode: ON")
    
    # Add tabs with different colors
    tabs.addTab("Logs", log_widget, core.fvec3(0, 1, 0))
    tabs.addTab("Status", status_widget, core.fvec3(0, 0, 1))
    tabs.addTab("Debug", debug_widget, core.fvec3(1, 0, 0))
    
    ui.setContent(tabs)
    
    # Simulate activity
    def simulate_activity():
        for i in range(50):
            time.sleep(0.5)
            
            # Add log messages
            log_widget.addLine(f"Log entry {i+1}")
            
            # Update status periodically
            if i % 5 == 0:
                status_widget.addLine(f"Status update {i//5 + 1}")
            
            # Add debug info
            if i % 3 == 0:
                debug_widget.addLine(f"Debug: Operation {i//3 + 1}")
    
    thread = threading.Thread(target=simulate_activity)
    thread.daemon = True
    thread.start()
    
    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        pass

def split_layout_example():
    """Split layout example"""
    print("=== Split Layout Example ===")
    
    ncui_cleanup.setup()
    ui = ncui.Context.instance()
    
    # Create horizontal split
    hsplit = ncui.HorizSplit()
    hsplit.split_position = 0.6  # 60% left, 40% right
    hsplit.split_color = core.fvec3(0.5, 0.5, 0.5)  # Gray splitter
    
    # Left side content
    left_content = ncui.TextLines()
    left_content.setColor(core.fvec3(0, 1, 0))  # Green
    left_content.addLine("Left Panel")
    left_content.addLine("This is the left side")
    
    # Right side - vertical split
    vsplit = ncui.VerticalSplit()
    vsplit.split_position = 0.7  # 70% top, 30% bottom
    vsplit.split_color = core.fvec3(0.5, 0.5, 0.5)  # Gray splitter
    
    # Right top content
    right_top = ncui.TextLines()
    right_top.setColor(core.fvec3(0, 0, 1))  # Blue
    right_top.addLine("Right Top")
    right_top.addLine("This is the right top")
    
    # Right bottom content
    right_bottom = ncui.TextLines()
    right_bottom.setColor(core.fvec3(1, 0, 0))  # Red
    right_bottom.addLine("Right Bottom")
    right_bottom.addLine("This is the right bottom")
    
    # Set up split hierarchy
    vsplit.top = right_top
    vsplit.bottom = right_bottom
    
    hsplit.left = left_content
    hsplit.right = vsplit
    
    ui.setContent(hsplit)
    
    # Add content to all panels
    def update_panels():
        for i in range(30):
            time.sleep(1)
            left_content.addLine(f"Left {i+3}")
            right_top.addLine(f"Right Top {i+3}")
            right_bottom.addLine(f"Right Bottom {i+3}")
    
    thread = threading.Thread(target=update_panels)
    thread.daemon = True
    thread.start()
    
    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        pass

def boxes_example():
    """Box widgets example"""
    print("=== Box Widgets Example ===")
    
    ncui_cleanup.setup()
    ui = ncui.Context.instance()
    
    # Create layout with boxes
    layout = ncui.VerticalPack()
    layout.spacing = 1.0
    
    # Create colored boxes
    box1 = ncui.Box()
    box1.bgcolor = core.fvec3(1, 0, 0)  # Red background
    box1.fgcolor = core.fvec3(1, 1, 1)  # White text
    box1.text = "Red Box"
    
    box2 = ncui.Box()
    box2.bgcolor = core.fvec3(0, 1, 0)  # Green background
    box2.fgcolor = core.fvec3(0, 0, 0)  # Black text
    box2.text = "Green Box"
    
    box3 = ncui.Box()
    box3.bgcolor = core.fvec3(0, 0, 1)  # Blue background
    box3.fgcolor = core.fvec3(1, 1, 0)  # Yellow text
    box3.text = "Blue Box"
    
    # Add text widget for information
    info = ncui.TextLines()
    info.setColor(core.fvec3(1, 1, 1))  # White
    info.addLine("Box demonstration")
    info.addLine("Different colored boxes")
    info.addLine("Press Ctrl+C to exit")
    
    layout.addChild(box1)
    layout.addChild(box2)
    layout.addChild(box3)
    layout.addChild(info)
    
    ui.setContent(layout)
    
    # Update box text periodically
    def update_boxes():
        for i in range(20):
            time.sleep(2)
            box1.text = f"Red Box {i+1}"
            box2.text = f"Green Box {i+1}"
            box3.text = f"Blue Box {i+1}"
            info.addLine(f"Update {i+1}")
    
    thread = threading.Thread(target=update_boxes)
    thread.daemon = True
    thread.start()
    
    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        pass

def main():
    """Main function to run examples"""
    if len(sys.argv) > 1:
        example = sys.argv[1]
        if example == "basic":
            basic_widgets_example()
        elif example == "layout":
            layout_example()
        elif example == "tabs":
            tabs_example()
        elif example == "split":
            split_layout_example()
        elif example == "boxes":
            boxes_example()
        else:
            print(f"Unknown example: {example}")
            print("Available examples: basic, layout, tabs, split, boxes")
    else:
        print("Usage: python example_ncui.py <example>")
        print("Examples:")
        print("  basic  - Basic widget usage")
        print("  layout - Layout management")
        print("  tabs   - Tab group example")
        print("  split  - Split layout example")
        print("  boxes  - Box widgets example")

if __name__ == "__main__":
    main() 