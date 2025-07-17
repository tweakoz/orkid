#!/usr/bin/env python3

from orkengine import core
import time, random, threading, math

# Initialize core
core.coreappinit()

# Setup logging
logger = core.Logger.instance()
logger.enableNotCurses()
chan_test = logger.configureChannel("COMBINED_VIZ_TEST", core.vec3(0.8, 0.2, 0.8), True)

# Create UI context
uictx = core.ncui.context()

# =================================================================
# Performance Visualizer Setup
# =================================================================

# Create generic performance visualizer widget
perfviz = core.ncui.PerformanceVisualizer()
perfviz.title = "System Performance Monitor"
perfviz.update_interval = 0.1  # Fast updates for demo
perfviz.sparkline_width = 25   # Compact sparklines

# Create performance data source
data_source = core.ncui.PerformanceDataSource()
data_source.name = "Demo System Metrics"

# Simulate some performance data with lambdas
simulation_data = {
    'cpu_usage': 45.0,
    'memory_usage': 60.0,
    'disk_io': 100,
    'network_speed': 50.0,
    'active_threads': 8,
    'cache_hits': 1000,
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

# =================================================================
# OPQ Visualizer Setup
# =================================================================

# Create OPQ visualizer widget with sparklines
opq_viz = core.ncui.OPQVisualizer()
opq_viz.update_interval = 0.125

# Use the existing concurrent queue
conq = core.opq_concurrentQueue()
opq_viz.setTargetOPQ(conq)

# Start performance tracking to get ops/sec and latency metrics
conq.startPerformanceTracking()

# =================================================================
# Layout Setup: Single HPack containing both visualizers
# =================================================================

# Create single horizontal pack for both visualizers
main_hpack = core.ncui.HorizontalPack()

# Add both visualizers to the same horizontal pack (side by side)
main_hpack.addChild(perfviz)  # Performance visualizer on left
main_hpack.addChild(opq_viz)  # OPQ visualizer on right
main_hpack.height = 12  # Set height to max of both
# Use the split/swap technique to show both logger tabs and visualizers
splitV = core.ncui.VerticalSplit()
splitV.split_position = 0.5  # 50% for logger, 50% for visualizers
logger_tabs = uictx.swapContent(splitV)
splitV.top = logger_tabs
splitV.bottom = main_hpack

# =================================================================
# Logging and Thread Setup
# =================================================================

# Log test info
chan_test.log("Combined Visualizers Test")
chan_test.log("Performance Visualizer (left) + OPQ Visualizer (right)")
chan_test.log("Logger tabs shown above, visualizers below side by side")
chan_test.log("Performance Visualizer: Lambda-based data providers")
chan_test.log("OPQ Visualizer: Real concurrent queue performance")
chan_test.log("Both visualizers show sparklines with trends over time")
chan_test.log("Update intervals: Perf=0.1s, OPQ=0.125s")
chan_test.log("Simulation: Random realistic performance data + real OPQ workload")

# Create simulation update thread for performance visualizer
def simulation_thread():
    """Background thread to update simulation data"""
    start_time = time.time()
    keep_going = True
    while keep_going:  # Run for 30 seconds
        update_simulation()
        time.sleep(0.1)  # Update at 10Hz
        curtime = time.time()
        elapsed = curtime - start_time
        chan_test.status("CUR-EPOCHTIME", str(curtime))
        chan_test.status("SIM-ELAPSED", str(elapsed))
        keep_going = elapsed<30.0

# Create OPQ workload thread
def opq_work_thread():
    """Background thread to generate OPQ workload"""
    cur_time = time.time()
    while time.time() - cur_time < 30:  # Run for 30 seconds
        core.opq_createTestWorkload(
            conq,
            random.randrange(500,1000),  # Number of OPQ items to create
            random.uniform(.00004,.00006),   # work duration per item in seconds
        )
        core.opq_createBurstWorkload(
            conq,
            random.randrange(500,5000),  # burst size
            random.randrange(1,5),   # number of bursts
            random.uniform(.00001,.00003),   # burst interval in seconds
        )
        time.sleep(random.uniform(.01, .02))  # Random delay between submissions

# Start both threads
sim_thread = threading.Thread(target=simulation_thread)
opq_thread = threading.Thread(target=opq_work_thread)

chan_test.log("Starting simulation threads...")
sim_thread.start()
opq_thread.start()

# Let the visualizers run
chan_test.log("Both visualizers running. Watch the sparklines!")
chan_test.log("Left: Performance metrics with lambda providers")
chan_test.log("Right: Real OPQ performance with actual workload")
chan_test.log("You will need to wait until the simulation completes (30 seconds-ish) before exit will be allowed")


# Wait for both threads to finish
sim_thread.join()
opq_thread.join()

chan_test.log("Simulation complete. Draining OPQ...")
conq.drain()
chan_test.log("Exit is now OK")
perfviz.data_source = None  # Clear data source with lambda captures
data_source = None          # Clear reference to data source
simulation_data = None      # Clear captured simulation data

uictx.waitForExit()

for name in list(locals().keys()):
    if not name.startswith('_'):
        del locals()[name]
