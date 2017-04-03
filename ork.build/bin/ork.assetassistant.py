#!/usr/local/bin/python3
# Copyright 2017 - Michael T. Mayers
# Licensed under the GPLV3 - see https://www.gnu.org/licenses/gpl-3.0.html

import os,sys, string, math

from PyQt5.QtCore import QSize, Qt, QProcess
from PyQt5.QtWidgets import QApplication, QVBoxLayout, QHBoxLayout, QLabel, QWidget, QComboBox, QCheckBox
from PyQt5.QtWidgets import QLineEdit, QTextEdit, QPushButton, QFileDialog, QStyle, QStyleFactory
from PyQt5.QtWidgets import QMainWindow, QDockWidget
from PyQt5.QtGui import QPalette, QPixmap

scriptdir = os.path.dirname(os.path.realpath(__file__))
os.system('syslog -s -l error "%s"' % scriptdir)

basedir = os.path.abspath(scriptdir+"/../..")
datadir = basedir+"/data"
srcdir = datadir+"/src"
dstdir = datadir+"/pc"

#############################################################################
def bgcolor(r,g,b):
   return "background-color: rgb(%g,%g,%g); " % (r,g,b)
def fgcolor(r,g,b):
   return "color: rgb(%g,%g,%g); " % (r,g,b)

#############################################################################

class Edit:
   def __init__(self,label):
      height = 24
      self.key = label
      self.value = ""
      self.layout = QHBoxLayout()
      self.labl = QLabel(label)
      self.labl.setFixedHeight(height)
      self.labl.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
      self.edit = QLineEdit( )
      self.edit.setFixedHeight(height)
      self.edit.setText(self.value)
      self.edit.setStyleSheet(bgcolor(128,128,128)+fgcolor(255,255,128))
      self.edit.textChanged.connect(self.editChanged)
      self.layout.addWidget(self.labl)
      self.layout.addWidget(self.edit)

   def editChanged(self,text):
      self.value = text

   def onChangedExternally(self,text):
      self.value = text
      self.edit.setText(text)

   def addToLayout(self,l):
      l.addLayout(self.layout)

#############################################################################

class View(Edit):
   def __init__(self,key,val):
      Edit.__init__(self,key)
      self.edit.setStyleSheet("background-color: rgb(128,128,160); color: rgb(255,255,128); ")
      self.edit.setReadOnly(True)
      super(View,self).onChangedExternally(val)


#############################################################################

class AssetWindow(QWidget):

   ##########################################
    def __init__(self):
     super(AssetWindow, self).__init__()
     mainLayout = QVBoxLayout()

     viewBase = View("<BASE>",basedir)
     viewData = View("<DATA>",datadir)
     viewSrc = View("<SRC>",srcdir)
     viewDst = View("<DST>",dstdir)
     viewBase.addToLayout(mainLayout)
     viewData.addToLayout(mainLayout)
     viewSrc.addToLayout(mainLayout)
     viewDst.addToLayout(mainLayout)

     sbutton = QPushButton()
     style = QStyleFactory.create("Macintosh")
     icon = style.standardIcon(QStyle.SP_FileIcon)
     sbutton.setIcon(icon)
     #sbutton.setStyleSheet("background-color: rgb(0, 0, 64); border-radius: 2; ")
     sbutton.pressed.connect(self.selectInput)

     self.srced = Edit("Source File")
     slay2 = QHBoxLayout()
     slay2.addLayout(self.srced.layout)
     self.srced.edit.setStyleSheet(bgcolor(128,32,128)+fgcolor(0,255,0))
     slay2.addWidget(sbutton)
     mainLayout.addLayout(slay2)

     ad_lo = QHBoxLayout()
     self.assettypeview = View("AssetType","???")
     self.assettypeview.addToLayout(ad_lo)
     self.assettypeview.layout.setStretchFactor(self.assettypeview.labl,1)
     self.assettypeview.layout.setStretchFactor(self.assettypeview.edit,3)

     self.dsted = View("Dest File","");
     self.dsted.addToLayout(ad_lo)
     self.dsted.layout.setStretchFactor(self.dsted.edit,8)
     self.dsted.layout.setStretchFactor(self.dsted.labl,1)
     ad_lo.setStretchFactor(self.assettypeview.layout,1)
     ad_lo.setStretchFactor(self.dsted.layout,3)

     mainLayout.addLayout(ad_lo)

     gobutton = QPushButton("Go")
     gobutton.setStyleSheet(bgcolor(32,32,128)+fgcolor(255,255,128))
     mainLayout.addWidget(gobutton)
     gobutton.pressed.connect(self.goPushed)
     self.setLayout(mainLayout)

   ##########################################

    def selectInput(self):
     options = QFileDialog.Options()
     options |= QFileDialog.DontUseNativeDialog
     src = QFileDialog.getOpenFileName(self,"QFileDialog.getOpenFileName()",srcdir,"Collada Files (*.dae)", options=options )
     src = src[0]
     src = src.replace(srcdir,"<SRC>")
     self.srced.onChangedExternally(src)
     dst = src.replace("<SRC>","<DST>")
     dstdir, dstfile = os.path.split(dst)
     dstldir, dstrdir = os.path.split(dstdir)
     print (dstdir, dstfile)
     print (dstldir, dstrdir)
     if dstrdir=="ref":
        dst = dst.replace("/ref/","/")
        self.assettypeview.onChangedExternally("Dae(Mesh)")
     dst = dst.replace(".dae",".xgm")
     self.dsted.onChangedExternally(dst)

   ##########################################

    def goPushed(self):
      srcfile = self.srced.value
      srcpath = srcfile.replace("<SRC>","./data/src")
      dstfile = self.dsted.value
      dstpath = dstfile.replace("<DST>","./data/pc")
      print(srcpath,dstpath)
      os.chdir(basedir)
      orkbin = "./stage/bundle/OrkidTool.app/Contents/MacOS/ork.tool.test.osx.release"
      cmd = orkbin + " -filter dae:xgm -in " + srcpath + " -out " + dstpath
      
      self.stdout = ""
      self.stderr = ""
      def onSubProcStdout():
         bytes = self.process.readAllStandardOutput()
         self.stdout += str(bytes)
         print(self.stdout)
      def onSubProcStderr():
         bytes = self.process.readAllStandardError()
         self.stderr += str(bytes)
         print(self.stderr)
      def finished(text):
         print( "process done...\n")

      self.process = QProcess()
      self.process.readyReadStandardError.connect(onSubProcStderr)
      self.process.readyReadStandardOutput.connect(onSubProcStdout);
      self.process.finished.connect(finished);
      

      self.process.start(cmd)
      #...
      #void MainWindow::updateError()
      #{
      #QByteArray data = myProcess->readAllStandardError();
      #textEdit_verboseOutput->append(QString(data));
      #}

      #void MainWindow::updateText()
      #{
      #QByteArray data = myProcess->readAllStandardOutput();
      #textEdit_verboseOutput->append(QString(data));
      #}

      #os.system(cmd)

#############################################################################

if __name__ == '__main__':
    app = QApplication(sys.argv)
    aw = AssetWindow()
    if False:
      mainwin = QMainWindow()
      qd = QDockWidget()
      qd.setWidget()
      mainwin.setCentralWidget(qd)
    else:
      mainwin = aw
    mainwin.resize(640,180)
    mainwin.setWindowTitle("Orkid Asset Assistant \N{COPYRIGHT SIGN} 2017 - TweakoZ")
    #bgimg = 'background-image: url("file://%s/platform_lev2/editor/OrkAssistantBackdrop.png")' % datadir
    #print(bgimg)
    app.setStyleSheet("QWidget {background-color: rgb(64,64,96); color: rgb(255,255,255);}")
    mainwin.show()
    sys.exit(app.exec_())