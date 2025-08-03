#!/usr/bin/env python3

from orkengine import core
import time, random, threading, math
import gc

# Initialize core
core.coreappinit()

# Setup logging
logger = core.Logger.instance()
logger.enableNotCurses()
chan_test = logger.configureChannel("PERF_VIZ_TEST", core.vec3(0.8, 0.2, 1.0), True)

# Create UI context
uictx = core.ncui.context()

# Create generic performance visualizer widget
perfviz = core.ncui.PerformanceVisualizer()
perfviz.title = "System Performance Monitor"
perfviz.update_interval = 0.1  # Fast updates for demo
perfviz.sparkline_width = 30   # Compact 15-character sparklines

# Create performance data source
data_source = core.ncui.PerformanceDataSource()
data_source.name = "Demo System Metrics"

# Simulate some performance data with lambdas
simulation_data = {
    'cpu_usage': 0.0,
    'memory_usage': 0.0,
    'disk_io': 0,
    'network_speed': 0.0,
    'active_threads': 0,
    'cache_hits': 0,
    'error_count': 0,
    'uptime': 0.0
}

def update_simulation():
    """Update simulation data to create realistic-looking metrics"""
    global simulation_data
    simulation_data['cpu_usage'] = max(0.0, min(100.0, 
        simulation_data['cpu_usage'] + random.uniform(-5.0, 5.0)))
    simulation_data['memory_usage'] = max(0.0, min(100.0, 
        simulation_data['memory_usage'] + random.uniform(-2.0, 2.0)))
    simulation_data['disk_io'] = max(0, 
        simulation_data['disk_io'] + random.randint(-50, 100))
    simulation_data['network_speed'] = max(0.0, 
        simulation_data['network_speed'] + random.uniform(-10.0, 20.0))
    simulation_data['active_threads'] = max(1, 
        simulation_data['active_threads'] + random.randint(-2, 3))
    simulation_data['cache_hits'] += random.randint(0, 50)
    if random.random() < 0.05:  # 5% chance of error
        simulation_data['error_count'] += 1
    simulation_data['uptime'] += 0.1

# Initialize simulation data
simulation_data['cpu_usage'] = 45.0
simulation_data['memory_usage'] = 60.0  
simulation_data['disk_io'] = 100
simulation_data['network_speed'] = 50.0
simulation_data['active_threads'] = 8
simulation_data['cache_hits'] = 1000
simulation_data['error_count'] = 0
simulation_data['uptime'] = 0.0

# Add data items using lambda providers
data_source.addDoubleItem("cpu", "CPU Usage", "%", 
    lambda: simulation_data['cpu_usage'])

data_source.addDoubleItem("memory", "Memory Usage", "%", 
    lambda: simulation_data['memory_usage'])

data_source.addIntItem("disk_io", "Disk I/O", "MB/s", 
    lambda: simulation_data['disk_io'])

data_source.addDoubleItem("network", "Network Speed", "Mbps", 
    lambda: simulation_data['network_speed'])

data_source.addIntItem("threads", "Active Threads", "", 
    lambda: simulation_data['active_threads'])

data_source.addIntItem("cache_hits", "Cache Hits", "", 
    lambda: simulation_data['cache_hits'], False)  # No sparkline for cumulative data

data_source.addIntItem("errors", "Error Count", "", 
    lambda: simulation_data['error_count'], False)  # No sparkline for cumulative data

data_source.addStringItem("uptime", "Uptime", 
    lambda: f"{simulation_data['uptime']:.1f}s")

# Advanced math demonstration - sine wave pattern
data_source.addDoubleItem("sine_wave", "Sine Wave", "units", 
    lambda: 50.0 + 30.0 * math.sin(time.time()))

# Connect data source to visualizer
perfviz.data_source = data_source

# Set up layout with logger tabs and visualizer
test_vpack = core.ncui.VerticalPack()
test_hpack = core.ncui.HorizontalPack()
test_hpack.addChild(perfviz)
test_vpack.addChild(test_hpack)

# Use the split/swap technique to show both logger tabs and visualizer
splitV = core.ncui.VerticalSplit()
splitV.split_position = 0.6  # 60% for logger, 40% for visualizer
logger_tabs = uictx.swapContent(splitV)
splitV.top = logger_tabs
splitV.bottom = test_vpack

test_hpack.height = 12

# Log test info
chan_test.log("Generic Performance Visualizer Test")
chan_test.log("Lambda-based data provider demonstration")
chan_test.log("Logger tabs shown above, visualizer below")
chan_test.log("Data items: CPU, Memory, Disk I/O, Network, Threads, Cache, Errors, Uptime")
chan_test.log("Features: Auto-sizing, sparklines, exception safety")
chan_test.log("Update interval: 0.1s for fast demo")
chan_test.log("Sparklines: ▁▂▃▄▅▆▇█ characters show trends over time")
chan_test.log("Simulation: Random realistic-looking performance data")

# Create simulation update thread
def simulation_thread():
    """Background thread to update simulation data"""
    start_time = time.time()
    while time.time() - start_time < 10:  # Run for 30 seconds
        update_simulation()
        time.sleep(0.1)  # Update at 10Hz

thr = threading.Thread(target=simulation_thread)
thr.start()

# Let the visualizer run
chan_test.log("Simulation running. Watch the sparklines!")
thr.join()  # Wait for simulation to finish

chan_test.log("Simulation complete. Final data collection for 5 seconds...")

chan_test.log("Generic Performance Visualizer test complete!")

# Test theory: Explicit cleanup to break lambda reference cycles
chan_test.log("Breaking reference cycles...")
perfviz.data_source = None  # Clear data source with lambda captures
data_source = None          # Clear reference to data source
simulation_data = None      # Clear captured simulation data
