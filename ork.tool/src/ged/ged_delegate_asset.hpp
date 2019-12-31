////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <QtWidgets/QAction>
#include <QtWidgets/QInputDialog>
#include <QtWidgets/QMenu>
#include <ork/asset/Asset.h>
#include <ork/lev2/gfx/dbgfontman.h>
#include <orktool/ged/ged_io.h>

namespace ork { namespace tool { namespace ged {

///////////////////////////////////////////////////////////////////////////////

template <typename IODriver> void GedAssetNode<IODriver>::OnMouseDoubleClicked(const ork::ui::Event& ev) {
  ObjModel& model = mModel;
  OnCreateObject();
  model.Attach(model.CurrentObject());
}

///////////////////////////////////////////////////////////////////////////////

template <typename IODriver> void GedAssetNode<IODriver>::DoDraw(lev2::GfxTarget* pTARG) {
  int pnamew = propnameWidth() + 16;
  // GedItemNode::Draw( pTARG );
  GetSkin()->DrawBgBox(this, miX + pnamew, miY, miW - pnamew, miH, GedSkin::ESTYLE_BACKGROUND_2);
  GetSkin()->DrawText(this, miX + pnamew, miY + 2, _content.c_str());
  GetSkin()->DrawText(this, miX + 6, miY + 2, _propname.c_str());
}

///////////////////////////////////////////////////////////////////////////////

template <typename IODriver> void GedAssetNode<IODriver>::OnCreateObject() {
  ConstString annoclass = GetOrkProp()->GetAnnotation("editor.assetclass");
  ConstString annotype  = GetOrkProp()->GetAnnotation("editor.assettype");

  ork::asset::AssetClass* passetclass = rtti::autocast(ork::asset::AssetClass::FindClass(annoclass));
  OrkAssert(passetclass);

  if (passetclass) {
    ChoiceList* chclist = 0;
    ;

    chclist = mModel.GetChoiceManager()->GetChoiceList(annotype.c_str());

    QMenu qm;

    QAction* pact = qm.addAction("none");
    pact->setData(QVariant("none"));
    QAction* pact2 = qm.addAction("reload");
    pact2->setData(QVariant("reload"));

    if (chclist) {
      QMenu* qm2 = chclist->CreateMenu();

      qm.addMenu(qm2);
    }

    pact = qm.exec(QCursor::pos());

    // IODriver::LayerDeserializerType aldser( mIoDriver );

    if (pact) {
      QVariant UserData = pact->data();
      QString UserName  = UserData.toString();
      std::string pname = UserName.toStdString();

      file::Path apath(pname.c_str());

      OrkAssert(GetOrkProp());

      QVariant chcvalprop = pact->property("chcval");

      const AttrChoiceValue* chcval =
          chcvalprop.isValid() ? (const AttrChoiceValue*)chcvalprop.value<void*>() : (const AttrChoiceValue*)0;

      if (chcval) {
        if (0 != strstr(chcval->GetValue().c_str(), "asset<")) {
          asset::Asset* passet = 0;
          sscanf(chcval->GetValue().c_str(), "asset<%p>", &passet);
          if (passet) {
            mIoDriver.SetValue(passet);
          }
          return;
        }
      }

      if (pname == "none") {
        mIoDriver.SetValue((ork::Object*)0);
      } else if (pname == "reload") {
        const ork::Object* poldobjectc;
        mIoDriver.GetValue(poldobjectc);

        ork::Object* poldobject = const_cast<ork::Object*>(poldobjectc);
        asset::Asset* poldasset = rtti::autocast(poldobject);

        if (poldasset) {
          lev2::GfxEnv::GetRef().GetGlobalLock().Lock();
          {
            // poldasset- Load(true);
            // = DynAssetManager::GetRef().LoadManaged( anno.c_str(),
            // passet->GetAssetName().c_str(), true );
          }
          lev2::GfxEnv::GetRef().GetGlobalLock().UnLock();
          mIoDriver.SetValue(poldasset);
        }
      } else {
        asset::Asset* passet = 0;
        // dont worry, if pname doesnt start with "data://", DynAssetManager
        // will prepend it
        passet = passetclass->DeclareAsset(pname.c_str());
        if (passet) {
          mIoDriver.SetValue(passet);
        }
        lev2::GfxEnv::GetRef().GetGlobalLock().Lock();
        {
          if (passet) {
            passet->Load();
          }
        }
        lev2::GfxEnv::GetRef().GetGlobalLock().UnLock();
        OrkAssert(passet);
        // mModel.SigNewObject(passet);
      }
      // SetLabel();
    }
  }
}

///////////////////////////////////////////////////////////////////////////////

template <typename IODriver> void GedAssetNode<IODriver>::SetLabel() {
  const ork::Object* pobject = 0;
  mIoDriver.GetValue(pobject);
  const asset::Asset* passet = rtti::autocast(pobject);

  ConstString anno = GetOrkProp()->GetAnnotation("editor.assettype");

  printf("passet<%p> nam<%s>\n", passet, (passet != 0) ? passet->GetName().c_str() : nullptr);

  if (passet) {
    _content = ork::CreateFormattedString("%s<%s>", anno.c_str(), passet->GetName().c_str());
  } else {
    _content = ork::CreateFormattedString("%s<none>", anno.c_str());
  }
}

///////////////////////////////////////////////////////////////////////////////

template <typename IODriver>
GedAssetNode<IODriver>::GedAssetNode(ObjModel& mdl, const char* name, const reflect::IObjectProperty* prop, Object* obj)
    : GedItemNode(mdl, name, prop, obj)
    , mIoDriver(mdl, prop, obj) {
  mdl.GetGedWidget()->PushItemNode(this);
  mdl.GetGedWidget()->PopItemNode(this);
}

///////////////////////////////////////////////////////////////////////////////
}}} // namespace ork::tool::ged
