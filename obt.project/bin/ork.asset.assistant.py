#!/usr/bin/env python3
# Copyright 2017 - Michael T. Mayers
# Licensed under the GPLV3 - see https://www.gnu.org/licenses/gpl-3.0.html

import os,sys, string, math

from PyQt5.QtCore import QSize, Qt, QProcess, QSettings
from PyQt5.QtWidgets import QApplication, QVBoxLayout, QHBoxLayout, QLabel, QWidget, QComboBox, QCheckBox
from PyQt5.QtWidgets import QLineEdit, QTextEdit, QPushButton, QFileDialog, QStyle, QStyleFactory
from PyQt5.QtWidgets import QMainWindow, QDockWidget, QPlainTextEdit
from PyQt5.QtGui import QPalette, QPixmap
from pathlib import Path
import orkassasshl as hilite

scriptdir = os.path.dirname(os.path.realpath(__file__))
#os.system('syslog -s -l error "%s"' % scriptdir)

settings = QSettings("TweakoZ", "OrkidTool");
settings.beginGroup("App");
settings.endGroup();

basedir = Path(os.getenv("ORKID_WORKSPACE_DIR"))
datadir = basedir/"ork.data"
srcdir = str(datadir/"src")
dstdir = str(datadir/"pc")
datadir = str(datadir)
basedir = str(basedir)

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

class AssetWidget(QWidget):

   ##########################################
    def __init__(self,mainwin):
     super(AssetWidget, self).__init__()
     self.mainwin = mainwin
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
     #icon = style.standardIcon(QStyle.SP_FileIcon)
     #sbutton.setIcon(icon)
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
     src = QFileDialog.getOpenFileNames(self,"QFileDialog.getOpenFileName()",srcdir,"Colladas(*.dae);;Images(*.png *.tga)", options=options )
     print(src)
     src = " ".join(src[0])
     src = src.replace(srcdir,"<SRC>")
     self.srced.onChangedExternally(src)
     dst = src.replace("<SRC>","<DST>")
     dstdir, dstfile = os.path.split(dst)
     dstldir, dstrdir = os.path.split(dstdir)
     print (dstdir, dstfile)
     print (dstldir, dstrdir)
     extension = os.path.splitext(src)[1]
     print(extension)
     if extension==".dae":
       if (src.find("/ref/")!=-1):
         dst = dst.replace("/ref/","/")
         self.assettypeview.onChangedExternally("Dae(Mesh)")
         dst = dst.replace(".dae",".xgm")
       elif (src.find("/anims/")!=-1):
         dst = dst.replace("/anims/","/")
         self.assettypeview.onChangedExternally("Dae(Anim)")
         dst = dst.replace(".dae",".xga")
     if extension==".png":
        dst = dst.replace("/ref/","/")
        dst = dst.replace(".png",".dds")
        self.assettypeview.onChangedExternally("Tex(PNG)")
     self.dsted.onChangedExternally(dst)

    ##########################################
    def doItem(self,srcfile,dstfile):
       srcpath = srcfile.replace("<SRC>","./ork.data/src")
       extension = os.path.splitext(srcpath)[1]
       dstpath = dstfile.replace("<DST>","./ork.data/pc")
       print(srcpath,dstpath)
       os.chdir(basedir)
       orkbin = "ork.tool.release"

       if extension == ".dae":
           if self.assettypeview.value == "Dae(Mesh)":
               orkfilt = "dae:xgm"
           elif self.assettypeview.value == "Dae(Anim)":
               orkfilt = "dae:xga"
       elif extension == ".tga":
           orkfilt = "nvcompress"
       elif extension == ".png":
           orkfilt = "nvcompress"

       if orkfilt!=None:
           if orkfilt == "nvcompress":
               dstpath = dstpath.replace(".tga",".dds")
               dstpath = dstpath.replace(".png",".dds")
               cmd = "nvcompress -bc3 %s %s " % (srcpath,dstpath)
               dp = Path(dstpath)
               print(dp,dp.parent)
               dpex = os.path.exists(dp.parent)
               if False==dpex:
                   dp.parent.mkdir(parents=True)
               dpex = os.path.exists(dp.parent)
               print(dpex)
               print(cmd)
           else:
               cmd = orkbin + (" -filter %s -in " % orkfilt) + srcpath + " -out " + dstpath


       class SubProc:
           def __init__(self,mainwin):

               te = QPlainTextEdit()
               hilighter = hilite.Highlighter(te.document())

               qd = QDockWidget("Dae:Xgm")
               qd.setWidget(te)
               qd.setMinimumSize(480,240)
               qd.setFeatures(QDockWidget.DockWidgetMovable|QDockWidget.DockWidgetVerticalTitleBar|QDockWidget.DockWidgetFloatable)
               qd.setAllowedAreas(Qt.LeftDockWidgetArea)
               qdss = "QWidget{background-color: rgb(64,64,128); color: rgb(160,160,192);}"
               qdss += "QDockWidget::title {background-color: rgb(32,32,48); color: rgb(255,0,0);}"
               qd.setStyleSheet(qdss)
               mainwin.addDockWidget(Qt.LeftDockWidgetArea,qd)
               if mainwin.prevDockWidget!=None:
                   mainwin.tabifyDockWidget(mainwin.prevDockWidget,qd)
               mainwin.prevDockWidget = qd
               self.stdout = ""
               self.stderr = ""
               def onSubProcStdout():
                   bytes = self.process.readAllStandardOutput()
                   self.stdout += str(bytes, encoding='ascii')
                   te.setPlainText(self.stdout+self.stderr)
               def onSubProcStderr():
                   bytes = self.process.readAllStandardError()
                   self.stderr += str(bytes, encoding='ascii')
                   te.setPlainText(self.stdout+self.stderr)
               def finished(text):
                   print( "process done...\n")

               self.process = QProcess()
               self.process.readyReadStandardError.connect(onSubProcStderr)
               self.process.readyReadStandardOutput.connect(onSubProcStdout);
               self.process.finished.connect(finished);


               self.process.start(cmd)
           def join(self):
               self.process.waitForFinished()

       sp = SubProc(self.mainwin)
       sp.join()
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

    ##########################################

    def goPushed(self):
      slist = self.srced.value.split(" ")
      dlist = self.dsted.value.split(" ")
      assert(len(slist)==len(dlist))
      for i in range(0,len(slist)):
        srcfile = slist[i]
        dstfile = dlist[i]
        print( srcfile, dstfile )
        self.doItem(srcfile,dstfile)
#############################################################################

class AssetWindow(QMainWindow):
  def __init__(self):
    super(AssetWindow, self).__init__()
    self.prevDockWidget = None
    aw = AssetWidget(self)
    qd = QDockWidget()
    qd.setWidget(aw)
    self.setCentralWidget(qd)
    self.setMinimumSize(480,360)
    self.setMaximumSize(1920,1080)
    self.setDockOptions(QMainWindow.AnimatedDocks | QMainWindow.ForceTabbedDocks);


#############################################################################

if __name__ == '__main__':
    app = QApplication(sys.argv)

    mainwin = AssetWindow()

    mainwin.resize(960,360)
    mainwin.setWindowTitle("Orkid Asset Assistant \N{COPYRIGHT SIGN} 2017 - TweakoZ")

    appss = "QWidget {background-color: rgb(64,64,96); color: rgb(255,255,255);}"
    appss += "QTabWidget::pane { border-top: 2px solid #303030;}"
    appss += """QTabBar::tab {
    background: rgb(0,0,0);
    color: rgb(255,255,255);
    }"""
    appss += """QTabBar::tab:selected, QTabBar::tab:hover {
    background: rgb(64,0,0);
    color: rgb(255,255,128);
    }"""

    app.setStyleSheet(appss)
    mainwin.show()
    sys.exit(app.exec_())
