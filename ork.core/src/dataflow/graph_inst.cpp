////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#include <ork/application/application.h>
#include <ork/dataflow/dataflow.h>
#include <ork/dataflow/scheduler.h>
#include <ork/kernel/orklut.hpp>
///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace dataflow {
///////////////////////////////////////////////////////////////////////////////
GraphInst::GraphInst()
    : _scheduler(nullptr)
    , mbInProgress(false) {
}
///////////////////////////////////////////////////////////////////////////////
GraphInst::~GraphInst() {
}
/*
///////////////////////////////////////////////////////////////////////////////
void GraphInst::doNotify(const ork::event::Event* event) {
  if (auto pev = dynamic_cast<const ItemRemovalEvent*>(event)) {
    if (pev->mProperty == GraphInst::GetClassStatic()->Description().property("Modules")) {
      std::string ps = pev->mKey.get<std::string>();
      object_ptr_t pobj  = pev->mOldValue.get<object_ptr_t>();
      delete pobj;
      return;
    }
  } else if (auto pev = dynamic_cast<const MapItemCreationEvent*>(event)) {
    if (pev->mProperty == GraphInst::GetClassStatic()->Description().property("Modules")) {
      std::string psname    = pev->mKey.get<std::string>();
      object_ptr_t pnewobj = pev->mNewItem.get<object_ptr_t>();
      dgmoduledata_ptr_t pdgmod     = rtti::autocast(pnewobj);

      pdgmod->SetParent(this);
      pdgmod->SetName(psname);
    }
  }
}
*/
///////////////////////////////////////////////////////////////////////////////
void GraphInst::Clear() {
  while (false == mModuleQueue.empty())
    mModuleQueue.pop();
  _scheduler        = nullptr;
  mbInProgress      = false;
}
///////////////////////////////////////////////////////////////////////////////
bool GraphInst::isDirty(void) const {
  for (auto item : _module_insts ) {
    auto module = typedModuleInst<DgModuleInst>(item);
    if(module->isDirty())
      return true;
  }
  return false;
}
///////////////////////////////////////////////////////////////////////////////
bool GraphInst::isPending() const {
  return mbInProgress;
}
///////////////////////////////////////////////////////////////////////////////
void GraphInst::setPending(bool bv) {
  mbInProgress = bv;
}
///////////////////////////////////////////////////////////////////////////////
/*void graph::SetModuleDirty( module* pmod )
{
    OrkAssert( _topologyDirty == false );
    //std::deque<module*>::const_iterator it = std::find( mModuleQueue.begin(), mModuleQueue.end(), pmod );
    //if( it == mModuleQueue.end() ) // NOT IN QUEUE ALREADY, so ADD IT
    //{
    //mModuleQueue.push_back( pmod );
    //}
    //if( _scheduler )
    //{
    //  _scheduler->QueueModule( pmod );
    //}
}*/
///////////////////////////////////////////////////////////////////////////////
}} // namespace ork::dataflow
///////////////////////////////////////////////////////////////////////////////
