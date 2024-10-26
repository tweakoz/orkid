#!/usr/bin/env python3

################################################################################
# lev2 sample which renders a UI to a window
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################

from orkengine.core import *
from orkengine.lev2 import *

################################################################################

class UiTestApp(object):

  def __init__(self):
    super().__init__()
    self.ezapp = OrkEzApp.create(self)
    self.ezapp.setRefreshPolicy(RefreshFastest, 0)
    self.ezapp.topWidget.enableUiDraw()
    lg_group = self.ezapp.topLayoutGroup
    lg_group.margin = 8
    griditems = lg_group.makeGrid( width = 2,
                                   height = 2,
                                   margin = 1,
                                   uiclass = ui.LambdaBox,
                                   args = ["box",vec4(1,0,1,1)] )

    print(griditems)

    griditems[0].widget.onPressed(lambda: print("GRIDITEM0 PUSHED"))
    griditems[1].widget.onPressed(lambda: print("GRIDITEM1 PUSHED"))
    griditems[2].widget.onPressed(lambda: print("GRIDITEM2 PUSHED"))
    griditems[3].widget.onPressed(lambda: print("GRIDITEM3 PUSHED"))
    
    print(self.ezapp.mainwin)
    print(self.ezapp.mainwin.appwin)
    print(self.ezapp.topWidget)
    print(self.ezapp.topWidget.name)
    print(lg_group.name)
    print(lg_group)
    print(lg_group.layout)
    print(lg_group.layout.top)
    print(lg_group.layout.bottom)
    print(lg_group.layout.left)
    print(lg_group.layout.right)
    print(lg_group.layout.centerH)
    print(lg_group.layout.centerV)
    print(self.ezapp.uicontext)

###############################################################################

UiTestApp().ezapp.mainThreadLoop()
