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

struct SpirvUniformSet;
struct SpirvSamplerSet;
struct SpirvUniformSetItem;
struct SpirvSampler;
struct SpirvUniformBlock;
struct SpirvUniformBlockItem;

using spirvuniset_ptr_t     = std::shared_ptr<SpirvUniformSet>;
using spirvsmpset_ptr_t     = std::shared_ptr<SpirvSamplerSet>;
using spirvunisetitem_ptr_t = std::shared_ptr<SpirvUniformSetItem>;
using spirvsampler_ptr_t    = std::shared_ptr<SpirvSampler>;

using spirvuniblk_ptr_t     = std::shared_ptr<SpirvUniformBlock>;
using spirvuniblkitem_ptr_t = std::shared_ptr<SpirvUniformBlockItem>;

using shader_bin_t     = std::vector<uint32_t>;

struct SpirvUniformSetItem {
  std::string _datatype;
  std::string _identifier;
  bool _is_array = false;
  size_t _array_length = 0;
  size_t _offset = 0;
};
struct SpirvSampler {
  size_t _binding_id = -1;
  std::string _datatype;
  std::string _identifier;
};

struct SpirvUniformSet {
  std::string _name;
  std::unordered_map<std::string, spirvunisetitem_ptr_t> _items_by_name;
  std::vector<spirvunisetitem_ptr_t> _items_by_order;
};
struct SpirvSamplerSet {
  std::string _name;
  size_t _descriptor_set_id = 0;
  std::unordered_map<std::string, spirvsampler_ptr_t> _samplers_by_name;
  std::vector<spirvsampler_ptr_t> _samplers_by_order;
};

struct SpirvUniformBlockItem {
  std::string _datatype;
  std::string _identifier;
  bool _is_array = false;
  size_t _array_length = 0;
  size_t _offset = 0;
};
struct SpirvUniformBlock {
  std::string _name;
  size_t _descriptor_set_id = 0;
  std::unordered_map<std::string, spirvuniblkitem_ptr_t> _items_by_name;
  std::vector<spirvuniblkitem_ptr_t> _items_by_order;
};

struct SpirvCompilerGlobals;
using spirvcompilerglobals_constptr_t = std::shared_ptr<const SpirvCompilerGlobals>;
struct SpirvCompilerGlobals {
  SpirvCompilerGlobals();
  static spirvcompilerglobals_constptr_t instance();
  std::unordered_map<std::string, size_t> _io_data_sizes;
  std::unordered_map<std::string, size_t> _block_data_sizes;
  std::unordered_map<std::string, std::string> _id_renames;
};

struct SpirvCompiler {

  SpirvCompiler(transunit_ptr_t _transu, bool vulkan);
  void processShader(shader_ptr_t sh);
  

private:

  void _convertSamplerSets();
  void _convertUniformSets();
  void _convertUniformBlocks();
  void _appendText(miscgroupnode_ptr_t grp, const char* formatstring, ...);
  void _collectLibBlocks();
  void _processGlobalRenames();

  void _inheritLibrary(libblock_ptr_t lib_node);
  void _inheritTypes(typeblock_ptr_t typ_node);
  void _inheritSamplerSet(std::string unisetname, spirvsmpset_ptr_t smpset_node);
  void _inheritUniformSet(std::string unisetname, spirvuniset_ptr_t uniset_node);
  void _inheritUniformBlk(std::string uniblkname, spirvuniblk_ptr_t uniblk_node);
  void _inheritIO(astnode_ptr_t interface_node);
  void _inheritExtension(semainhext_ptr_t ext_node);

  void _beginShader(shader_ptr_t sh);
  void _compileShader(shaderc_shader_kind shader_type);

  std::string _ifLayoutHeader(astnode_ptr_t layout_node, int iloc=-1);
  std::string _ifTypedId(astnode_ptr_t tid_node);

  std::string _ifIoItem(astnode_ptr_t layout_node, 
                        astnode_ptr_t tid_node,
                        std::string direction,
                        size_t& IO_index);

  transunit_ptr_t _transu;
  shader_ptr_t _shader;
  miscgroupnode_ptr_t _shader_group;
  miscgroupnode_ptr_t _interface_group;
  miscgroupnode_ptr_t _types_group;
  miscgroupnode_ptr_t _extension_group;
  miscgroupnode_ptr_t _uniforms_group;
  miscgroupnode_ptr_t _libraries_group;
  size_t _input_index = 0;
  size_t _output_index = 0;
  size_t _descriptor_set_counter = 0;
  size_t _binding_id = 0;
  bool _vulkan = true;

public:

  shader_bin_t _spirv_binary;
  std::string _shader_name;
  std::unordered_map<std::string, spirvsmpset_ptr_t> _spirvsamplersets;
  std::unordered_map<std::string, spirvuniset_ptr_t> _spirvuniformsets;
  std::unordered_map<std::string, spirvuniblk_ptr_t> _spirvuniformblks;

};

using spirv_compiler_ptr_t = std::shared_ptr<SpirvCompiler>;

/////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace ork::lev2::shadlang::spirv
