#pragma once 

//#include <cstdint>
#include <type_traits>
//#include <new>
#include <ork/math/cmatrix4.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::vulkan {
///////////////////////////////////////////////////////////////////////////////
struct VkBufferLayout {
    //////////////////////////////////////////////
    VkBufferLayout()
        : _cursor(0) {
    }
    //////////////////////////////////////////////
    template <typename T>
    std::size_t getAlignment() const {
        if (std::is_same<T, float>::value || std::is_same<T, int>::value) {
            return sizeof(T);
        }
        if (std::is_same<T, glm::vec2>::value) {
            return sizeof(float) * 2;
        }
        if (std::is_same<T, glm::vec3>::value || std::is_same<T, glm::vec4>::value) {
            return sizeof(float) * 4;
        }
        if (std::is_same<T, glm::mat4>::value) {
            return sizeof(float) * 4; // mat4 is treated as an array of vec4
        }
        return alignof(T);
    }
    template <typename datatype_t>
    size_t layoutItem() {
        std::size_t alignment = getAlignment<datatype_t>();
        std::size_t space = sizeof(datatype_t);
        
        // Align the cursor
        _cursor = (_cursor + alignment - 1) & ~(alignment - 1);
        
        size_t rval = _cursor; // This is where the current item will be placed
        
        _cursor += space; // Advance the cursor by the size of the datatype
        
        return rval;
    }
    //////////////////////////////////////////////
    size_t cursor() const {
        return _cursor;
    }
    //////////////////////////////////////////////

private:
    std::size_t _cursor;
    // ... other member functions as needed ...
};
///////////////////////////////////////////////////////////////////////////////
} //namespace ork::lev2::gfx::vulkan {
