////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/rtti/RTTIX.inl>
#include <ork/kernel/core/singleton.h>
#include <ork/object/AutoConnector.h>
#include <ork/kernel/msgrouter.inl>
#include <ork/kernel/sigslot2.h>

namespace ork::lev2::editor {
////////////////////////////////////////////////////////////////////////////////

struct Editor;
struct ManipulatorInterface;
struct SelectionManager;

using editor_ptr_t = std::shared_ptr<Editor>;
using manipinterface_ptr_t  = std::shared_ptr<ManipulatorInterface>;
using selmgr_ptr_t = std::shared_ptr<SelectionManager>;

////////////////////////////////////////////////////////////////////////////////
} //namespace ork::lev2::editor {
