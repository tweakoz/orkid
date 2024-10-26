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

ImplementReflectionX(ork::lev2::ged::GedMapNode, "GedMapNode");

////////////////////////////////////////////////////////////////
namespace ork::lev2::ged {
////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
KeyDecoName::KeyDecoName(const char* pkey) // precomposed name/index
{
  ArrayString<256> tempstr(pkey);
  const char* pfindcolon = strstr(tempstr.c_str(), ":");
  if (pfindcolon) {
    size_t ilen           = strlen(tempstr.c_str());
    int ipos              = (pfindcolon - tempstr.c_str());
    const char* pindexstr = pfindcolon + 1;

    size_t inumlen = ilen - ipos;

    if (inumlen) {
      miMultiIndex = atoi(pindexstr);
      char* pchar  = tempstr.begin() + ipos;
      pchar[0]     = 0;
      mActualKey.set(tempstr.c_str());
    }
  } else {
    mActualKey.set(pkey);
    miMultiIndex = 0;
  }
}
///////////////////////////////////////////////////////////////////////////////
KeyDecoName::KeyDecoName(const char* pkey, int index) // decomposed name/index
    : mActualKey(pkey)
    , miMultiIndex(index) {
}
///////////////////////////////////////////////////////////////////////////////
PropTypeString KeyDecoName::DecoratedName() const {
  PropTypeString rval;
  rval.format("%s:%d", mActualKey.c_str(), miMultiIndex);
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

void GedMapNode::describeX(class_t* clazz) {
}

///////////////////////////////////////////////////////////////////////////////

struct MapIoDriverImpl{
  GedMapNode* _node = nullptr;
  const reflect::IMap* _map_prop = nullptr;
  object_ptr_t _obj = nullptr;
};
using mapiodriverimpl_ptr_t = std::shared_ptr<MapIoDriverImpl>;

///////////////////////////////////////////////////////////////////////////////

GedMapNode::GedMapNode(
    GedContainer* c,               //
    const char* name,              //
    const reflect::IMap* map_prop, //
    object_ptr_t obj)
    : GedItemNode(c, name, map_prop, obj)
    , mMapProp(map_prop) { //

    ///////////////////////////////////////////
    PersistHashContext HashCtx;
    HashCtx._object   = _object;
    HashCtx._property = _property;
    auto pmap         = _container->_model->persistMapForHash(HashCtx);
    ///////////////////////////////////////////
    mbSingle = pmap->typedValueWithDefault<bool>("single",true);
    mItemIndex = pmap->typedValueWithDefault<int>("index",0);
    ///////////////////////////////////////////


  c->PushItemNode(this);

  auto model = c->_model;

  auto enumerated_items = map_prop->enumerateElements(obj);

  auto key_comparator = [](const reflect::map_pair_t& a, const reflect::map_pair_t& b) -> bool { //
    auto& KA = a.first;
    auto& KB = b.first;

    if( auto k_as_str = KA.tryAs<std::string>() )
        return k_as_str.value() < KB.get<std::string>();
    else if( auto k_as_cstr = KA.tryAs<const char*>() )
        return strcmp(k_as_cstr.value(),KB.get<const char*>())<0;
    else if( auto k_as_int = KA.tryAs<int>() )
        return k_as_int.value() < KB.get<int>();
    else
      OrkAssert(false);
    return false;
  };

  std::sort(enumerated_items.begin(), enumerated_items.end(), key_comparator );

  for (auto e : enumerated_items) {
    auto key = e.first;

    std::string keyname;
    if( auto k_as_str = key.tryAs<std::string>() ){
      keyname = k_as_str.value();
      KeyDecoName kdeca(keyname.c_str());
      addKey(kdeca);
    }
    else if( auto k_as_cstr = key.tryAs<const char*>() ){
      keyname = k_as_cstr.value();
      KeyDecoName kdeca(keyname.c_str());
      addKey(kdeca);
    }
    else if( auto k_as_int = key.tryAs<int>() ){
      keyname = FormatString("%d", k_as_int.value() );
      KeyDecoName kdeca(keyname.c_str());
      addKey(kdeca);
    }
    else
      OrkAssert(false);

    printf( "GedMapNode<%p> keyname<%s>\n", this, keyname.c_str() );

    auto iodriver = std::make_shared<NewIoDriver>();
    auto ioimpl = iodriver->_impl.makeShared<MapIoDriverImpl>();
    ioimpl->_node = this;
    ioimpl->_map_prop = map_prop;
    iodriver->_par_prop = map_prop;
    iodriver->_object = obj;
    iodriver->_abstract_val = e.second;
    iodriver->_onValueChanged = [=](){
      map_prop->setElement( obj, key, iodriver->_abstract_val );
      c->_model->enqueueUpdate();
    };
    auto item_node = model->createAbstractNode(keyname.c_str(), iodriver );

  }

  c->PopItemNode(this);

  updateVisibility();
}

void GedMapNode::focusItem(const PropTypeString& key) {
}

bool GedMapNode::isKeyPresent(const KeyDecoName& pkey) const {
  PropTypeString deco = pkey.DecoratedName();
  bool bret           = (mMapKeys.find(deco) != mMapKeys.end());
  return bret;
}
void GedMapNode::addKey(const KeyDecoName& pkey) {
  mMapKeys.insert(std::make_pair(pkey.DecoratedName(), pkey));
}

////////////////////////////////////////////////////////////////

void GedMapNode::addItem(ui::event_constptr_t ev) {
  const int klabh = get_charh();
  const int kdim  = klabh - 2;

  int ibasex = (kdim + 4) * 2 + 3;
  int sx = ev->miScreenPosX;
  int sy = ev->miScreenPosY;

  int W = miW - ibasex - 6;
  int H = klabh*2;

  size_t num_items = mMapKeys.size();
  std::string initial_key = FormatString("item-%d", num_items);

  std::string edittext = ui::popupLineEdit(_l2context(),sx,sy,W,H,initial_key);

  if (edittext.length()) {
    KeyDecoName kdeca(edittext.c_str());

    if (isKeyPresent(kdeca)) {
      OrkAssert(false);
    }

    reflect::map_abstract_item_t reflect_key = edittext;

    addKey(kdeca);
    mMapProp->insertDefaultElement(_object, reflect_key);
    _container->_model->enqueueUpdateAll();

  }
}

////////////////////////////////////////////////////////////////

void GedMapNode::removeItem(ui::event_constptr_t ev) {
  const int klabh = get_charh();
  const int kdim  = klabh - 2;

  int ibasex = (kdim + 4) * 2 + 3;
  int sx = ev->miScreenPosX;
  int sy = ev->miScreenPosY;

  int W = miW - ibasex - 6;
  int H = klabh*2;

  size_t num_items = mMapKeys.size();
  std::vector<std::string> choice_names;
  choice_names.push_back("--cancel--");
  for( auto item : mMapKeys ){
    choice_names.push_back( item.second.mActualKey.c_str() );
  }

  auto dimensions = ui::ChoiceList::computeDimensions(choice_names);
  std::string choice = ui::popupChoiceList( _l2context(), //
                                            sx,sy,
                                            choice_names,
                                            dimensions);
  if (choice!="--cancel--") {

    reflect::map_abstract_item_t reflect_key = choice;
    mMapProp->removeElement(_object, reflect_key);
    _container->_model->enqueueUpdateAll();

  }

}

////////////////////////////////////////////////////////////////

void GedMapNode::moveItem(ui::event_constptr_t ev) {
  /*
  const int klabh = get_charh();
  const int kdim  = klabh - 2;

  int ibasex = (kdim + 4) * 3 + 3;

  QString qstra = GedInputDialog::getText(ev, this, 0, ibasex, 0, miW - ibasex - 6, klabh);

  ork::msleep(100);
  QString qstrb = GedInputDialog::getText(ev, this, 0, ibasex, 0, miW - ibasex - 6, klabh);

  std::string sstra = qstra.toStdString();
  std::string sstrb = qstrb.toStdString();

  if (sstra.length() && sstrb.length()) {
    KeyDecoName kdeca(sstra.c_str());
    KeyDecoName kdecb(sstrb.c_str());

    if (IsKeyPresent(kdecb)) {
      if (false == IsMultiMap())
        return;
    }
    if (IsKeyPresent(kdeca)) {
      mModel.SigPreNewObject();

      GedMapIoDriver iodriver(mModel, mMapProp, GetOrkObj());
      iodriver.move(kdeca, sstrb.c_str());

      mModel.Attach(mModel.CurrentObject());
    }
  }*/
}

////////////////////////////////////////////////////////////////

void GedMapNode::duplicateItem(ui::event_constptr_t ev) {
  /*
  const int klabh = get_charh();
  const int kdim  = klabh - 2;

  int ibasex = (kdim + 4) * 3 + 3;

  QString qstra = GedInputDialog::getText(ev, this, 0, ibasex, 0, miW - ibasex - 6, klabh);

  ork::msleep(100);
  QString qstrb = GedInputDialog::getText(ev, this, 0, ibasex, 0, miW - ibasex - 6, klabh);

  std::string sstra = qstra.toStdString();
  std::string sstrb = qstrb.toStdString();

  if (sstra.length() && sstrb.length() && sstra != sstrb) {
    KeyDecoName kdeca(sstra.c_str());
    KeyDecoName kdecb(sstrb.c_str());

    if (IsKeyPresent(kdecb)) {
      if (false == IsMultiMap())
        return;
    }
    if (false == IsKeyPresent(kdeca)) {
      return;
    }

    mModel.SigPreNewObject();

    GedMapIoDriver iodriver(mModel, mMapProp, GetOrkObj());
    iodriver.duplicate(kdeca, sstrb.c_str());

    mModel.Attach(mModel.CurrentObject());
  }
  */
}

////////////////////////////////////////////////////////////////

void GedMapNode::importItem(ui::event_constptr_t ev) {
  /*
  const int klabh   = get_charh();
  const int kdim    = klabh - 2;
  int ibasex        = (kdim + 4) * 3 + 3;
  QString qstra     = GedInputDialog::getText(ev, this, 0, ibasex, 0, miW - ibasex - 6, klabh);
  std::string sstra = qstra.toStdString();
  if (sstra.length()) {
    KeyDecoName kdeca(sstra.c_str());
    if (IsKeyPresent(kdeca)) {
      if (false == IsMultiMap())
        return;
    }
    mModel.SigPreNewObject();
    GedMapIoDriver iodriver(mModel, mMapProp, GetOrkObj());
    iodriver.importfile(kdeca, sstra.c_str());
    mModel.Attach(mModel.CurrentObject());
  }
  */
}

////////////////////////////////////////////////////////////////

void GedMapNode::exportItem(ui::event_constptr_t ev) {
  /*
  const int klabh   = get_charh();
  const int kdim    = klabh - 2;
  int ibasex        = (kdim + 4) * 3 + 3;
  QString qstra     = GedInputDialog::getText(ev, this, 0, ibasex, 0, miW - ibasex - 6, klabh);
  std::string sstra = qstra.toStdString();
  if (sstra.length()) {
    KeyDecoName kdeca(sstra.c_str());
    if (IsKeyPresent(kdeca)) {
      GedMapIoDriver iodriver(mModel, mMapProp, GetOrkObj());
      iodriver.exportfile(kdeca, sstra.c_str());
      mModel.Attach(mModel.CurrentObject());
    }
  }
*/
}

////////////////////////////////////////////////////////////////

bool GedMapNode::OnMouseDoubleClicked(ui::event_constptr_t ev) {
  const int klabh = get_charh();
  const int kdim  = klabh - 2;
  // Qt::MouseButtons Buttons = pEV->buttons();
  // Qt::KeyboardModifiers modifiers = pEV->modifiers();

  int ix = ev->miX;
  int iy = ev->miY;

  auto model = _container->_model;
  PersistHashContext HashCtx;
  HashCtx._object   = _object;
  HashCtx._property = _property;
  auto pmap         = _container->_model->persistMapForHash(HashCtx);

  //printf("GedMapNode<%p> ilx<%d> ily<%d>\n", this, ix, iy);

  if (ix >= koff && ix <= kdim && iy >= koff && iy <= kdim) { // drop down

    mbSingle = !mbSingle;
    pmap->setTypedValue<bool>("single", mbSingle);
    updateVisibility();
    return true;
  }

  ///////////////////////////////////////

  if (not mbConst) {
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

  if (mbImpExp) { // Import Export

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
  std::vector<std::string> choices;
  std::map<std::string, int> choice_to_index;
  for (int it = 0; it < inumitems; it++) {
    auto pchild       = child(it);
    const char* pname = pchild->_propname.c_str();
    choices.push_back(pname);
    choice_to_index[pname] = it;
  }
  fvec2 dimensions   = ui::ChoiceList::computeDimensions(choices);
  int sx             = ev->miScreenPosX;
  int sy             = ev->miScreenPosY;
  std::string choice = ui::popupChoiceList(
      _l2context(), //
      sx - (int(dimensions.x) >> 1),
      sy - (int(dimensions.y) >> 1),
      choices,
      dimensions);
  printf("choice<%s>\n", choice.c_str());
  auto it_choice = choice_to_index.find(choice);
  OrkAssert(it_choice != choice_to_index.end());
  mItemIndex = it_choice->second;
  ///////////////////////////////////////////
  pmap->setTypedValue<int>("index", mItemIndex);
  ///////////////////////////////////////////
  updateVisibility();
  ///////////////////////////////////////
  return false;
}

////////////////////////////////////////////////////////////////

void GedMapNode::updateVisibility() {

  int inumitems = numChildren();

  if (mbSingle) {
    for (int it = 0; it < inumitems; it++) {
      child(it)->SetVisible(it == mItemIndex);
    }
  } else {
    for (int it = 0; it < inumitems; it++) {
      child(it)->SetVisible(true);
    }
  }
  _container->DoResize();
}

////////////////////////////////////////////////////////////////

void GedMapNode::DoDraw(Context* pTARG) {

  auto skin = _container->_activeSkin;

  const int klabh = get_charh();
  const int kdim  = 9;

  int inumind = mbConst ? 0 : 4;
  if (mbImpExp)
    inumind += 2;

  /////////////////
  // drop down box
  /////////////////

  int ioff = koff;
  int idim = (kdim);

  int dbx1 = miX + ioff;
  int dbx2 = dbx1 + idim;
  int dby1 = miY + ioff + 1;
  int dby2 = dby1 + idim;

  int labw = this->propnameWidth();

  int ity = get_text_center_y();

  skin->DrawBgBox(this, miX, miY, miW, miH, GedSkin::ESTYLE_BACKGROUND_1);
  skin->DrawOutlineBox(this, miX, miY, miW, miH, GedSkin::ESTYLE_DEFAULT_OUTLINE);

  skin->DrawBgBox(this, miX, miY, miW, klabh, GedSkin::ESTYLE_BACKGROUND_MAPNODE_LABEL);

  if (mbSingle) {
    skin->DrawRightArrow(this, dbx1+1, dby1, GedSkin::ESTYLE_BUTTON_OUTLINE);
  } else {
    skin->DrawDownArrow(this, dbx1+1, dby1, GedSkin::ESTYLE_BUTTON_OUTLINE);
  }


  dbx1 += (idim + 4);
  dbx2 = dbx1 + idim;

  if (mbConst == false) {
    int idimh = idim >> 1;

    // +
    skin->DrawOutlineBox(this, dbx1, dby1, idim, idim, GedSkin::ESTYLE_BUTTON_OUTLINE);
    skin->DrawLine(this, dbx1 + idimh, dby1, dbx1 + idimh, dby1 + idim, GedSkin::ESTYLE_BUTTON_OUTLINE);
    skin->DrawLine(this, dbx1, dby1 + idimh, dbx2, dby1 + idimh, GedSkin::ESTYLE_BUTTON_OUTLINE);

    dbx1 += (idim + 4);
    dbx2 = dbx1 + idim;

    // -
    skin->DrawOutlineBox(this, dbx1, dby1, idim, idim, GedSkin::ESTYLE_BUTTON_OUTLINE);
    skin->DrawLine(this, dbx1, dby1 + idimh, dbx2, dby1 + idimh, GedSkin::ESTYLE_BUTTON_OUTLINE);

    dbx1 += (idim + 4);
    dbx2     = dbx1 + idim;
    int dbxc = dbx1 + idimh;

    // Rename
    skin->DrawOutlineBox(this, dbx1, dby1, idim, idim, GedSkin::ESTYLE_BUTTON_OUTLINE);
    skin->DrawText(this, dbx1, dby1 - 3, "R");

    // Duplicate
    dbx1 += (idim + 4);
    dbx2 = dbx1 + idim;
    dbxc = dbx1 + idimh;
    skin->DrawOutlineBox(this, dbx1, dby1, idim, idim, GedSkin::ESTYLE_BUTTON_OUTLINE);
    skin->DrawText(this, dbx1, dby1 - 3, "D");

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
  skin->DrawText(this, dbx1, ity - 2, _propname.c_str());
}

////////////////////////////////////////////////////////////////

} // namespace ork::lev2::ged
