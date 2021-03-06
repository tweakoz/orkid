#!/usr/bin/env python3
################################################################################
import sys, os
from PySide2 import QtWidgets
from PySide2.QtWidgets import QStyleFactory
################################################################################
def onButton():
  from orkengine import core,lev2,lev2qt
  print(dir(lev2qt))
  print(dir(lev2qt.Icecream))
  a = lev2qt.mytype("yo")
  ic = lev2qt.Icecream(a)
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
