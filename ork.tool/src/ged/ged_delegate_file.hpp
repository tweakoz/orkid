////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <QMenu>

namespace ork { namespace tool {
void FindAssetChoices(
    const file::Path& sdir, //
    util::choicelist_ptr_t choice_list,
    const std::string& wildcard);
}} // namespace ork::tool

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace tool { namespace ged {
///////////////////////////////////////////////////////////////////////////////
class UserFileChoices : public ork::util::ChoiceList {
  file::Path mBaseDir;
  std::string mWildCard;

public:
  virtual void EnumerateChoices(bool bforcenocache = false) {
    FindAssetChoices(mBaseDir, mWildCard);
  }

  UserFileChoices(const file::Path& basedir, std::string WildCard)
      : mBaseDir(basedir)
      , mWildCard(WildCard) {
  }
};
///////////////////////////////////////////////////////////////////////////////
template <typename IODriver> void GedFileNode<IODriver>::Describe() {
}
///////////////////////////////////////////////////////////////////////////////
template <typename IODriver> void GedFileNode<IODriver>::OnMouseDoubleClicked(ork::ui::event_constptr_t ev) {
  OnCreateObject();
}
///////////////////////////////////////////////////////////////////////////////
template <typename IODriver> void GedFileNode<IODriver>::DoDraw(lev2::Context* pTARG) {
  int inamw = propnameWidth() + 8;
  // int ilabw = contentWidth()+8;

  int ity = get_text_center_y();

  // GedItemNode::Draw( pTARG );
  const char* plab = _content.c_str();
  GetSkin()->DrawBgBox(this, miX + inamw, miY + 2, miW - inamw, miH - 4, GedSkin::ESTYLE_BACKGROUND_2);
  GetSkin()->DrawText(this, miX + inamw + 2, ity, plab);
  GetSkin()->DrawText(this, miX + 6, ity, _propname.c_str());
}
///////////////////////////////////////////////////////////////////////////////
template <typename IODriver> void GedFileNode<IODriver>::SetLabel() {
  file::Path pth;

  mIoDriver.GetValue(pth);

  if (pth.length() == 0) {
    _content = "none";
  } else {
    _content = pth.c_str();
  }
}
///////////////////////////////////////////////////////////////////////////////
template <typename IODriver>
GedFileNode<IODriver>::GedFileNode(ObjModel& mdl, const char* name, const reflect::ObjectProperty* prop, Object* obj)
    : GedItemNode(mdl, name, prop, obj)
    , mIoDriver(mdl, prop, obj) {
}
///////////////////////////////////////////////////////////////////////////////
template <typename IODriver> void GedFileNode<IODriver>::OnCreateObject() {
  ConstString anno = GetOrkProp()->GetAnnotation("editor.filetype");

  if (anno.length()) {
    std::string wildcard = CreateFormattedString("*.%s", anno.c_str());

    std::string urlbasetxt = "data://";

    ConstString anno_urlbase = GetOrkProp()->GetAnnotation("editor.filebase");

    if (anno_urlbase.length()) {
      urlbasetxt = anno_urlbase.c_str();
    }

    auto chclist = std::make_shared<UserFileChoices>(urlbasetxt.c_str(), wildcard);
    chclist->EnumerateChoices();

    QMenu qm;

    QAction* pact = qm.addAction("none");
    pact->setData(QVariant("none"));
    QAction* pact2 = qm.addAction("reload");
    pact2->setData(QVariant("reload"));

    QMenu* qm2 = qmenuFromChoiceList(chclist);
    qm.addMenu(qm2);

    pact = qm.exec(QCursor::pos());

    if (pact) {
      QVariant UserData = pact->data();
      std::string pname = UserData.toString().toStdString();

      file::Path apath(pname.c_str());

      apath.SetExtension(anno.c_str());

      if (pname == "none") {
        mModel.SigPreNewObject();
        mIoDriver.SetValue(file::Path(""));
        mModel.SigModelInvalidated();
      } else if (pname == "reload") {
        mModel.SigPreNewObject();
        // file::Path gpath;
        // objprop->Get(gpath, GetOrkObj() );
        mModel.SigModelInvalidated();
      } else {
        mModel.SigPreNewObject();
        mIoDriver.SetValue(apath);
        mModel.SigModelInvalidated();
      }
      SetLabel();
    }
  }
}
///////////////////////////////////////////////////////////////////////////////
}}} // namespace ork::tool::ged
///////////////////////////////////////////////////////////////////////////////
