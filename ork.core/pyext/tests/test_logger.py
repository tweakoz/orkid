#!/usr/bin/env python3

from orkengine.core import Logger, vec3, ncui
import time

logger = Logger.instance()

chan_test = logger.configureChannel("TEST", vec3(1), True)
chan_test.log("This is a test message")
chan_test2 = logger.configureChannel("TEST2", vec3(1,0,0), True)
chan_test2.log("This is a test message")

time.sleep(1)


logger.enableNotCurses()
chan_test.status("ST1","OK")
chan_test.status("ST2","OK")
chan_test2.status("ST1","OK")
chan_test2.status("ST2","OK")
chan_test2.status("ST3","OK")

for i in range(80):
  chan_test.log("Test message %d" % i)
  chan_test2.log("Test2 message %d" % i)

uictx = ncui.context()
uictx.waitForExit()
