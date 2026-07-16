////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <cstdint>
#include <string>
#include <vector>

////////////////////////////////////////////////////////////////////////////////
// Generic external-GPU-requirements seam.
//
//  A producer that must influence how the graphics backend creates its Vulkan
//  instance/device (a VR/XR driver, a video-decode client, a compute peer, ...)
//  publishes an ExternalGpuRequirements before graphics init. The backend merges
//  the requested instance/device extensions with its own (never replacing the
//  MoltenVK portability set), clamps the requested API-version window, and — when
//  a specific physical device is required — selects that device.
//
//  Handles are carried as opaque uint64 (not VkInstance/VkPhysicalDevice/...) so
//  this header, and every consumer of it (e.g. the VR device base), stays free of
//  any Vulkan type dependency. The backend reinterpret_casts back to the concrete
//  Vulkan handle at the point of use. A zero handle means "none" (== VK_NULL_HANDLE).
////////////////////////////////////////////////////////////////////////////////

namespace ork::lev2 {

////////////////////////////////////////////////////////////////////////////////

struct ExternalGpuRequirements {
  std::vector<std::string> _instanceExtensions;
  std::vector<std::string> _deviceExtensions;
  uint64_t _requiredPhysicalDevice = 0; // opaque VkPhysicalDevice; 0 == VK_NULL_HANDLE
  uint32_t _minApiVersion          = 0; // 0 == no lower bound
  uint32_t _maxApiVersion          = 0; // 0 == no upper bound
};

////////////////////////////////////////////////////////////////////////////////
// GraphicsBindingInfo : the MAIN context's device+queue, handed back to the
//  producer after graphics init so it can bind its own session to them.
////////////////////////////////////////////////////////////////////////////////

struct GraphicsBindingInfo {
  uint64_t _vkInstance       = 0; // opaque VkInstance
  uint64_t _vkPhysicalDevice = 0; // opaque VkPhysicalDevice
  uint64_t _vkDevice         = 0; // opaque VkDevice
  uint32_t _queueFamilyIndex = 0;
  uint32_t _queueIndex       = 0;
};

////////////////////////////////////////////////////////////////////////////////

// Register requirements. MUST be called before the graphics backend creates its
// Vulkan instance; calling it afterwards asserts loud.
void setExternalGpuRequirements(const ExternalGpuRequirements& reqs);

// The registered requirements, or nullptr if nothing was registered.
const ExternalGpuRequirements* externalGpuRequirements();

// Backend-internal: mutable slot for fields that can only be resolved AFTER the
// Vulkan instance exists (e.g. the required physical device — its handle does not
// exist before instance creation). Creates the slot if absent; performs no
// post-init lock check (the backend is the trusted consumer, not an external
// producer). Do not use from outside the graphics backend.
ExternalGpuRequirements* _externalGpuRequirementsMutable();

// Backend-internal: mark graphics init done. Any later setExternalGpuRequirements
// asserts loud.
void _lockExternalGpuRequirements();

////////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
////////////////////////////////////////////////////////////////////////////////
