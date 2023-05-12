////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/reflect/properties/ITypedMap.h>
#include <ork/kernel/core_interface.h>
#include <ork/kernel/any.h>
#include <ork/util/choiceman.h>

namespace ork { namespace dataflow {
class outplugbase;
}} // namespace ork::dataflow
namespace ork { namespace tool { namespace ged {

///////////////////////////////////////////////////////////////////////////////
template <typename IODriver, typename T> class GedSimpleNode : public GedItemNode {
public:
  GedSimpleNode(ObjModel& mdl, const char* name, const reflect::ObjectProperty* prop, ork::Object* obj);

  void OnUiEvent(ork::ui::event_constptr_t ev) final;

  /*virtual*/ void DoDraw(lev2::Context* pTARG);
  IODriver& RefIODriver() {
    return mIoDriver;
  }

private:
  IODriver mIoDriver;
};

///////////////////////////////////////////////////////////////////////////////
template <typename IODriver> class GedAssetNode : public GedItemNode {
  DECLARE_TRANSPARENT_TEMPLATE_ABSTRACT_RTTI(GedAssetNode<IODriver>, GedItemNode);

public:
  GedAssetNode(ObjModel& mdl, const char* name, const reflect::ObjectProperty* prop, ork::Object* obj);
  void OnCreateObject();
  void SetLabel();
  void DoDraw(lev2::Context* pTARG); // virtual
  IODriver& RefIODriver() {
    return mIoDriver;
  }

private:
  IODriver mIoDriver;

  void OnMouseDoubleClicked(ork::ui::event_constptr_t ev) final;

  virtual bool DoDrawDefault() const {
    return false;
  }
};
///////////////////////////////////////////////////////////////////////////////
template <typename IODriver> class GedFileNode : public GedItemNode {
  DECLARE_TRANSPARENT_TEMPLATE_ABSTRACT_RTTI(GedFileNode<IODriver>, GedItemNode);

public:
  GedFileNode(ObjModel& mdl, const char* name, const reflect::ObjectProperty* prop, ork::Object* obj);
  void OnCreateObject();
  void SetLabel();
  void DoDraw(lev2::Context* pTARG); // virtual
  IODriver& RefIODriver() {
    return mIoDriver;
  }

private:
  IODriver mIoDriver;

  void OnMouseDoubleClicked(ork::ui::event_constptr_t ev) final;

  virtual bool DoDrawDefault() const {
    return false;
  }
};
///////////////////////////////////////////////////////////////////////////////
class UserChoices : public util::ChoiceList {
  IUserChoiceDelegate& mucd;
  orkmap<PoolString, IUserChoiceDelegate::ValueType> mUserChoices;

public:
  virtual void EnumerateChoices(bool bforcenocache = false);
  UserChoices(IUserChoiceDelegate& ucd, ork::Object* pobj, ork::Object* puserobj);
};
///////////////////////////////////////////////////////////////////////////////

class IPlugChoiceDelegate : public Object {
  RttiDeclareAbstract(IPlugChoiceDelegate, Object);

public:
  typedef orkmap<std::string, ork::dataflow::outplugbase*> OutPlugMapType;

  virtual void EnumerateChoices(GedItemNode* pnode, OutPlugMapType& Choices) = 0;
};

class IOpsDelegate;

struct OpsTask {
  IOpsDelegate* mpDelegate;
  ork::Object* _target;

  OpsTask()
      : mpDelegate(0)
      , _target(0) {
  }
};

class IOpsDelegate : public Object {
  RttiDeclareAbstract(IOpsDelegate, Object);

  typedef orkvector<OpsTask*> TaskList;
  static LockedResource<TaskList> gCurrentTasks;

  float mfProgress;

public:
  virtual void Execute(ork::Object* ptarget) = 0;
  float GetProgress() const {
    return mfProgress;
  }
  void SetProgress(float fv) {
    mfProgress = fv;
  }

  static void AddTask(ork::object::ObjectClass* pdelegclass, ork::Object* ptarget);
  static void RemoveTask(ork::object::ObjectClass* pdelegclass, ork::Object* ptarget);
  static OpsTask* GetTask(ork::object::ObjectClass* pdelegclass, ork::Object* ptarget);

protected:
  IOpsDelegate()
      : mfProgress(0.0f) {
  }
};
class OpsNode : public GedItemNode {
  void DoDraw(lev2::Context* pTARG) override;
  void OnMouseClicked(ork::ui::event_constptr_t ev) final;
  orkvector<std::pair<std::string, any64>> mOps;

  virtual bool DoDrawDefault() const final {
    return false;
  }

public:
  OpsNode(ObjModel& mdl, const char* name, const reflect::ObjectProperty* prop, ork::Object* obj);
};
class ObjectImportDelegate : public IOpsDelegate {
  RttiDeclareConcrete(ObjectImportDelegate, tool::ged::IOpsDelegate);
  void Execute(ork::Object* ptarget) final;
};
class ObjectExportDelegate : public IOpsDelegate {
  RttiDeclareConcrete(ObjectExportDelegate, tool::ged::IOpsDelegate);
  void Execute(ork::Object* ptarget) final;
};
///////////////////////////////////////////////////////////////////////////////
}}} // namespace ork::tool::ged
