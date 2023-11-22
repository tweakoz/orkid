#!/opt/homebrew/bin/python3
# Copyright 2017 - Michael T. Mayers
# Licensed under the GPLV3 - see https://www.gnu.org/licenses/gpl-3.0.html

import os,sys, string, math
from obt import path, pathtools, command

from PyQt5.QtCore import QSize, Qt, QProcess, QSettings
from PyQt5.QtWidgets import QApplication, QVBoxLayout, QHBoxLayout, QLabel, QWidget, QComboBox, QCheckBox
from PyQt5.QtWidgets import QLineEdit, QTextEdit, QPushButton, QFileDialog, QStyle, QStyleFactory
from PyQt5.QtWidgets import QMainWindow, QDockWidget, QPlainTextEdit
from PyQt5.QtGui import QPalette, QPixmap
from pathlib import Path
import _pyqthilighter as hilite

scriptdir = os.path.dirname(os.path.realpath(__file__))

settings = QSettings("TweakoZ", "OrkidTool");
settings.beginGroup("App");
settings.endGroup();

this_dir = path.directoryOfInvokingModule()
basedir = Path(os.getenv("ORKID_WORKSPACE_DIR"))
datadir = basedir/"ork.data"
srcdir = str(datadir/"src")
dstdir = str(datadir/"pc")
datadir = str(datadir)
basedir = str(basedir)

# check src_dir in settings, apply if valid
if settings.contains("src_dir"):
   try_srcdir = settings.value("src_dir")
   print(try_srcdir)
   srcdir = try_srcdir
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

class AssetWidget(QWidget):

   ##########################################
    def __init__(self,mainwin):
     super(AssetWidget, self).__init__()
     self.mainwin = mainwin
     mainLayout = QVBoxLayout()

     sbutton = QPushButton()
     style = QStyleFactory.create("Macintosh")
     icon = style.standardIcon(QStyle.SP_FileIcon)
     sbutton.setIcon(icon)
     sbutton.setStyleSheet("background-color: rgb(0, 0, 64); border-radius: 2; ")
     sbutton.pressed.connect(self.selectInput)
     self.srced = Edit("Source Directory")
     self.srced.onChangedExternally(str(srcdir))
     slay2 = QHBoxLayout()
     slay2.addLayout(self.srced.layout)
     self.srced.edit.setStyleSheet(bgcolor(64,32,64)+fgcolor(255,255,0))
     slay2.addWidget(sbutton)
     mainLayout.addLayout(slay2)

     gobutton = QPushButton("Go")
     gobutton.setStyleSheet(bgcolor(32,32,128)+fgcolor(255,255,128))
     mainLayout.addWidget(gobutton)
     gobutton.pressed.connect(self.goPushed)
     self.setLayout(mainLayout)
     self.gobutton = gobutton

     self.output_texts = dict()

   ##########################################

    def selectInput(self):
     
     src = QFileDialog.getExistingDirectory(self,
        "Select Asset Source Directory",
        srcdir,
        QFileDialog.ShowDirsOnly)

     self.srced.onChangedExternally(src)
     # preserve folder selection for other
     # invokations of this program (via QSettings)
     settings.setValue("src_dir",src)

   ##########################################

    def goPushed(self):
      srcpath = self.srced.value
      os.chdir(srcpath)
      te = QPlainTextEdit()
      hilighter = hilite.Highlighter(te.document())

      self.gobutton.setStyleSheet(bgcolor(128,32,0)+fgcolor(255,255,128))

      #######################################

      qd = QDockWidget("Export")
      qd.setWidget(te)
      qd.setMinimumSize(480,240)
      qd.setFeatures(QDockWidget.DockWidgetMovable|QDockWidget.DockWidgetVerticalTitleBar|QDockWidget.DockWidgetFloatable)
      qd.setAllowedAreas(Qt.RightDockWidgetArea)
      qdss = "QWidget{background-color: rgb(64,64,128); color: rgb(160,160,192);}"
      qdss += "QDockWidget::title {background-color: rgb(32,32,48); color: rgb(255,0,0);}"
      qd.setStyleSheet(qdss)
      mainwin.addDockWidget(Qt.RightDockWidgetArea,qd)

      #######################################

      if mainwin.prevDockWidget!=None:
         mainwin.tabifyDockWidget(mainwin.prevDockWidget,qd)
      mainwin.prevDockWidget = qd

      #######################################
      # remove all other "Dae:Xgm" widgets
      #######################################

      for qdw in mainwin.findChildren(QDockWidget):
         if qdw.windowTitle() == "Export" and qdw!=qd:
            mainwin.removeDockWidget(qdw)

      #######################################

      self.output_texts = dict()

      class SubProc:
         def __init__(self,asswidget, index, cmdlist):
            #############################
            self.asswidget = asswidget
            self.stdout = ""
            self.stderr = ""
            def onSubProcStdout():
               bytes = self.process.readAllStandardOutput()
               self.stdout += str(bytes, encoding='ascii')
            def onSubProcStderr():
               bytes = self.process.readAllStandardError()
               self.stderr += str(bytes, encoding='ascii')
            def finished(text):
               # serialize output to text edit
               # (serially so other processes are not interleaved in the text edit)
               asswidget.output_texts[index] = self.stdout+self.stderr

               merge_output_text = ""
               for key in sorted(asswidget.output_texts.keys()):
                  merge_output_text += asswidget.output_texts[key]

               te.setPlainText(merge_output_text)
               self.asswidget.update()
               asswidget.running_count -= 1
               if asswidget.running_count == 0:
                  self.asswidget.gobutton.setStyleSheet(bgcolor(32,32,128)+fgcolor(255,255,128))

               #print( "process done...\n")
            #############################
            self.process = QProcess()
            self.process.readyReadStandardError.connect(onSubProcStderr)
            self.process.readyReadStandardOutput.connect(onSubProcStdout);
            self.process.finished.connect(finished);
            #############################
            convlist = [str(x) for x in cmdlist]
            cmdstr = " ".join(convlist)
            self.process.start(cmdstr)
            asswidget.running_count += 1
            # when process finished, async update QT GUI


      #############################
      # enumerate all 
      #############################

      a = pathtools.recursive_patglob(srcpath,"*.blend")
      print(a)
      orkbin = "ork.blender.export.character.py"

      #############################
      index = 0
      self.running_count = 0
      for item in a:
         cmdlist = [orkbin, "-i", item, "-o", path.temp()/"test.glb"]
         sp = SubProc(self,index,cmdlist)
         index += 1

      #############################

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
    # set font size bigger
    font = app.font()
    font.setPointSize(18)
    app.setFont(font)

    app.setStyleSheet(appss)
    mainwin.show()
    sys.exit(app.exec_())