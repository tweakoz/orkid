////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <string>
#include <map>
#include <memory>
#include <vector>

namespace ork::lev2::vulkan {

///////////////////////////////////////////////////////////////////////////////

struct VkMergedResourceBinding {
    enum class Type : uint32_t {
        Sampler = 0,
        UniformBlock = 1,
        StorageBuffer = 2
    };

    Type type = Type::Sampler;
    std::string name;
    std::string datatype;
    uint32_t binding_id = 0;
    std::string original_source;
    uint32_t stage_flags = 0; // VkShaderStageFlags for which stages use this binding
};

using vk_merged_resource_binding_ptr_t = std::shared_ptr<VkMergedResourceBinding>;

///////////////////////////////////////////////////////////////////////////////

struct VkDescriptorSetSource {
    std::string source_name;
    std::string source_type;
    std::vector<vk_merged_resource_binding_ptr_t> bindings;
};

using vk_descriptor_set_source_ptr_t = std::shared_ptr<VkDescriptorSetSource>;

///////////////////////////////////////////////////////////////////////////////

struct VkMergedResources {
    std::map<int, std::vector<vk_descriptor_set_source_ptr_t>> descriptor_sets;
};

using vk_merged_resources_ptr_t = std::shared_ptr<VkMergedResources>;

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::lev2::vulkan 