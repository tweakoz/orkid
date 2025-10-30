#!/usr/bin/env ork.python

################################################################################
# lev2 sample which opens a folder dialog asynchronously with callback
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################

import sys, math, random, numpy, obt.path, time
from orkengine import core
from orkengine.lev2 import *

##################################################
# Initialize ezapp 
#  required for opq queues, event processing, ui
##################################################

ezapp = lev2appinit()

##################################################

result_received = False
selected_folder = None

def on_folder_selected(path):
  global result_received, selected_folder
  print("CALLBACK: selected_path<%s>" % path)
  selected_folder = path
  result_received = True

################################################
# Start async folder dialog - does not block
################################################

ui.popupFolderDialogAsync("Select Folder", str(obt.path.stage()), on_folder_selected)


##################################################
# we need the main thread loop to process events 
#   for the dialog bindings to function
##################################################

counter = 0
ezapp.mainThreadBegin() 
while not result_received:
    ezapp.mainThreadIter()
    time.sleep(1.0)
    counter += 1
    print(f"waiting for folder dialog counter: {counter}")
ezapp.mainThreadEnd()

if result_received:
    print("SUCCESS: Received folder selection via callback")
    print("Final result: %s" % selected_folder)

shutdownApp()
