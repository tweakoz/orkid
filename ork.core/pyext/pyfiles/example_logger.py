#!/usr/bin/env python3

"""
Example usage of the Orkid Core Logger Python bindings.

This script demonstrates:
1. Basic logger usage
2. Channel configuration
3. NotCurses UI integration
4. Proper cleanup handling
"""

import sys
import time
import ork.core as core
import ork.core.pyfiles.ncui_cleanup as ncui_cleanup

def basic_logger_example():
    """Basic logger usage without NotCurses"""
    print("=== Basic Logger Example ===")
    
    # Get the logger singleton
    logger = core.Logger.instance()
    
    # Configure some channels
    debug_chan = logger.configureChannel("DEBUG", core.fvec3(0, 1, 0), True)  # Green
    info_chan = logger.configureChannel("INFO", core.fvec3(0, 0, 1), True)   # Blue
    warn_chan = logger.configureChannel("WARN", core.fvec3(1, 1, 0), True)   # Yellow
    error_chan = logger.configureChannel("ERROR", core.fvec3(1, 0, 0), True) # Red
    
    # Log some messages
    debug_chan.log("This is a debug message")
    info_chan.log("This is an info message")
    warn_chan.log("This is a warning message")
    error_chan.log("This is an error message")
    
    # Use status reporting
    info_chan.status("progress", "Starting task...")
    time.sleep(1)
    info_chan.status("progress", "50% complete")
    time.sleep(1)
    info_chan.status("progress", "Task completed!")
    
    # Test channel properties
    print(f"Debug channel enabled: {debug_chan.enabled}")
    print(f"Debug channel name: {debug_chan.name}")
    print(f"Debug channel color: {debug_chan.color}")
    
    # Disable a channel
    debug_chan.enabled = False
    debug_chan.log("This won't be shown")
    
    # Re-enable
    debug_chan.enabled = True
    debug_chan.log("This will be shown again")

def notcurses_logger_example():
    """Logger with NotCurses UI"""
    print("=== NotCurses Logger Example ===")
    
    # Set up cleanup (important for NotCurses)
    ncui_cleanup.setup()
    
    # Get the logger and enable NotCurses
    logger = core.Logger.instance()
    logger.enableNotCurses()
    
    # Configure channels
    app_chan = logger.configureChannel("APP", core.fvec3(0, 1, 1), True)    # Cyan
    net_chan = logger.configureChannel("NETWORK", core.fvec3(1, 0, 1), True) # Magenta
    db_chan = logger.configureChannel("DATABASE", core.fvec3(0.5, 0.5, 1), True) # Light blue
    
    # Simulate application activity
    app_chan.log("Application starting...")
    app_chan.status("state", "Initializing")
    
    net_chan.log("Connecting to server...")
    net_chan.status("connection", "Connecting")
    
    db_chan.log("Opening database connection...")
    db_chan.status("db_state", "Connecting")
    
    # Simulate some work
    for i in range(10):
        app_chan.log(f"Processing item {i+1}/10")
        app_chan.status("progress", f"{((i+1)/10)*100:.0f}%")
        
        if i == 3:
            net_chan.status("connection", "Connected")
            net_chan.log("Server connection established")
        
        if i == 5:
            db_chan.status("db_state", "Connected")
            db_chan.log("Database connection established")
        
        time.sleep(0.5)
    
    app_chan.status("state", "Running")
    app_chan.log("Application ready!")
    
    # Keep running until interrupted
    try:
        while True:
            app_chan.log("Application tick")
            time.sleep(2)
    except KeyboardInterrupt:
        app_chan.log("Shutting down...")
        app_chan.status("state", "Shutting down")

def channel_management_example():
    """Example of dynamic channel management"""
    print("=== Channel Management Example ===")
    
    logger = core.Logger.instance()
    
    # Create channels dynamically
    channels = {}
    colors = [
        core.fvec3(1, 0, 0),    # Red
        core.fvec3(0, 1, 0),    # Green
        core.fvec3(0, 0, 1),    # Blue
        core.fvec3(1, 1, 0),    # Yellow
        core.fvec3(1, 0, 1),    # Magenta
        core.fvec3(0, 1, 1),    # Cyan
    ]
    
    for i in range(6):
        name = f"WORKER_{i+1}"
        channel = logger.configureChannel(name, colors[i], True)
        channels[name] = channel
        channel.log(f"Worker {i+1} initialized")
    
    # Simulate worker activity
    for tick in range(20):
        for name, channel in channels.items():
            if tick % 3 == 0:
                channel.log(f"Processing task {tick//3 + 1}")
            channel.status("status", f"Tick {tick}")
        time.sleep(0.1)
    
    # Disable some channels
    for i, (name, channel) in enumerate(channels.items()):
        if i % 2 == 0:
            channel.enabled = False
            print(f"Disabled channel: {name}")
    
    # Continue with fewer channels
    for tick in range(5):
        for name, channel in channels.items():
            if channel.enabled:
                channel.log(f"Final task {tick}")
        time.sleep(0.2)

def main():
    """Main function to run examples"""
    if len(sys.argv) > 1:
        example = sys.argv[1]
        if example == "basic":
            basic_logger_example()
        elif example == "notcurses":
            notcurses_logger_example()
        elif example == "channels":
            channel_management_example()
        else:
            print(f"Unknown example: {example}")
            print("Available examples: basic, notcurses, channels")
    else:
        print("Usage: python example_logger.py <example>")
        print("Examples:")
        print("  basic     - Basic logger usage")
        print("  notcurses - Logger with NotCurses UI")
        print("  channels  - Channel management")

if __name__ == "__main__":
    main() 