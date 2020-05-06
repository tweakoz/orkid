////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <orktool/qtui/qtui_tool.h>
///////////////////////////////////////////////////////////////////////////////
#include <orktool/ged/ged.h>
#include <orktool/ged/ged_delegate.h>
#include <orktool/ged/ged_io.h>
///////////////////////////////////////////////////////////////////////////////
#include <ork/reflect/IProperty.h>
#include <ork/reflect/IObjectProperty.h>
#include <ork/reflect/IObjectPropertyObject.h>
#include <ork/dataflow/dataflow.h>
#include <ork/file/file.h>
#include <ork/stream/FileInputStream.h>
#include <ork/stream/FileOutputStream.h>
#include <ork/reflect/serialize/XMLSerializer.h>
#include <ork/reflect/serialize/XMLDeserializer.h>
#include <ork/kernel/fixedlut.hpp>
#include <ork/kernel/Array.hpp>
#include <QMenu>

///////////////////////////////////////////////////////////////////////////////

INSTANTIATE_TRANSPARENT_RTTI(ork::tool::ged::GedFactory, "GedFactory");
INSTANTIATE_TRANSPARENT_RTTI(ork::tool::ged::IPlugChoiceDelegate, "IPlugChoiceDelegate");
INSTANTIATE_TRANSPARENT_RTTI(ork::tool::ged::IOpsDelegate, "IOpsDelegate");

///////////////////////////////////////////////////////////////////////////////

template class ork::fixedvector<ork::tool::ged::GedSkin::GedPrim, ork::tool::ged::GedSkin::PrimContainer::kmaxprims>;
template class ork::fixedvector<ork::tool::ged::GedSkin::GedPrim*, ork::tool::ged::GedSkin::PrimContainer::kmaxprims>;

template class ork::fixedvector<ork::tool::ged::GedSkin::GedPrim*, ork::tool::ged::GedSkin::PrimContainer::kmaxprimsper>;

template class ork::fixedvector<ork::tool::ged::GedSkin::PrimContainer, 32>;
template class ork::fixedvector<ork::tool::ged::GedSkin::PrimContainer*, 32>;

/*template class ork::fixedvector<
    ork::tool::ged::GedSkin::GedPrim,
    ork::tool::ged::GedSkin::PrimContainer::kmaxprims
>;*/

template class ork::
    fixedvector<std::pair<int, ork::tool::ged::GedSkin::PrimContainer*>, ork::tool::ged::GedSkin::kMaxPrimContainers>;

template class ork::fixedlut<int, ork::tool::ged::GedSkin::PrimContainer*, ork::tool::ged::GedSkin::kMaxPrimContainers>;

///////////////////////////////////////////////////////////////////////////////

namespace ork { namespace dataflow {
extern bool gbGRAPHLIVE;
}} // namespace ork::dataflow

namespace ork { namespace tool { namespace ged {

void GedFactory::Describe() {
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

GedItemNode*
GedFactory::CreateItemNode(ObjModel& mdl, const ConstString& Name, const reflect::IObjectProperty* prop, Object* obj) const {
  GedItemNode* PropContainerW = new GedLabelNode(mdl, Name.c_str(), prop, obj);
  return PropContainerW;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void EnumerateFactories(
    const ork::Object* pdestobj,
    const reflect::IObjectProperty* prop,
    orkset<object::ObjectClass*>& FactoryClassSet) {
  /////////////////////////////////////////////////////////

  ConstString anno = prop->GetAnnotation("editor.factorylistbase");

  orkvector<std::string> Classes;
  SplitString(anno.c_str(), Classes, " ");
  int inumclasses = int(Classes.size());

  if (inumclasses) {
    for (int i = 0; i < inumclasses; i++) {
      const std::string& ps = Classes[i];

      rtti::Class* pclass = rtti::Class::FindClass(ps.c_str());

      object::ObjectClass* pobjclass = rtti::downcast<object::ObjectClass*>(pclass);

      //////////////////////////////////////////////

      OrkAssert(pobjclass);

      orkstack<object::ObjectClass*> FactoryStack;

      FactoryStack.push(pobjclass);

      while (FactoryStack.empty() == false) {
        object::ObjectClass* pclass = FactoryStack.top();
        FactoryStack.pop();

        if (pclass->HasFactory()) {
          //////////////////////////////////////////////
          // check if class marked uninstantiable by editor
          //////////////////////////////////////////////

          auto instanno = pclass->Description().classAnnotation("editor.instantiable");

          bool bok2add = instanno.IsA<bool>() ? instanno.Get<bool>() : true;

          if (pdestobj) // check if also OK with the destination object
          {
            ObjectFactoryFilter factfilterev;
            factfilterev.mpClass = pclass;
            pdestobj->Query(&factfilterev);
            bok2add &= factfilterev.mbFactoryOK;
          }

          if (bok2add) {
            // pact = qm.addAction( pclass->Name().c_str() );
            // pact->setData( QVariant( pclass->Name().c_str() ) );

            FactoryClassSet.insert(pclass);
          }
        }

        rtti::Class* const pfirstchild = pclass->FirstChild();
        rtti::Class* pchild            = pfirstchild;

        while (pchild) {
          object::ObjectClass* pobjchildclass = rtti::downcast<object::ObjectClass*>(pchild);
          OrkAssert(pobjchildclass);
          FactoryStack.push(pobjchildclass);
          pchild = (pchild->NextSibling() == pfirstchild) ? 0 : pchild->NextSibling();
        }
      }
    }
  }
}

void EnumerateFactories(object::ObjectClass* pobjclass, orkset<object::ObjectClass*>& FactoryClassVect) {
  OrkAssert(pobjclass);
  orkstack<object::ObjectClass*> FactoryStack;
  FactoryStack.push(pobjclass);
  while (FactoryStack.empty() == false) {
    object::ObjectClass* pclass = FactoryStack.top();
    FactoryStack.pop();
    if (pclass->HasFactory()) { //////////////////////////////////////////////
      // check if class marked uninstantiable by editor
      //////////////////////////////////////////////

      auto instanno = pclass->Description().classAnnotation("editor.instantiable");

      bool bok2add = instanno.IsA<bool>() ? instanno.Get<bool>() : true;

      /*ConstString instanno = pclass->Description().classAnnotation( "editor.instantiable" );
      bool bok2add = true;
      if( instanno.length() )
      {	if( instanno == "false" )
          {
              bok2add = false;
          }
      }*/
      if (bok2add) {
        if (pclass->HasFactory()) {
          FactoryClassVect.insert(pclass);
        }
      }
    }
    rtti::Class* const pfirstchild = pclass->FirstChild();
    rtti::Class* pchild            = pfirstchild;
    while (pchild) {
      object::ObjectClass* pobjchildclass = rtti::downcast<object::ObjectClass*>(pchild);
      OrkAssert(pobjchildclass);
      FactoryStack.push(pobjchildclass);
      pchild = (pchild->NextSibling() == pfirstchild) ? 0 : pchild->NextSibling();
    }
  }
}

QMenu* CreateFactoryMenu(const orkset<object::ObjectClass*>& FactoryClassVect) {
  QMenu* pMenu = new QMenu(0);

  orkmap<ork::PoolString, object::ObjectClass*> ClassMap;

  for (orkset<object::ObjectClass*>::const_iterator it = FactoryClassVect.begin(); it != FactoryClassVect.end(); it++) {
    object::ObjectClass* objclass = (*it);

    bool badd = true;

    if (badd) {
      const ork::PoolString& pstr = objclass->Name();
      ClassMap[pstr]              = objclass;
    }
  }

  for (orkmap<ork::PoolString, object::ObjectClass*>::const_iterator it = ClassMap.begin(); it != ClassMap.end(); it++) {
    object::ObjectClass* objclass = it->second;
    ork::PoolString name          = it->first;

    QAction* pchildact = pMenu->addAction(name.c_str());

    QVariant UserData(QString(name.c_str()));
    pchildact->setData(UserData);
  }

  return pMenu;
}

///////////////////////////////////////////////////////////////////////////////

void UserChoices::EnumerateChoices(bool bforcenocache) {
  mucd.EnumerateChoices(mUserChoices);

  for (orkmap<PoolString, IUserChoiceDelegate::ValueType>::const_iterator it = mUserChoices.begin(); it != mUserChoices.end();
       it++) {
    const char* item = it->first.c_str();
    AttrChoiceValue myval(item, item);
    myval.SetCustomData(it->second);
    add(myval);
  }
}

UserChoices::UserChoices(IUserChoiceDelegate& ucd, ork::Object* pobj, ork::Object* puserobj)
    : mucd(ucd) {
  mucd.SetObject(pobj, puserobj);
  EnumerateChoices();
}

//////////////////////////////////////////////////////////////////////////////
void IPlugChoiceDelegate::Describe() {
}
void IOpsDelegate::Describe() {
  ObjectImportDelegate::GetClassStatic();
  ObjectExportDelegate::GetClassStatic();
}
//////////////////////////////////////////////////////////////////////////////

static const int ops_ioff = 2;
LockedResource<IOpsDelegate::TaskList> IOpsDelegate::gCurrentTasks;

void IOpsDelegate::AddTask(ork::object::ObjectClass* pdelegclass, ork::Object* ptarget) {
  if (0 == GetTask(pdelegclass, ptarget)) {
    IOpsDelegate* deleg = rtti::autocast(pdelegclass->CreateObject());
    if (deleg) {
      OpsTask* ptask    = new OpsTask;
      ptask->mpDelegate = deleg;
      ptask->mpTarget   = ptarget;

      TaskList& tsklist = gCurrentTasks.LockForWrite();
      { tsklist.push_back(ptask); }
      gCurrentTasks.UnLock();

      deleg->Execute(ptarget);
    }
  }
}
void IOpsDelegate::RemoveTask(ork::object::ObjectClass* pdelegclass, ork::Object* ptarget) {
  OpsTask* ptask = GetTask(pdelegclass, ptarget);

  if (0 != ptask) {
    TaskList& tsklist = gCurrentTasks.LockForWrite();
    {
      TaskList::iterator iterase = tsklist.end();

      for (TaskList::iterator it = tsklist.begin(); it != tsklist.end(); it++) {
        OpsTask* ttask = (*it);

        if (ttask == ptask) {
          iterase = it;
        }
      }

      if (iterase != tsklist.end()) {
        tsklist.erase(iterase);
      }
    }
    gCurrentTasks.UnLock();
  }
}
OpsTask* IOpsDelegate::GetTask(ork::object::ObjectClass* pdelegclass, ork::Object* ptarget) {
  OpsTask* pret           = 0;
  const TaskList& tsklist = gCurrentTasks.LockForRead();
  {
    for (TaskList::const_iterator it = tsklist.begin(); it != tsklist.end(); it++) {
      OpsTask* ptask = (*it);

      if (ptask->mpDelegate->GetClass() == pdelegclass) {
        if (ptask->mpTarget == ptarget) {
          pret = ptask;
        }
      }
    }
  }
  gCurrentTasks.UnLock();
  return pret;
}

void OpsNode::DoDraw(lev2::Context* pTARG) // virtual
{
  bool bispick = pTARG->FBI()->isPickState();

  int inumops = int(mOps.size());

  const int ops_size = ((miW - inumops * 3) / inumops);

  int icentery = get_text_center_y();

  GetSkin()->DrawOutlineBox(this, miX, miY, miW, miH, GedSkin::ESTYLE_DEFAULT_OUTLINE);
  for (int i = 0; i < int(mOps.size()); i++) {
    int ix = miX + ops_ioff + (i * ops_size + 2);

    ork::Object* ptarget = GetOrkObj();

    bool is_iopsdeleg    = mOps[i].second.IsA<ork::object::ObjectClass*>();
    bool is_iopmaplambda = mOps[i].second.IsA<ork::reflect::OpMap::lambda_t>();

    if (is_iopmaplambda) {
      const auto& l = mOps[i].second.Get<ork::reflect::OpMap::lambda_t>();

      if (bispick)
        GetSkin()->DrawBgBox(this, ix, miY + 2, (ops_size - ops_ioff), miH - 4, GedSkin::ESTYLE_BACKGROUND_OPS);
      else
        GetSkin()->DrawBgBox(this, ix, miY + 2, (ops_size - ops_ioff), miH - 4, GedSkin::ESTYLE_DEFAULT_OUTLINE);
      GetSkin()->DrawText(this, ix + 6, icentery, mOps[i].first.c_str());

    } else if (is_iopsdeleg) {
      auto pclass = mOps[i].second.Get<ork::object::ObjectClass*>();

      OpsTask* ptask = IOpsDelegate::GetTask(pclass, ptarget);

      bool bactive = (ptask != 0);

      GedSkin::ESTYLE estyle = bactive ? GedSkin::ESTYLE_DEFAULT_OUTLINE : GedSkin::ESTYLE_BACKGROUND_OPS;

      GetSkin()->DrawBgBox(this, ix, miY + 2, (ops_size - ops_ioff), miH - 4, estyle);

      if (ptask) {
        static int ispinner = 0;

        float fprogress = ptask->mpDelegate->GetProgress();
        int inw         = int(float((ops_size - ops_ioff) - 1) * fprogress);
        GetSkin()->DrawBgBox(this, ix + 1, miY + 3, inw, miH - 6, GedSkin::ESTYLE_BACKGROUND_2);

        std::string Label = CreateFormattedString("%s(%d)", mOps[i].first.c_str(), int(fprogress * 100.0f));
        GetSkin()->DrawText(this, ix, icentery, Label.c_str());

        float fspinner = float(ispinner) / 100.0f;
        float fxo      = sinf(fspinner) * float(miH / 3);
        float fyo      = cosf(fspinner) * float(miH / 3);

        float fxc = ix + (ops_size - ops_ioff) - (miH / 3);
        float fyc = miY + (miH / 2);

        int ix0 = int(fxc - fxo);
        int ix1 = int(fxc + fxo);
        int iy0 = int(fyc - fyo);
        int iy1 = int(fyc + fyo);

        GetSkin()->DrawLine(this, ix0, iy0, ix1, iy1, GedSkin::ESTYLE_BACKGROUND_2);

        ispinner++;
      } else {
        GetSkin()->DrawText(this, ix + 6, icentery, mOps[i].first.c_str());
      }
    }
  }
}

void OpsNode::OnMouseClicked(ork::ui::event_constptr_t ev) {
  int inumops        = int(mOps.size());
  const int ops_size = ((miW - inumops * 3) / inumops);

  // Qt::MouseButton button = pEV->button();
  int ix = ev->miX - this->miX;
  int iy = ev->miY - this->miY;

  // int ix = miX+ops_ioff+(i*ops_size+2);

  int index = (ix - ops_ioff) / (ops_size + 2);

  const auto& opnam = mOps[index].first;
  const auto& oper  = mOps[index].second;

  orkprintf("op<%s> typ<%s>\n", opnam.c_str(), TypeIdName(oper.GetTypeInfo()).c_str());

  if (oper.IsA<ork::object::ObjectClass*>()) {
    auto pclass = oper.Get<ork::object::ObjectClass*>();
    if (pclass) {
      mModel.SigRepaint();
      IOpsDelegate::AddTask(pclass, GetOrkObj());
      // mModel.SigRepaint();
    }
  } else if (oper.IsA<ork::reflect::OpMap::lambda_t>()) {
    const auto& l = oper.Get<ork::reflect::OpMap::lambda_t>();

    printf("Executing OpLambda on Obj<%p>\n", GetOrkObj());
    l(GetOrkObj());

    mModel.SigRepaint();
  }
}

OpsNode::OpsNode(ObjModel& mdl, const char* name, const reflect::IObjectProperty* prop, ork::Object* obj)
    : GedItemNode(mdl, name, prop, obj) {
  object::ObjectClass* objclass = rtti::downcast<object::ObjectClass*>(obj->GetClass());

  std::set<ork::reflect::OpMap*> opm_set;
  std::set<ork::object::ObjectClass*> iop_set;

  while (objclass != nullptr) {
    auto obj_ops = objclass->Description().classAnnotation("editor.object.ops");

    if (obj_ops.IsA<ork::reflect::OpMap*>()) {
      auto opm = obj_ops.Get<ork::reflect::OpMap*>();

      if (opm && (opm_set.end() == opm_set.find(opm))) {
        opm_set.insert(opm);

        for (const auto& item : opm->mLambdaMap) {
          mOps.push_back(std::pair<std::string, any64>(item.first, item.second));
        }
      }

    } else if (obj_ops.IsA<ConstString>()) {
      orkvector<std::string> opstrings;
      ConstString obj_ops_str = obj_ops.Get<ConstString>();
      SplitString(obj_ops_str.c_str(), opstrings, " ");
      for (int i = 0; i < int(opstrings.size()); i++) {
        const std::string opstr = opstrings[i];
        orkvector<std::string> opbreak;
        SplitString(opstr.c_str(), opbreak, ":");

        rtti::Class* the_class = rtti::Class::FindClass(opbreak[1].c_str());
        if (the_class) {
          OrkAssert(the_class->IsSubclassOf(IOpsDelegate::GetClassStatic()));
          ork::object::ObjectClass* pclass = rtti::autocast(the_class);

          if (pclass && (iop_set.end() == iop_set.find(pclass))) {
            iop_set.insert(pclass);

            mOps.push_back(std::pair<std::string, any64>(opbreak[0], pclass));
          }
        }
      }
    }

    objclass = rtti::downcast<object::ObjectClass*>(objclass->Parent());
  }
}

//////////////////////////////////////////////////////////////////////////////

static const int koff = 1;
// static const int kdim = GedMapNode::klabelsize-2;

void GedGroupNode::DoDraw(lev2::Context* pTARG) {
  int inumitems   = GetNumItems();
  int stack_depth = mModel.StackSize();

  /////////////////
  // drop down box
  /////////////////
  int icentery = get_text_center_y();

  int ioff = koff;
  int idim = get_charh();

  int dbx1 = miX + ioff;
  int dbx2 = dbx1 + idim;
  int dby1 = miY + ioff;
  int dby2 = dby1 + idim;

  int labw = this->propnameWidth();
  int labx = miX + 12;
  if (labx < dbx2 + 3)
    labx = dbx2 + 3;

  ////////////////////////////////

  int il = miX + miW - (idim * 2);
  int iw = idim - 1;
  int ih = idim - 2;

  int il2 = il + idim + 1;
  int iy2 = dby2 - 1;
  int ihy = dby1 + (ih / 2);
  int ihx = il + (iw / 2);

  GetSkin()->DrawBgBox(this, miX, miY, miW, miH, GedSkin::ESTYLE_BACKGROUND_1);
  GetSkin()->DrawText(this, labx, icentery, _propname.c_str());
  GetSkin()->DrawBgBox(this, miX, miY, miW, get_charh(), GedSkin::ESTYLE_BACKGROUND_GROUP_LABEL);

  ////////////////////////////////
  // draw stack depth indicator on top node
  ////////////////////////////////

  if ((stack_depth - 1) && (GetOrkObj() == mModel.BrowseStackTop()) && mIsObjNode) {
    std::string arrs;
    for (int i = 0; i < (stack_depth - 1); i++)
      arrs += "<";
    GetSkin()->DrawText(this, il - idim - ((stack_depth - 1) * get_charw()), icentery, arrs.c_str());
  }

  ////////////////////////////////

  if (inumitems) {
    if (mbCollapsed) {
      GetSkin()->DrawRightArrow(this, dbx1, dby1, idim, idim, GedSkin::ESTYLE_BUTTON_OUTLINE);
      GetSkin()->DrawLine(this, dbx1 + 1, dby1, dbx1 + 1, dby2, GedSkin::ESTYLE_BUTTON_OUTLINE);
    } else {
      GetSkin()->DrawDownArrow(this, dbx1, dby1, idim, idim, GedSkin::ESTYLE_BUTTON_OUTLINE);
      GetSkin()->DrawLine(this, dbx1, dby1 + 1, dbx2, dby1 + 1, GedSkin::ESTYLE_BUTTON_OUTLINE);
    }
  }

  ////////////////////////////////
  // draw newged button
  ////////////////////////////////

  if (GetOrkObj() && mIsObjNode) {

    // GetSkin()->DrawOutlineBox( this, il, dby1, iw, ih, GedSkin::ESTYLE_BUTTON_OUTLINE );
    GetSkin()->DrawLine(this, il, iy2, ihx, dby1, GedSkin::ESTYLE_BUTTON_OUTLINE);
    GetSkin()->DrawLine(this, il + idim - 2, iy2, ihx, dby1, GedSkin::ESTYLE_BUTTON_OUTLINE);
    GetSkin()->DrawLine(this, il, iy2, ihx, ihy, GedSkin::ESTYLE_BUTTON_OUTLINE);
    GetSkin()->DrawLine(this, il + idim - 2, iy2, ihx, ihy, GedSkin::ESTYLE_BUTTON_OUTLINE);

    GetSkin()->DrawOutlineBox(this, il2, dby1, iw, ih, GedSkin::ESTYLE_BUTTON_OUTLINE);
    GetSkin()->DrawOutlineBox(this, il2 + 3, dby1 + 3, iw - 6, ih - 6, GedSkin::ESTYLE_BUTTON_OUTLINE);
  }

  ////////////////////////////////
}

GedGroupNode::GedGroupNode(
    ObjModel& mdl,
    const char* name,
    const reflect::IObjectProperty* prop,
    ork::Object* obj,
    bool is_obj_node)
    : GedItemNode(mdl, name, prop, obj)
    , mbCollapsed(false == is_obj_node)
    , mIsObjNode(is_obj_node) {

  std::string fixname = name;
  /////////////////////////////////////////////////////////////////
  // localize collapse states to instances of properties underneath other properties
  GedItemNode* parent = mdl.GetGedWidget()->ParentItemNode();
  if (parent) {
    const char* parname = parent->_propname.c_str();
    if (parname) {
      fixname += CreateFormattedString("_%s_", parname);
    }
  }
  /////////////////////////////////////////////////////////////////

  int ilen = (int)fixname.length();
  for (int i = 0; i < ilen; i++) {
    switch (fixname[i]) {
      case ':':
      case '<':
      case '>':
      case ' ':
        fixname[i] = ' ';
        break;
    }
  }

  mPersistID.format("%s_group_collapse", fixname.c_str());

  ///////////////////////////////////////////
  PersistHashContext HashCtx;
  PersistantMap* pmap = mdl.GetPersistMap(HashCtx);
  ///////////////////////////////////////////

  const std::string& str_collapse = pmap->GetValue(mPersistID.c_str());

  if (str_collapse == "false") {
    mbCollapsed = false;
  }

  CheckVis();
}
///////////////////////////////////////////////////////////////////////////////
void GedGroupNode::OnMouseDoubleClicked(ork::ui::event_constptr_t ev) {
  int inumitems = GetNumItems();

  bool isCTRL = ev->mbCTRL;

  const int kdim = get_charh();

  printf("GedGroupNode<%p>::mouseDoubleClickEvent inumitems<%d>\n", this, inumitems);

  if (inumitems) {
    int ix = ev->miX;
    int iy = (ev->miY);

    //////////////////////////////
    // spawn/stack
    //////////////////////////////

    int ioff = koff;
    int idim = get_charh();
    int dby1 = miY + ioff;
    int dby2 = dby1 + idim;
    int il   = miW - (idim * 2);
    int iw   = idim - 1;
    int ih   = idim - 2;

    int il2 = il + idim + 1;
    int iy2 = dby2 - 1;
    int ihy = dby1 + (ih / 2);
    int ihx = il + (iw / 2);

    printf("iy<%d> KOFF<%d> KDIM<%d>\n", iy, koff, kdim);
    if (iy >= koff && iy <= kdim) {
      if (ix >= il && ix < il2 && mIsObjNode) {
        if (GetOrkObj()) {
          ork::Object* top = mModel.BrowseStackTop();
          if (top == GetOrkObj()) {
            mModel.PopBrowseStack();
            top = mModel.BrowseStackTop();
            if (top) {
              mModel.Attach(top, false);
            }
          } else {
            mModel.PushBrowseStack(GetOrkObj());
            mModel.Attach(GetOrkObj(), false);
          }
        }

      } else if (ix >= il2 && ix < il2 + idim && mIsObjNode) {
        if (GetOrkObj()) {
          // spawn new window here
          mModel.SigSpawnNewGed(GetOrkObj());
        }
      } else if (ix >= koff && ix <= kdim) // drop down
      {
        mbCollapsed = !mbCollapsed;

        ///////////////////////////////////////////
        PersistHashContext HashCtx;
        PersistantMap* pmap = mModel.GetPersistMap(HashCtx);
        ///////////////////////////////////////////

        pmap->SetValue(mPersistID.c_str(), mbCollapsed ? "true" : "false");

        if (isCTRL) // also do siblings
        {
          GedItemNode* par = GetParent();
          if (par) {
            int inumc = par->GetNumItems();
            for (int i = 0; i < inumc; i++) {
              GedItemNode* item    = par->GetItem(i);
              GedGroupNode* pgroup = rtti::autocast(item);
              if (pgroup) {
                pgroup->mbCollapsed = mbCollapsed;
                pgroup->CheckVis();
                pmap->SetValue(pgroup->mPersistID.c_str(), mbCollapsed ? "true" : "false");
              }
            }
          }
        }

        CheckVis();
        return;
      }
    }
  }
}
///////////////////////////////////////////////////////////////////////////////
void GedGroupNode::CheckVis() {
  int inumitems = GetNumItems();

  if (inumitems) {
    if (mbCollapsed) {
      for (int it = 0; it < inumitems; it++) {
        GetItem(it)->SetVisible(false);
      }
    } else {
      for (int it = 0; it < inumitems; it++) {
        GetItem(it)->SetVisible(true);
      }
    }
  }
  mModel.GetGedWidget()->DoResize();
}
//////////////////////////////////////////////////////////////////////////////

class GraphImportDelegate : public IOpsDelegate {
  RttiDeclareConcrete(GraphImportDelegate, tool::ged::IOpsDelegate);
  void Execute(ork::Object* ptarget) final {
    ork::dataflow::graph_inst* pgraph = rtti::autocast(ptarget);

    if (pgraph) {
      lev2::GfxEnv::GetRef().GetGlobalLock().Lock();
      QString FileName           = QFileDialog::getOpenFileName(0, "Import Dataflow Graph", 0, "DataflowGraph (*.dfg)");
      file::Path::NameType fname = FileName.toStdString().c_str();
      if (fname.length()) {
        // SetRecentSceneFile(FileName.toAscii().data(),SCENEFILE_DIR);
        if (ork::FileEnv::filespec_to_extension(fname).length() == 0)
          fname += ".dfg";
        stream::FileInputStream istream(fname.c_str());
        reflect::serialize::XMLDeserializer iser(istream);
        // ork::stream::FileOutputStream ostream(fname.c_str());
        // ork::reflect::serialize::XMLSerializer oser(ostream);
        // oser.Serialize(ptex);
        pgraph->DeserializeInPlace(iser);
      }
      lev2::GfxEnv::GetRef().GetGlobalLock().UnLock();
    }
    tool::ged::IOpsDelegate::RemoveTask(GraphImportDelegate::GetClassStatic(), ptarget);
  }
};
class GraphExportDelegate : public IOpsDelegate {
  RttiDeclareConcrete(GraphExportDelegate, tool::ged::IOpsDelegate);
  void Execute(ork::Object* ptarget) final {
    ork::dataflow::graph_inst* pgraph = rtti::autocast(ptarget);

    if (pgraph) {
      lev2::GfxEnv::GetRef().GetGlobalLock().Lock();
      QString FileName           = QFileDialog::getSaveFileName(0, "Export Dataflow Graph", 0, "DataflowGraph (*.dfg)");
      file::Path::NameType fname = FileName.toStdString().c_str();
      if (fname.length()) {
        // SetRecentSceneFile(FileName.toAscii().data(),SCENEFILE_DIR);
        if (ork::FileEnv::filespec_to_extension(fname).length() == 0)
          fname += ".dfg";
        ork::stream::FileOutputStream ostream(fname.c_str());
        ork::reflect::serialize::XMLSerializer oser(ostream);
        // oser.Serialize(ptex);
        pgraph->SerializeInPlace(oser);
      }
      lev2::GfxEnv::GetRef().GetGlobalLock().UnLock();
    }
    tool::ged::IOpsDelegate::RemoveTask(GraphExportDelegate::GetClassStatic(), ptarget);
  }
};
void ObjectImportDelegate::Execute(ork::Object* ptarget) {
  if (ptarget) {
    lev2::GfxEnv::GetRef().GetGlobalLock().Lock();
    QString FileName           = QFileDialog::getOpenFileName(0, "Import Object (Be careful!)", 0, "Orkid Object (*.mox)");
    file::Path::NameType fname = FileName.toStdString().c_str();
    if (fname.length()) {
      // SetRecentSceneFile(FileName.toAscii().data(),SCENEFILE_DIR);
      if (ork::FileEnv::filespec_to_extension(fname).length() == 0)
        fname += ".mox";
      stream::FileInputStream istream(fname.c_str());
      reflect::serialize::XMLDeserializer iser(istream);
      // ork::stream::FileOutputStream ostream(fname.c_str());
      // ork::reflect::serialize::XMLSerializer oser(ostream);
      // oser.Serialize(ptex);
      ptarget->DeserializeInPlace(iser);
    }
    lev2::GfxEnv::GetRef().GetGlobalLock().UnLock();
  }
  tool::ged::IOpsDelegate::RemoveTask(ObjectImportDelegate::GetClassStatic(), ptarget);
}
void ObjectExportDelegate::Execute(ork::Object* ptarget) {
  ork::Object* pobj = rtti::autocast(ptarget);

  if (pobj) {
    lev2::GfxEnv::GetRef().GetGlobalLock().Lock();
    QString FileName           = QFileDialog::getSaveFileName(0, "Export Object ", 0, "Orkid Object (*.mox)");
    file::Path::NameType fname = FileName.toStdString().c_str();
    if (fname.length()) {
      if (ork::FileEnv::filespec_to_extension(fname).length() == 0)
        fname += ".mox";
      ork::stream::FileOutputStream ostream(fname.c_str());
      ork::reflect::serialize::XMLSerializer oser(ostream);
      // oser.Serialize(ptex);
      pobj->SerializeInPlace(oser);
    }
    lev2::GfxEnv::GetRef().GetGlobalLock().UnLock();
  }
  tool::ged::IOpsDelegate::RemoveTask(ObjectExportDelegate::GetClassStatic(), ptarget);
}
void GraphImportDelegate::Describe() {
}
void GraphExportDelegate::Describe() {
}
void ObjectImportDelegate::Describe() {
}
void ObjectExportDelegate::Describe() {
}

void GedObject::OnUiEvent(ork::ui::event_constptr_t ev) {
  switch (ev->miEventCode) {
    case ui::UIEV_DRAG:
      OnMouseDragged(ev);
      break;
    case ui::UIEV_MOVE:
      OnMouseMoved(ev);
      break;
    case ui::UIEV_DOUBLECLICK:
      OnMouseDoubleClicked(ev);
      break;
    case ui::UIEV_RELEASE:
      OnMouseReleased(ev);
      break;
  }
}

}}} // namespace ork::tool::ged

INSTANTIATE_TRANSPARENT_RTTI(ork::tool::ged::GraphImportDelegate, "dflowgraphimport");
INSTANTIATE_TRANSPARENT_RTTI(ork::tool::ged::GraphExportDelegate, "dflowgraphexport");
INSTANTIATE_TRANSPARENT_RTTI(ork::tool::ged::ObjectImportDelegate, "objectimport");
INSTANTIATE_TRANSPARENT_RTTI(ork::tool::ged::ObjectExportDelegate, "objectexport");
