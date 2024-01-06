#!/usr/bin/env python3

################################################################################
# singularity test for timestamps
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################

from orkengine.core import *
from orkengine.lev2 import singularity

################################################################################

timebase = singularity.TimeBase()
timebase.numerator = 4
timebase.denominator = 4
timebase.tempo = 120
timebase.ppb = 100

################################################################################

timestamp_a = singularity.TimeStamp()

timestamp_a.measures = 0
timestamp_a.beats = 0
timestamp_a.clocks = 0

################################################################################

timestamp_b = singularity.TimeStamp()

timestamp_b.measures = 2
timestamp_b.beats = 0
timestamp_b.clocks = 0

################################################################################

timestamp_c = timestamp_b - timestamp_a
timestamp_d = timestamp_b + singularity.TimeStamp(1,3,700) + singularity.TimeStamp(1,3,707)
timestamp_e = timebase.reduce(timestamp_d)

timestamp_neg = timestamp_a-timestamp_b

################################################################################

time_a = timebase.time(timestamp_a)
time_b = timebase.time(timestamp_b)
time_c = timebase.time(timestamp_c)
time_d = timebase.time(timestamp_d)
time_e = timebase.time(timestamp_e)

time_neg = timebase.time(timestamp_neg)

################################################################################

print("timebase: %s" % timebase)
print("timestamp_a: %s    time_a: %s" % (timestamp_a,time_a))
print("timestamp_b: %s    time_b: %s" % (timestamp_b,time_b))
print("timestamp_c: %s    time_c: %s" % (timestamp_c,time_c))
print("timestamp_d: %s    time_d: %s" % (timestamp_d,time_d))
print("timestamp_e: %s    time_e: %s" % (timestamp_e,time_e))
print("timestamp_neg: %s    time_neg: %s" % (timestamp_neg,time_neg))

