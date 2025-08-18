////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <map>
#include <string>
#include <unordered_map>

namespace ork::lev2::shadlang {

///////////////////////////////////////////////////////////////////////////////
// Common structures for resource merging and visualization
///////////////////////////////////////////////////////////////////////////////

struct MergedShaderResources {
    struct ResourceBinding {
        enum class Type { 
            Sampler, 
            UniformBlock, 
            SSBO 
        };
        
        Type type;
        std::string name;
        std::string datatype;
        int binding_id;
        std::string original_source; // Which original resource this came from
    };
    
    // Map: descriptor_set_id -> binding_id -> ResourceBinding
    std::map<int, std::map<int, ResourceBinding>> descriptor_sets;
    std::string shader_name;
};

using merged_resources_map_t = std::unordered_map<std::string, MergedShaderResources>;

///////////////////////////////////////////////////////////////////////////////
// Helper functions for resource merging
///////////////////////////////////////////////////////////////////////////////

// Get a human-readable string for resource type
inline std::string getResourceTypeString(MergedShaderResources::ResourceBinding::Type type) {
    switch (type) {
        case MergedShaderResources::ResourceBinding::Type::Sampler:
            return "Sampler";
        case MergedShaderResources::ResourceBinding::Type::UniformBlock:
            return "UniformBlock";
        case MergedShaderResources::ResourceBinding::Type::SSBO:
            return "SSBO";
        default:
            return "Unknown";
    }
}

// Get a color for visualization based on resource type
inline std::string getResourceTypeColor(MergedShaderResources::ResourceBinding::Type type) {
    switch (type) {
        case MergedShaderResources::ResourceBinding::Type::Sampler:
            return "#40ff40"; // Green for samplers
        case MergedShaderResources::ResourceBinding::Type::UniformBlock:
            return "#4040ff"; // Blue for uniform blocks
        case MergedShaderResources::ResourceBinding::Type::SSBO:
            return "#ff4040"; // Red for SSBOs
        default:
            return "#808080"; // Gray for unknown
    }
}

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::lev2::shadlang 