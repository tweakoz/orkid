////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/lev2/gfx/shadlang.h>
#include <ork/util/parser_peg.h>
#include "shadlang_backend_spirv.h"

namespace ork::lev2::shadlang::spirv {
using namespace SHAST;
constexpr size_t MAX_PUSH_CONSTANT_SIZE = 4096; // Vulkan spec limit
/////////////////////////////////////////////////////////////////////////////////////////////////
struct LayoutStandard430 { // layout by glsl standard 430
  //////////////////////////////////////////////
  LayoutStandard430()
      : _cursor(0) {
  }
  void incrementDatatype(const std::string& dtname, size_t array_len = 1) {
    auto& block_sizes = SpirvCompilerGlobals::instance()->_block_data_sizes;
    auto it           = block_sizes.find(dtname);
    if (it == block_sizes.end()) {
      printf("dtname<%s> not found in block_sizes\n", dtname.c_str());
      OrkAssert(false);
    }
    size_t item_size = it->second;

    // Handle alignment
    if (dtname == "vec3" || dtname == "ivec3" || dtname == "uvec3") {
      item_size = 16; // Align to 16 bytes
    } else if (dtname == "mat3" || dtname == "imat3" || dtname == "umat3") {
      item_size = 48; // 3 vec3s each aligned to 16 bytes
    }

    // Handle arrays
    if (array_len > 0) {
      item_size *= array_len;

      // Arrays are aligned to the size of one element
      auto it = block_sizes.find(dtname);
      OrkAssert(it != block_sizes.end());
      size_t alignment = it->second;
      if (_cursor % alignment != 0) {
        _cursor += alignment - (_cursor % alignment);
      }
    }

    _cursor += item_size;
  }
  //////////////////////////////////////////////
  size_t cursor() const {
    return _cursor;
  }
  //////////////////////////////////////////////
  std::size_t _cursor;
};
/////////////////////////////////////////////////////////////////////////////////////////////////
SpirvCompilerGlobals::SpirvCompilerGlobals() {
  bool _vulkan            = true;
  _io_data_sizes["bool"]  = 1;
  _io_data_sizes["int"]   = 1;
  _io_data_sizes["uint"]  = 1;
  _io_data_sizes["float"] = 1;
  _io_data_sizes["vec2"]  = 1;
  _io_data_sizes["vec3"]  = 1;
  _io_data_sizes["vec4"]  = 1;
  _io_data_sizes["mat2"]  = 2;
  _io_data_sizes["mat3"]  = 3;
  _io_data_sizes["mat4"]  = 4;
  _io_data_sizes["ivec2"] = 1;
  _io_data_sizes["ivec3"] = 1;
  _io_data_sizes["ivec4"] = 1;
  _io_data_sizes["imat2"] = 2;
  _io_data_sizes["imat3"] = 3;
  _io_data_sizes["imat4"] = 4;
  _io_data_sizes["uvec2"] = 1;
  _io_data_sizes["uvec3"] = 1;
  _io_data_sizes["uvec4"] = 1;
  _io_data_sizes["umat2"] = 2;
  _io_data_sizes["umat3"] = 3;
  _io_data_sizes["umat4"] = 4;

  _block_data_sizes["bool"]  = 4;
  _block_data_sizes["int"]   = 4;
  _block_data_sizes["uint"]  = 4;
  _block_data_sizes["float"] = 4;
  _block_data_sizes["vec2"]  = 8;
  _block_data_sizes["vec3"]  = 12; // Note: Due to alignment, it will take up 16 bytes in a buffer!
  _block_data_sizes["vec4"]  = 16;
  _block_data_sizes["mat2"]  = 16; // 2 vec2s
  _block_data_sizes["mat3"]  = 36; // 3 vec3s, but due to alignment, it will take up more space!
  _block_data_sizes["mat4"]  = 64; // 4 vec4s
  _block_data_sizes["ivec2"] = 8;
  _block_data_sizes["ivec3"] = 12; // Same alignment note as vec3
  _block_data_sizes["ivec4"] = 16;
  _block_data_sizes["imat2"] = 16;
  _block_data_sizes["imat3"] = 36;
  _block_data_sizes["imat4"] = 64;
  _block_data_sizes["uvec2"] = 8;
  _block_data_sizes["uvec3"] = 12;
  _block_data_sizes["uvec4"] = 16;
  _block_data_sizes["umat2"] = 16;
  _block_data_sizes["umat3"] = 36;
  _block_data_sizes["umat4"] = 64;

  if (_vulkan) {
    _id_renames["ofx_instanceID"] = "gl_InstanceIndex";
    _id_renames["gl_VertexID"]    = "gl_VertexIndex";

  } else {
    _id_renames["ofx_depth"]      = "gl_FragDepth";
    _id_renames["ofx_instanceID"] = "gl_InstanceID";
  }
  _id_renames["PI"]          = "3.141592654";
  _id_renames["PI2"]         = "6.283185307";
  _id_renames["INV_PI"]      = "0.3183098861837907";
  _id_renames["INV_PI2"]     = "0.15915494309189535";
  _id_renames["PIDIV2"]      = "1.5707963267949";
  _id_renames["DEGTORAD"]    = "0.017453";
  _id_renames["RADTODEG"]    = "57.29578";
  _id_renames["E"]           = "2.718281828459";
  _id_renames["SQRT2"]       = "1.4142135623730951";
  _id_renames["GOLDENRATIO"] = "1.6180339887498948482";
  _id_renames["EPSILON"]     = "0.0000001";
  _id_renames["DTOR"]        = "0.017453292519943295";
  _id_renames["RTOD"]        = "57.29577951308232";

#if defined(__APPLE__)
  _id_renames["ORK_GPU_SHADER"] = "";
#else
  _id_renames["ORK_GPU_SHADER"] = "GL_NV_gpu_shader5";
#endif
}
/////////////////////////////////////////////////////////////////////////////////////////////////
spirvcompilerglobals_constptr_t SpirvCompilerGlobals::instance() {
  static spirvcompilerglobals_constptr_t _instance = std::make_shared<SpirvCompilerGlobals>();
  return _instance;
}
/////////////////////////////////////////////////////////////////////////////////////////////////
SpirvCompiler::SpirvCompiler(transunit_ptr_t transu, bool vulkan)
    : _transu(transu)
    , _vulkan(vulkan) {

  _processGlobalRenames();
  _convertSamplerSets();
  _convertUniformSets();
  _convertUniformBlocks();
  _convertStorageInterfaces();
}

/////////////////////////////////////////////////////////////////////////////////////////////////
void SpirvCompiler::_beginShader(shader_ptr_t shader) {

    printf("_beginShader<%s> try check interface'\n", shader->_name.c_str());

  _shader          = shader;
  _shader_group    = std::make_shared<MiscGroupNode>();
  _interface_group = std::make_shared<MiscGroupNode>();
  _extension_group = std::make_shared<MiscGroupNode>();
  _uniforms_group  = std::make_shared<MiscGroupNode>();
  _libraries_group = std::make_shared<MiscGroupNode>();
  _types_group     = std::make_shared<MiscGroupNode>();
  _input_index     = 0;
  _output_index    = 0;

  _collected_uniform_sets.clear();

  /////////////////////////////////////////////////
  // process shader inheritances
  /////////////////////////////////////////////////
  InheritanceTracker tracker(_transu);
  _binding_id = 0;

  // Determine shader type
  bool is_vertex_shader   = (std::dynamic_pointer_cast<VertexShader>(shader) != nullptr);
  bool is_fragment_shader = (std::dynamic_pointer_cast<FragmentShader>(shader) != nullptr);
  bool is_geometry_shader = (std::dynamic_pointer_cast<GeometryShader>(shader) != nullptr);
  bool is_compute_shader  = (std::dynamic_pointer_cast<ComputeShader>(shader) != nullptr);

  ////////////////////////////////////////////////
  tracker._onInheritLibrary = [&](std::string INHID, libblock_ptr_t lib_block) { //
    _inheritLibrary(lib_block);
  };
  ////////////////////////////////////////////////
  tracker._onInheritTypes = [&](std::string INHID, typeblock_ptr_t typ_block) { //
    _inheritTypes(typ_block);
  };
  ////////////////////////////////////////////////
  tracker._onInheritSamplerSet = [=](std::string INHID, astnode_ptr_t sset) { //
    auto it_sset = _spirvsamplersets.find(INHID);
    OrkAssert(it_sset != _spirvsamplersets.end());
    auto spirvsmpset = it_sset->second;
    _inheritSamplerSet(INHID, spirvsmpset);
  };
  ////////////////////////////////////////////////
  tracker._onInheritUniformSet = [=](std::string INHID, astnode_ptr_t uset) { //
    auto it_uset = _spirvuniformsets.find(INHID);
    OrkAssert(it_uset != _spirvuniformsets.end());
    auto spirvuniset = it_uset->second;
    _inheritUniformSet(INHID, spirvuniset);
  };
  ////////////////////////////////////////////////
  tracker._onInheritUniformBlk = [=](std::string INHID, astnode_ptr_t ublk) { //
    auto it_ublk = _spirvuniformblks.find(INHID);
    OrkAssert(it_ublk != _spirvuniformblks.end());
    auto spirvuniblk = it_ublk->second;
    _inheritUniformBlk(INHID, spirvuniblk);
  };
  ////////////////////////////////////////////////
  tracker._onInheritStorageInterface = [=](std::string INHID, astnode_ptr_t interface_node) { //
    printf("sh<%s> Processing inheritance of storage interface '%s'\n", shader->_name.c_str(), INHID.c_str());
    
    auto it_storage = _spirvstorageinterfaces.find(INHID);
    if (it_storage != _spirvstorageinterfaces.end()) {
      auto spirvstorageif = it_storage->second;
      _inheritStorageInterface(INHID, spirvstorageif);
    } else {
      printf("WARNING: Storage interface '%s' not found in processed storage interfaces\n", INHID.c_str());
    }
  };
  ////////////////////////////////////////////////
  tracker._onInheritInterface = [=](std::string INHID, astnode_ptr_t interface_node) { //

    printf("sh<%s> Processing inheritance of interface '%s'\n", shader->_name.c_str(), INHID.c_str());

    // Only inherit the appropriate interface type for each shader
    bool is_vertex_interface   = (std::dynamic_pointer_cast<VertexInterface>(interface_node) != nullptr);
    bool is_fragment_interface = (std::dynamic_pointer_cast<FragmentInterface>(interface_node) != nullptr);
    bool is_geometry_interface = (std::dynamic_pointer_cast<GeometryInterface>(interface_node) != nullptr);
    bool is_compute_interface  = (std::dynamic_pointer_cast<ComputeInterface>(interface_node) != nullptr);

    // Skip vertex interfaces when processing fragment shaders
    // (they will be handled by the fragment interface inheritance)
    if (is_fragment_shader && is_vertex_interface) {
      return;
    }

    // Skip fragment interfaces when processing vertex shaders
    if (is_vertex_shader && is_fragment_interface) {
      return;
    }

    _inheritIO(interface_node);
  };
  ////////////////////////////////////////////////
  tracker._onInheritExtension = [=](std::string INHID, astnode_ptr_t ast_node) { //
    auto as_ext_node = std::dynamic_pointer_cast<SemaInheritExtension>(ast_node);
    OrkAssert(as_ext_node);
    _inheritExtension(as_ext_node);
  };
  ////////////////////////////////////////////////
  tracker.fetchInheritances(shader);
  /////////////////////////////////////////////////
}
/////////////////////////////////////////////////////////////////////////////////////////////////
void SpirvCompiler::processShader(shader_ptr_t sh) {
  _beginShader(sh);
  if (auto as_vsh = std::dynamic_pointer_cast<VertexShader>(sh)) {
    _compileShader(shaderc_glsl_vertex_shader);
  } else if (auto as_gsh = std::dynamic_pointer_cast<GeometryShader>(sh)) {
    _compileShader(shaderc_glsl_geometry_shader);
  } else if (auto as_fsh = std::dynamic_pointer_cast<FragmentShader>(sh)) {
    _compileShader(shaderc_glsl_fragment_shader);
  } else if (auto as_csh = std::dynamic_pointer_cast<ComputeShader>(sh)) {
    _compileShader(shaderc_glsl_compute_shader);
  } else {
    OrkAssert(false);
  }
}
/////////////////////////////////////////////////////////////////////////////////////////////////
void SpirvCompiler::_appendText(miscgroupnode_ptr_t grp, const char* formatstring, ...) {
  char formatbuffer[512];
  va_list args;
  va_start(args, formatstring);
  vsnprintf(&formatbuffer[0], sizeof(formatbuffer), formatstring, args);
  va_end(args);
  grp->appendTypedChild<InsertLine>(formatbuffer);
}
/////////////////////////////////////////////////////////////////////////////////////////////////
void SpirvCompiler::_processGlobalRenames() {

  const auto& RENAMES = SpirvCompilerGlobals::instance()->_id_renames;

  auto sema_identifiers = SHAST::AstNode::collectNodesOfType<SHAST::SemaIdentifier>(_transu);
  for (auto it : sema_identifiers) {
    auto id     = it->typedValueForKey<std::string>("identifier_name").value();
    auto it_ren = RENAMES.find(id);
    if (it_ren != RENAMES.end()) {
      auto newid = it_ren->second;
      it->setValueForKey<std::string>("identifier_name", newid);
    }
  }
  auto prim_identifiers = SHAST::AstNode::collectNodesOfType<SHAST::PrimaryIdentifier>(_transu);
  for (auto it : prim_identifiers) {
    auto id     = it->typedValueForKey<std::string>("identifier_name").value();
    auto it_ren = RENAMES.find(id);
    if (it_ren != RENAMES.end()) {
      auto newid = it_ren->second;
      it->setValueForKey<std::string>("identifier_name", newid);
    }
  }
}
/////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////
void SpirvCompiler::_convertSamplerSets() {
  auto ast_smpsets = SHAST::AstNode::collectNodesOfType<SHAST::SamplerSet>(_transu);

  //////////////////////////////////////////////////////////////////////////
  // Stage 1: Create all sampler sets, parse their direct properties
  //////////////////////////////////////////////////////////////////////////
  for (auto ast_smpset : ast_smpsets) {
    auto smpset_name               = ast_smpset->typedValueForKey<std::string>("object_name").value();
    auto smpset                    = std::make_shared<SpirvSamplerSet>();
    _spirvsamplersets[smpset_name] = smpset;
    smpset->_name                  = smpset_name;
    smpset->_descriptor_set_id     = -1; // Initialize to invalid value

    // Check for direct DescriptorSetId
    auto dsetids = SHAST::AstNode::collectNodesOfType<SHAST::DescriptorSetId>(ast_smpset);
    if (dsetids.size() == 1) {
      int dset_id                = dsetids[0]->typedValueForKey<int>("descriptor_set_id").value();
      smpset->_descriptor_set_id = dset_id;
      OrkAssert((dset_id >= 0) and (dset_id <= 4));
    }

    // Process local sampler declarations
    auto sampler_declarations = SHAST::AstNode::collectNodesOfType<SHAST::SamplerDeclaration>(ast_smpset);
    for (auto decl : sampler_declarations) {
      auto sampler_type = decl->childAs<SHAST::SamplerType>(0);
      OrkAssert(sampler_type);
      auto smp_typename = sampler_type->typedValueForKey<std::string>("sampler_type").value();
      auto semaid       = decl->childAs<SemaIdentifier>(1);
      auto smp_name     = semaid->typedValueForKey<std::string>("identifier_name").value();

      auto sampler                        = std::make_shared<SpirvSampler>();
      sampler->_datatype                  = smp_typename;
      sampler->_identifier                = smp_name;
      smpset->_samplers_by_name[smp_name] = sampler;
    }
  }

  //////////////////////////////////////////////////////////////////////////
  // Stage 2: Process inheritance
  //////////////////////////////////////////////////////////////////////////
  for (auto ast_smpset : ast_smpsets) {
    auto smpset_name = ast_smpset->typedValueForKey<std::string>("object_name").value();
    auto smpset      = _spirvsamplersets[smpset_name];

    auto inherit_items = SHAST::AstNode::collectNodesOfType<SHAST::InheritListItem>(ast_smpset);

    if (inherit_items.size() > 0) {
      bool parent_was_found = false;

      for (auto inherit_item : inherit_items) {
        auto inherit_obj = inherit_item->typedValueForKey<std::string>("inherited_object").value();

        // Check if the inherited item is a SamplerSet
        auto parent_it = _spirvsamplersets.find(inherit_obj);
        if (parent_it != _spirvsamplersets.end()) {
          auto parent_smpset = parent_it->second;

          // Inherit descriptor set ID
          smpset->_descriptor_set_id = parent_smpset->_descriptor_set_id;

          // Create new map with parent samplers first, then local samplers
          std::unordered_map<std::string, spirvsampler_ptr_t> new_samplers_by_name;

          // First add all parent samplers
          for (auto parent_sampler_item : parent_smpset->_samplers_by_name) {
            auto sampler_name   = parent_sampler_item.first;
            auto parent_sampler = parent_sampler_item.second;

            // Clone the sampler
            auto inherited_sampler         = std::make_shared<SpirvSampler>();
            inherited_sampler->_datatype   = parent_sampler->_datatype;
            inherited_sampler->_identifier = parent_sampler->_identifier;

            new_samplers_by_name[sampler_name] = inherited_sampler;
          }

          // Then add local samplers (checking for duplicates)
          for (auto local_sampler_item : smpset->_samplers_by_name) {
            auto sampler_name  = local_sampler_item.first;
            auto local_sampler = local_sampler_item.second;

            // Check for duplicates
            auto it = new_samplers_by_name.find(sampler_name);
            if (it != new_samplers_by_name.end()) {
              printf("SamplerSet<%s> redefines inherited sampler<%s> - not allowed!\n", smpset_name.c_str(), sampler_name.c_str());
              OrkAssert(false);
            }

            new_samplers_by_name[sampler_name] = local_sampler;
          }

          // Replace the sampler set's samplers with the new combined map
          smpset->_samplers_by_name = new_samplers_by_name;

          parent_was_found = true;
          break; // Only inherit from first valid SamplerSet parent
        }
      }

      if (!parent_was_found) {
        printf(
            "SamplerSet<%s> inherits from another SamplerSet<%s> which is not found!\n",
            smpset_name.c_str(),
            inherit_items[0]->typedValueForKey<std::string>("inherited_object").value().c_str());
        OrkAssert(false);
      }
    } else {
      // No inheritance - must have direct descriptor set ID
      if (smpset->_descriptor_set_id < 0) {
        printf("SamplerSet<%s> must have either DescriptorSetId or inherit from another SamplerSet!\n", smpset_name.c_str());
        OrkAssert(false);
      }
    }
  }
}
/////////////////////////////////////////////////////////////////////////////////////////////////
void SpirvCompiler::_convertUniformSets() {
  auto ast_unisets = SHAST::AstNode::collectNodesOfType<SHAST::UniformSet>(_transu);
  // const auto& DATASIZES = SpirvCompilerGlobals::instance()->_data_sizes;
  for (auto ast_uniset : ast_unisets) {
    auto decls = SHAST::AstNode::collectNodesOfType<SHAST::DataDeclarationBase>(ast_uniset);
    //////////////////////////////////////
    auto uni_name               = ast_uniset->typedValueForKey<std::string>("object_name").value();
    auto uniset                 = std::make_shared<SpirvUniformSet>();
    _spirvuniformsets[uni_name] = uniset;
    uniset->_name               = uni_name;
    //////////////////////////////////////
    LayoutStandard430 layout;
    for (auto d : decls) {
      auto tid = d->childAs<SHAST::TypedIdentifier>(0);
      OrkAssert(tid);
      auto dt                    = tid->typedValueForKey<std::string>("data_type").value();
      auto id                    = tid->typedValueForKey<std::string>("identifier_name").value();
      auto item                  = std::make_shared<SpirvUniformSetItem>();
      item->_datatype            = dt;
      item->_identifier          = id;
      uniset->_items_by_name[id] = item;
      uniset->_items_by_order.push_back(item);
      item->_offset = layout.cursor();

      if (auto as_array = std::dynamic_pointer_cast<ArrayDeclaration>(d)) {
        auto len_node       = as_array->childAs<SHAST::SemaIntegerLiteral>(1);
        item->_is_array     = true;
        auto ary_len_str    = len_node->typedValueForKey<std::string>("literal_value").value();
        item->_array_length = atoi(ary_len_str.c_str());

        layout.incrementDatatype(dt, item->_array_length);
        // offset += item_size*item->_array_length;
      } else {
        layout.incrementDatatype(dt, 0);
      }
      if (layout.cursor() > MAX_PUSH_CONSTANT_SIZE) {
        printf("uniset<%s> pushconstant overflow length<%zu>\n", uni_name.c_str(), layout.cursor());
        OrkAssert(false);
      }
    }
    //////////////////////////////////////
  }
}
/////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////
void SpirvCompiler::_convertUniformBlocks() {
  auto ast_uniblks = SHAST::AstNode::collectNodesOfType<SHAST::UniformBlk>(_transu);

  //////////////////////////////////////////////////////////////////////////
  // Stage 1: Create all uniform blocks, parse their direct properties
  //////////////////////////////////////////////////////////////////////////
  for (auto ast_uniblk : ast_uniblks) {
    auto uni_name               = ast_uniblk->typedValueForKey<std::string>("object_name").value();
    auto uniblk                 = std::make_shared<SpirvUniformBlock>();
    _spirvuniformblks[uni_name] = uniblk;
    uniblk->_name               = uni_name;
    uniblk->_descriptor_set_id  = -1; // Initialize to invalid value

    // Check for direct DescriptorSetId
    auto dsetids = SHAST::AstNode::collectNodesOfType<SHAST::DescriptorSetId>(ast_uniblk);
    if (dsetids.size() == 1) {
      int dset_id                = dsetids[0]->typedValueForKey<int>("descriptor_set_id").value();
      uniblk->_descriptor_set_id = dset_id;
      OrkAssert((dset_id >= 0) and (dset_id <= 4));
      if (0)
        printf("uniblk dset_id<%d>\n", dset_id);
    }

    // Parse local data declarations
    auto decls = SHAST::AstNode::collectNodesOfType<SHAST::DataDeclarationBase>(ast_uniblk);
    LayoutStandard430 layout;

    for (auto d : decls) {
      auto tid = d->childAs<SHAST::TypedIdentifier>(0);
      OrkAssert(tid);
      auto dt = tid->typedValueForKey<std::string>("data_type").value();
      auto id = tid->typedValueForKey<std::string>("identifier_name").value();

      auto it = dt.find("sampler");
      if (it != std::string::npos) {
        printf("sampler<%s:%s> in uniform block<%s> not allowed!\n", dt.c_str(), id.c_str(), uni_name.c_str());
        OrkAssert(false);
      }

      auto item         = std::make_shared<SpirvUniformBlockItem>();
      item->_datatype   = dt;
      item->_offset     = layout.cursor();
      item->_identifier = id;

      uniblk->_items_by_name[id] = item;
      uniblk->_items_by_order.push_back(item);

      if (auto as_array = std::dynamic_pointer_cast<ArrayDeclaration>(d)) {
        auto len_node       = as_array->childAs<SHAST::SemaIntegerLiteral>(1);
        item->_is_array     = true;
        auto ary_len_str    = len_node->typedValueForKey<std::string>("literal_value").value();
        item->_array_length = atoi(ary_len_str.c_str());
        layout.incrementDatatype(dt, item->_array_length);
      } else {
        layout.incrementDatatype(dt, 0);
      }
    }

    if (layout.cursor() > 65536) {
      printf("uniblk<%s> buffer overflow length<%zu>\n", uni_name.c_str(), layout.cursor());
      OrkAssert(false);
    }
  }

  //////////////////////////////////////////////////////////////////////////
  // Stage 2: Process inheritance
  //////////////////////////////////////////////////////////////////////////
  for (auto ast_uniblk : ast_uniblks) {
    auto uni_name = ast_uniblk->typedValueForKey<std::string>("object_name").value();
    auto uniblk   = _spirvuniformblks[uni_name];

    auto inherit_items = SHAST::AstNode::collectNodesOfType<SHAST::InheritListItem>(ast_uniblk);

    if (inherit_items.size() > 0) {
      bool parent_was_found = false;

      for (auto inherit_item : inherit_items) {
        auto inherit_obj = inherit_item->typedValueForKey<std::string>("inherited_object").value();

        // Check if the inherited item is a UniformBlock
        auto parent_it = _spirvuniformblks.find(inherit_obj);
        if (parent_it != _spirvuniformblks.end()) {
          auto parent_uniblk = parent_it->second;

          // Inherit descriptor set ID
          uniblk->_descriptor_set_id = parent_uniblk->_descriptor_set_id;

          // Prepend parent items to the beginning
          std::vector<spirvuniblkitem_ptr_t> new_items_by_order;
          std::unordered_map<std::string, spirvuniblkitem_ptr_t> new_items_by_name;

          // First add all parent items
          LayoutStandard430 new_layout;
          for (auto parent_item : parent_uniblk->_items_by_order) {
            // Clone the item
            auto inherited_item           = std::make_shared<SpirvUniformBlockItem>();
            inherited_item->_datatype     = parent_item->_datatype;
            inherited_item->_identifier   = parent_item->_identifier;
            inherited_item->_offset       = new_layout.cursor();
            inherited_item->_is_array     = parent_item->_is_array;
            inherited_item->_array_length = parent_item->_array_length;

            new_items_by_order.push_back(inherited_item);
            new_items_by_name[inherited_item->_identifier] = inherited_item;

            // Update layout cursor
            if (inherited_item->_is_array) {
              new_layout.incrementDatatype(inherited_item->_datatype, inherited_item->_array_length);
            } else {
              new_layout.incrementDatatype(inherited_item->_datatype, 0);
            }
          }

          // Then add local items (with updated offsets)
          for (auto local_item : uniblk->_items_by_order) {
            // Check for duplicates
            auto it = new_items_by_name.find(local_item->_identifier);
            if (it != new_items_by_name.end()) {
              printf(
                  "UniformBlock<%s> redefines inherited item<%s> - not allowed!\n",
                  uni_name.c_str(),
                  local_item->_identifier.c_str());
              OrkAssert(false);
            }

            // Update offset based on inherited items
            local_item->_offset = new_layout.cursor();

            new_items_by_order.push_back(local_item);
            new_items_by_name[local_item->_identifier] = local_item;

            // Update layout cursor
            if (local_item->_is_array) {
              new_layout.incrementDatatype(local_item->_datatype, local_item->_array_length);
            } else {
              new_layout.incrementDatatype(local_item->_datatype, 0);
            }
          }

          // Replace the block's items with the new combined list
          uniblk->_items_by_order = new_items_by_order;
          uniblk->_items_by_name  = new_items_by_name;

          parent_was_found = true;
          break; // Only inherit from first valid UniformBlock parent
        }
      }

      if (!parent_was_found) {
        printf(
            "UniformBlock<%s> inherits from another UniformBlock<%s> which is not found!\n",
            uni_name.c_str(),
            inherit_items[0]->typedValueForKey<std::string>("inherited_object").value().c_str());
        OrkAssert(false);
      }
    } else {
      // No inheritance - must have direct descriptor set ID
      if (uniblk->_descriptor_set_id < 0) {
        printf("UniformBlock<%s> must have either DescriptorSetId or inherit from another UniformBlock!\n", uni_name.c_str());
        OrkAssert(false);
      }
    }
  }
}
/////////////////////////////////////////////////////////////////////////////////////////////////
void SpirvCompiler::_convertStorageInterfaces() {
  auto ast_storage_ifs = SHAST::AstNode::collectNodesOfType<SHAST::StorageInterface>(_transu);
  
  for (auto ast_storage_if : ast_storage_ifs) {
    auto storage_name = ast_storage_if->typedValueForKey<std::string>("object_name").value();
    auto spirv_sif = std::make_shared<SpirvStorageInterface>();
    _spirvstorageinterfaces[storage_name] = spirv_sif;
    spirv_sif->_name = storage_name;
    
    // Get descriptor set ID
    auto dsid_node = ast_storage_if->findFirstChildOfType<DescriptorSetId>();
    if (dsid_node) {
      spirv_sif->_descriptor_set_id = dsid_node->typedValueForKey<int>("descriptor_set_id").value();
    }
    
    // Get the storage interface item (buffer block)
    auto sitem_node = ast_storage_if->findFirstChildOfType<StorageInterfaceItem>();
    if (sitem_node) {
      // Get buffer name
      auto sitemn_node = sitem_node->findFirstChildOfType<StorageInterfaceItemName>();
      if (sitemn_node) {
        auto semaid_nodes = AstNode::collectNodesOfType<SemaIdentifier>(sitemn_node);
        if (!semaid_nodes.empty()) {
          spirv_sif->_buffer_name = getSemaIdString(semaid_nodes[0]);
        }
      }
      
      // Parse buffer members using std430 layout
      auto decls = sitem_node->findFirstChildOfType<DataDeclarations>();
      if (decls) {
        LayoutStandard430 layout;
        for (auto decl_sub : decls->_children) {
          if (auto as_ddecl = std::dynamic_pointer_cast<DataDeclaration>(decl_sub)) {
            auto tid = as_ddecl->childAs<TypedIdentifier>(0);
            if (tid) {
              auto dt = tid->typedValueForKey<std::string>("data_type").value();
              auto id = tid->typedValueForKey<std::string>("identifier_name").value();
              
              auto item = std::make_shared<SpirvStorageInterfaceItem>();
              item->_datatype = dt;
              item->_identifier = id;
              item->_is_array = false;

              // Calculate offset, size, and stride using std430 rules
              auto& block_sizes = SpirvCompilerGlobals::instance()->_block_data_sizes;
              auto size_it = block_sizes.find(dt);
              if (size_it != block_sizes.end()) {
                item->_offset = layout.cursor();
                item->_size = size_it->second;

                // Handle special alignments for vec3 and mat3
                if (dt == "vec3" || dt == "ivec3" || dt == "uvec3") {
                  item->_stride = 16; // vec3 aligns to 16
                } else if (dt == "mat3" || dt == "imat3" || dt == "umat3") {
                  item->_size = 48; // 3 columns × 16 bytes
                  item->_stride = 48;
                } else {
                  item->_stride = item->_size;
                }

                layout.incrementDatatype(dt);
              } else {
                printf("WARNING: Unknown datatype '%s' in storage interface\n", dt.c_str());
                item->_offset = layout.cursor();
                item->_size = 0;
                item->_stride = 0;
              }
              
              spirv_sif->_items_by_name[id] = item;
              spirv_sif->_items_by_order.push_back(item);
            }
          } else if (auto as_adecl = std::dynamic_pointer_cast<ArrayDeclaration>(decl_sub)) {
            auto tid = as_adecl->childAs<TypedIdentifier>(0);
            if (tid) {
              auto dt = tid->typedValueForKey<std::string>("data_type").value();
              auto id = tid->typedValueForKey<std::string>("identifier_name").value();
              auto len_node = as_adecl->childAs<SemaIntegerLiteral>(1);
              auto ary_len_str = len_node->typedValueForKey<std::string>("literal_value").value();
              auto ary_len = atoi(ary_len_str.c_str());
              
              auto item = std::make_shared<SpirvStorageInterfaceItem>();
              item->_datatype = dt;
              item->_identifier = id;
              item->_is_array = true;
              item->_array_length = ary_len;

              // Calculate offset, size, and stride for array using std430 rules
              auto& block_sizes = SpirvCompilerGlobals::instance()->_block_data_sizes;
              auto size_it = block_sizes.find(dt);
              if (size_it != block_sizes.end()) {
                item->_offset = layout.cursor();
                item->_size = size_it->second;

                // Calculate stride (spacing between array elements)
                if (dt == "vec3" || dt == "ivec3" || dt == "uvec3") {
                  item->_stride = 16; // vec3 aligns to 16
                } else if (dt == "mat3" || dt == "imat3" || dt == "umat3") {
                  item->_size = 48; // 3 columns × 16 bytes
                  item->_stride = 48;
                } else if (dt.find("mat") != std::string::npos) {
                  // All matrices align to 16 bytes
                  item->_stride = ((item->_size + 15) / 16) * 16;
                } else {
                  // For scalars and vectors, stride equals size (except vec3)
                  item->_stride = item->_size;
                }

                layout.incrementDatatype(dt, ary_len);
              } else {
                printf("WARNING: Unknown datatype '%s' in storage interface array\n", dt.c_str());
                item->_offset = layout.cursor();
                item->_size = 0;
                item->_stride = 0;
              }
              
              spirv_sif->_items_by_name[id] = item;
              spirv_sif->_items_by_order.push_back(item);
            }
          }
        }
        spirv_sif->_buffer_size = layout.cursor();
      }
    }
  }
}
/////////////////////////////////////////////////////////////////////////////////////////////////

void SpirvCompiler::_inheritSamplerSet(
    std::string unisetname,        //
    spirvsmpset_ptr_t spirvsset) { //
  OrkAssert((spirvsset->_descriptor_set_id >= 0) and (spirvsset->_descriptor_set_id <= 4));
  /////////////////////
  // samplers
  /////////////////////
  for (auto item : spirvsset->_samplers_by_name) {
    auto dt = item.second->_datatype;
    auto id = item.second->_identifier;

    // Try to find binding ID from merged resources first
    int binding_id = _findBindingIdFromMergedResources(id, unisetname);
    if (binding_id == -1) {
      // Fallback to original behavior if not found in merged resources
      binding_id = _binding_id;
      _binding_id++;
    }

    auto line = FormatString(
        "layout(set=%zu, binding=%d) uniform %s %s;", //
        spirvsset->_descriptor_set_id,                //
        binding_id,                                   //
        dt.c_str(),                                   //
        id.c_str());
    _appendText(_uniforms_group, line.c_str());
  }
}

/////////////////////////////////////////////////////////////////////////////////////////////////

void SpirvCompiler::_inheritUniformSet(
    std::string unisetname,        //
    spirvuniset_ptr_t spirvuset) { //
  if (_vulkan) {
    // Just collect for now, don't emit
    _collected_uniform_sets.push_back(spirvuset);
  } else { // opengl
    OrkAssert(false);
  }
}

/////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////
void SpirvCompiler::_emitMergedPushConstants() {
  if (!_vulkan || _collected_uniform_sets.empty()) {
    return;
  }

  // Calculate total size and merge all items
  LayoutStandard430 merged_layout;
  std::vector<std::pair<spirvunisetitem_ptr_t, std::string>> all_items; // item, source_set_name

  for (auto spirvuset : _collected_uniform_sets) {
    for (auto item : spirvuset->_items_by_order) {
      // Check for duplicate names
      for (const auto& existing : all_items) {
        if (existing.first->_identifier == item->_identifier) {
          printf("ERROR: Duplicate uniform name '%s' found in uniform sets!\n", item->_identifier.c_str());
          OrkAssert(false);
        }
      }

      // Clone item with new offset
      auto merged_item           = std::make_shared<SpirvUniformSetItem>();
      merged_item->_datatype     = item->_datatype;
      merged_item->_identifier   = item->_identifier;
      merged_item->_is_array     = item->_is_array;
      merged_item->_array_length = item->_array_length;
      merged_item->_offset       = merged_layout.cursor();

      all_items.push_back({merged_item, spirvuset->_name});

      // Update layout
      if (merged_item->_is_array) {
        merged_layout.incrementDatatype(merged_item->_datatype, merged_item->_array_length);
      } else {
        merged_layout.incrementDatatype(merged_item->_datatype, 0);
      }
    }
  }

  // Check size limit AFTER collecting all items
  size_t total_size = merged_layout.cursor();
  if (total_size > MAX_PUSH_CONSTANT_SIZE) {
    printf("ERROR: Combined push_constant size %zu exceeds 4096 byte limit!\n", total_size);
    printf("Uniform sets included:\n");
    for (auto spirvuset : _collected_uniform_sets) {
      printf("  - %s\n", spirvuset->_name.c_str());
    }
    OrkAssert(false);
  }

  if (all_items.size()) {
    // Emit single push_constant block ONCE, outside the loop
    _appendText(_uniforms_group, "layout(push_constant) uniform PushConstants {");

    // Add comment showing which sets were merged
    std::string sets_comment = "  // Merged from: ";
    for (size_t i = 0; i < _collected_uniform_sets.size(); i++) {
      if (i > 0)
        sets_comment += ", ";
      sets_comment += _collected_uniform_sets[i]->_name;
    }
    _appendText(_uniforms_group, sets_comment.c_str());

    // Emit all items
    for (const auto& item_pair : all_items) {
      auto item       = item_pair.first;
      auto source_set = item_pair.second;

      std::string line = "  ";
      if (item->_is_array) {
        line += FormatString(
            "%s %s[%zu]; // from %s", item->_datatype.c_str(), item->_identifier.c_str(), item->_array_length, source_set.c_str());
      } else {
        line += FormatString("%s %s; // from %s", item->_datatype.c_str(), item->_identifier.c_str(), source_set.c_str());
      }
      _appendText(_uniforms_group, line.c_str());
    }

    _appendText(_uniforms_group, "};");
  }

  // Clear for next shader
  _collected_uniform_sets.clear();
}
/////////////////////////////////////////////////////////////////////////////////////////////////
void SpirvCompiler::_inheritLibrary(libblock_ptr_t lib_block) {

  auto libname = lib_block->typedValueForKey<std::string>("object_name").value();

  auto decorator = FormatString("// begin library<%s>", libname.c_str());
  _appendText(_libraries_group, decorator.c_str());

  auto lib_children   = lib_block->_children;
  auto libgroup       = _libraries_group->appendTypedChild<MiscGroupNode>();
  libgroup->_children = lib_children;

  decorator = FormatString("// end library<%s>", libname.c_str());
  _appendText(_libraries_group, decorator.c_str());
}
/////////////////////////////////////////////////////////////////////////////////////////////////
void SpirvCompiler::_inheritTypes(typeblock_ptr_t typ_block) {

  auto libname = typ_block->typedValueForKey<std::string>("object_name").value();

  auto decorator = FormatString("// begin types<%s>", libname.c_str());
  _appendText(_types_group, decorator.c_str());

  auto typ_children   = typ_block->_children;
  auto libgroup       = _types_group->appendTypedChild<MiscGroupNode>();
  libgroup->_children = typ_children;

  decorator = FormatString("// end types<%s>", libname.c_str());
  _appendText(_types_group, decorator.c_str());
}
/////////////////////////////////////////////////////////////////////////////////////////////////
void SpirvCompiler::_inheritUniformBlk(
    std::string uniblkname,        //
    spirvuniblk_ptr_t spirvublk) { //
  OrkAssert((spirvublk->_descriptor_set_id >= 0) and (spirvublk->_descriptor_set_id <= 4));
  if (_vulkan) {
    size_t _offset = 0;
    /////////////////////
    // loose unis
    /////////////////////

    // Try to find binding ID from merged resources first
    int binding_id = _findBindingIdFromMergedResources(uniblkname, uniblkname);
    if (binding_id == -1) {
      // Fallback to original behavior if not found in merged resources
      binding_id = _binding_id;
      _binding_id++;
    }

    auto line = FormatString(
        "layout(set=%zu, binding=%d) uniform %s {", //
        spirvublk->_descriptor_set_id,              //
        binding_id,                                 //
        uniblkname.c_str());
    _appendText(_uniforms_group, line.c_str());
    for (auto item : spirvublk->_items_by_order) {
      auto dt = item->_datatype;
      auto id = item->_identifier;
      if (item->_is_array) {
        size_t array_len = item->_array_length;
        auto str         = FormatString("%s %s[%zu];", dt.c_str(), id.c_str(), array_len);
        _appendText(_uniforms_group, str.c_str());
      } else {
        _appendText(_uniforms_group, (dt + " " + id + ";").c_str());
      }
    }
    _appendText(_uniforms_group, "};");
  } else { // opengl
    OrkAssert(false);
  }
}
/////////////////////////////////////////////////////////////////////////////////////////////////
void SpirvCompiler::_inheritStorageInterface(
    std::string storage_name,
    spirvstorageif_ptr_t spirv_sif) {
  

  printf("Inheriting storage interface '%s'\n", storage_name.c_str());
  OrkAssert((spirv_sif->_descriptor_set_id >= 0) and (spirv_sif->_descriptor_set_id <= 4));
  
  // Get binding ID from merged resources
  int binding_id = _findBindingIdFromMergedResources(storage_name, storage_name);
  if (binding_id == -1) {
    // Fallback to auto-increment if not found
    printf("WARNING: Storage interface '%s' not found in merged resources, using fallback binding\n", storage_name.c_str());
    binding_id = _binding_id++;
  }
  
  // Emit the GLSL storage buffer declaration
  auto header = FormatString("// Storage interface: %s", storage_name.c_str());
  _appendText(_uniforms_group, header.c_str());
  bool is_readonly = true; // TODO: change grammar, parse from AST
  
  auto layout_line = FormatString(
      "layout(set=%zu, binding=%d, std430) %s buffer %s {",
      spirv_sif->_descriptor_set_id,
      binding_id,
      is_readonly ? "readonly" : "",
      spirv_sif->_buffer_name.c_str());
  _appendText(_uniforms_group, layout_line.c_str());
  
  // Emit buffer members
  for (auto item : spirv_sif->_items_by_order) {
    if (item->_is_array) {
      auto member_line = FormatString("  %s %s[%zu];", 
                                      item->_datatype.c_str(), 
                                      item->_identifier.c_str(), 
                                      item->_array_length);
      _appendText(_uniforms_group, member_line.c_str());
    } else {
      auto member_line = FormatString("  %s %s;", 
                                      item->_datatype.c_str(), 
                                      item->_identifier.c_str());
      _appendText(_uniforms_group, member_line.c_str());
    }
  }
  
  auto closing = FormatString("}; // end storage interface %s", storage_name.c_str());
  _appendText(_uniforms_group, closing.c_str());
}

/////////////////////////////////////////////////////////////////////////////////////////////////

std::string SpirvCompiler::_ifLayoutHeader(astnode_ptr_t layout_node, int iloc) {
  std::string outhdr = "layout(";
  size_t num_items   = layout_node->_children.size();
  if (iloc >= 0) {
    OrkAssert(num_items == 0);
    outhdr += FormatString("location=%d", iloc);
  } else {
    for (size_t i = 0; i < num_items; i++) {
      auto item = layout_node->childAs<InterfaceLayoutItem>(i);
      switch (item->_children.size()) {
        case 1: { // SemaId
          auto key = childAsSemaIdString(item, 0);
          outhdr += key;
          break;
        }
        case 2: { // SemaId = SemaIntegerLiteral
          auto key = childAsSemaIdString(item, 0);
          auto val = childAsSemaInteger(item, 1);
          outhdr += key + "=" + FormatString("%d", val);
          break;
        }
        default:
          OrkAssert(false);
          break;
      }
      if (i < (num_items - 1)) {
        outhdr += ",";
      }
    }
  }
  outhdr += ") ";
  return outhdr;
}

/////////////////////////////////////////////////////////////////////////////////////////////////

std::string SpirvCompiler::_ifTypedId(astnode_ptr_t tid_node) {
  auto dt = tid_node->typedValueForKey<std::string>("data_type").value();
  auto id = tid_node->typedValueForKey<std::string>("identifier_name").value();
  return FormatString("%s %s", dt.c_str(), id.c_str());
}

/////////////////////////////////////////////////////////////////////////////////////////////////

std::string SpirvCompiler::_ifIoItem(
    astnode_ptr_t layout_node, //
    astnode_ptr_t tid_node,
    std::string direction,
    size_t& IO_index) { //

  const auto& DATASIZES = SpirvCompilerGlobals::instance()->_io_data_sizes;

  std::string item_str;

  // InterfaceLayout
  // TypedIdentifier
  // InterfaceLayout TypedIdentifier

  bool has_layout = layout_node != nullptr;

  bool has_tid = tid_node != nullptr;

  ////////////////////////////////
  // determine if layout needs a location
  //  if it's a gl_ builtin, it doesn't
  ////////////////////////////////

  bool need_location = has_tid;
  if (has_tid) {
    auto id = tid_node->typedValueForKey<std::string>("identifier_name").value();
    if (id.find("gl_") == 0) {
      need_location = false;
    }
  }

  ////////////////////////////////
  // layout
  ////////////////////////////////

  // Check for interpolation qualifier first
  std::string qualifier_str;
  if (tid_node) {
    auto interp_qual = tid_node->typedValueForKey<std::string>("interpolation_qualifier");
    if (interp_qual) {
      qualifier_str = interp_qual.value() + " ";
    }
  }

  if (layout_node) {

    size_t num_items = layout_node->_children.size();
    for (size_t i = 0; i < num_items; i++) {
      auto item = layout_node->childAs<InterfaceLayoutItem>(i);
      auto key  = childAsSemaIdString(item, 0);
      if (key == "location") {
        need_location = false;
      }
    }

    // dumpAstNode(layout_node);
    item_str = _ifLayoutHeader(layout_node, need_location ? IO_index : -1) + " " + qualifier_str + direction + " ";
  } else if (need_location) {
    item_str = FormatString("layout(location=%d) %s%s ", IO_index, qualifier_str.c_str(), direction.c_str());
  } else {
    item_str = qualifier_str + direction + " ";
  }

  ////////////////////////////////
  // typed identifier
  ////////////////////////////////

  if (tid_node) {

    item_str += _ifTypedId(tid_node);
    if (need_location) {
      auto dt = tid_node->typedValueForKey<std::string>("data_type").value();
      auto it = DATASIZES.find(dt);
      OrkAssert(it != DATASIZES.end());
      IO_index += it->second;
    }

    bool is_geom_shader = (std::dynamic_pointer_cast<GeometryShader>(_shader) != nullptr);

    if (is_geom_shader) { // geometry shaders need [] on their inputs (broadcast from vertex)

      if (direction == "in") {
        item_str += "[]";
      }
    }
  }

  return item_str;
}

/////////////////////////////////////////////////////////////////////////////////////////////////

void SpirvCompiler::_inheritIO(astnode_ptr_t interface_node) {
  //
  // Handle inherited interfaces - specifically vertex outputs becoming fragment inputs
  //
  printf("_inheritIO interface '%s'\n", interface_node->typedValueForKey<std::string>("object_name").value().c_str());
  auto ifname    = interface_node->typedValueForKey<std::string>("object_name").value();
  auto decorator = FormatString("// begin interface<%s>", ifname.c_str());
  _appendText(_interface_group, decorator.c_str());

  // Check interface type
  bool is_fragment_interface = (std::dynamic_pointer_cast<FragmentInterface>(interface_node) != nullptr);
  bool is_vertex_interface   = (std::dynamic_pointer_cast<VertexInterface>(interface_node) != nullptr);
  bool is_geometry_interface = (std::dynamic_pointer_cast<GeometryInterface>(interface_node) != nullptr);
  bool is_compute_interface  = (std::dynamic_pointer_cast<ComputeInterface>(interface_node) != nullptr);
  // bool is_storage_interface  = (std::dynamic_pointer_cast<StorageInterface>(interface_node) != nullptr);

  // For fragment interfaces, first convert inherited vertex outputs to inputs
  if (is_fragment_interface) {
    // Find inherited vertex interfaces
    std::vector<astnode_ptr_t> inherited_vtx_outputs;

    auto inherit_items = AstNode::collectNodesOfType<SemaInheritVertexInterface>(interface_node);
    for (auto inherit_item : inherit_items) {
      auto inherit_id = inherit_item->typedValueForKey<std::string>("inherit_id").value();
      auto vtx_if     = _transu->find<VertexInterface>(inherit_id);
      if (vtx_if) {
        // Collect outputs from the vertex interface
        auto output_groups = AstNode::collectNodesOfType<InterfaceOutputs>(vtx_if);
        for (auto output_group : output_groups) {
          auto outputs = AstNode::collectNodesOfType<InterfaceOutput>(output_group);
          for (auto output : outputs) {
            inherited_vtx_outputs.push_back(output);
          }
        }
      }
    }

    // Convert vertex outputs to fragment inputs
    if (!inherited_vtx_outputs.empty()) {
      _appendText(_interface_group, "// Inputs inherited from vertex interface outputs");
      for (auto vtx_output : inherited_vtx_outputs) {
        auto as_layout = vtx_output->childAs<InterfaceLayout>(0);
        auto as_tid    = (as_layout != nullptr) ? vtx_output->childAs<TypedIdentifier>(1) : vtx_output->childAs<TypedIdentifier>(0);

        if (as_tid) {
          // Skip built-in variables like gl_Position
          auto id = as_tid->typedValueForKey<std::string>("identifier_name").value();
          if (id.find("gl_") == 0) {
            continue;
          }

          // Create input from vertex output
          auto input_str = _ifIoItem(as_layout, as_tid, "in", _input_index);
          _appendText(_interface_group, "%s;", input_str.c_str());
        }
      }
    }
  }

  // Process regular inputs for all interface types
  auto input_groups = AstNode::collectNodesOfType<InterfaceInputs>(interface_node);
  for (auto input_group : input_groups) {
    auto inputs = AstNode::collectNodesOfType<InterfaceInput>(input_group);
    for (auto input : inputs) {
      auto as_layout = input->childAs<InterfaceLayout>(0);
      auto as_tid    = (as_layout != nullptr) ? input->childAs<TypedIdentifier>(1) : input->childAs<TypedIdentifier>(0);

      auto input_str = _ifIoItem(as_layout, as_tid, "in", _input_index);
      _appendText(_interface_group, "%s;", input_str.c_str());
    }
  }

  // Process outputs
  auto output_groups = AstNode::collectNodesOfType<InterfaceOutputs>(interface_node);
  for (auto output_group : output_groups) {
    auto outputs = AstNode::collectNodesOfType<InterfaceOutput>(output_group);
    for (auto output : outputs) {
      auto as_layout = output->childAs<InterfaceLayout>(0);
      auto as_tid    = (as_layout != nullptr) ? output->childAs<TypedIdentifier>(1) : output->childAs<TypedIdentifier>(0);

      auto output_str = _ifIoItem(as_layout, as_tid, "out", _output_index);
      _appendText(_interface_group, "%s;", output_str.c_str());
    }
  }

  // Process storage groups
  auto storage_groups = AstNode::collectNodesOfType<InterfaceStorageRefs>(interface_node);
  for (auto storage_group : storage_groups) {
    dumpAstNode(storage_group);
    auto storage_refs = AstNode::collectNodesOfType<SemaIdentifier>(storage_group);
    for (auto storage : storage_refs) {
      auto id_name = storage->typedValueForKey<std::string>("identifier_name").value();
      //printf("InterfaceStorageRef %s\n", id_name.c_str());
      auto sto_if = _transu->find<StorageInterface>(id_name);
      if (sto_if) {
        auto storage_name = sto_if->typedValueForKey<std::string>("object_name").value();
        //printf(" found StorageInterface '%s' name<%s>\n", id_name.c_str(), storage_name.c_str());
        //_assert_on_done = true;
        auto dsid_node  = sto_if->findFirstChildOfType<DescriptorSetId>();
        int dset_id     = dsid_node->typedValueForKey<int>("descriptor_set_id").value();
        auto sitem_node  = sto_if->findFirstChildOfType<StorageInterfaceItem>();
        auto sitemn_node  = sitem_node->findFirstChildOfType<StorageInterfaceItemName>();
        auto semaid_nodes = AstNode::collectNodesOfType<SemaIdentifier>(sitemn_node);
        auto sitem_name  = getSemaIdString(semaid_nodes[0]);
        /*layout(set = 0, binding = 0) buffer ObjectBuffer
        {
            int some_int;
            float fixed_array[42];
            float variable_array[];
        };*/
        /////////////////////////////////////////////////////////////////////
        auto header = FormatString("// begin interface<%s> sitem_name<%s>", //
                                   id_name.c_str(),                        //
                                   sitem_name.c_str());                //
        //auto header = FormatString("// begin interface<%s> sitem_ast<%s>", //
        //                           id_name.c_str(),                        //
        //                           (void*)toASTstring(sitemn_node).c_str());                //
        _appendText(_interface_group, header.c_str());
        /////////////////
        // Try to find binding ID from merged resources first
        int binding_id = _findBindingIdFromMergedResources(storage_name, storage_name);
        if (binding_id == -1) {
          // Fallback to original behavior if not found in merged resources
          printf("WARNING: Storage interface '%s' not found in merged resources, using fallback binding\n", storage_name.c_str());
          binding_id = _binding_id;
          _binding_id++;
        }
        /////////////////
        bool is_readonly = true; // TODO: change grammar, parse from AST
        /////////////////
        auto layout_line = FormatString(
            "layout(set=%d, binding=%d) %s buffer %s {", //
            dset_id,                                   //
            binding_id,                                //
            is_readonly ? "readonly" : "",             //
            sitem_name.c_str());
        _appendText(_interface_group, layout_line.c_str());
        /////////////////
        auto decls = sitem_node->findFirstChildOfType<DataDeclarations>();
        for (auto decl_sub : decls->_children) {
          if (auto as_ddecl = std::dynamic_pointer_cast<DataDeclaration>(decl_sub)) {
            auto tid = as_ddecl->childAs<TypedIdentifier>(0);
            OrkAssert(tid);
            auto dt = tid->typedValueForKey<std::string>("data_type").value();
            auto id = tid->typedValueForKey<std::string>("identifier_name").value();
            _appendText(_interface_group, " %s %s;", dt.c_str(), id.c_str());
          } else if (auto as_adecl = std::dynamic_pointer_cast<ArrayDeclaration>(decl_sub)) {
            auto tid = as_adecl->childAs<TypedIdentifier>(0);
            OrkAssert(tid);
            auto dt          = tid->typedValueForKey<std::string>("data_type").value();
            auto id          = tid->typedValueForKey<std::string>("identifier_name").value();
            auto len_node    = as_adecl->childAs<SemaIntegerLiteral>(1);
            auto ary_len_str = len_node->typedValueForKey<std::string>("literal_value").value();
            auto ary_len     = atoi(ary_len_str.c_str());
            _appendText(_interface_group, " %s %s[%d];", dt.c_str(), id.c_str(), ary_len);
          } else {
            OrkAssert(false);
          }
        }
        /////////////////
        auto closing_comment = FormatString(
            "}; // layout(set=%d, binding=%d) buffer %s", //
            dset_id,                                       //
            binding_id,                                    //
            sitem_name.c_str());
        _appendText(_interface_group, closing_comment.c_str());
        /////////////////
        auto tailer = FormatString("// end interface<%s>", id_name.c_str());
        _appendText(_interface_group, tailer.c_str());
      } else {
        printf("ERROR: StorageInterface '%s' not found!\n", id_name.c_str());
        OrkAssert(false);
      }
    }
  }

  decorator = FormatString("// end interface<%s>", ifname.c_str());
  _appendText(_interface_group, decorator.c_str());
}
/////////////////////////////////////////////////////////////////////////////////////////////////
void SpirvCompiler::_inheritExtension(semainhext_ptr_t extension_node) {
  auto ext_name       = extension_node->typedValueForKey<std::string>("extension_name").value();
  const auto& RENAMES = SpirvCompilerGlobals::instance()->_id_renames;
  auto ren            = RENAMES.find(ext_name);
  if (ren != RENAMES.end()) {
    ext_name = ren->second;
  }
  if (ext_name != "") {
    _appendText(_extension_group, "#extension %s : enable", ext_name.c_str());
  }
}
/////////////////////////////////////////////////////////////////////////////////////////////////
void SpirvCompiler::_compileShader(shaderc_shader_kind shader_type) {

  ///////////////////////////////////////////////////////
  // shut up InheritListItem's
  ///////////////////////////////////////////////////////

  auto inhs = AstNode::collectNodesOfType<SHAST::InheritListItem>(_transu);
  for (auto inh : inhs) {
    AstNode::treeops::removeFromParent(inh);
  }

  _emitMergedPushConstants();

  ///////////////////////////////////////////////////////
  // final prep for shaderc
  // build final ast
  ///////////////////////////////////////////////////////

  _shader_name = _shader->typedValueForKey<std::string>("object_name").value();
  auto fn_sig  = FormatString("void main() // %s", _shader_name.c_str());
  // auto fn_inv  = FormatString("void main() { %s(); }", _shader_name.c_str());

  _shader_group->appendTypedChild<InsertLine>("#version 450");
  _shader_group->appendChild(_extension_group);
  _shader_group->appendChild(_types_group);
  _shader_group->appendChild(_uniforms_group);
  _shader_group->appendChild(_interface_group);
  _shader_group->appendChild(_libraries_group);
  _shader_group->appendTypedChild<InsertLine>(fn_sig);
  _shader_group->appendChildrenFrom(_shader); // compound statement
  //_shader_group->appendTypedChild<InsertLine>(fn_inv);

  ///////////////////////////////////////////////////////
  // emit
  ///////////////////////////////////////////////////////

  auto as_glsl = shadlang::toGLFX1(_shader_group);

  ///////////////////////////////////////////////////////
  // compile with shaderc
  ///////////////////////////////////////////////////////

  shaderc::Compiler compiler;
  shaderc::CompileOptions options;
  shaderc::SpvCompilationResult result = compiler.CompileGlslToSpv( //
      as_glsl.c_str(),                                              // glsl source (string)
      as_glsl.length(),                                             // glsl source length
      shader_type,                                                  // shader type
      "main",                                                       // entry point name
      options);

  printf("// shader<%s>:\n%s\n", _shader_name.c_str(), as_glsl.c_str());
  if (result.GetCompilationStatus() != shaderc_compilation_status_success) {
    std::cerr << result.GetErrorMessage();
    OrkAssert(false);
  }
  auto output_path = file::Path::temp_dir() / FormatString("%s.glsl", _shader_name.c_str());
  bool OK          = File::writeString(output_path, as_glsl);

  output_path   = file::Path::temp_dir() / FormatString("%s.spv", _shader_name.c_str());
  _spirv_binary = shader_bin_t(result.cbegin(), result.cend());
  File::writeBinary(output_path, _spirv_binary.data(), _spirv_binary.size() * sizeof(uint32_t));
  OrkAssert(not _assert_on_done);
}
/////////////////////////////////////////////////////////////////////////////////////////////////
// Helper function to find binding ID from merged resources in the transunit
int SpirvCompiler::_findBindingIdFromMergedResources(const std::string& resource_name, const std::string& source_name) {
  // Find the current pass that contains this shader
  auto passes = AstNode::collectNodesOfType<Pass>(_transu);

  for (auto pass : passes) {
    // Check if this pass contains the current shader
    auto vtx_refs = AstNode::collectNodesOfType<VertexShaderRef>(pass);
    auto frg_refs = AstNode::collectNodesOfType<FragmentShaderRef>(pass);
    auto geo_refs = AstNode::collectNodesOfType<GeometryShaderRef>(pass);
    auto com_refs = AstNode::collectNodesOfType<ComputeShaderRef>(pass);

    bool pass_contains_shader = false;
    std::string shader_name   = _shader->typedValueForKey<std::string>("object_name").value();

    for (auto vtx_ref : vtx_refs) {
      auto ref_name = vtx_ref->typedValueForKey<std::string>("ref_id").value();
      if (ref_name == shader_name) {
        pass_contains_shader = true;
        break;
      }
    }
    for (auto frg_ref : frg_refs) {
      auto ref_name = frg_ref->typedValueForKey<std::string>("ref_id").value();
      if (ref_name == shader_name) {
        pass_contains_shader = true;
        break;
      }
    }
    for (auto geo_ref : geo_refs) {
      auto ref_name = geo_ref->typedValueForKey<std::string>("ref_id").value();
      if (ref_name == shader_name) {
        pass_contains_shader = true;
        break;
      }
    }
    for (auto com_ref : com_refs) {
      auto ref_name = com_ref->typedValueForKey<std::string>("ref_id").value();
      if (ref_name == shader_name) {
        pass_contains_shader = true;
        break;
      }
    }

    if (pass_contains_shader) {
      // Find the merged resources node for this pass
      auto merged_resources = pass->findFirstChildOfType<MergedShaderResourcesNode>();
      if (merged_resources) {
        // Look through all descriptor sets
        auto descriptor_sets = AstNode::collectNodesOfType<DescriptorSetNode>(merged_resources);
        for (auto descriptor_set : descriptor_sets) {
          auto source_nodes = AstNode::collectNodesOfType<DescriptorSetSourceNode>(descriptor_set);
          for (auto source_node : source_nodes) {
            auto source_node_name = source_node->_source_name;
            if (source_node_name == source_name) {
              // Found the source, now look for the resource
              auto binding_nodes = AstNode::collectNodesOfType<ResourceBindingNode>(source_node);
              for (auto binding_node : binding_nodes) {
                if (binding_node->_binding_name == resource_name) {
                  return binding_node->_binding_id;
                }
              }
            }
          }
        }
      }
    }
  }

  // If not found in merged resources, return -1 to indicate fallback to original behavior
  return -1;
}
/////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::shadlang::spirv
