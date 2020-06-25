////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/python/context.h>
#include <ork/reflect/properties/register.h>
#include <ork/util/hotkey.h>
#include <ork/stream/FileOutputStream.h>
#include <ork/stream/FileInputStream.h>
#include <ork/reflect/serialize/XMLSerializer.h>
#include <ork/reflect/serialize/XMLDeserializer.h>
#include <ork/kernel/opq.h>
#include <ork/application/application.h>
///////////////////////////////////////////////////////////////////////////////
#include <ork/lev2/gfx/gfxprimitives.h>
#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/lev2/gfx/particle/particle.h>
#include <ork/lev2/gfx/gfxmaterial_test.h>
#include <ork/lev2/qtui/qtui.hpp>
///////////////////////////////////////////////////////////////////////////////
#include <pkg/ent/editor/edmainwin.h>
#include <pkg/ent/ReferenceArchetype.h>
///////////////////////////////////////////////////////////////////////////////
#include <orktool/qtui/qtui_tool.h>
#include <orktool/qtui/qtmainwin.h>
#include <orktool/ged/ged.h>
#include <orktool/ged/ged_delegate.h>
///////////////////////////////////////////////////////////////////////////
#include "vpSceneEditor.h"
#include <QtCore/QSettings>
#include <QtWidgets/QDockWidget>
#include <QtWidgets/QInputDialog>
///////////////////////////////////////////////////////////////////////////////
using namespace ork::lev2;
///////////////////////////////////////////////////////////////////////////////
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::EditorMainWindow, "EditorMainWindow");
///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace ent {
///////////////////////////////////////////////////////////////////////////////
Simulation* GetEditorSimulation() {
  return gEditorMainWindow->mEditorBase.GetEditSimulation();
}
///////////////////////////////////////////////////////////////////////////////
void SceneTopoChanged() {
  gEditorMainWindow->SlotUpdateAll();
}
///////////////////////////////////////////////////////////////////////////////
EditorMainWindow* gEditorMainWindow;
///////////////////////////////////////////////////////////////////////////
void RegisterMainWinDefaultModule(EditorMainWindow& emw);
void RegisterLightingModule(EditorMainWindow& emw);
///////////////////////////////////////////////////////////////////////////
void EditorMainWindow::Describe() {
  ///////////////////////////////////////////////////////////
  RegisterAutoSignal(EditorMainWindow, NewObject);
  ///////////////////////////////////////////////////////////
  RegisterAutoSlot(EditorMainWindow, UpdateAll);
  RegisterAutoSlot(EditorMainWindow, OnTimer);
  RegisterAutoSlot(EditorMainWindow, SimulationInvalidated);
  RegisterAutoSlot(EditorMainWindow, ObjectSelected);
  RegisterAutoSlot(EditorMainWindow, ObjectDeSelected);
  RegisterAutoSlot(EditorMainWindow, SpawnNewGed);
  RegisterAutoSlot(EditorMainWindow, ClearSelection);
  RegisterAutoSlot(EditorMainWindow, PostNewObject);
  ///////////////////////////////////////////////////////////
}
///////////////////////////////////////////////////////////////////////////
void EditorMainWindow::SlotSimulationInvalidated(ork::Object* pSI) {
}
///////////////////////////////////////////////////////////////////////////
void EditorMainWindow::SlotObjectSelected(ork::Object* pobj) {
  EntData* pdata = rtti::autocast(pobj);
  if (pdata) {
    fmtx4 mtx;
    mtx = pdata->GetDagNode().GetTransformNode().GetTransform().GetMatrix();
    mEditorBase.setSpawnMatrix(mtx);
    mEditorBase.ManipManager().AttachObject(pobj);
  }
  mGedModelObj.Attach(pobj);
}
///////////////////////////////////////////////////////////////////////////
void EditorMainWindow::SlotPostNewObject(ork::Object* pobj) {
  ent::Archetype* parch = rtti::autocast(pobj);
  if (parch) {
    mGedModelObj.Attach(pobj);
  }
}
///////////////////////////////////////////////////////////////////////////
void EditorMainWindow::SlotObjectDeSelected(ork::Object* pobj) {
}
void EditorMainWindow::SlotClearSelection() {
  mGedModelObj.Attach(NULL);
}
void EditorMainWindow::SigNewObject(ork::Object* pOBJ) {
  mSignalNewObject(&EditorMainWindow::SigNewObject, pOBJ);
}
///////////////////////////////////////////////////////////////////////////
void EditorMainWindow::ToggleFullscreen() {
  _fullscreen = !_fullscreen;
  isFullScreen() ? showNormal() : showFullScreen();
  if (isFullScreen()) {
    // setWindowFlags(Qt::Window);

  } else {
    // setWindowFlags(Qt::Window | Qt::CustomizeWindowHint);
  }
}
///////////////////////////////////////////////////////////////////////////
void EditorMainWindow::Exit() {
  // todo - better cleanup method
  exit(0);
}
///////////////////////////////////////////////////////////////////////////
EditorMainWindow::EditorMainWindow(QWidget* parent, const std::string& applicationClassName, QApplication& App)
    : MiniorkMainWindow(parent)
    , mpSplashScreen(0)
    , mQtApplication(App)
    , ConstructAutoSlot(UpdateAll)
    , ConstructAutoSlot(OnTimer)
    , ConstructAutoSlot(ObjectSelected)
    , ConstructAutoSlot(ObjectDeSelected)
    , ConstructAutoSlot(SpawnNewGed)
    , ConstructAutoSlot(ClearSelection)
    , ConstructAutoSlot(PostNewObject)
    , ConstructAutoSlot(SimulationInvalidated) {

  _fullscreen = false;
  // setWindowFlags(Qt::Window | Qt::FramelessWindowHint);

  SetupSignalsAndSlots();

  //////////////////////////////////
  //////////////////////////////////

  HotKeyConfiguration defaultconfig;
  defaultconfig.Default();

  defaultconfig.AddHotKey("camera_fwd", "w");
  defaultconfig.AddHotKey("camera_left", "a");
  defaultconfig.AddHotKey("camera_bwd", "s");
  defaultconfig.AddHotKey("camera_right", "d");
  defaultconfig.AddHotKey("camera_x", "x");
  defaultconfig.AddHotKey("camera_y", "y");
  defaultconfig.AddHotKey("camera_z", "z");

  defaultconfig.AddHotKey("camera_rotl", "num4");
  defaultconfig.AddHotKey("camera_rotr", "num6");
  defaultconfig.AddHotKey("camera_rotu", "num8");
  defaultconfig.AddHotKey("camera_rotd", "num2");
  defaultconfig.AddHotKey("camera_realign", "num0");

  defaultconfig.AddHotKey("camera_in", "c");
  defaultconfig.AddHotKey("camera_out", "num1");
  defaultconfig.AddHotKey("camera_up", "num9");
  defaultconfig.AddHotKey("camera_down", "num3");

  defaultconfig.AddHotKey("camera_aper-", "[");
  defaultconfig.AddHotKey("camera_aper+", "]");

  defaultconfig.AddHotKey("camera_origin", "o");
  defaultconfig.AddHotKey("camera_pick2focus", "p");
  defaultconfig.AddHotKey("camera_focus2pick", "f");

  defaultconfig.AddHotKey("camera_mouse_dolly", "alt-mmb-none");
  defaultconfig.AddHotKey("camera_mouse_rot", "alt-lmb-none");

  HotKeyManager::GetRef().AddHotKeyConfiguration("editmode", defaultconfig);

  HotKeyManager::GetRef().Save();
  HotKeyManager::GetRef().Load();

  //////////////////////////////////
  //////////////////////////////////

  AddBuiltInActions();

  //////////////////////////////////
  //////////////////////////////////

  // QPixmap pixmap("editor/splash.png");
  // mpSplashScreen = new QSplashScreen(pixmap);
  // mpSplashScreen->show();

  f64 SplashTimeBase = OldSchool::GetRef().GetLoResTime();

  mEditorBase.RegisterChoices();

  mGedModelObj.SetChoiceManager(mEditorBase._choicemanager);

  /////////////////////////////////////////////////////////////////////

  object::Connect(&this->GetSigNewObject(), &mEditorBase.GetSlotNewObject());

  object::Connect(&mEditorBase.selectionManager().GetSigObjectSelected(), &this->GetSlotObjectSelected());

  object::Connect(&mEditorBase.selectionManager().GetSigObjectDeSelected(), &this->GetSlotObjectDeSelected());

  object::Connect(&mEditorBase.selectionManager().GetSigClearSelection(), &this->GetSlotClearSelection());

  bool bconOK =
      object::Connect(&mEditorBase, AddPooledLiteral("SigObjectDeleted"), &mDataflowEditor, AddPooledLiteral("SlotClear"));
  OrkAssert(bconOK);

  bconOK = object::Connect(&mEditorBase, AddPooledLiteral("SigNewScene"), &mDataflowEditor, AddPooledLiteral("SlotClear"));
  OrkAssert(bconOK);

  bconOK = object::Connect(&mGedModelObj.GetSigPostNewObject(), &this->GetSlotPostNewObject());

  OrkAssert(bconOK);

  ////////////////////////////////////

  auto genviewblk = [=]() {
    auto camwin = NewCamView(false);
    SceneObjPropEdit();
    if (ork::python::isPythonEnabled()) {
      // QDockWidget *pdw3 = NewPyConView(false);
    }
    // QDockWidget *pdw3 = NewDataflowView(false);
    ////////////////////////////////////
    // tabifyDockWidget( pdw2, pdw3 );
    // tabifyDockWidget( pdw4, pdw2 );
    setCentralWidget(camwin);
  };

  genviewblk();
  ////////////////////////////////////

  setCorner(Qt::TopLeftCorner, Qt::LeftDockWidgetArea);
  setCorner(Qt::BottomLeftCorner, Qt::LeftDockWidgetArea);
  setCorner(Qt::TopRightCorner, Qt::RightDockWidgetArea);
  setCorner(Qt::BottomRightCorner, Qt::RightDockWidgetArea);

  //////////////////////////////////////////////////////////

  CreateFunctionMenus();

  ////////////////////////////////////////////////

  ork::file::Path collapse_filename("collapse_state.cst");
  if (ork::FileEnv::DoesFileExist(collapse_filename)) {
    ork::tool::ged::PersistMapContainer& container = mGedModelObj.GetPersistMapContainer();

    stream::FileInputStream istream(collapse_filename.c_str());
    reflect::serialize::XMLDeserializer iser(istream);
    bool bOK = ork::Object::xxxDeserializeInPlace(&container, iser);
    OrkAssert(bOK);
  }

  ////////////////////////////////////////////////

  mGedModelObj.EnablePaint();

  LoadLayout();

  this->activateWindow();
}

EditorMainWindow::~EditorMainWindow() {
  ork::tool::ged::PersistMapContainer& container = mGedModelObj.GetPersistMapContainer();
  ork::file::Path collapse_filename("collapse_state.cst");
  stream::FileOutputStream ostream(collapse_filename.c_str());
  reflect::serialize::XMLSerializer oser(ostream);
  ork::Object::xxxSerializeInPlace(&container, oser);
}

///////////////////////////////////////////////////////////////////////////

void EditorMainWindow::SlotOnTimer() {
}

///////////////////////////////////////////////////////////////////////////
bool EditorMainWindow::event(QEvent* qevent) {
  switch (qevent->type()) {
    case ork::tool::QQedRefreshEvent::gevtype:
      return true;
      break;
    default:
      break;
  }
  return MiniorkMainWindow::event(qevent);
}
///////////////////////////////////////////////////////////////////////////
void EditorMainWindow::SlotUpdateAll() {
  if (mEditorBase.mpScene) {
    tool::QQedRefreshEvent* prev = new tool::QQedRefreshEvent;
    mQtApplication.postEvent(this, prev); //, Qt::LowEventPriority );
  }
}
///////////////////////////////////////////////////////////////////////////
void EditorMainWindow::ArchExport() {
  auto lamb = [=]() { this->mEditorBase.EditorArchExport(); };
  opq::updateSerialQueue()->enqueueAndWait(opq::Op(lamb));
}
///////////////////////////////////////////////////////////////////////////
void EditorMainWindow::ArchImport() {
  auto lamb = [=]() { this->mEditorBase.EditorArchImport(); };
  opq::updateSerialQueue()->enqueueAndWait(opq::Op(lamb));
}
///////////////////////////////////////////////////////////////////////////
void EditorMainWindow::ArchMakeReferenced() {
  auto lamb = [=]() { this->mEditorBase.EditorArchMakeReferenced(); };
  opq::updateSerialQueue()->enqueueAndWait(opq::Op(lamb));
}
///////////////////////////////////////////////////////////////////////////
void EditorMainWindow::ArchMakeLocal() {
  auto lamb = [=]() { this->mEditorBase.EditorArchMakeLocal(); };
  opq::updateSerialQueue()->enqueueAndWait(opq::Op(lamb));
}
///////////////////////////////////////////////////////////////////////////
void EditorMainWindow::NewEntity() {
  mEditorBase.QueueOpASync(NewEntityReq::makeShared());
}
///////////////////////////////////////////////////////////////////////////
void EditorMainWindow::NewEntities() {
  bool ok;
  int i = QInputDialog::getInt(
      this,
      tr("New Entities..."),
      tr("Entity Count:"),
      1,    // value
      1,    // min
      1024, // max
      1,    // step
      &ok);
  if (ok) {
    opq::updateSerialQueue()->enqueueAndWait(opq::Op([=]() { this->mEditorBase.EditorNewEntities(i); }));
  }
}
///////////////////////////////////////////////////////////////////////////
void EditorMainWindow::Group() {
  mEditorBase.EditorGroup();
}
///////////////////////////////////////////////////////////////////////////
void EditorMainWindow::Dupe() {
  mEditorBase.EditorDupe();
}
///////////////////////////////////////////////////////////////////////////

void EditorMainWindow::AddBuiltInActions() {
  if (1)
    RegisterMainWinDefaultModule(*this);
  if (0)
    RegisterLightingModule(*this);
  ork::msleep(100);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void EditorMainWindow::SaveLayout() {
  QSettings settings("TweakoZ", "MiniorkEditor");
  settings.beginGroup("mainWindow");
  settings.setValue("geometry", saveGeometry());
  settings.setValue("state", saveState());
  settings.endGroup();
}

///////////////////////////////////////////////////////////////////////////////

void EditorMainWindow::LoadLayout() {
  QSettings settings("TweakoZ", "MiniorkEditor");
  settings.beginGroup("mainWindow");
  restoreGeometry(settings.value("geometry").toByteArray());
  restoreState(settings.value("state").toByteArray());
  settings.endGroup();
}

///////////////////////////////////////////////////////////////////////////////

static void ReplaceArchetype(ent::SceneData* pscene, const ent::Archetype* old_arch, ent::Archetype* new_arch) {

  orkmap<PoolString, ent::EntData*> EntDatas;
  orkmap<PoolString, SceneObject*> scene_objects = pscene->GetSceneObjects();
  for (auto it = scene_objects.begin(); it != scene_objects.end(); it++) {
    ent::SceneObject* pso = it->second;
    ent::EntData* ped     = rtti::autocast(pso);
    if (ped) {
      if (ped->GetArchetype() == old_arch) {
        EntDatas.insert(std::pair<PoolString, ent::EntData*>(it->first, ped));
      }
    }
  }
  /////////////////////////////////////////////////////////////
  SceneObject* pnonconst = pscene->FindSceneObjectByName(old_arch->GetName());
  gEditorMainWindow->mEditorBase.EditorDeleteObject(pnonconst);
  pscene->AddSceneObject(new_arch);
  /////////////////////////////////////////////////////////////
  for (orkmap<PoolString, ent::EntData*>::const_iterator it = EntDatas.begin(); it != EntDatas.end(); it++) {
    ent::EntData* ped = it->second;
    ped->SetArchetype(new_arch);
  }
  /////////////////////////////////////////////////////////////
  gEditorMainWindow->SigNewObject(new_arch);
}

///////////////////////////////////////////////////////////////////////////////

struct EntArchDeRef final : public ork::tool::ged::IOpsDelegate {
  RttiDeclareConcretePublic(EntArchDeRef, ork::tool::ged::IOpsDelegate);

  EntArchDeRef() {
  }
  ~EntArchDeRef() final {
  }

  void Execute(ork::Object* ptarget) final {
    SetProgress(0.0f);
    ent::EntData* pentdata = rtti::autocast(ptarget);
    if (0 != pentdata) {
      const ent::Archetype* parch = pentdata->GetArchetype();
      if (0 != parch) {
        const ent::ReferenceArchetype* prefarch = rtti::autocast(parch);
        if (0 != prefarch) {
          ent::SceneData* pscene = parch->GetSceneData();
          if (0 != pscene) {
            ArchetypeAsset* passet = prefarch->GetAsset();
            if (0 != passet) {
              ent::Archetype* pderefarch = passet->GetArchetype();
              if (0 != pderefarch) {
                ReplaceArchetype(pscene, parch, pderefarch);
                /////////////////////////////////////////////////////////////
                SetProgress(1.0f);
                gEditorMainWindow->mEditorBase.selectionManager().AddObjectToSelection(pderefarch);
              }
            }
          }
        }
      }
    }
    tool::ged::IOpsDelegate::RemoveTask(EntArchDeRef::GetClassStatic(), ptarget);
  }
};

void EntArchDeRef::Describe() {
}

///////////////////////////////////////////////////////////////////////////////

struct EntArchReRef final : public ork::tool::ged::IOpsDelegate {
  RttiDeclareConcretePublic(EntArchReRef, ork::tool::ged::IOpsDelegate);
  EntArchReRef() {
  }
  ~EntArchReRef() final {
  }

  template <typename T> void find_and_replace(T& source, const T& find, const T& replace) {
    size_t j     = 0;
    size_t idiff = replace.length() - find.length();
    for (; (j = source.find(find, j)) != T::npos;) {
      if (idiff > 0) {
        size_t isourcelen = source.length();
        source.resize(isourcelen + idiff);
        size_t inslen = isourcelen - j;
        for (size_t i = (inslen - 1); i < inslen; i--) {
          size_t isrc  = (j + i);
          size_t idst  = (isrc + idiff);
          source[idst] = source[isrc];
        }
      }
      source.replace(j, find.length(), replace);
      j += idiff;
    }
  }

  void Execute(ork::Object* ptarget) final {
    SetProgress(0.0f);
    ent::EntData* pentdata = rtti::autocast(ptarget);
    if (0 != pentdata) {
      const ent::Archetype* parch = pentdata->GetArchetype();
      if (0 != parch) {
        const ent::ReferenceArchetype* prefarch = rtti::autocast(parch);
        if (0 != prefarch) {
          ent::SceneData* pscene = parch->GetSceneData();
          if (0 != pscene) {
            const PoolString OriginalName = parch->GetName();
            /////////////////////////////////////////////////////////////////////
            ArrayString<512> assetname;
            MutableString(assetname).format("data://archetypes%s.mox", parch->GetName().c_str());
            file::Path::NameType newname(parch->GetName().c_str());
            file::Path assetpath(assetname.c_str());
            file::Path absolutepath = assetpath.ToAbsolute();
            file::Path::NameType absolutepath_raw(absolutepath.c_str());
            /////////////////////////////////////////////////////////////////////
            find_and_replace<file::Path::NameType>(absolutepath_raw, "\\pc\\", "\\src\\");
            file::Path SrcFileName(absolutepath_raw);
            SrcFileName.SetExtension(".mox");
            stream::FileOutputStream ostream(SrcFileName.c_str());
            reflect::serialize::XMLSerializer oser(ostream);
            oser.Serialize(parch);
            /////////////////////////////////////////////////////////////////////
            find_and_replace<file::Path::NameType>(absolutepath_raw, "\\src\\", "\\pc\\");
            file::Path PcFileName(absolutepath_raw);
            PcFileName.SetExtension(".mox");
            stream::FileOutputStream ostream2(PcFileName.c_str());
            reflect::serialize::XMLSerializer oser2(ostream2);
            oser2.Serialize(parch);
            /////////////////////////////////////////////////////////////////////
            gEditorMainWindow->mEditorBase.mpArchChoices->EnumerateChoices();
            gEditorMainWindow->mEditorBase.mpRefArchChoices->EnumerateChoices(true);
            /////////////////////////////////////////////////////////////////////
            auto arch_asset                     = asset::AssetManager<ArchetypeAsset>::Load(assetname.c_str());
            ent::ReferenceArchetype* newrefarch = new ent::ReferenceArchetype;
            newrefarch->SetAsset(arch_asset.get());
            newrefarch->SetName(OriginalName);
            /////////////////////////////////////////////////////////////
            ReplaceArchetype(pscene, parch, newrefarch);
            /////////////////////////////////////////////////////////////
            SetProgress(1.0f);
            gEditorMainWindow->mEditorBase.selectionManager().AddObjectToSelection(newrefarch);
          }
        }
      }
    }
    tool::ged::IOpsDelegate::RemoveTask(EntArchReRef::GetClassStatic(), ptarget);
    /////////////////////////////////////////////////////////////
  }
};

void EntArchReRef::Describe() {
}

///////////////////////////////////////////////////////////////////////////////

struct EntArchSplit final : public ork::tool::ged::IOpsDelegate {
  RttiDeclareConcretePublic(EntArchSplit, ork::tool::ged::IOpsDelegate);

  EntArchSplit() {
  }
  ~EntArchSplit() final {
  }

  void Execute(ork::Object* ptarget) final {
    SetProgress(0.0f);
    tool::ged::IOpsDelegate::RemoveTask(EntArchSplit::GetClassStatic(), ptarget);
  }
};

void EntArchSplit::Describe() {
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
}} // namespace ork::ent
///////////////////////////////////////////////////////////////////////////////

INSTANTIATE_TRANSPARENT_RTTI(ork::ent::EntArchDeRef, "EntArchDeRef");
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::EntArchReRef, "EntArchReRef");
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::EntArchSplit, "EntArchSplit");
