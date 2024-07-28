////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/lev2/ui/context.h>
#include <ork/lev2/ui/popups.inl>
#include <ork/lev2/ui/ged/ged.h>
#include <ork/lev2/ui/ged/ged_node.h>
#include <ork/lev2/ui/ged/ged_skin.h>
#include <ork/lev2/ui/ged/ged_container.h>
#include <ork/kernel/core_interface.h>
#include <ork/lev2/gfx/dbgfontman.h>
#include <ork/reflect/properties/registerX.inl>
#include <ork/reflect/editorsupport/std_annotations.inl>

ImplementReflectionX(ork::lev2::ged::GedArrayNode, "GedArrayNode");

////////////////////////////////////////////////////////////////
namespace ork::lev2::ged {
////////////////////////////////////////////////////////////////

void GedArrayNode::describeX(class_t* clazz) {
}

///////////////////////////////////////////////////////////////////////////////

struct ArrayIoDriverImpl {
  GedArrayNode* _node                = nullptr;
  const reflect::IArray* _array_prop = nullptr;
  object_ptr_t _obj                  = nullptr;
  int _index                         = -1;
};
using arrayiodriverimpl_ptr_t = std::shared_ptr<ArrayIoDriverImpl>;

///////////////////////////////////////////////////////////////////////////////

GedArrayNode::GedArrayNode(
    GedContainer* c,                 //
    const char* name,                //
    const reflect::IArray* ary_prop, //
    object_ptr_t obj)
    : GedItemNode(c, name, ary_prop, obj)
    , _arrayProperty(ary_prop) { //

  c->PushItemNode(this);

  auto model            = c->_model;
  auto enumerated_items = ary_prop->enumerateElements(obj);
  int index             = 0;

  if (0)
    printf("!!! GedArrayNode<%p> numitems<%zu>\n", this, enumerated_items.size());

  for (auto e : enumerated_items) {

    if (0)
      printf("!!! GedArrayNode<%p> item<%d>\n", this, index);

    auto iodriver             = std::make_shared<NewIoDriver>();
    auto ioimpl               = iodriver->_impl.makeShared<ArrayIoDriverImpl>();
    ioimpl->_node             = this;
    ioimpl->_array_prop       = ary_prop;
    ioimpl->_index            = index++;
    iodriver->_par_prop       = ary_prop;
    iodriver->_object         = obj;
    iodriver->_abstract_val   = e;
    iodriver->_onValueChanged = [=]() {
      // ary_prop->setElement(obj, key, iodriver->_abstract_val);
      c->_model->enqueueUpdate();
    };
    auto itemstr = FormatString("%d", index - 1);
    if (e.isA<object_ptr_t>()) {
      auto clazz     = e.get<object_ptr_t>()->GetClass();
      auto try_namer = clazz->annotationTyped<reflect::obj_to_string_fn_t>("editor.ged.item.namer");
      if (try_namer) {
        itemstr = try_namer.value()(e.get<object_ptr_t>());
      }
    }
    auto item_node = model->createAbstractNode(itemstr.c_str(), iodriver);
  }

  c->PopItemNode(this);
}

////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////

bool GedArrayNode::OnMouseDoubleClicked(ui::event_constptr_t ev) {
  const int klabh = get_charh();
  const int kdim  = klabh - 2;

  int ix = ev->miX;
  int iy = ev->miY;

  auto model = _container->_model;

#if 0

//printf("GedArrayNode<%p> ilx<%d> ily<%d>\n", this, ix, iy);

if (ix >= koff && ix <= kdim && iy >= koff && iy <= kdim) // drop down
{
  _isSingle = !_isSingle;

  ///////////////////////////////////////////
  PersistHashContext HashCtx;
  HashCtx._object   = _object;
  HashCtx._property = _property;
  auto pmap         = _container->_model->persistMapForHash(HashCtx);
  ///////////////////////////////////////////

  pmap->setValue("single", _isSingle ? "true" : "false");

  updateVisibility();
  return true;
}

///////////////////////////////////////

if (mbConst == false) {
  ix -= (kdim + 4);
  if( (ix >= koff) and (ix <= kdim) and (iy >= koff) and (iy <= kdim)) { // add item
    addItem(ev);
    // model.Attach(model.CurrentObject());
    printf("MAPADDITEM\n");
    model->enqueueUpdate();
    return true;
  }
  ix -= (kdim + 4);
  if( (ix >= koff) and (ix <= kdim) and (iy >= koff) and (iy <= kdim)) { // remove item
    removeItem(ev);
    // model->Attach(model->CurrentObject());
    model->enqueueUpdate();
    return true;
  }
  ix -= (kdim + 4);
  if( (ix >= koff) and (ix <= kdim) and (iy >= koff) and (iy <= kdim)) { // rename Item
    moveItem(ev);
    // model->Attach(model->CurrentObject());
    model->enqueueUpdate();
    return true;
  }
  ix -= (kdim + 4);
  if( (ix >= koff) and (ix <= kdim) and (iy >= koff) and (iy <= kdim)) { // duplicate Item
    duplicateItem(ev);
    // model->Attach(model->CurrentObject());
    model->enqueueUpdate();
    return true;
  }
}

///////////////////////////////////////

if (mbImpExp) // Import Export
{
  ix -= (kdim + 4);
  if (ix >= koff && ix <= kdim && iy >= koff && iy <= kdim) // drop down
  {
    importItem(ev);
    // model->Attach(model->CurrentObject());
    model->enqueueUpdate();
    return true;
  }
  ix -= (kdim + 4);
  if (ix >= koff && ix <= kdim && iy >= koff && iy <= kdim) // drop down
  {
    exportItem(ev);
    // model->Attach(model->CurrentObject());
    model->enqueueUpdate();
    return true;
  }
}

///////////////////////////////////////

int inumitems = numChildren();

// QMenu* pmenu = new QMenu(0);

for (int it = 0; it < inumitems; it++) {
  auto pchild       = child(it);
  const char* pname = pchild->_propname.c_str();
  // QAction* pchildact  = pmenu->addAction(pname);
  // QString qstr(CreateFormattedString("%d", it).c_str());
  // QVariant UserData(qstr);
  // pchildact->setData(UserData);
}
/*QAction* pact = pmenu->exec(QCursor::pos());
if (pact) {
  QVariant UserData = pact->data();
  std::string pname = UserData.toString().toStdString();
  int index         = 0;
  sscanf(pname.c_str(), "%d", &index);
  _selectedItemIndex = index;
  ///////////////////////////////////////////
  PersistHashContext HashCtx;
  HashCtx.mObject     = GetOrkObj();
  HashCtx.mProperty   = GetOrkProp();
  PersistantMap* pmap = model->GetPersistMap(HashCtx);
  ///////////////////////////////////////////
  pmap->SetValue("index", CreateFormattedString("%d", _selectedItemIndex));
  updateVisibility();
}*/

#endif
  ///////////////////////////////////////
  return false;
}

////////////////////////////////////////////////////////////////

void GedArrayNode::updateVisibility() {

  int inumitems = numChildren();

  if (_isSingle) {
    for (int it = 0; it < inumitems; it++) {
      child(it)->SetVisible(it == _selectedItemIndex);
    }
  } else {
    for (int it = 0; it < inumitems; it++) {
      child(it)->SetVisible(true);
    }
  }
  _container->DoResize();
}

////////////////////////////////////////////////////////////////

void GedArrayNode::DoDraw(Context* pTARG) {
  auto skin = _container->_activeSkin;

  const int klabh = get_charh();
  const int kdim  = klabh - 2;

  int inumind = _isConst ? 0 : 4;
  // if (mbImpExp)
  // inumind += 2;

  /////////////////
  // drop down box
  /////////////////

  int ioff = koff;
  int idim = (kdim);

  int dbx1 = miX + ioff;
  int dbx2 = dbx1 + idim;
  int dby1 = miY + ioff;
  int dby2 = dby1 + idim;

  int labw = this->propnameWidth();

  int ity = get_text_center_y();

  skin->DrawBgBox(this, miX, miY, miW, miH, GedSkin::ESTYLE_BACKGROUND_1);
  skin->DrawOutlineBox(this, miX, miY, miW, miH, GedSkin::ESTYLE_DEFAULT_OUTLINE);

  skin->DrawBgBox(this, miX, miY, miW, klabh, GedSkin::ESTYLE_BACKGROUND_MAPNODE_LABEL);
  /*

if (_isSingle) {
  skin->DrawRightArrow(this, dbx1, dby1, idim, idim, GedSkin::ESTYLE_BUTTON_OUTLINE);
  skin->DrawLine(this, dbx1 + 1, dby1, dbx1 + 1, dby2, GedSkin::ESTYLE_BUTTON_OUTLINE);
} else {
  skin->DrawDownArrow(this, dbx1, dby1, idim, idim, GedSkin::ESTYLE_BUTTON_OUTLINE);
  skin->DrawLine(this, dbx1, dby1 + 1, dbx2, dby1 + 1, GedSkin::ESTYLE_BUTTON_OUTLINE);
}

dbx1 += (idim + 4);
dbx2 = dbx1 + idim;

if (mbConst == false) {
  int idimh = idim >> 1;

  skin->DrawOutlineBox(this, dbx1, dby1, idim, idim, GedSkin::ESTYLE_BUTTON_OUTLINE);
  skin->DrawLine(this, dbx1 + idimh, dby1, dbx1 + idimh, dby1 + idim, GedSkin::ESTYLE_BUTTON_OUTLINE);
  skin->DrawLine(this, dbx1, dby1 + idimh, dbx2, dby1 + idimh, GedSkin::ESTYLE_BUTTON_OUTLINE);

  dbx1 += (idim + 4);
  dbx2 = dbx1 + idim;

  skin->DrawOutlineBox(this, dbx1, dby1, idim, idim, GedSkin::ESTYLE_BUTTON_OUTLINE);
  skin->DrawLine(this, dbx1, dby1 + idimh, dbx2, dby1 + idimh, GedSkin::ESTYLE_BUTTON_OUTLINE);

  dbx1 += (idim + 4);
  dbx2     = dbx1 + idim;
  int dbxc = dbx1 + idimh;

  skin->DrawOutlineBox(this, dbx1, dby1, idim, idim, GedSkin::ESTYLE_BUTTON_OUTLINE);
  skin->DrawText(this, dbx1, dby1 - 1, "R");

  dbx1 += (idim + 4);
  dbx2 = dbx1 + idim;
  dbxc = dbx1 + idimh;
  skin->DrawOutlineBox(this, dbx1, dby1, idim, idim, GedSkin::ESTYLE_BUTTON_OUTLINE);
  skin->DrawText(this, dbx1, dby1 - 1, "D");

  dbx1 += (idim + 4);
  dbx2 = dbx1 + idim;
}

if (mbImpExp) {
  int idimh = idim >> 1;

  skin->DrawOutlineBox(this, dbx1, dby1, idim, idim, GedSkin::ESTYLE_BUTTON_OUTLINE);
  skin->DrawText(this, dbx1, dby1 - 1, "I");

  dbx1 += (idim + 4);
  dbx2     = dbx1 + idim;
  int dbxc = dbx1 + idimh;

  skin->DrawOutlineBox(this, dbx1, dby1, idim, idim, GedSkin::ESTYLE_BUTTON_OUTLINE);
  skin->DrawText(this, dbx1, dby1 - 1, "O");

  dbx1 += (idim + 4);
  dbx2 = dbx1 + idim;
  // skin->DrawLine( this, dbx1, dby1, dbxc, dby1+idimh, GedSkin::ESTYLE_BUTTON_OUTLINE );
  // skin->DrawLine( this, dbxc, dby1+idimh, dbx2, dby1, GedSkin::ESTYLE_BUTTON_OUTLINE );
}
*/
  skin->DrawText(this, dbx1, ity - 2, _propname.c_str());
}

////////////////////////////////////////////////////////////////

} // namespace ork::lev2::ged
