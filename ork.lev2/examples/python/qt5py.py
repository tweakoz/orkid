#!/usr/bin/env python3
import sys, os
from orkengine import core,lev2
from PyQt5.QtWidgets import QApplication, QWidget, QPushButton, QVBoxLayout
from PyQt5.QtWidgets import QStyleFactory
################################################################################
def onButton():
  a = core.vec3(0,1,0)
  print(a)
################################################################################
print(QStyleFactory.keys())
app = QApplication([])
window = QWidget()
layout = QVBoxLayout()
button = QPushButton('PushMe')
button.clicked.connect(onButton)
layout.addWidget(button)
window.setLayout(layout)
window.show()
app.exec_()
