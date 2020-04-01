////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/application/application.h>
#include <orktool/qtui/qtmainwin.h>
#include <orktool/qtui/qtui_tool.h>
///////////////////////////////////////////////////////////////////////////////
#include <QtCore/QSettings>
#include <QtWidgets/QMessageBox>

///////////////////////////////////////////////////////////////////////////////
#include <ork/reflect/serialize/XMLDeserializer.h>
#include <ork/reflect/serialize/XMLSerializer.h>
#include <ork/stream/FileInputStream.h>
#include <ork/stream/FileOutputStream.h>
///////////////////////////////////////////////////////////////////////////////
#include <orktool/toolcore/dataflow.h>
///////////////////////////////////////////////////////////////////////////////
#include <pkg/ent/editor/editor.h>
#include <pkg/ent/entity.h>
#include <pkg/ent/scene.h>
///////////////////////////////////////////////////////////////////////////////
#include <ork/kernel/opq.h>
#include <orktool/toolcore/FunctionManager.h>
#include <pkg/ent/editor/edmainwin.h>
#include "vpSceneEditor.h"
///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace ent {
///////////////////////////////////////////////////////////////////////////////

void SetRecentSceneFile(ConstString file, int);
ConstString GetRecentSceneFile(int);

enum {
  SCENEFILE_DIR,
  EXPORT_DIR,
};

void EditorMainWindow::OpenSceneFile() {
  ork::opq::assertOnQueue2(opq::mainSerialQueue());

  PerfMarkerPushState();
  PerfMarkerDisable();

  static auto orkdatapath = tool::getDataDir() + "scene.mox";
  auto datapath           = qs(orkdatapath);
  printf("LOADSCNE from<%s>\n", datapath.toStdString().c_str());
  gUpdateStatus.SetState(EUPD_STOP);

  auto openfiledialog = new QFileDialog(nullptr, tr("Load OrkidScene File"), datapath);
  openfiledialog->setFileMode(QFileDialog::ExistingFiles);
  openfiledialog->setDirectory(datapath);
  openfiledialog->setNameFilter(tr("OrkSceneFile (*.mox *.mob)"));
  openfiledialog->setViewMode(QFileDialog::Detail);

  openfiledialog->setOption(QFileDialog::DontUseNativeDialog, false);
  openfiledialog->setOption(QFileDialog::DontConfirmOverwrite, false);

  openfiledialog->setOption(QFileDialog::HideNameFilterDetails, false);
  openfiledialog->setAcceptMode(QFileDialog::AcceptOpen);

  QStringList selectedFiles;
  std::string fname;
  if (openfiledialog->exec()) {
    selectedFiles = openfiledialog->selectedFiles();
    fname         = selectedFiles.first().toStdString();
  }

  gUpdateStatus.SetState(EUPD_START);
  PerfMarkerPopState();
  this->activateWindow();

  if (fname.length() == 0)
    return;

  orkdatapath = fname;

  ///////////////////////////////////////////////
  // Check if Read-Only flag is set (SVN requires lock?)
  ///////////////////////////////////////////////
  bool writable = FileEnv::IsFileWritable(file::Path(fname.c_str()));
  if (!writable) {
    gUpdateStatus.SetState(EUPD_STOP);
    int result = QMessageBox::question(
        this,
        "File is Read-Only",
        "Do you want to open the file Read-Only?",
        QMessageBox::Yes,
        QMessageBox::No,
        QMessageBox::Cancel);
    gUpdateStatus.SetState(EUPD_START);
    if (result == QMessageBox::Yes)
      this->SetReadOnly(true);
    else
      return;
  } else
    this->SetReadOnly(false);
  ///////////////////////////////////////////////
  QueueLoadScene(fname);
}

///////////////////////////////////////////////////////////////////////////

void EditorMainWindow::QueueLoadScene(const std::string& filename) {
  ///////////////////////////////////////////////
  // outer load on concurrent thread
  ///////////////////////////////////////////////
  auto on_loaded = [=]() {
    opq::mainSerialQueue()->enqueue(opq::Op([=]() {
      printf("Scene<%s> Loaded\n", filename.c_str());
      SetRecentSceneFile(filename.c_str(), SCENEFILE_DIR);
      this->mCurrentFileName = QString(filename.c_str());
      // printf("calling SlotUpdateAll\n");
      this->SlotUpdateAll();
      // printf("called SlotUpdateAll\n");
    }));
  };
  ///////////////////////////////////////////////
  LoadSceneReq R;
  R.mFileName = filename;
  R.SetOnLoadedOp(on_loaded);
  mEditorBase.QueueOpASync(R);
}

///////////////////////////////////////////////////////////////////////////

void EditorMainWindow::SaveSceneFile() {
  tool::GetGlobalDataFlowScheduler()->GraphSet().LockForRead();

  // mEditorBase.EditorUnGroup();

  QString FileName = QFileDialog::getSaveFileName(0, "Save OrkidScene File", mCurrentFileName, "OrkSceneFile (*.mox *.mob)");
  this->activateWindow();
  file::Path::NameType fname = FileName.toStdString().c_str();
  if (fname.length()) {
    SetRecentSceneFile(FileName.toStdString().c_str(), SCENEFILE_DIR);
    if (FileEnv::filespec_to_extension(fname).length() == 0)
      fname += ".mox";
    if (mEditorBase.mpScene) {
      if (!strcmp(GetRecentSceneFile(SCENEFILE_DIR).c_str(), fname.c_str())) {
        const QString oldName(fname.c_str());
        QString newName(fname.c_str());
        // QDir::rename ( const QString & oldName, const QString & newName )
      }

      stream::FileOutputStream ostream(fname.c_str());
      reflect::serialize::XMLSerializer oser(ostream);
      oser.Serialize(mEditorBase.mpScene);

      tokenlist hook;
      hook.push_back("OnSceneSave");
      ork::FunctionManager::GetRef().ExecuteFunction(hook);
    }
    SetSceneFile(QString(fname.c_str()));
  }
  tool::GetGlobalDataFlowScheduler()->GraphSet().UnLock();
}

///////////////////////////////////////////////////////////////////////////

void EditorMainWindow::SaveSelected() {
  orkset<ork::Object*> SelectedObjects;
  SelectedObjects = mEditorBase.selectionManager().getActiveSelection();

  if (SelectedObjects.size() == 1) {
    const ork::Object* pobj = *SelectedObjects.begin();

    const EntData* pent = rtti::autocast(pobj);

    if (pent) {
      const Archetype* parch = pent->GetArchetype();

      if (parch) {
        pobj = parch;
      }
    }

    const Archetype* parch = rtti::autocast(pobj);

    if (parch) {
      tool::GetGlobalDataFlowScheduler()->GraphSet().LockForRead();
      {
        QString FileName =
            QFileDialog::getSaveFileName(0, "Export Archetype", GetRecentSceneFile(EXPORT_DIR).c_str(), "OrkSceneFile (*.mox)");

        file::Path::NameType fname = FileName.toStdString().c_str();
        if (fname.length()) {
          file::Path::NameType ext = FileEnv::filespec_to_extension(fname);

          if (ext.length() == 0) {
            fname += ".mox";
          }

          SetRecentSceneFile(FileName.toStdString().c_str(), EXPORT_DIR);

          stream::FileOutputStream ostream(fname.c_str());
          reflect::serialize::XMLSerializer oser(ostream);
          oser.Serialize(pobj);
        }
      }
      tool::GetGlobalDataFlowScheduler()->GraphSet().UnLock();
    }
  }
}

///////////////////////////////////////////////////////////////////////////

void EditorMainWindow::MergeFile() {
  tool::GetGlobalDataFlowScheduler()->GraphSet().LockForRead();
  {
    QString FileName =
        QFileDialog::getOpenFileName(0, "Import Archetype", GetRecentSceneFile(EXPORT_DIR).c_str(), "OrkMergeFile (*.mox *.arch)");
    std::string fname = FileName.toStdString();
    if (fname.length()) {
      SetRecentSceneFile(FileName.toStdString().c_str(), EXPORT_DIR);

      stream::FileInputStream istream(fname.c_str());
      reflect::serialize::XMLDeserializer dser(istream);

      rtti::ICastable* pcastable = 0;
      bool bOK                   = dser.Deserialize(pcastable);
      if (bOK) {
        Archetype* parch = rtti::autocast(pcastable);

        if (parch) {
          SigNewObject(parch);
          //
        }
      }
    }
  }
  tool::GetGlobalDataFlowScheduler()->GraphSet().UnLock();
}

///////////////////////////////////////////////////////////////////////////

ConstString GetRecentSceneFile(int dialog_id) {

  char dialog_id_str[4];
  QSettings settings("Michael T. Mayers", "Nova Tool");

  settings.beginGroup("RecentFiles");

  sprintf(dialog_id_str, "%d", dialog_id);
  auto stdstr = settings.value(dialog_id_str, "").toString().toStdString();
  return AddPooledString(stdstr.c_str());
}

void SetRecentSceneFile(ConstString file, int dialog_id) {
  char dialog_id_str[4];
  QSettings settings("Michael T. Mayers", "Nova Tool");
  QString string(file.c_str());
  settings.beginGroup("RecentFiles");
  sprintf(dialog_id_str, "%d", dialog_id);
  settings.setValue(dialog_id_str, string);
  settings.endGroup();
}

///////////////////////////////////////////////////////////////////////////////
}} // namespace ork::ent
///////////////////////////////////////////////////////////////////////////////
