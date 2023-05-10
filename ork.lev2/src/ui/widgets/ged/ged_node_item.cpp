////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/lev2/ui/ged/ged.h>
#include <ork/lev2/ui/ged/ged_node.h>
#include <ork/lev2/ui/ged/ged_skin.h>
#include <ork/lev2/ui/ged/ged_container.h>
#include <ork/lev2/gfx/dbgfontman.h>
#include <ork/kernel/core_interface.h>

////////////////////////////////////////////////////////////////
namespace ork::lev2::ged {
////////////////////////////////////////////////////////////////

void GedObject::OnUiEvent(ork::ui::event_constptr_t ev) {
  switch (ev->_eventcode) {
    case ui::EventCode::DRAG:
      //OnMouseDragged(ev);
      break;
    case ui::EventCode::MOVE:
      //OnMouseMoved(ev);
      break;
    case ui::EventCode::DOUBLECLICK:
      //OnMouseDoubleClicked(ev);
      break;
    case ui::EventCode::RELEASE:
      //OnMouseReleased(ev);
      break;
  }
}

////////////////////////////////////////////////////////////////

GedItemNode::GedItemNode(GedContainer* container, //
                         const char* name,  //
                         const reflect::ObjectProperty* prop,  //
                         object_ptr_t obj) //
    : _container(container)
    , mbcollapsed(false)
    , _propname(name)
    , _property(prop)
    , _object(obj)
    , miW(0)
    , miH(0)
    , mbVisible(true)
    , _parent(0)
    , mbInvalid(true) {

  int stack_depth = _container->GetStackDepth();

  if (not (stack_depth == 0)) {
    //GedItemNode* parent = _container->ParentItemNode();
  }

  Init();
}

////////////////////////////////////////////////////////////////

GedItemNode::~GedItemNode() {
}

//////////////////////////////////////////////////////////////////////////////

void GedItemNode::Init() {
  if (CanSideBySide() == false) {
  }

  _container->PushItemNode(this);
  _container->PopItemNode(this);
}

//////////////////////////////////////////////////////////////////////////////

int GedItemNode::CalcHeight(void) {
  int ih = get_charh() + 8;
  if (false == mbcollapsed) {
    int inum = numChildren();
    for (int i = 0; i < inum; i++) {
      bool bvis = _children[i]->IsVisible();

      if (bvis) {
        ih += _children[i]->CalcHeight();
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

   printf( "GedItemNode<%p> Layout ix<%d> iy<%d> iw<%d> ih<%d>\n", this, ix, iy, iw, ih );

  bool bsidebyside = CanSideBySide();

  int inx = ix;

  if (bsidebyside) {
    inx += get_charw() + 4 + (propnameWidth() + 1);
  } else {
    iy += get_charh();
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
geditemnode_ptr_t GedItemNode::child(int idx) const{
  return _children[idx];
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
void GedItemNode::Draw(lev2::Context* pTARG) {
  auto skin = _container->_activeSkin;

  if (mbInvalid) {
    ReSync();
  }
  if (_doDrawDefault) {
    int labw = this->propnameWidth();

    skin->DrawBgBox(this, miX, miY, miW, miH, GedSkin::ESTYLE_BACKGROUND_1);
    skin->DrawOutlineBox(this, miX, miY, miW, miH, GedSkin::ESTYLE_DEFAULT_OUTLINE);
  }
  DoDraw(pTARG);

  mbInvalid = false;
}

////////////////////////////////////////////////////////////////
} //namespace ork::lev2::ged {
