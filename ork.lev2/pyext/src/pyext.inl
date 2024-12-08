#include "pyext.h"

namespace ork::lev2 {

template <typename T>
std::function<scenegraph::drawable_node_ptr_t(T, std::string, scenegraph::layer_ptr_t, fxpipeline_ptr_t)>
createNodeLambdaFromPrimType() {
  return [](T prim,
            std::string named, //
            scenegraph::layer_ptr_t layer,
            fxpipeline_ptr_t mtl_inst) -> scenegraph::drawable_node_ptr_t { //
    auto node                                                               //
        = prim->createNode(named, layer, mtl_inst);
    node->_userdata->template makeValueForKey<T>("_primitive") = prim; // hold on to reference
    return node;
  };
}

} // namespace ork::lev2
