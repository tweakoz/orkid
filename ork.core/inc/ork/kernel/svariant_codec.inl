#pragma once

#include <ork/kernel/svariant.h>

namespace ork {

template <int size> struct SvarDecoder{

    using val_t = static_variant<size>;

    template <typename T> attempt_cast_const<T> decode(const val_t& inp_value) {
        return inp_value. template tryAs<T>();
    }
};

} //namespace ork {
