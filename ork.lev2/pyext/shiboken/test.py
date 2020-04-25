#!/usr/bin/env python3
################################################################################
import sys, os
from orkengine import core,lev2,lev2qt
from PySide2 import QtWidgets
from PySide2.QtWidgets import QStyleFactory
################################################################################
def onButton():
  print(dir(lev2qt))
  ic = lev2qt.Icecream("chocko")
  print(dir(ic))
  print(ic)
  print(ic.getFlavor)
  print(ic.getFlavor())
################################################################################
print(QStyleFactory.keys())
sys.argv.extend(["--platformtheme", "qt5ct"])
app = QtWidgets.QApplication(sys.argv)
button = QtWidgets.QPushButton("PushMe")
button.resize(256, 64)
button.clicked.connect(onButton)
button.show()
################################################################################
sys.exit(app.exec_())
