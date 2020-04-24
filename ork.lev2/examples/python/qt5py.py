#!/usr/bin/env python3
#env["QT_QPA_PLATFORMTHEME"]="qt5ct"
import sys, os
from PySide2 import QtWidgets
from PySide2.QtWidgets import QStyleFactory
print(QStyleFactory.keys())
sys.argv.extend(["--platformtheme", "qt5ct"])
app = QtWidgets.QApplication(sys.argv)
hello = QtWidgets.QPushButton("Hello world!")
hello.resize(256, 64)
hello.show()
sys.exit(app.exec_())
