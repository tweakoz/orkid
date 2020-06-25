////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <orktool/qtui/qtui_tool.h>
#include <ork/application/application.h>

///////////////////////////////////////////////////////////////////////////////

#include <ork/lev2/gfx/gfxmodel.h>
//
#include <ork/lev2/gfx/texman.h>
#include <ork/lev2/gfx/shadman.h>
#include <ork/lev2/gfx/gfxmaterial_ui.h>
#include <ork/kernel/timer.h>
#include <ork/reflect/RegisterProperty.h>
#include <ork/reflect/properties/DirectMapTyped.h>
#include <ork/reflect/properties/DirectMapTyped.hpp>

///////////////////////////////////////////////////////////////////////////////
#include <orktool/toolcore/dataflow.h>
///////////////////////////////////////////////////////////////////////////////

#include "vpSceneEditor.h"
#include "uiToolsDefault.h"
#include <pkg/ent/editor/edmainwin.h>

#include <pkg/ent/scene.h>
#include <pkg/ent/ReferenceArchetype.h>

///////////////////////////////////////////////////////////////////////////////

namespace ork { namespace ent {

class ObjectFactory : public ork::Object {
  RttiDeclareConcrete(ObjectFactory, ork::Object) public : ObjectFactory(rtti::Class* pclass = NULL);
  const rtti::Class* GetFactoryClass() const {
    return mpClass;
  }

private:
  rtti::Class* mpClass;
};
void ObjectFactory::Describe() {
}

template <typename T> class ObjectFactoryList {
public:
  ObjectFactoryList();

  int GetNumFactories() const {
    return int(mFactories.size());
  }
  const ObjectFactory* GetFactory(int idx) const {
    return mFactories[idx];
  }

private:
  orkvector<ObjectFactory*> mFactories;
};

///////////////////////////////////////////////////////////////////////////////

ObjectFactory::ObjectFactory(rtti::Class* pclass)
    : mpClass(pclass) {
}

///////////////////////////////////////////////////////////////////////////////

void BuildFactoryList(rtti::Class* root, orkvector<ObjectFactory*>& factories) {
  if (root->hasFactory()) {
    object::ObjectClass* pobjclass = rtti::autocast(root);
    auto instanno                  = pobjclass->Description().classAnnotation("editor.instantiable");

    bool bok2add = instanno.IsA<bool>() ? instanno.Get<bool>() : true;

    if (bok2add) {
      factories.push_back(new ObjectFactory(root));
    }
  }

  rtti::Class* child = root->FirstChild();
  if (child)
    do {
      BuildFactoryList(child, factories);
      child = child->NextSibling();
    } while (child != root->FirstChild());
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> ObjectFactoryList<T>::ObjectFactoryList() {
  BuildFactoryList(T::GetClassStatic(), mFactories);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

class ArchetypeChoiceFunctor : public ork::util::ChoiceFunctor {
public:
  ArchetypeChoiceFunctor(SceneEditorBase& SceneEditor)
      : mSceneEditor(SceneEditor) {
  }

private:
  SceneEditorBase& mSceneEditor;

  virtual std::string ComputeValue(const std::string& ValueStr) const {
    Archetype* parch = 0;
    PropTypeString tstr(ValueStr.c_str());
    Object* ValueObj = PropType<Object*>::FromString(tstr);

    orkprintf("ValueStr %s\n", ValueStr.c_str());
    orkprintf("ValueObj %p\n", ValueObj);

    if (0 == strncmp("/New/Reference/", ValueStr.c_str(), 15)) // might be an asset
    {
      std::string str(tstr.c_str());
      str = str.substr(15, str.length() - 15);
      orkprintf("str %s\n", str.c_str());
      std::string str2 = CreateFormattedString("data://archetypes/%s", str.c_str());
      ork::file::Path XRefPath(str2.c_str());
      ork::file::Path tdos = XRefPath.ToAbsolute(ork::file::Path::EPATHTYPE_DOS);

      std::string ExtRefName = CreateFormattedString("/arch/Referenced/%s", str.c_str());
      SceneObject* ptest     = mSceneEditor.mpScene->FindSceneObjectByName(AddPooledString(ExtRefName.c_str()));

      orkprintf("ptest %p\n", ptest);

      if (ptest == 0) {
        parch = mSceneEditor.NewReferenceArchetype(str);
      } else {
        parch = rtti::autocast(ptest);
      }
    } else if (ValueObj->GetClass()->IsSubclassOf(Archetype::GetClassStatic())) {
      parch = rtti::autocast(ValueObj);
    } else if (ValueObj->GetClass()->IsSubclassOf(ObjectFactory::GetClassStatic())) {
      ObjectFactory* pfact                  = rtti::autocast(ValueObj);
      const object::ObjectClass* parchclass = rtti::autocast(pfact->GetFactoryClass());
      parch                                 = rtti::autocast(parchclass->CreateObject());
      // parch->Compose();

      // std::string DefName = parch->DefaultUserName();
      // parch->SetName( parch->GetScene()->TryObjectName(DefName).c_str() );
      // parch->Default();
      // parch->GetScene()->AddSceneObject( parch );
      // parch->SetObjectFlag( CObject::EFLAGS_EXPANDED, true );
    }

    PropType<Object*>::ToString(parch, tstr);
    return std::string(tstr.c_str());
  }
};

ArchetypeChoices::ArchetypeChoices(SceneEditorBase& SceneEditor)
    : mSceneEditor(SceneEditor) {
}

void ArchetypeChoices::EnumerateChoices(bool bforcenocache) {
  static ArchetypeChoiceFunctor Functor(mSceneEditor);
  static const ObjectFactoryList<Archetype> Factories;

  if (mSceneEditor.mpScene) {
    clear();

    //////////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////
    // enumerate user-instantiated archetypes

    const orkmap<PoolString, SceneObject*>& SceneObjects = mSceneEditor.mpScene->GetSceneObjects();

    for (orkmap<PoolString, SceneObject*>::const_iterator it = SceneObjects.begin(); it != SceneObjects.end(); it++) {
      SceneObject* pobj = it->second;

      object::ObjectClass* pclass = rtti::autocast(pobj->GetClass());

      if (pclass->IsSubclassOf(Archetype::GetClassStatic())) {
        Archetype* parch = rtti::autocast(pobj);

        std::string ObjPtrStr = CreateFormattedString("%p", pobj);
        std::string ObjName   = CreateFormattedString("%s", parch->GetName().c_str());

        util::AttrChoiceValue ChoiceVal(ObjName, ObjPtrStr);
        ChoiceVal.SetFunctor(&Functor);
        add(ChoiceVal);
      }
    }

    //////////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////
    // enumerate archetype factories

    int inumfact = Factories.GetNumFactories();

    for (int ifact = 0; ifact < inumfact; ifact++) {
      const ObjectFactory* pfact        = Factories.GetFactory(ifact);
      const object::ObjectClass* pclass = rtti::autocast(pfact->GetFactoryClass());

      std::string ObjPtrStr = CreateFormattedString("%p", pfact);

      util::AttrChoiceValue ChoiceVal(std::string("/New/") + pfact->GetFactoryClass()->Name().c_str(), ObjPtrStr);
      ChoiceVal.SetFunctor(&Functor);

      add(ChoiceVal);
    }

    //////////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////
    // enumerate external archetype files

    ork::file::Path searchdir("data://archetypes/");
    file::Path::NameType wildcard = "*.mox";
    orkvector<file::Path::NameType> files =
        FileEnv::filespec_search(wildcard, searchdir.ToAbsolute(ork::file::Path::EPATHTYPE_DOS));
    size_t inumfiles = files.size();

    ork::file::Path searchabs = searchdir.ToAbsolute(ork::file::Path::EPATHTYPE_DOS);
    ork::file::Path::NameType searchbase(searchabs.c_str());

    for (size_t ifile = 0; ifile < inumfiles; ++ifile) {
      ork::file::Path objPath(files[ifile].c_str());

      auto objBase = ork::file::Path(objPath.StripBasePath(searchbase).GetFolder(ork::file::Path::EPATHTYPE_POSIX));

      const char* pfolder = objBase.c_str();
      const char* pname   = objPath.GetName().c_str();

      file::Path::NameType ObjPtrStr = file::Path::NameType(pfolder) + file::Path::NameType(pname);

      file::Path::NameType ChoiceStr = file::Path::NameType("/New/Reference/") + ObjPtrStr;

      util::AttrChoiceValue ChoiceVal(ChoiceStr.c_str(), ChoiceStr.c_str());
      ChoiceVal.SetFunctor(&Functor);
      ChoiceVal.AddKeyword("userarchetype=true");
      add(ChoiceVal);
    }

    //////////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////
  }
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

class SystemDataChoiceFunctor : public ork::util::ChoiceFunctor {
public:
  SystemDataChoiceFunctor(SceneEditorBase& SceneEditor)
      : mSceneEditor(SceneEditor) {
  }

private:
  SceneEditorBase& mSceneEditor;

  virtual std::string ComputeValue(const std::string& ValueStr) const {
    SystemData* sdat = 0;
    PropTypeString tstr(ValueStr.c_str());
    Object* ValueObj = PropType<Object*>::FromString(tstr);

    orkprintf("ValueStr %s\n", ValueStr.c_str());
    orkprintf("ValueObj %p\n", ValueObj);

    if (ValueObj->GetClass()->IsSubclassOf(SystemData::GetClassStatic())) {
      sdat = rtti::autocast(ValueObj);
    }
    PropType<Object*>::ToString(sdat, tstr);
    return std::string(tstr.c_str());
  }
};

SystemDataChoices::SystemDataChoices(SceneEditorBase& SceneEditor)
    : mSceneEditor(SceneEditor) {
}

void SystemDataChoices::EnumerateChoices(bool bforcenocache) {
  static SystemDataChoiceFunctor Functor(mSceneEditor);
  static const ObjectFactoryList<SystemData> Factories;

  if (mSceneEditor.mpScene) {
    clear();

    //////////////////////////////////////////////////////////////////////////

    int inumfact = Factories.GetNumFactories();

    for (int ifact = 0; ifact < inumfact; ifact++) {
      const ObjectFactory* pfact        = Factories.GetFactory(ifact);
      const object::ObjectClass* pclass = rtti::autocast(pfact->GetFactoryClass());

      std::string ObjPtrStr = CreateFormattedString("%p", pfact);

      util::AttrChoiceValue ChoiceVal(std::string("/New/") + pfact->GetFactoryClass()->Name().c_str(), ObjPtrStr);
      ChoiceVal.SetFunctor(&Functor);

      add(ChoiceVal);
    }

    //////////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////
    // enumerate external archetype files

    ork::file::Path searchdir("data://archetypes/");
    file::Path::NameType wildcard = "*.mox";
    orkvector<file::Path::NameType> files =
        FileEnv::filespec_search(wildcard, searchdir.ToAbsolute(ork::file::Path::EPATHTYPE_DOS));
    size_t inumfiles = files.size();

    ork::file::Path searchabs = searchdir.ToAbsolute(ork::file::Path::EPATHTYPE_DOS);
    ork::file::Path::NameType searchbase(searchabs.c_str());

    for (size_t ifile = 0; ifile < inumfiles; ++ifile) {
      ork::file::Path objPath(files[ifile].c_str());

      auto objBase = ork::file::Path(objPath.StripBasePath(searchbase).GetFolder(ork::file::Path::EPATHTYPE_POSIX));

      const char* pfolder = objBase.c_str();
      const char* pname   = objPath.GetName().c_str();

      file::Path::NameType ObjPtrStr = file::Path::NameType(pfolder) + file::Path::NameType(pname);

      file::Path::NameType ChoiceStr = file::Path::NameType("/New/Reference/") + ObjPtrStr;

      util::AttrChoiceValue ChoiceVal(ChoiceStr.c_str(), ChoiceStr.c_str());
      ChoiceVal.SetFunctor(&Functor);
      ChoiceVal.AddKeyword("userarchetype=true");
      add(ChoiceVal);
    }

    //////////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////
  }
}

}} // namespace ork::ent

INSTANTIATE_TRANSPARENT_RTTI(ork::ent::ObjectFactory, "ork::ent::ObjectFactory")
