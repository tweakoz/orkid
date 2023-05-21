////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/lev2/ui/ged/ged.h>
#include <ork/lev2/ui/ged/ged_node.h>
#include <ork/lev2/ui/ged/ged_skin.h>
#include <ork/lev2/ui/ged/ged_surface.h>
#include <ork/lev2/ui/ged/ged_container.h>
#include <ork/lev2/gfx/dbgfontman.h>
#include <ork/kernel/core_interface.h>
#include <ork/reflect/properties/registerX.inl>

ImplementReflectionX(ork::lev2::ged::GedObject, "GedObject");
ImplementReflectionX(ork::lev2::ged::GedItemNode, "GedItemNode");

////////////////////////////////////////////////////////////////
namespace ork::lev2::ged {
////////////////////////////////////////////////////////////////

void GedObject::describeX(class_t* clazz) {
}

////////////////////////////////////////////////////////////////

bool GedObject::OnUiEvent(ork::ui::event_constptr_t ev) {
  switch (ev->_eventcode) {
    case ui::EventCode::DRAG:
      return OnMouseDragged(ev);
      break;
    case ui::EventCode::MOVE:
      return OnMouseMoved(ev);
      break;
    case ui::EventCode::DOUBLECLICK:
      return OnMouseDoubleClicked(ev);
      break;
    case ui::EventCode::RELEASE:
      return OnMouseReleased(ev);
      break;
    default:
      break;
  }
  return false;
}

////////////////////////////////////////////////////////////////

void GedItemNode::describeX(class_t* clazz) {
}

////////////////////////////////////////////////////////////////

GedItemNode::GedItemNode(
    GedContainer* container,             //
    const char* name,                    //
    const reflect::ObjectProperty* prop, //
    object_ptr_t obj)                    //
    : _container(container)
    , mbcollapsed(false)
    , _propname(name)
    , _property(prop)
    , _object(obj) {

  int stack_depth = _container->GetStackDepth();

  if (not(stack_depth == 0)) {
    // GedItemNode* parent = _container->ParentItemNode();
  }

  Init();
}

////////////////////////////////////////////////////////////////

GedItemNode::GedItemNode(
    GedContainer* container,             //
    const char* name,                    //
    newiodriver_ptr_t iodriver)                    //
    : GedItemNode(container,name,iodriver->_par_prop,iodriver->_object) {
    _iodriver = iodriver;
}

////////////////////////////////////////////////////////////////

GedItemNode::~GedItemNode() {
}

//////////////////////////////////////////////////////////////////////////////

void GedItemNode::SetVisible(bool bv) {
  mbVisible = bv;
}
bool GedItemNode::IsVisible() const {
  return mbVisible;
}

void GedItemNode::SetXY(int ix, int iy) {
  miX = ix;
  miY = iy;
}
void GedItemNode::SetWH(int iw, int ih) {
  miW = iw;
  miH = ih;
}
int GedItemNode::GetX() const {
  return miX;
}
int GedItemNode::GetY() const {
  return miY;
}
int GedItemNode::height() const {
  return micalch;
}
int GedItemNode::width() const {
  return miW;
}
bool GedItemNode::CanSideBySide() const {
  return false;
}
void GedItemNode::Invalidate() {
  mbInvalid = true;
}
void GedItemNode::ReSync() {
}
///////////////////////////////////////////////////
void GedItemNode::onActiGedObjectvate() {
}
void GedItemNode::onDeactivate() {
}
int GedItemNode::contentCenterX() const {
  return miX + (miW >> 1) - (contentWidth() >> 1);
}
int GedItemNode::propnameCenterX() const {
  return miX + (miW >> 1) - (propnameWidth() >> 1);
}

//////////////////////////////////////////////////////////////////////////////

void GedItemNode::Init() {
  if (CanSideBySide() == false) {
  }

  _container->PushItemNode(this);
  _container->PopItemNode(this);
}

//////////////////////////////////////////////////////////////////////////////

lev2::Context* GedItemNode::_l2context() const{
  return lev2::contextForCurrentThread();
}

//////////////////////////////////////////////////////////////////////////////

int GedItemNode::computeHeight() const {
  micalch = doComputeHeight();
  return micalch;
}

//////////////////////////////////////////////////////////////////////////////

int GedItemNode::doComputeHeight() const {
  int ih = get_charh() + 8;
  if (false == mbcollapsed) {
    int inum = numChildren();
    for (int i = 0; i < inum; i++) {
      auto child = _children[i];
      if (child->IsVisible()) {
        ih += child->computeHeight();
      }
    }
  }
  return ih;
}

///////////////////////////////////////////////////////////////////////////////

bool GedItemNode::IsObjectHilighted(const GedObject* pobj) const {
  auto pvp = _container->_viewport;
  const GedObject* pmoobj = pvp->GetMouseOverNode();
  return (pmoobj == pobj);
}

//////////////////////////////////////////////////////////////////////////////

void GedItemNode::Layout(int ix, int iy, int iw, int ih) {

  miX = ix;
  miY = iy;
  miW = iw;
  miH = ih;

  OrkAssert(_container != nullptr);
  auto skin = _container->_activeSkin;

  if(0)
    printf("GedItemNode<%p:%s> Layout ix<%d> iy<%d> iw<%d> ih<%d>\n", this, this->_propname.c_str(), ix, iy, iw, ih);

  bool bsidebyside = CanSideBySide();

  int inx = ix;

  if (bsidebyside) {
    inx += get_charw() + 4 + (propnameWidth() + 1);
  } else {
    iy += skin ? skin->_bannerHeight + 1 : get_charh() + 4; 
  }

  int inumitems = numChildren();

  for (int i = 0; i < inumitems; i++) {
    bool bvis = _children[i]->IsVisible();


    if (bvis) {
      int h = _children[i]->micalch;
      _children[i]->Layout(ix + 4, iy + 2, iw - 8, h - 4);
      iy += h;
    }
  }
}

////////////////////////////////////////////////////////////////

int GedItemNode::get_charh() const {
  auto skin = _container->_activeSkin;
  return skin ? skin->char_h() : 8;
}

////////////////////////////////////////////////////////////////

int GedItemNode::get_charw() const {
  auto skin = _container->_activeSkin;
  return skin ? skin->char_w() : 8;
}

////////////////////////////////////////////////////////////////

int GedItemNode::get_text_center_y() const {
  int ich = get_charh();
  int iwd = ich >> 3; //(miH-ich)>>1;
  int ity = miY + iwd;
  return ity;
}

//////////////////////////////////////////////////////////////////////////////
void GedItemNode::addChild(geditemnode_ptr_t w) {
  w->SetDecoIndex(int(_children.size()));
  _children.push_back(w);
  w->_parent = this;
}
//////////////////////////////////////////////////////////////////////////////
int GedItemNode::numChildren() const {
  return int(_children.size());
}
//////////////////////////////////////////////////////////////////////////////
geditemnode_ptr_t GedItemNode::child(int idx) const {
  return _children[idx];
}
///////////////////////////////////////////////////////////////////////////////
int GedItemNode::propnameWidth() const {
  auto skin = _container->_activeSkin;
  int istrw                   = (int)strlen(_propname.c_str());
  const lev2::FontDesc& fdesc = skin->_font->GetFontDesc();
  int ilabw                   = fdesc.stringWidth(istrw);
  return ilabw;
}
///////////////////////////////////////////////////////////////////////////////
int GedItemNode::contentWidth() const {
  auto skin = _container->_activeSkin;
  size_t istrw                = _content.length();
  const lev2::FontDesc& fdesc = skin->_font->GetFontDesc();
  int ilabw                   = fdesc.stringWidth(istrw);
  return ilabw;
}
///////////////////////////////////////////////////////////////////////////////
void GedItemNode::Draw(lev2::Context* pTARG) {
  auto skin = _container->_activeSkin;

  if (mbInvalid) {
    ReSync();
  }
  if (_doDrawDefault) {
    int labw = this->propnameWidth();

    skin->DrawBgBox(this, miX, miY, miW, miH, GedSkin::ESTYLE_BACKGROUND_1);
    //skin->DrawOutlineBox(this, miX, miY, miW, miH, GedSkin::ESTYLE_DEFAULT_OUTLINE);
  }
  DoDraw(pTARG);

  mbInvalid = false;
}

////////////////////////////////////////////////////////////////
} // namespace ork::lev2::ged
