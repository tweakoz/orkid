#pragma once 

namespace ork::lev2 {

struct _DRAWABLE_INSTANCED_IMPL {

    _DRAWABLE_INSTANCED_IMPL() {
    }
    ~_DRAWABLE_INSTANCED_IMPL(){
        if(_onDestroy){
            _onDestroy();
        }
    }
    instanceddrawinstancedata_ptr_t _instancedata;
    ork::void_lambda_t _onDestroy = nullptr;
};

using drawable_instanced_impl_ptr_t = std::shared_ptr<_DRAWABLE_INSTANCED_IMPL>;
} // namespace ork::lev2