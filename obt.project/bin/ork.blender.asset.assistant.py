#!/usr/bin/env os-python
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

#############################################################################

class OutputData:
   def __init__(self, index, text, input_path):
      self.input_path = input_path
      self.index = index
      self.text = text

class Config:
   def __init__(self, name, base_path, filter):
      self.name = name
      self.base_path = base_path
      self.filter = filter

#############################################################################

settings = QSettings("TweakoZ", "OrkidTool");
settings.beginGroup("App");
settings.endGroup();

this_dir = path.directoryOfInvokingModule()
srcdir = this_dir
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
      self.edit.setText(str(text))

   def addToLayout(self,l):
      l.addLayout(self.layout)

#############################################################################

class AssetWidget(QWidget):

   def plusConfig(self):
     name = self.configname.text()
     if name in self.configs.keys():
       return
     cfg = Config(name,self.srced.value,self.filter_ed.value)
     self.configs[name] = cfg
     self.updateConfigList()
     settings.setValue("configs",self.configs)
     self.configlist.setCurrentText(name)
     self.selectConfig()

   def minusConfig(self):
     name = self.configname.text()
     if name in self.configs.keys():
       # first cache closest key to the one we are deleteing...
       keys = list(self.configs.keys())
       keys.sort()
       index = keys.index(name)
       if index > 0:
         closest_key = keys[index-1]
       else:
         closest_key = keys[index+1]
       del self.configs[name]
     self.updateConfigList()
     settings.setValue("configs",self.configs)

   def selectConfig(self):
     name = self.configlist.currentText()
     if name in self.configs.keys():
       cfg = self.configs[name]
       self.configname.setText(cfg.name)
       self.srced.onChangedExternally(cfg.base_path)
       self.filter_ed.onChangedExternally(cfg.filter)
       self.updateAssetList()
     else:
       self.configname.setText("new-config")
       self.srced.onChangedExternally(this_dir)
       self.filter_ed.onChangedExternally("*")
       self.updateAssetList()
   ##########################################

   def __init__(self,mainwin):
     super(AssetWidget, self).__init__()
     self.mainwin = mainwin
     self.output_texts = dict()
     top_layout = QHBoxLayout()
     v1_layout = QVBoxLayout()
     v2_layout = QVBoxLayout()

     style = QStyleFactory.create("Fusion")
     file_icon = style.standardIcon(QStyle.SP_FileIcon)
     button_style = "background-color: rgb(0, 0, 64); border-radius: 2; "
     editstylesheet = "QWidget{background-color: rgb(64,64,128); color: rgb(160,160,192);}"
     editstylesheet += "QDockWidget::title {background-color: rgb(32,32,48); color: rgb(255,0,0);}"

     ########################################

     if settings.contains("configs"):
       self.configs = settings.value("configs") 
     else:
       self.configs = dict()
       default_config = Config("default",this_dir,"*")
       self.configs["default"] = default_config

     ########################################
     # configuration list
     ########################################

     cfg_layout = QHBoxLayout()

     cfg_plusbutton = QPushButton("+")
     cfg_minusbutton = QPushButton("-")
     cfg_plusbutton.setStyleSheet("QWidget{background-color: rgb(0,0,0); color: rgb(255,255,255);}")
     cfg_minusbutton.setStyleSheet("QWidget{background-color: rgb(0,0,0); color: rgb(255,255,255);}")
     cfg_plusbutton.setMinimumSize(24,28)
     cfg_minusbutton.setMinimumSize(24,28)


     self.configname = QLineEdit("default")
     self.configname.setStyleSheet("QWidget{background-color: rgb(0,0,32); color: rgb(255,255,0);}")
     #self.configname.setStyleSheet(editstylesheet)
     self.configname.setMinimumSize(64,28)
     cfg_layout.addWidget(self.configname)
     cfg_layout.addWidget(cfg_plusbutton)
     cfg_layout.addWidget(cfg_minusbutton)

     v1_layout.addLayout(cfg_layout)

     self.configlist = QComboBox()
     self.configlist.setStyleSheet("QWidget{background-color: rgb(0,0,0); color: rgb(255,255,255);}")
     v1_layout.addWidget(self.configlist)

     ##########################################
     # source directory 
     ##########################################

     sbutton = QPushButton()
     sbutton.setIcon(file_icon)
     sbutton.setStyleSheet(button_style)
     sbutton.setMinimumSize(48,24)
     sbutton.setStyleSheet("QWidget{background-color: rgb(0,0,0); color: rgb(255,255,255);}")
     self.srced = Edit("Source Directory")
     self.srced.edit.setMinimumSize(64,28)
     slay2 = QHBoxLayout()
     slay2.addLayout(self.srced.layout)
     self.srced.edit.setStyleSheet(bgcolor(64,32,64)+fgcolor(255,255,0))
     slay2.addWidget(sbutton)
     v1_layout.addLayout(slay2)

     pattern = "*"
     if settings.contains("pattern"):
       pattern = settings.value("pattern")

     self.filter_ed = Edit("filter",pattern)
     self.filter_ed.edit.setMinimumSize(64,28)
     self.filter_ed.edit.setStyleSheet("QWidget{background-color: rgb(0,0,32); color: rgb(255,255,0);}")
     v1_layout.addLayout(self.filter_ed.layout)

     ########################################
     # asset list
     ########################################

     self.assetlist = QPlainTextEdit()
     self.assetlist.setStyleSheet(editstylesheet)
     self.assetlist.setReadOnly(True)
     v1_layout.addWidget(self.assetlist)

     ########################################
     # go button 
     ########################################

     gobutton = QPushButton("Go")
     gobutton.setStyleSheet(bgcolor(32,32,128)+fgcolor(255,255,128))
     v1_layout.addWidget(gobutton)
     self.gobutton = gobutton

     ##########################
     # output console
     ##########################

     self.output_console = QPlainTextEdit()
     self.output_console.setReadOnly(True)
     self.output_hilighter = hilite.Highlighter(self.output_console.document())
     qdss = "QWidget{background-color: rgb(32,32,64); color: rgb(255,255,255);}"
     self.output_console.setStyleSheet(qdss)
     v2_layout.addWidget(self.output_console)
     ##########################
     top_layout.addLayout(v1_layout)
     top_layout.addLayout(v2_layout)
     self.setLayout(top_layout)

     ##########################
     # connect signal slots
     ##########################

     self.updateConfigList()

     self.srced.onChangedExternally(str(srcdir))
     self.filter_ed.edit.textChanged.connect(self.updateAssetList)
     sbutton.pressed.connect(self.selectSourceDirectory)
     cfg_plusbutton.pressed.connect(self.plusConfig)
     cfg_minusbutton.pressed.connect(self.minusConfig)  
     gobutton.pressed.connect(self.goPushed)
     self.configlist.currentIndexChanged.connect(self.selectConfig)

     self.updateAssetList()

     ##########################


   ##########################################

   def selectSourceDirectory(self):
      src = QFileDialog.getExistingDirectory(self,
        "Select Asset Source Directory",
        str(srcdir),
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

   def updateConfigList(self):
      self.configlist.clear()
      for key in self.configs.keys():
        self.configlist.addItem(key)

   ##########################################

   def goPushed(self):
      srcpath = self.srced.value
      os.chdir(srcpath)

      self.gobutton.setStyleSheet(bgcolor(128,32,0)+fgcolor(255,255,128))
      self.gobutton.setEnabled(False)

      #######################################

      self.output_texts = dict()
      self.subprocs = dict()
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
               exitcode = self.process.exitCode()
               # when process finished, async update QT GUI
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
                  merge_output_text += "\n## exit code: %d\n" % exitcode

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
         self.subprocs[index] = sp
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

    mainwin.resize(1600,960)
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