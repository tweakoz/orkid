#!/usr/bin/env python3

from orkengine import core
import time, random, threading

# Initialize core
core.coreappinit()

# Setup logging
logger = core.Logger.instance()
logger.enableNotCurses()
chan_test = logger.configureChannel("OPQ_VIZ_TEST", core.vec3(0.2, 0.8, 1.0), True)

# Create UI context
uictx = core.ncui.context()

# Create OPQ visualizer widget with sparklines
opq_viz = core.ncui.OPQVisualizer()
opq_viz.update_interval = 0.125

# Configure performance data update interval (default 0.5s, set to 0.25s for faster updates)

test_vpack = core.ncui.VerticalPack()
test_hpack = core.ncui.HorizontalPack()
test_hpack.height = opq_viz.height  # Set height for horizontal pack
test_hpack.addChild(opq_viz)
test_vpack.addChild(test_hpack)

# Use the existing concurrent queue
conq = core.opq_concurrentQueue()
opq_viz.setTargetOPQ(conq)

# Start performance tracking to get ops/sec and latency metrics
conq.startPerformanceTracking()

# Use the split/swap technique to show both logger tabs and visualizer
splitV = core.ncui.VerticalSplit()
splitV.split_position = 0.4  # 60% for logger, 40% for visualizer
logger_tabs = uictx.swapContent(splitV)
splitV.top = logger_tabs
splitV.bottom = test_vpack

# Log test info
chan_test.log("OPQ Visualizer Test")
chan_test.log("Basic widget creation test")
chan_test.log("Logger tabs shown above, visualizer below")
chan_test.log("Using existing concurrent queue as target")
chan_test.log("Performance tracking: ON (update interval: 0.25s)")
chan_test.log("Metrics displayed: ops/sec, REAL latency, thread count, queue status")
chan_test.log("Latency measurement: Using _enqueueTime -> execution time")
chan_test.log("NEW: 10-second historical sparklines for each metric!")
chan_test.log("Sparklines: ▁▂▃▄▅▆▇█ characters show trends over time")

cur_time = time.time()
def submit_work():
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

thr = threading.Thread(target=submit_work)
thr.start()
thr.join()  # Wait for the thread to finish


chan_test.log("Work submitted. draining OPQ....")
conq.drain()
# Let the visualizer run for longer to see performance metrics
chan_test.log("Ok to shutdown")
# release all locals (namely the lambdas)
uictx.waitForExit()  # Wait for user to exit
for name in list(locals().keys()):
    if not name.startswith('_'):
        del locals()[name]
