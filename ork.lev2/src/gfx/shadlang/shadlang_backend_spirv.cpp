////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/lev2/gfx/shadlang.h>
#include "shadlang_backend_spirv.h"

namespace ork::lev2::shadlang::spirv {
using namespace SHAST;
/////////////////////////////////////////////////////////////////////////////////////////////////
struct LayoutStandard430 { // layout by glsl standard 430
    //////////////////////////////////////////////
    LayoutStandard430()
        : _cursor(0) {
    }
    void incrementDatatype(const std::string& dtname, size_t array_len = 1) {
        auto& block_sizes = SpirvCompilerGlobals::instance()->_block_data_sizes;
        auto it = block_sizes.find(dtname);
        if( it == block_sizes.end() ){
          printf( "dtname<%s> not found in block_sizes\n", dtname.c_str() );
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
SpirvCompilerGlobals::SpirvCompilerGlobals(){
  bool _vulkan = true;
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
    _id_renames["gl_VertexID"] = "gl_VertexIndex";

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
    _id_renames["ORK_GPU_SHADER"]        = "";
  #else
    _id_renames["ORK_GPU_SHADER"]        = "GL_NV_gpu_shader5";
  #endif

}
/////////////////////////////////////////////////////////////////////////////////////////////////
spirvcompilerglobals_constptr_t SpirvCompilerGlobals::instance(){
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
}

/////////////////////////////////////////////////////////////////////////////////////////////////
void SpirvCompiler::_beginShader(shader_ptr_t shader) {
  _shader          = shader;
  _shader_group    = std::make_shared<MiscGroupNode>();
  _interface_group = std::make_shared<MiscGroupNode>();
  _extension_group = std::make_shared<MiscGroupNode>();
  _uniforms_group  = std::make_shared<MiscGroupNode>();
  _libraries_group = std::make_shared<MiscGroupNode>();
  _types_group = std::make_shared<MiscGroupNode>();
  _input_index     = 0;
  _output_index    = 0;

  printf( "begin shader<%s>\n", shader->_name.c_str() );

  /////////////////////////////////////////////////
  /////////////////////////////////////////////////
  // process shader inheritances
  /////////////////////////////////////////////////
  /////////////////////////////////////////////////
  InheritanceTracker tracker(_transu);
  _binding_id = 0;
  ////////////////////////////////////////////////
  tracker._onInheritLibrary = [&](std::string INHID, libblock_ptr_t lib_block) { //
    printf( "INHERIT LIB<%s> depth<%zu>\n", INHID.c_str(), tracker._stack_depth );
    _inheritLibrary(lib_block);
  };
  ////////////////////////////////////////////////
  tracker._onInheritTypes = [&](std::string INHID, typeblock_ptr_t typ_block) { //
    printf( "INHERIT TYP<%s> depth<%zu>\n", INHID.c_str(), tracker._stack_depth );
    _inheritTypes(typ_block);
  };
  ////////////////////////////////////////////////
  tracker._onInheritSamplerSet = [=](std::string INHID, astnode_ptr_t sset) { //
    printf( "INHERIT SSET<%s>\n", INHID.c_str() );
    auto it_sset  = _spirvsamplersets.find(INHID);
    OrkAssert(it_sset != _spirvsamplersets.end());
    auto spirvsmpset = it_sset->second;
    _inheritSamplerSet(INHID, spirvsmpset);
  };
  ////////////////////////////////////////////////
  tracker._onInheritUniformSet = [=](std::string INHID, astnode_ptr_t uset) { //
    printf( "INHERIT USET<%s>\n", INHID.c_str() );
    auto it_uset  = _spirvuniformsets.find(INHID);
    OrkAssert(it_uset != _spirvuniformsets.end());
    auto spirvuniset = it_uset->second;
    _inheritUniformSet(INHID, spirvuniset);
  };
  ////////////////////////////////////////////////
  tracker._onInheritUniformBlk = [=](std::string INHID, astnode_ptr_t ublk) { //
    printf( "INHERIT UBLK<%s>\n", INHID.c_str() );
      auto it_ublk  = _spirvuniformblks.find(INHID);
      OrkAssert(it_ublk != _spirvuniformblks.end());
      auto spirvuniblk = it_ublk->second;
    _inheritUniformBlk(INHID, spirvuniblk);
  };
  ////////////////////////////////////////////////
  tracker._onInheritInterface = [=](std::string INHID, astnode_ptr_t interface_node) { //
    printf( "INHERIT IO<%s>\n", INHID.c_str() );
    _inheritIO(interface_node);
  }; 
  ////////////////////////////////////////////////
  tracker._onInheritExtension = [=](std::string INHID, astnode_ptr_t ast_node) { //
    printf( "INHERIT EXTENSIONS<%s>\n", INHID.c_str() );
    auto as_ext_node = std::dynamic_pointer_cast<SemaInheritExtension>(ast_node);
    OrkAssert(as_ext_node);
    _inheritExtension(as_ext_node);
  }; 
  ////////////////////////////////////////////////
  tracker.fetchInheritances(shader);
  /////////////////////////////////////////////////
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
void SpirvCompiler::_convertSamplerSets() {
  auto ast_smpsets = SHAST::AstNode::collectNodesOfType<SHAST::SamplerSet>(_transu);
  for (auto ast_smpset : ast_smpsets) {
    auto dsetids = SHAST::AstNode::collectNodesOfType<SHAST::DescriptorSetId>(ast_smpset);
    OrkAssert(dsetids.size() == 1);
    int dset_id = dsetids[0]->typedValueForKey<int>("descriptor_set_id").value();
    printf("sampler dset_id<%d>\n", dset_id);
    //////////////////////////////////////
    auto sampler_declarations = SHAST::AstNode::collectNodesOfType<SHAST::SamplerDeclaration>(ast_smpset);
    //////////////////////////////////////
    auto smpset_name               = ast_smpset->typedValueForKey<std::string>("object_name").value();
    auto smpset                 = std::make_shared<SpirvSamplerSet>();
    _spirvsamplersets[smpset_name] = smpset;
    smpset->_name               = smpset_name;
    smpset->_descriptor_set_id = dset_id;
    OrkAssert((dset_id>=0) and (dset_id<=4));
    //////////////////////////////////////
    for (auto decl : sampler_declarations) {
      dumpAstNode(decl);
      auto sampler_type = decl->childAs<SHAST::SamplerType>(0);
      OrkAssert(sampler_type);
      auto smp_typename = sampler_type->typedValueForKey<std::string>("sampler_type").value();
      auto semaid         = decl->childAs<SemaIdentifier>(1);
      auto smp_name = semaid->typedValueForKey<std::string>("identifier_name").value();
      auto sampler                     = std::make_shared<SpirvSampler>();
      sampler->_datatype               = smp_typename;
      sampler->_identifier             = smp_name;
      smpset->_samplers_by_name[smp_name] = sampler;
    }
    //////////////////////////////////////
  }
}
/////////////////////////////////////////////////////////////////////////////////////////////////
void SpirvCompiler::_convertUniformSets() {
  auto ast_unisets = SHAST::AstNode::collectNodesOfType<SHAST::UniformSet>(_transu);
  //const auto& DATASIZES = SpirvCompilerGlobals::instance()->_data_sizes;
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
      auto dt         = tid->typedValueForKey<std::string>("data_type").value();
      auto id         = tid->typedValueForKey<std::string>("identifier_name").value();
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
        //offset += item_size*item->_array_length;
      }
      else{
        layout.incrementDatatype(dt, 0);
      }
      if(layout.cursor()>256){
        printf( "uniset<%s> pushconstant overflow length<%zu>\n", uni_name.c_str(), layout.cursor() );
        OrkAssert(false);
      }
    }
    //////////////////////////////////////
  }
}
/////////////////////////////////////////////////////////////////////////////////////////////////
void SpirvCompiler::_convertUniformBlocks() {
  auto ast_uniblks = SHAST::AstNode::collectNodesOfType<SHAST::UniformBlk>(_transu);
  for (auto ast_uniblk : ast_uniblks) {
    auto dsetids = SHAST::AstNode::collectNodesOfType<SHAST::DescriptorSetId>(ast_uniblk);
    OrkAssert(dsetids.size() == 1);
    int dset_id = dsetids[0]->typedValueForKey<int>("descriptor_set_id").value();
    printf("uniblk dset_id<%d>\n", dset_id);
    //////////////////////////////////////
    auto decls = SHAST::AstNode::collectNodesOfType<SHAST::DataDeclarationBase>(ast_uniblk);
    //////////////////////////////////////
    auto uni_name               = ast_uniblk->typedValueForKey<std::string>("object_name").value();
    auto uniblk                 = std::make_shared<SpirvUniformBlock>();
    _spirvuniformblks[uni_name] = uniblk;
    uniblk->_name               = uni_name;
    uniblk->_descriptor_set_id = dset_id;
    OrkAssert((dset_id>=0) and (dset_id<=4));
    //////////////////////////////////////
    LayoutStandard430 layout;
    //////////////////////////////////////
    for (auto d : decls) {
      auto tid = d->childAs<SHAST::TypedIdentifier>(0);
      OrkAssert(tid);
      auto dt                    = tid->typedValueForKey<std::string>("data_type").value();

      auto id                    = tid->typedValueForKey<std::string>("identifier_name").value();

      auto it = dt.find("sampler");
      if( it != std::string::npos ){
        printf( "sampler<%s:%s> in uniform block<%s> not allowed!\n", dt.c_str(), id.c_str(), uni_name.c_str() );
        OrkAssert(false);
      }


      auto item                  = std::make_shared<SpirvUniformBlockItem>();
      item->_datatype            = dt;
      item->_offset = layout.cursor();
      item->_identifier          = id;
      uniblk->_items_by_name[id] = item;
      uniblk->_items_by_order.push_back(item);

      if (auto as_array = std::dynamic_pointer_cast<ArrayDeclaration>(d)) {
        auto len_node       = as_array->childAs<SHAST::SemaIntegerLiteral>(1);
        item->_is_array     = true;
        auto ary_len_str    = len_node->typedValueForKey<std::string>("literal_value").value();
        item->_array_length = atoi(ary_len_str.c_str());
        layout.incrementDatatype(dt, item->_array_length);
        // dumpAstNode(as_array);
      }
      else{
        layout.incrementDatatype(dt, 0);
      }
    }
    if(layout.cursor()>65536){
      printf( "uniblk<%s> buffer overflow length<%zu>\n", uni_name.c_str(), layout.cursor() );
      OrkAssert(false);
    }
  }
}

/////////////////////////////////////////////////////////////////////////////////////////////////

void SpirvCompiler::_inheritSamplerSet(
    std::string unisetname,        //
    spirvsmpset_ptr_t spirvsset) { //
    /////////////////////
    // samplers
    /////////////////////
    for (auto item : spirvsset->_samplers_by_name) {
      auto dt   = item.second->_datatype;
      auto id   = item.second->_identifier;
      auto line = FormatString(
          "layout(set=%zu, binding=%d) uniform %s %s;",   //
          spirvsset->_descriptor_set_id,                 //
          _binding_id,                                   //
          dt.c_str(),                                    //
          id.c_str());
      _appendText(_uniforms_group, line.c_str());
     _binding_id++;
    }

}

/////////////////////////////////////////////////////////////////////////////////////////////////

void SpirvCompiler::_inheritUniformSet(
    std::string unisetname,        //
    spirvuniset_ptr_t spirvuset) { //
  if (_vulkan) {
    /////////////////////
    // loose unis
    /////////////////////
    if( spirvuset->_items_by_order.size() ){
      auto line = FormatString(
          "layout(push_constant) uniform %s {", //
          //spirvuset->_descriptor_set_id,               //
          //_binding_id,                                 //
          unisetname.c_str());
      _appendText(_uniforms_group, line.c_str());
      for (auto item : spirvuset->_items_by_order) {
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
    }
    //_binding_id++;
  } else { // opengl
    OrkAssert(false);
  }
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
  if (_vulkan) {
    size_t _offset = 0;
    /////////////////////
    // loose unis
    /////////////////////
    auto line = FormatString(
        "layout(set=%zu, binding=%zu) uniform %s {", //
        spirvublk->_descriptor_set_id,               //
        _binding_id,                                 //
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
    _binding_id++;
  } else { // opengl
    OrkAssert(false);
  }
}

/////////////////////////////////////////////////////////////////////////////////////////////////

std::string SpirvCompiler::_ifLayoutHeader(astnode_ptr_t layout_node, int iloc) {
  std::string outhdr = "layout(";
  size_t num_items = layout_node->_children.size();
  if(iloc>=0){
    OrkAssert(num_items==0);
    outhdr += FormatString("location=%d", iloc);
  }
  else{
    for( size_t i=0; i<num_items; i++  ){
      auto item = layout_node->childAs<InterfaceLayoutItem>(i);
      switch(item->_children.size() ){
        case 1:{ // SemaId
          auto key = childAsSemaIdString(item,0);
          outhdr += key;
          break;
        }
        case 2:{ // SemaId = SemaIntegerLiteral
          auto key = childAsSemaIdString(item,0);
          auto val = childAsSemaInteger(item,1);
          outhdr += key + "=" + FormatString("%d",val);
          break;
        }
        default:
          OrkAssert(false);
          break;
      }
      if(i<(num_items-1)){
        outhdr += ",";
      }
    }
  }
  outhdr += ") ";
  return outhdr;
}

/////////////////////////////////////////////////////////////////////////////////////////////////

std::string SpirvCompiler::_ifTypedId(astnode_ptr_t tid_node){
  auto dt = tid_node->typedValueForKey<std::string>("data_type").value();
  auto id = tid_node->typedValueForKey<std::string>("identifier_name").value();
  return FormatString( "%s %s", dt.c_str(), id.c_str() );
}

/////////////////////////////////////////////////////////////////////////////////////////////////

std::string SpirvCompiler::_ifIoItem(astnode_ptr_t layout_node, //
                                     astnode_ptr_t tid_node,
                                     std::string direction,
                                     size_t& IO_index) { //

  const auto& DATASIZES = SpirvCompilerGlobals::instance()->_io_data_sizes;

  std::string item_str;

  // InterfaceLayout 
  // TypedIdentifier
  // InterfaceLayout TypedIdentifier

  bool has_layout = layout_node!=nullptr;


  bool has_tid = tid_node!=nullptr;

  ////////////////////////////////
  // determine if layout needs a location
  //  if it's a gl_ builtin, it doesn't
  ////////////////////////////////

  bool need_location = has_tid;
  if(has_tid){
    auto id = tid_node->typedValueForKey<std::string>("identifier_name").value();
    if (id.find("gl_") == 0) {
      need_location = false;
    }
  }

  ////////////////////////////////
  // layout
  ////////////////////////////////

  if( layout_node ){
  
    size_t num_items = layout_node->_children.size();
    for( size_t i=0; i<num_items; i++  ){
      auto item = layout_node->childAs<InterfaceLayoutItem>(i);
      auto key = childAsSemaIdString(item,0);
      if(key=="location"){
        need_location = false;
      }
    }

    dumpAstNode(layout_node);
    item_str = _ifLayoutHeader(layout_node, need_location ? IO_index : -1) + " "+direction+" ";
  }
  else if(need_location){
    item_str = FormatString("layout(location=%d) %s ", IO_index, direction.c_str() );
  }
  else{
    item_str = direction + " ";
  }

  ////////////////////////////////
  // typed identifier
  ////////////////////////////////

  if( tid_node ){

    item_str += _ifTypedId(tid_node);
    if(need_location){
      auto dt = tid_node->typedValueForKey<std::string>("data_type").value();
      auto it = DATASIZES.find(dt);
      OrkAssert(it != DATASIZES.end());
      IO_index += it->second;
    }

    bool is_geom_shader = (std::dynamic_pointer_cast<GeometryShader>(_shader)!=nullptr);

    if(is_geom_shader){ // geometry shaders need [] on their inputs (broadcast from vertex)

      if( direction == "in" ){
        item_str += "[]";
      }
    }

  }

  return item_str;
}

/////////////////////////////////////////////////////////////////////////////////////////////////

void SpirvCompiler::_inheritIO(astnode_ptr_t interface_node) {
  //
  // TODO inherited interfaces
  //

  //const auto& DATASIZES = SpirvCompilerGlobals::instance()->_io_data_sizes;

  auto ifname    = interface_node->typedValueForKey<std::string>("object_name").value();
  auto decorator = FormatString("// begin interface<%s>", ifname.c_str());
  _appendText(_interface_group, decorator.c_str());

  auto input_groups  = AstNode::collectNodesOfType<InterfaceInputs>(interface_node);
  auto output_groups = AstNode::collectNodesOfType<InterfaceOutputs>(interface_node);
  auto storage_groups = AstNode::collectNodesOfType<InterfaceStorages>(interface_node);
  // printf("  num_input_groups<%zu>\n", input_groups.size());
  // printf("  num_output_groups<%zu>\n", output_groups.size());
  /////////////////////////////////////////
  for (auto input_group : input_groups) {
    auto inputs = AstNode::collectNodesOfType<InterfaceInput>(input_group);
    printf("  num_inputs<%zu>\n", inputs.size());
    for (auto input : inputs) {
      auto as_layout = input->childAs<InterfaceLayout>(0);
      auto as_tid = (as_layout!=nullptr) //
                  ? input->childAs<TypedIdentifier>(1) //
                  : input->childAs<TypedIdentifier>(0);

      auto input_str = _ifIoItem(as_layout, as_tid, "in", _input_index);
      _appendText(_interface_group, "%s;", input_str.c_str() );
    }
  }
  /////////////////////////////////////////
  for (auto output_group : output_groups) {
    // printf("  num_outputs<%zu>\n", outputs.size());
    ////////////////////////////////
    auto outputs = AstNode::collectNodesOfType<InterfaceOutput>(output_group);
    ////////////////////////////////
    for (auto output : outputs) {
      auto as_layout = output->childAs<InterfaceLayout>(0);
      auto as_tid = (as_layout!=nullptr) //
                  ? output->childAs<TypedIdentifier>(1) //
                  : output->childAs<TypedIdentifier>(0);

      auto output_str = _ifIoItem(as_layout, as_tid, "out", _output_index);
      _appendText(_interface_group, "%s;", output_str.c_str() );
    }
  }
  /////////////////////////////////////////
  for (auto storage_group : storage_groups) {
    auto storages = AstNode::collectNodesOfType<InterfaceStorage>(storage_group);
    printf("  NUM_STORAGES<%zu>\n", storages.size());
    for (auto storage : storages) {
      dumpAstNode(storage);
      ///////////////////////////////////////////////////////////
      // parse storage top
      ///////////////////////////////////////////////////////////
      auto layout = storage->findFirstChildOfType<InterfaceLayout>();
      auto decls  = storage->findFirstChildOfType<DataDeclarations>();
      auto ast_storage_type = storage->childAs<SemaIdentifier>(1);
      auto ast_storage_name = storage->childAs<SemaIdentifier>(3);
      OrkAssert(layout);
      OrkAssert(decls);
      OrkAssert(ast_storage_type);
      OrkAssert(ast_storage_name);
      auto storage_type = ast_storage_type->typedValueForKey<std::string>("identifier_name").value();
      auto storage_name = ast_storage_name->typedValueForKey<std::string>("identifier_name").value();
      printf("storage_type<%s>\n", storage_type.c_str());
      printf("storage_name<%s>\n", storage_name.c_str());

      ///////////////////////////////////////////////////////////
      // parse/emit layout
      ///////////////////////////////////////////////////////////
      auto ast_std = layout->childAs<InterfaceLayoutItem>(0);
      auto ast_bin = layout->childAs<InterfaceLayoutItem>(1);
      OrkAssert(ast_std);
      OrkAssert(ast_bin);
      auto std = getSemaIdString(ast_std->_children[0]);
      auto bin = getSemaIdString(ast_bin->_children[0]);

      auto ast_bin_num = ast_bin->childAs<SemaIntegerLiteral>(1);
      OrkAssert(ast_bin_num);
      auto bin_num = atoi(ast_bin_num->typedValueForKey<std::string>("literal_value").value().c_str());

      _appendText(_interface_group, //
                  "layout(%s, binding=%d) %s %s {", //
                  std.c_str(), // 
                  bin_num, // 
                  storage_type.c_str(), // 
                  storage_name.c_str() );

      ///////////////////////////////////////////////////////////
      // parse data/array declarations
      ///////////////////////////////////////////////////////////

      for( auto decl_sub : decls->_children ){
        if( auto as_ddecl = std::dynamic_pointer_cast<DataDeclaration>(decl_sub) ){
          auto tid = as_ddecl->childAs<TypedIdentifier>(0);
          OrkAssert(tid);
          auto dt = tid->typedValueForKey<std::string>("data_type").value();
          auto id = tid->typedValueForKey<std::string>("identifier_name").value();
          printf( "STORAGE DATADECL dt<%s> id<%s>\n", dt.c_str(), id.c_str() );
          _appendText(_interface_group, " %s %s;", dt.c_str(), id.c_str());
        }
        else if( auto as_adecl = std::dynamic_pointer_cast<ArrayDeclaration>(decl_sub) ){
          auto tid = as_adecl->childAs<TypedIdentifier>(0);
          OrkAssert(tid);
          auto dt = tid->typedValueForKey<std::string>("data_type").value();
          auto id = tid->typedValueForKey<std::string>("identifier_name").value();
          auto len_node = as_adecl->childAs<SemaIntegerLiteral>(1);
          auto ary_len_str = len_node->typedValueForKey<std::string>("literal_value").value();
          auto ary_len = atoi(ary_len_str.c_str());
          printf( "STORAGE ARYDECL dt<%s> id<%s> len<%d>\n", dt.c_str(), id.c_str(), ary_len );
          _appendText(_interface_group, " %s %s[%d];", dt.c_str(), id.c_str(), ary_len);
        }
        else{
          OrkAssert(false);
        }
      }

      _appendText(_interface_group, "};");

    }
  }
  /////////////////////////////////////////
  decorator = FormatString("// end interface<%s>", ifname.c_str());
  _appendText(_interface_group, decorator.c_str());
}
/////////////////////////////////////////////////////////////////////////////////////////////////
void SpirvCompiler::_inheritExtension(semainhext_ptr_t extension_node) {
  auto ext_name = extension_node->typedValueForKey<std::string>("extension_name").value();
  const auto& RENAMES = SpirvCompilerGlobals::instance()->_id_renames;
  auto ren = RENAMES.find(ext_name);
  if(ren!=RENAMES.end()){
    ext_name = ren->second;
  }
  if(ext_name!=""){
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

  ///////////////////////////////////////////////////////
  // final prep for shaderc
  // build final ast
  ///////////////////////////////////////////////////////

  _shader_name = _shader->typedValueForKey<std::string>("object_name").value();
  auto fn_sig = FormatString("void %s()", _shader_name.c_str() );
  auto fn_inv = FormatString("void main() { %s(); }", _shader_name.c_str() );

  _shader_group->appendTypedChild<InsertLine>("#version 450");
  _shader_group->appendChild(_extension_group);
  _shader_group->appendChild(_types_group);
  _shader_group->appendChild(_uniforms_group);
  _shader_group->appendChild(_interface_group);
  _shader_group->appendChild(_libraries_group);
  _shader_group->appendTypedChild<InsertLine>(fn_sig);
  _shader_group->appendChildrenFrom(_shader); // compound statement
  _shader_group->appendTypedChild<InsertLine>(fn_inv);

  ///////////////////////////////////////////////////////
  // emit
  ///////////////////////////////////////////////////////

  auto as_glsl = shadlang::toGLFX1(_shader_group);
  printf("// shader<%s>:\n%s\n", _shader_name.c_str(), as_glsl.c_str());

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

  if (result.GetCompilationStatus() != shaderc_compilation_status_success) {
    std::cerr << result.GetErrorMessage();
    OrkAssert(false);
  }

  _spirv_binary = shader_bin_t(result.cbegin(), result.cend());
}
/////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::shadlang::spirv
