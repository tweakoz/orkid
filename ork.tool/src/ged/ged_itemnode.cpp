////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <orktool/qtui/qtui_tool.h>
///////////////////////////////////////////////////////////////////////////////
#include <orktool/qtui/qtmainwin.h>
#include <ork/reflect/IProperty.h>
#include <ork/reflect/properties/I.h>
#include <orktool/ged/ged.h>
#include <ork/lev2/gfx/dbgfontman.h>

INSTANTIATE_TRANSPARENT_RTTI(ork::tool::ged::GedItemNode, "ged.itemnode");
///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace tool { namespace ged {
//////////////////////////////////////////////////////////////////////////////
void GedItemNode::Describe() {
}
//////////////////////////////////////////////////////////////////////////////
GedItemNode::GedItemNode(ObjModel& mdl, const char* name, const reflect::I* prop, ork::Object* obj)
    : mModel(mdl)
    , mbcollapsed(false)
    , mRoot(mdl.GetGedWidget())
    , _propname(name)
    , mOrkProp(prop)
    , mOrkObj(obj)
    , miW(0)
    , miH(0)
    , mbVisible(true)
    , _parent(0)
    , mbInvalid(true) {

  int stack_depth = mdl.GetGedWidget()->GetStackDepth();

  if (false == (stack_depth == 0)) {
    GedItemNode* parent = mdl.GetGedWidget()->ParentItemNode();
  }

  Init();
}
void GedItemNode::SigInvalidateProperty() {
  mModel.SigPropertyInvalidated(GetOrkObj(), GetOrkProp());
}
//////////////////////////////////////////////////////////////////////////////
GedItemNode::~GedItemNode() {
  // orkprintf( "deleting OrkItemNode<%s>\n", this->_propname.c_str() );
  DestroyChildren();
}
//////////////////////////////////////////////////////////////////////////////
void GedItemNode::DestroyChildren() {
  int inumchildren = (int)mItems.size();

  for (int ic = 0; ic < inumchildren; ic++) {
    GedItemNode* child = mItems[ic];
    delete child;
  }
  mItems.clear();
}
//////////////////////////////////////////////////////////////////////////////
void GedItemNode::Init() {
  if (CanSideBySide() == false) {
  }

  GetGedWidget()->PushItemNode(this);
  GetGedWidget()->PopItemNode(this);
}
//////////////////////////////////////////////////////////////////////////////
int GedItemNode::CalcHeight(void) {
  int ih = get_charh() + 8;
  if (false == mbcollapsed) {
    int inum = GetNumItems();
    for (int i = 0; i < inum; i++) {
      bool bvis = GetItem(i)->IsVisible();

      if (bvis) {
        ih += GetItem(i)->CalcHeight();
      }
    }
  }
  micalch = ih;
  // printf( "GedItemNode<%p> CalcHeight() <%d>\n", this, ih );
  return ih;
}
//////////////////////////////////////////////////////////////////////////////
void GedItemNode::Layout(int ix, int iy, int iw, int ih) {
  miX = ix;
  miY = iy;
  miW = iw;
  miH = ih;

  // printf( "GedItemNode<%p> ix<%d> iy<%d> iw<%d> ih<%d>\n", this, ix, iy, iw, ih );

  bool bsidebyside = CanSideBySide();

  int inx = ix;

  if (bsidebyside) {
    inx += get_charw() + 4 + (propnameWidth() + 1);
  } else {
    iy += get_charh();
  }

  int inumitems = (int)mItems.size();

  for (int i = 0; i < inumitems; i++) {
    bool bvis = GetItem(i)->IsVisible();

    if (bvis) {
      int h = mItems[i]->micalch;
      mItems[i]->Layout(ix + 4, iy + 2, iw - 8, h - 4);
      iy += h;
    }
  }
}
int GedItemNode::get_charh() const {
  return GetSkin()->char_h();
}
int GedItemNode::get_charw() const {
  return GetSkin()->char_w();
}

int GedItemNode::get_text_center_y() const {
  int ich = get_charh();
  int iwd = ich >> 3; //(miH-ich)>>1;
  int ity = miY + iwd;
  return ity;
}
//////////////////////////////////////////////////////////////////////////////
void GedItemNode::AddItem(GedItemNode* w) {
  w->SetDecoIndex(int(mItems.size()));
  mItems.push_back(w);
  w->_parent = this;
}
//////////////////////////////////////////////////////////////////////////////
GedItemNode* GedItemNode::GetItem(int idx) const {
  return mItems[idx];
}
//////////////////////////////////////////////////////////////////////////////
int GedItemNode::GetNumItems() const {
  return int(mItems.size());
}
///////////////////////////////////////////////////////////////////////////////
void GedLabelNode::DoDraw(lev2::Context* pTARG) {
}
///////////////////////////////////////////////////////////////////////////////
bool GedItemNode::DoDrawDefault() const {
  return true;
}
///////////////////////////////////////////////////////////////////////////////
void GedItemNode::Draw(lev2::Context* pTARG) {
  if (mbInvalid) {
    ReSync();
  }
  if (DoDrawDefault()) {
    int labw = this->propnameWidth();

    GetSkin()->DrawBgBox(this, miX, miY, miW, miH, GedSkin::ESTYLE_BACKGROUND_1);
    GetSkin()->DrawOutlineBox(this, miX, miY, miW, miH, GedSkin::ESTYLE_DEFAULT_OUTLINE);
  }
  DoDraw(pTARG);

  mbInvalid = false;
}
///////////////////////////////////////////////////////////////////////////////
int GedItemNode::propnameWidth() const {
  int istrw                   = (int)strlen(_propname.c_str());
  const lev2::FontDesc& fdesc = lev2::FontMan::GetRef().currentFont()->GetFontDesc();
  int ilabw                   = fdesc.stringWidth(istrw);
  return ilabw;
}
///////////////////////////////////////////////////////////////////////////////
int GedItemNode::contentWidth() const {
  size_t istrw                = _content.length();
  const lev2::FontDesc& fdesc = lev2::FontMan::GetRef().currentFont()->GetFontDesc();
  int ilabw                   = fdesc.stringWidth(istrw);
  return ilabw;
}
///////////////////////////////////////////////////////////////////////////////

void GedRootNode::DoDraw(lev2::Context* pTARG) {
  int ih = (micalch > miH) ? micalch : miH;
  GetSkin()->DrawBgBox(this, miX, miY, miW, ih, GedSkin::ESTYLE_BACKGROUND_1);
  GetSkin()->DrawOutlineBox(this, miX, miY, miW, ih, GedSkin::ESTYLE_DEFAULT_OUTLINE);
}

GedRootNode::GedRootNode(ObjModel& mdl, const char* name, const reflect::I* prop, ork::Object* obj)
    : GedItemNode(mdl, name, prop, obj) {
}

void GedRootNode::Layout(int ix, int iy, int iw, int ih) {
  miX = ix;
  miY = iy;
  miW = iw;
  miH = ih;

  int inx = ix;

  iy += 2;
  ix += 2;

  int inumitems = (int)mItems.size();

  for (int i = 0; i < inumitems; i++) {
    bool bvis = GetItem(i)->IsVisible();

    if (bvis) {
      int h = mItems[i]->micalch;
      mItems[i]->Layout(ix + 3, iy + 2, iw - 6, h - 4);
      iy += h;
    }
  }
}
int GedRootNode::CalcHeight(void) {
  int ih = 2;
  if (false == mbcollapsed) {
    int inum = GetNumItems();
    for (int i = 0; i < inum; i++) {
      bool bvis = GetItem(i)->IsVisible();

      if (bvis) {
        ih += GetItem(i)->CalcHeight();
      }
    }
  }
  micalch = ih;
  return ih;
}
///////////////////////////////////////////////////////////////////////////////

bool GedItemNode::IsObjectHilighted(const GedObject* pobj) const {
  GedWidget* pwidg        = GetGedWidget();
  GedSurface* pvp              = pwidg->GetViewport();
  const GedObject* pmoobj = pvp->GetMouseOverNode();
  return (pmoobj == pobj);
}

GedSkin* GedItemNode::GetSkin() const {
  return mRoot->GetSkin();
}

///////////////////////////////////////////////////////////////////////////////
}}} // namespace ork::tool::ged
///////////////////////////////////////////////////////////////////////////////
