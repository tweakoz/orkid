////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2025, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
namespace ork::swift { class OrkidHandleBase; }
typedef ork::swift::OrkidHandleBase OrkidHandleBase;
extern "C" {
#else
typedef struct OrkidHandleBase OrkidHandleBase;
#endif

// ================================================================
// VarMap
// ================================================================

OrkidHandleBase* orkid_varmap_create(void);
OrkidHandleBase* orkid_varmap_get(OrkidHandleBase* vmap, const char* key);
void orkid_varmap_set(OrkidHandleBase* vmap, const char* key, OrkidHandleBase* value);
void orkid_varmap_remove(OrkidHandleBase* vmap, const char* key);
bool orkid_varmap_contains(OrkidHandleBase* vmap, const char* key);
const char** orkid_varmap_keys(OrkidHandleBase* vmap, int32_t* out_count);
void orkid_varmap_free_keys(const char** keys, int32_t count);
int32_t orkid_varmap_size(OrkidHandleBase* vmap);
void orkid_varmap_clear(OrkidHandleBase* vmap);
OrkidHandleBase* orkid_varmap_clone(OrkidHandleBase* vmap);
void orkid_varmap_invoke_callback(OrkidHandleBase* vmap, const char* key);

#ifdef __cplusplus
}
#endif
