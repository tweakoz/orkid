////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <sigslot/signal.hpp>
#include <memory>

////////////////////////////////////////////////////////////////
namespace ork::sigslot2 {
////////////////////////////////////////////////////////////////

struct AutoConnector : sigslot::observer{
   inline ~AutoConnector() override{
        this->disconnect_all();
    }
};

using autoconnector_ptr_t = std::shared_ptr<AutoConnector>;

using signal_void_t = sigslot::signal<>;
using signal_objptr_t = sigslot::signal<object_ptr_t>;

////////////////////////////////////////////////////////////////
} // namespace ork::sigslot2 {

////////////////////////////////////////////////////////////////

/*#define DeclareAutoSlotX(SlotName) \
  ork::sigslot2::autoslot_ptr_t _autoslot_##SlotName;

#define ConstrucAutoSlotX(SlotName) \
   _autoslot_##SlotName(std::make_shared<ork::sigslot2::AutoSlot2>(#SlotName))
*/
////////////////////////////////////////////////////////////////
