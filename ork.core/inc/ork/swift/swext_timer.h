////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#ifdef __cplusplus
namespace ork::swift { class OrkidHandleBase; }
typedef ork::swift::OrkidHandleBase OrkidHandleBase;
extern "C" {
#else
typedef struct OrkidHandleBase OrkidHandleBase;
#endif

// ================================================================
// Timer
// ================================================================

OrkidHandleBase* orkid_timer_create(void);
void orkid_timer_start(OrkidHandleBase* timer);
void orkid_timer_end(OrkidHandleBase* timer);
float orkid_timer_secs_since_start(const OrkidHandleBase* timer);
float orkid_timer_get_sync_time(void);

#ifdef __cplusplus
}
#endif
