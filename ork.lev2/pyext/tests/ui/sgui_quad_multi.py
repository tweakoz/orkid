#!/usr/bin/env ork.python

################################################################################
# lev2 sample which renders a UI with four views to the same scenegraph to a window
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################

from orkengine import core
from orkengine.lev2 import OrkEzApp

from ork.app.application import ComponentizedApplication
from ork.app.testlib.multiscene1 import MultiScene1Component

################################################################################

class UiSgQuadViewTestApp(ComponentizedApplication):

  def __init__(self):
    super().__init__()

    self.ezapp = OrkEzApp.create(self)
    self.multiscene = self.addComponent("multiscene1", MultiScene1Component )
    
###############################################################################

UiSgQuadViewTestApp().ezapp.mainThreadLoop(on_iter=lambda: False)
