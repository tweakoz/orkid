#!/opt/homebrew/bin/python3
# Copyright 2017 - Michael T. Mayers
# Licensed under the GPLV3 - see https://www.gnu.org/licenses/gpl-3.0.html

import os,sys, string, math, re, fnmatch
from obt import path, pathtools, command

from PyQt5.QtCore import QSize, Qt, QProcess, QSettings
from PyQt5.QtWidgets import QApplication, QVBoxLayout, QHBoxLayout, QLabel, QWidget, QComboBox, QCheckBox
from PyQt5.QtWidgets import QLineEdit, QTextEdit, QPushButton, QFileDialog, QStyle, QStyleFactory
from PyQt5.QtWidgets import QMainWindow, QDockWidget, QPlainTextEdit, QSpacerItem, QSizePolicy
from PyQt5.QtGui import QPalette, QPixmap
from pathlib import Path
import _pyqthilighter as hilite

settings = QSettings("TweakoZ", "OrkidTool");
settings.beginGroup("App");
settings.endGroup();

this_dir = path.directoryOfInvokingModule()

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
   def __init__(self,label,default=""):
      height = 24
      self.key = label
      self.value = default
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

class OutputData:
   def __init__(self, index, text, input_path):
      self.input_path = input_path
      self.index = index
      self.text = text

class AssetWidget(QWidget):

   ##########################################

    def __init__(self,mainwin):
     super(AssetWidget, self).__init__()
     self.mainwin = mainwin
     self.output_texts = dict()
     top_layout = QHBoxLayout()
     v1_layout = QVBoxLayout()
     v2_layout = QVBoxLayout()

     ##########################################
     # source directory 
     ##########################################

     sbutton = QPushButton()
     style = QStyleFactory.create("Macintosh")
     icon = style.standardIcon(QStyle.SP_FileIcon)
     sbutton.setIcon(icon)
     sbutton.setStyleSheet("background-color: rgb(0, 0, 64); border-radius: 2; ")
     sbutton.pressed.connect(self.selectSourceDirectory)
     self.srced = Edit("Source Directory")
     self.srced.onChangedExternally(str(srcdir))
     slay2 = QHBoxLayout()
     slay2.addLayout(self.srced.layout)
     self.srced.edit.setStyleSheet(bgcolor(64,32,64)+fgcolor(255,255,0))
     slay2.addWidget(sbutton)
     v1_layout.addLayout(slay2)

     pattern = "*"
     if settings.contains("pattern"):
       pattern = settings.value("pattern")

     self.filter_ed = Edit("filter",pattern)
     v1_layout.addLayout(self.filter_ed.layout)
     self.filter_ed.edit.textChanged.connect(self.updateAssetList)

     ########################################
     # asset list
     ########################################

     self.assetlist = QPlainTextEdit()
     qdss = "QWidget{background-color: rgb(64,64,128); color: rgb(160,160,192);}"
     qdss += "QDockWidget::title {background-color: rgb(32,32,48); color: rgb(255,0,0);}"
     self.assetlist.setStyleSheet(qdss)
     v1_layout.addWidget(self.assetlist)

     self.updateAssetList()

     ########################################
     # go button 
     ########################################

     gobutton = QPushButton("Go")
     gobutton.setStyleSheet(bgcolor(32,32,128)+fgcolor(255,255,128))
     v1_layout.addWidget(gobutton)
     gobutton.pressed.connect(self.goPushed)
     self.gobutton = gobutton

     ##########################
     # output console
     ##########################

     self.output_console = QPlainTextEdit()
     self.output_hilighter = hilite.Highlighter(self.output_console.document())
     qdss = "QWidget{background-color: rgb(32,32,64); color: rgb(255,255,255);}"
     self.output_console.setStyleSheet(qdss)
     v2_layout.addWidget(self.output_console)
     ##########################
     top_layout.addLayout(v1_layout)
     top_layout.addLayout(v2_layout)
     self.setLayout(top_layout)

   ##########################################

    def selectSourceDirectory(self):
      src = QFileDialog.getExistingDirectory(self,
        "Select Asset Source Directory",
        srcdir,
        QFileDialog.ShowDirsOnly)

      self.srced.onChangedExternally(src)
      # preserve folder selection for other
      # invokations of this program (via QSettings)
      settings.setValue("src_dir",src)
      self.updateAssetList()

   ##########################################

    def updateAssetList(self):
      srcpath = self.srced.value
      a = pathtools.recursive_patglob(srcpath,"*.blend")
      pattern = self.filter_ed.value
      regex = fnmatch.translate(pattern)
      compiled_regex = re.compile(regex)
      b = []
      for item in a:
        m = compiled_regex.match(item)
        if m != None:
          b.append(item)
      text = "\n".join(b)
      self.assetlist.setPlainText(text)
      settings.setValue("pattern",pattern)
      self.assetlist_list = b

   ##########################################

    def goPushed(self):
      srcpath = self.srced.value
      os.chdir(srcpath)

      self.gobutton.setStyleSheet(bgcolor(128,32,0)+fgcolor(255,255,128))
      self.gobutton.setEnabled(False)

      #######################################

      self.output_texts = dict()

      class SubProc:
         def __init__(self,asswidget, index, cmdlist, item_path):
            #############################
            self.index = index
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

               outdata = OutputData(self.index, self.stdout+self.stderr,item_path)
               asswidget.output_texts[index] = outdata

               merge_output_text = ""
               for key in sorted(asswidget.output_texts.keys()):
                  outdata = asswidget.output_texts[key]
                  merge_output_text += "\n"
                  merge_output_text += "#####################################################\n"
                  merge_output_text += "##  %d  %s\n" % (outdata.index,outdata.input_path)
                  merge_output_text += "#####################################################\n"
                  merge_output_text += "\n"
                  merge_output_text += outdata.text

               self.asswidget.output_console.setPlainText(merge_output_text)
               self.asswidget.update()
               asswidget.running_count -= 1
               if asswidget.running_count == 0:
                  self.asswidget.gobutton.setStyleSheet(bgcolor(32,32,128)+fgcolor(255,255,128))
                  self.asswidget.gobutton.setEnabled(True)

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

      orkbin = "ork.blender.export.character.py"

      #############################
      index = 0
      self.running_count = 0
      for item in self.assetlist_list:
         cmdlist = [orkbin, "-i", item, "-o", path.temp()/"test.glb"]
         sp = SubProc(self,index,cmdlist,item)
         index += 1

#############################################################################

class AssetWindow(QMainWindow):
  def __init__(self):
    super(AssetWindow, self).__init__()
    self.prevDockWidget = None
    aw = AssetWidget(self)
    qd = QDockWidget()
    qd.setWidget(aw)
    self.setCentralWidget(qd)
    self.setMinimumSize(480,180)
    #self.setMaximumSize(1920,1080)
    self.setDockOptions(QMainWindow.AnimatedDocks | QMainWindow.ForceTabbedDocks);


#############################################################################

if __name__ == '__main__':
    app = QApplication(sys.argv)

    mainwin = AssetWindow()

    mainwin.resize(960,240)
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