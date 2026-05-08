////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "headers/vulkan_ctx.h"
#include <ork/util/hexdump.inl>
#include <ctime>
#include <boost/filesystem.hpp>

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::vulkan {
///////////////////////////////////////////////////////////////////////////////
static logchannel_ptr_t logchan_vkpipcrep = logger()->configureChannel("VKPIPREP", fvec3(1, 1, .2), false);
///////////////////////////////////////////////////////////////////////////////



void VkFxInterface::_createPipelineReport(vkpipelinestate_ptr_t pipeline) {
  
  std::string report_filename;
  
  // Only generate report if debug flag is set
  // Generate pipeline report for debugging descriptor set issues
  
  auto shprog = _currentVKPASS;
  
  // Generate filename matching shader report schema
  // Use shader filename and technique name, process URI like in shader reports
  std::string shader_name_raw = shprog->_shader_file ? shprog->_shader_file->_shader_name : "unknown";
  
  // Process shader name to extract just the filename from URI (e.g., "orkshader://pbr.fxv2" -> "pbr.fxv2")
  file::Path shader_path  = shader_name_raw;
  auto shader_leaf        = shader_path.toBFS().leaf();
  std::string shader_name = shader_leaf.string();
  
  std::string technique_name = _currentORKTEK->_techniqueName;
  
  // Find pass index by searching through technique's passes
  int pass_num = 0;
  for (size_t i = 0; i < _currentVKTEK->_vk_passes.size(); ++i) {
    if (_currentVKTEK->_vk_passes[i].get() == _currentVKPASS) {
      pass_num = i;
      break;
    }
  }
  
  // Get stage directory
  const char* stage_env = std::getenv("OBT_STAGE");
  if (stage_env) {
    std::string stage_dir  = std::string(stage_env);
    std::string report_dir = stage_dir + "/vulkanpipe_reports";
    
    // Create directory if it doesn't exist
    file::Path report_path(report_dir);
    report_path.ensureDirectoryExists();
    
    // Generate report filename: shadername.technique.passnum.md
    report_filename = FormatString("%s/%s.%s.%d.md", report_dir.c_str(), shader_name.c_str(), technique_name.c_str(), pass_num);
    
    logchan_vkpipcrep->log("Pipeline report will be written to: %s", report_filename.c_str());
  }
  
  // Write the actual report if we have a filename and merged resources
  if (!report_filename.empty() && _currentVKPASS->_merged_resources) {
    FILE* fp = fopen(report_filename.c_str(), "w");
    if (fp) {
      // Header - use same shader name processing as filename
      fprintf(
          fp,
          "# Vulkan Pipeline Report: %s - %s - Pass %d\n",
          shader_name.c_str(),
          technique_name.c_str(),
          pass_num);
      time_t now = time(0);
      fprintf(fp, "Generated: %s", ctime(&now));
      
      // Descriptor Set Layout Creation
      fprintf(fp, "## Descriptor Set Layout Creation\n\n");
      
      int layout_index = 0;
      for (const auto& [set_id, sources] : _currentVKPASS->_merged_resources->descriptor_sets) {
        fprintf(fp, "### Descriptor Set %d\n\n", set_id);
        fprintf(fp, "**Layout Index:** %d | **Sources:** %zu\n\n", layout_index++, sources.size());
        
        // Table of bindings as created in layout
        fprintf(fp, "```\n");
        fprintf(fp, "Bind | Type         | Stages | Source              | Name\n");
        fprintf(fp, "-----|--------------|--------|---------------------|--------------------------------\n");
        
        // Collect all bindings for this set
        std::vector<std::tuple<int, std::string, std::string, std::string, std::string>> layout_bindings;
        
        for (const auto& source : sources) {
          for (const auto& binding : source->bindings) {
            std::string type_str;
            switch (binding->type) {
              case VkMergedResourceBinding::Type::Sampler:
                type_str = "Sampler";
                break;
              case VkMergedResourceBinding::Type::UniformBlock:
                type_str = "UBO";
                break;
              case VkMergedResourceBinding::Type::StorageBuffer:
                type_str = "SSBO";
                break;
              default:
                type_str = "Unknown";
                break;
            }
            
            layout_bindings.push_back(
                std::make_tuple(
                    binding->binding_id,
                    type_str,
                    "ALL_GFX", // We use VK_SHADER_STAGE_ALL_GRAPHICS
                    binding->original_source,
                    binding->name));
          }
        }
        
        // Sort by binding ID for clarity
        std::sort(layout_bindings.begin(), layout_bindings.end(), [](const auto& a, const auto& b) {
          return std::get<0>(a) < std::get<0>(b);
        });
        
        for (const auto& [bind_id, type, stages, source, name] : layout_bindings) {
          fprintf(fp, "%4d | %-12s | %-6s | %-19s | %s\n", bind_id, type.c_str(), stages.c_str(), source.c_str(), name.c_str());
        }
        fprintf(fp, "```\n\n");
      }
      
      fclose(fp);
      logchan_vkpipcrep->log("Pipeline report written to: %s", report_filename.c_str());
    }
  }
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::vulkan
///////////////////////////////////////////////////////////////////////////////