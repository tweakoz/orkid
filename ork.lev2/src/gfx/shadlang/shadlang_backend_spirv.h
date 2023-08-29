#include <ork/lev2/gfx/shadman.h>
#include <ork/lev2/gfx/shadlang.h>
#include <ork/lev2/gfx/shadlang_nodes.h>
#include <ork/util/hexdump.inl>
#include <shaderc/shaderc.hpp>

#if defined(__APPLE__)
// #include <MoltenVK/mvk_vulkan.h>
#endif

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::shadlang::spirv {
///////////////////////////////////////////////////////////////////////////////
using namespace shadlang::SHAST;

struct UniformSet;
struct UniformSetItem;
struct UniformSetSampler;

using uniset_ptr_t     = std::shared_ptr<UniformSet>;
using unisetitem_ptr_t = std::shared_ptr<UniformSetItem>;
using unisetsamp_ptr_t = std::shared_ptr<UniformSetSampler>;
using shader_bin_t     = std::vector<uint32_t>;

struct UniformSetItem {
  std::string _datatype;
  std::string _identifier;
};
struct UniformSetSampler {
  size_t _binding_id = -1;
  std::string _datatype;
  std::string _identifier;
};

struct UniformSet {
  std::string _name;
  size_t _descriptor_set_id = 0;
  std::unordered_map<std::string, unisetsamp_ptr_t> _samplers_by_name;
  std::unordered_map<std::string, unisetitem_ptr_t> _items_by_name;
  std::vector<unisetitem_ptr_t> _items_by_order;
};

struct SpirvCompiler {

  transunit_ptr_t _transu;
  shader_ptr_t _shader;
  miscgroupnode_ptr_t _shader_group;
  miscgroupnode_ptr_t _interface_group;
  miscgroupnode_ptr_t _extension_group;
  miscgroupnode_ptr_t _uniforms_group;
  miscgroupnode_ptr_t _libraries_group;
  shader_bin_t _spirv_binary;
  std::string _shader_name;
  std::unordered_map<std::string, uniset_ptr_t> _uniformsets;
  std::unordered_map<std::string, size_t> _data_sizes;
  std::unordered_map<std::string, std::string> _id_renames;
  size_t _descriptor_set_counter;
  std::unordered_map<std::string, transunit_ptr_t> _imported_units;
  std::unordered_map<std::string, libblock_ptr_t> _lib_blocks;
  size_t _input_index = 0;
  size_t _output_index = 0;

  SpirvCompiler(transunit_ptr_t _transu);
  void collectUnisets();
  void beginShader(shader_ptr_t sh);
  void appendText(miscgroupnode_ptr_t grp, const char* formatstring, ...);
  void process_imports();
  void process_libblocks();
  void processGlobalRenames();
  void process_inh_libraries(astnode_ptr_t par_node);
  void process_inh_unisets(astnode_ptr_t par_node);
  void process_inh_ios(astnode_ptr_t interface_node);
  void process_inh_extensions();
  template <typename T, typename U> void _process_inh_interface(astnode_ptr_t parent_node);
  template <typename T, typename U> void process_inh_interfaces();
  void compile_shader(shaderc_shader_kind shader_type);
};

using spirv_compiler_ptr_t = std::shared_ptr<SpirvCompiler>;

/////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T, typename U> //
void SpirvCompiler::_process_inh_interface(astnode_ptr_t parent_node) {
  auto inh_interfaces = AstNode::collectNodesOfType<T>(parent_node);
  printf("inh_interfaces<%zu>\n", inh_interfaces.size());
  for (auto INHVIF : inh_interfaces) {
    auto INHID  = INHVIF->template typedValueForKey<std::string>("inherit_id").value();
    printf("  inh_iface<%s> INHID<%s>\n", INHVIF->_name.c_str(), INHID.c_str());
    auto IFACE = _transu->template find<U>(INHID);
    ///////////////////////////////////////////////
    // search imported units for interface
    ///////////////////////////////////////////////
    if (nullptr == IFACE) {
      for (auto import_unit_item : _imported_units) {
        auto import_name = import_unit_item.first;
        auto import_unit = import_unit_item.second;
        IFACE            = import_unit->template find<U>(INHID);
        if (IFACE) {
          break;
        }
      }
    }
    ///////////////////////////////////////////////
    OrkAssert(IFACE);
    ///////////////////////////////////////////////
    _process_inh_interface<T, U>(IFACE);
    ///////////////////////////////////////////////
    process_inh_ios(IFACE);
    process_inh_unisets(IFACE);
  }
}
/////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T, typename U> //
void SpirvCompiler::process_inh_interfaces() {
  _process_inh_interface<T, U>(_shader);
}

} // namespace ork::lev2::shadlang::spirv
