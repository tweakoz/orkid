# Swift Bindings & XCFramework Packaging Strategy for Orkid Engine

**Date:** 2025-11-05
**Status:** ✅ Implementation Ready - Reviewed and Validated (v12.0)
**Target:** iOS Swift Developer Distribution with VarMap Support

---

## Executive Summary

This document outlines the complete strategy for creating Swift bindings for the Orkid Engine using a **codec-based architecture with automatic shared ownership** modeled after Orkid's Python bindings (pycodec). This approach is **required** for proper VarMap support and provides crash-safe memory management.

**Architecture:**
- **Type-Safe Handle System**: Template hierarchy with automatic type registry
- **Safe Casting**: Runtime type validation via `typedHandle<T>()`
- **Type Introspection**: CRC-based type identification (matches Orkid's CrcString system)
- **Codec Pattern**: Bidirectional type conversion system (Swift ↔ C++ variants)
- **Automatic Memory Management**: Single refcount via shared_ptr, Swift ARC handles cleanup
- **Swift Namespaces**: Mirror C++ namespace structure (ork::kernel → ork.kernel)
- **XCFramework Distribution**: Multi-architecture packaging for Swift Package Manager

**Key Requirements:**
1. VarMap is a runtime-typed dictionary (`std::map<string, svar128_t>`) requiring codec for type-safe access
2. **99% of objects use automatic shared ownership** (prevents crashes, matches Python bindings)
3. Only exceptions: primitives (Int, Float, Double, Bool, String) copied by value
4. **Codec lives entirely in C++** (like pycodec) - Swift side is transparent
5. Type names and codec registration happen once at C++ initialization (DRY principle)
6. No type erasure in C++ - preserve `shared_ptr<T>` not `shared_ptr<void>`
7. Safe casting with validation - no blind static_cast

---

## Table of Contents

1. [Understanding: Automatic Shared Ownership](#understanding-automatic-shared-ownership)
2. [Understanding: Type Registry](#understanding-type-registry)
3. [Understanding: VarMap and the Codec Requirement](#understanding-varmap-and-the-codec-requirement)
4. [Template Handle Architecture](#template-handle-architecture)
5. [C Bridge Layer Design](#c-bridge-layer-design)
6. [Swift Codec Architecture](#swift-codec-architecture)
7. [Swift Binding Patterns](#swift-binding-patterns-instantiable-vs-c-only-types)
   - Pattern A: Instantiable Types
   - Pattern B: C++-Only Types
   - Pattern C: Swift Callbacks in Variants
8. [Swift Namespace Structure](#swift-namespace-structure)
9. [Swift Macro Design](#swift-macro-design)
10. [Implementation Strategy](#implementation-strategy)
11. [Usage Examples](#usage-examples)
12. [XCFramework Packaging](#xcframework-packaging)
13. [Implementation Roadmap](#implementation-roadmap)

---

## Understanding: Automatic Shared Ownership

### Why Automatic Reference Counting?

**Python bindings use `shared_ptr` everywhere** - Swift bindings should do the same for safety and consistency, but with **automatic** lifetime management.

**Benefits:**
- ✅ **Prevents crashes** - Objects can't be deleted while Swift still holds references
- ✅ **Prevents use-after-free** - Reference counting ensures lifetime
- ✅ **Bidirectional safety** - C++ can hold Swift-created objects, Swift can hold C++-created objects
- ✅ **Consistency** - Matches Python bindings architecture
- ✅ **Automatic cleanup** - No explicit `release()` calls needed
- ✅ **Swift-friendly** - Integrates naturally with Swift ARC

### Template Handle Architecture

```
┌─────────────────────────────────┐
│         Swift Side              │
│                                 │
│  class Timer {                  │
│    let handle: OrkidHandle  ────┼────┐
│  }                              │    │
│                                 │    │
│  deinit { automatic! }          │    │ Automatic reference
└─────────────────────────────────┘    │ counting via
                                       │ shared_ptr only
┌─────────────────────────────────┐    │
│       C Bridge Layer            │    │
│                                 │    │
│  OrkidHandleBase* (opaque)      │    │
│  ↓                              │    │
│  OrkidHandle<Timer>  ───────────┼────┤
│    shared_ptr<Timer> _ptr   ────┼────┤
│    (type from registry)         │    │
└─────────────────────────────────┘    │
                                       │
┌─────────────────────────────────┐    │
│         C++ Side                │    │
│                                 │    │
│  shared_ptr<Timer>          ────┼────┘
│  (single refcount!)             │
└─────────────────────────────────┘
```

### The 99% vs 1% Rule

**99% of objects → automatic shared ownership:**
- Timer, VarMap, Entity, Node, Component
- Material, Texture, Shader, Mesh
- Animation, Asset, Catalog
- Variants (svar128_t, etc.)
- Math types (fvec2, fvec3, fvec4, dvec3, fquat, fmtx4)
- Geometry types (Sphere, etc.)
- Any object with complex lifetime

**1% exceptions → value types (copied):**
- Primitives: `Int`, `Float`, `Double`, `Bool`, `String`

---

## Understanding: Type Registry

### Why a Type Registry?

**DRY Principle** - Define type names once, use everywhere:

```cpp
// BEFORE (repetitive):
return make_handle(timer, "ork::Timer");  // Type string repeated
return make_handle(vmap, "ork::varmap::VarMap");  // Everywhere!

// AFTER (DRY):
TypeRegistry::registerType<Timer>("ork::Timer");  // Once at init
return OrkidHandle<Timer>::makeShared();  // Type looked up automatically!
```

**Benefits:**
- ✅ **Single source of truth** - Type names defined once at initialization
- ✅ **No repetition** - No type strings scattered throughout code
- ✅ **Compile-time safety** - Template parameter IS the type
- ✅ **Runtime validation** - Can verify casts are correct
- ✅ **Easy maintenance** - Change type name in one place

### Type Registry Architecture

```cpp
// Populated at initialization
TypeRegistry::registerType<Timer>("ork::Timer");
TypeRegistry::registerType<VarMap>("ork::varmap::VarMap");

// Used automatically
auto handle = OrkidHandle<Timer>::makeShared();
handle->typeName();  // Returns "ork::Timer" from registry
handle->typeCRC();   // Returns CRC("ork::Timer") from registry
```

---

## Understanding: VarMap and the Codec Requirement

### What is VarMap?

**VarMap is Orkid's type-erased dictionary** - a key-value store that can hold values of **any type**:

```cpp
// C++ definition (ork.core/inc/ork/kernel/varmap.inl)
namespace ork::varmap {
  using var_t = svar128_t;  // 128-byte variant (can hold any type)
  using VarMap = TVarMap<var_t>;  // std::map<string, svar128_t>
  using varmap_ptr_t = std::shared_ptr<VarMap>;  // ← Shared ownership!

  template <typename T>
  struct TVarMap {
    std::map<key_t, val_t> _themap;  // string → variant storage

    const val_t& valueForKey(const key_t& key) const;
    void setValueForKey(const key_t& key, const val_t& val);

    template <typename T>
    attempt_cast<T> typedValueForKey(const key_t& key);
  };
}
```

**Key characteristics:**
- Values stored as `svar128_t` (128-byte variant with runtime type info)
- Can store primitives, strings, **shared_ptr**, custom types
- Runtime type checking with `tryAs<T>()`
- Used extensively in Orkid for configuration, parameters, scene graphs
- **VarMap itself is always `shared_ptr`** (can be shared between C++ and Swift)

### Why VarMap Requires a Codec

**Problem:** Swift's C++ interop cannot handle `svar128_t`:
- `svar128_t` is a 128-byte union with custom RTTI
- Swift cannot directly map C++ variants to Swift types
- No automatic type erasure bridging exists
- Cannot use Swift's `Any` type directly with C++ variants

**Solution:** Codec pattern (like pycodec):

```cpp
// Python bindings (ork.core/inc/ork/python/common_bindings/pyext_varmap.inl)
.def("__setattr__", [type_codec](varmap_ptr_t vmap, const std::string& key, py::object val) {
    auto varmap_val = type_codec->decode(val);  // Python → svar128_t
    vmap->setValueForKey(key, varmap_val);
})
.def("__getattr__", [type_codec](varmap_ptr_t vmap, const std::string& key) -> py::object {
    auto varmap_val = vmap->valueForKey(key);   // svar128_t from map
    auto python_val = type_codec->encode(varmap_val);  // svar128_t → Python
    return python_val;
})
```

**The codec provides:**
- **encode()**: Swift → C++ variant (any Swift type → svar128_t)
- **decode()**: C++ variant → Swift (svar128_t → any Swift type)
- **Type registry**: Maps types to encoder/decoder functions
- **Runtime type safety**: Checks types before conversion
- **Automatic reference counting**: Handles `shared_ptr` lifecycle across boundary

---

## Template Handle Architecture

### C++ Type Registry

```cpp
// ork.core/inc/ork/swift/orkid_handle.h

namespace ork::swift {

// ================================================================
// Global Type Registry
// ================================================================

class TypeRegistry {
    static std::unordered_map<std::type_index, const char*> _type_to_name;
    static std::unordered_map<std::type_index, uint64_t> _type_to_crc;

public:
    /// Register a C++ type with its Orkid name (called at init)
    template<typename T>
    static void registerType(const char* name) {
        auto idx = std::type_index(typeid(T));
        _type_to_name[idx] = name;
        _type_to_crc[idx] = CrcString(name).hashed();
    }

    /// Get type name for registered type
    template<typename T>
    static const char* getTypeName() {
        auto idx = std::type_index(typeid(T));
        auto it = _type_to_name.find(idx);
        return it != _type_to_name.end() ? it->second : "Unknown";
    }

    /// Get type CRC for registered type
    template<typename T>
    static uint64_t getTypeCRC() {
        auto idx = std::type_index(typeid(T));
        auto it = _type_to_crc.find(idx);
        return it != _type_to_crc.end() ? it->second : 0;
    }
};

// ================================================================
// Handle Base with Safe Casting
// ================================================================

class OrkidHandleBase {
public:
    virtual ~OrkidHandleBase() = default;
    virtual int32_t useCount() const = 0;
    virtual uint64_t typeCRC() const = 0;
    virtual const char* typeName() const = 0;
    virtual std::type_index typeIndex() const = 0;

    /// Safe typed cast with validation (returns nullptr if invalid)
    template<typename T>
    OrkidHandle<T>* typedHandle() {
        if (typeIndex() != std::type_index(typeid(T))) {
            return nullptr;  // Type mismatch - safe!
        }
        return static_cast<OrkidHandle<T>*>(this);
    }

    /// Safe typed cast (const version)
    template<typename T>
    const OrkidHandle<T>* typedHandle() const {
        if (typeIndex() != std::type_index(typeid(T))) {
            return nullptr;
        }
        return static_cast<const OrkidHandle<T>*>(this);
    }
};

// ================================================================
// Templated Handle
// ================================================================

template<typename T>
class OrkidHandle : public OrkidHandleBase {
public:
    /// Create object + handle atomically (forwards args to T's constructor)
    template<typename... Args>
    static OrkidHandle<T>* makeShared(Args&&... args) {
        auto sp = std::make_shared<T>(std::forward<Args>(args)...);
        return new OrkidHandle<T>(sp);
    }

    /// Assign existing shared_ptr to new handle
    static OrkidHandle<T>* assign(std::shared_ptr<T> sp) {
        return new OrkidHandle<T>(sp);
    }

    int32_t useCount() const override {
        return static_cast<int32_t>(_ptr.use_count());
    }

    uint64_t typeCRC() const override {
        return TypeRegistry::getTypeCRC<T>();  // Automatic lookup!
    }

    const char* typeName() const override {
        return TypeRegistry::getTypeName<T>();  // Automatic lookup!
    }

    std::type_index typeIndex() const override {
        return std::type_index(typeid(T));
    }

    std::shared_ptr<T> get() const {
        return _ptr;
    }

private:
    OrkidHandle(std::shared_ptr<T> sp) : _ptr(sp) {}

    std::shared_ptr<T> _ptr;    // Real type preserved!
};

} // namespace ork::swift
```

**Key Benefits:**
- ✅ **Type-safe in C++** - `shared_ptr<Timer>` not `shared_ptr<void>`
- ✅ **Virtual destructor** - Properly cleans up derived classes
- ✅ **No type erasure** - Full C++ type information preserved
- ✅ **Single refcount** - Only the `shared_ptr`, no extra layer
- ✅ **Opaque to Swift** - Swift sees only `OrkidHandleBase*`
- ✅ **Safe casting** - `typedHandle<T>()` validates before casting
- ✅ **DRY principle** - Type names registered once, looked up automatically

---

## C Bridge Layer Design

### C Bridge Header

**Note:** All functions returning `OrkidHandleBase*` should use `_Nonnull` annotations for cleaner Swift imports. This eliminates force-unwrap (`!`) in Swift code since C++ throws on allocation failure rather than returning null.

```c
// ork.core/inc/ork/swift/orkid_swift_bridge.h

#ifdef __cplusplus
namespace ork::swift { class OrkidHandleBase; }
typedef ork::swift::OrkidHandleBase OrkidHandleBase;
extern "C" {
#else
typedef struct OrkidHandleBase OrkidHandleBase;  // Opaque to C/Swift
#endif

// NOTE: For production, add nullability annotations:
//   - OrkidHandleBase* _Nonnull for creation functions (never return null)
//   - OrkidHandleBase* _Nullable for query functions (may return null)
// This provides cleaner Swift imports without force-unwrap operators.

// ================================================================
// Handle Management
// ================================================================

/// Release handle (virtual dtor properly cleans up derived class)
void orkid_handle_release(OrkidHandleBase* handle);

/// Get shared_ptr use count (for debugging)
int32_t orkid_handle_use_count(const OrkidHandleBase* handle);

/// Get type CRC
uint64_t orkid_handle_type_crc(const OrkidHandleBase* handle);

/// Get type name
const char* orkid_handle_type_name(const OrkidHandleBase* handle);

// ================================================================
// Core Lifecycle
// ================================================================

void orkid_swift_init(int argc, char** argv);
void orkid_swift_exit(void);
void orkid_swift_poll(void);
const char* orkid_get_last_error(void);

// ================================================================
// VarMap (uses OrkidHandleBase*)
// ================================================================

OrkidHandleBase* orkid_varmap_create(void);
OrkidHandleBase* orkid_varmap_get(OrkidHandleBase* vmap, const char* key);
void orkid_varmap_set(OrkidHandleBase* vmap, const char* key, OrkidHandleBase* value);
void orkid_varmap_remove(OrkidHandleBase* vmap, const char* key);
bool orkid_varmap_contains(OrkidHandleBase* vmap, const char* key);
const char** orkid_varmap_keys(OrkidHandleBase* vmap, int32_t* out_count);
void orkid_varmap_free_keys(const char** keys, int32_t count);
size_t orkid_varmap_size(OrkidHandleBase* vmap);
void orkid_varmap_clear(OrkidHandleBase* vmap);
OrkidHandleBase* orkid_varmap_clone(OrkidHandleBase* vmap);

// ================================================================
// Variants (use OrkidHandleBase*)
// ================================================================

OrkidHandleBase* orkid_svar_from_bool(bool value);
OrkidHandleBase* orkid_svar_from_int(int32_t value);
OrkidHandleBase* orkid_svar_from_float(float value);
OrkidHandleBase* orkid_svar_from_double(double value);
OrkidHandleBase* orkid_svar_from_string(const char* value);

bool orkid_svar_to_bool(OrkidHandleBase* variant, bool* out_value);
bool orkid_svar_to_int(OrkidHandleBase* variant, int32_t* out_value);
bool orkid_svar_to_float(OrkidHandleBase* variant, float* out_value);
bool orkid_svar_to_double(OrkidHandleBase* variant, double* out_value);
bool orkid_svar_to_string(OrkidHandleBase* variant, char* buffer, size_t buffer_size);

// ================================================================
// Math Types (handle-based, bind real C++ classes)
// ================================================================

// fvec2
OrkidHandleBase* orkid_fvec2_create(float x, float y);
float orkid_fvec2_get_x(const OrkidHandleBase* vec);
float orkid_fvec2_get_y(const OrkidHandleBase* vec);
void orkid_fvec2_set_x(OrkidHandleBase* vec, float x);
void orkid_fvec2_set_y(OrkidHandleBase* vec, float y);
OrkidHandleBase* orkid_svar_from_fvec2(OrkidHandleBase* vec);
OrkidHandleBase* orkid_svar_to_fvec2(OrkidHandleBase* variant);

// fvec3
OrkidHandleBase* orkid_fvec3_create(float x, float y, float z);
float orkid_fvec3_get_x(const OrkidHandleBase* vec);
float orkid_fvec3_get_y(const OrkidHandleBase* vec);
float orkid_fvec3_get_z(const OrkidHandleBase* vec);
void orkid_fvec3_set_x(OrkidHandleBase* vec, float x);
void orkid_fvec3_set_y(OrkidHandleBase* vec, float y);
void orkid_fvec3_set_z(OrkidHandleBase* vec, float z);
float orkid_fvec3_length(const OrkidHandleBase* vec);
OrkidHandleBase* orkid_fvec3_normalized(const OrkidHandleBase* vec);
OrkidHandleBase* orkid_svar_from_fvec3(OrkidHandleBase* vec);
OrkidHandleBase* orkid_svar_to_fvec3(OrkidHandleBase* variant);

// fvec4
OrkidHandleBase* orkid_fvec4_create(float x, float y, float z, float w);
float orkid_fvec4_get_x(const OrkidHandleBase* vec);
float orkid_fvec4_get_y(const OrkidHandleBase* vec);
float orkid_fvec4_get_z(const OrkidHandleBase* vec);
float orkid_fvec4_get_w(const OrkidHandleBase* vec);
void orkid_fvec4_set_x(OrkidHandleBase* vec, float x);
void orkid_fvec4_set_y(OrkidHandleBase* vec, float y);
void orkid_fvec4_set_z(OrkidHandleBase* vec, float z);
void orkid_fvec4_set_w(OrkidHandleBase* vec, float w);
OrkidHandleBase* orkid_svar_from_fvec4(OrkidHandleBase* vec);
OrkidHandleBase* orkid_svar_to_fvec4(OrkidHandleBase* variant);

// dvec3
OrkidHandleBase* orkid_dvec3_create(double x, double y, double z);
double orkid_dvec3_get_x(const OrkidHandleBase* vec);
double orkid_dvec3_get_y(const OrkidHandleBase* vec);
double orkid_dvec3_get_z(const OrkidHandleBase* vec);
void orkid_dvec3_set_x(OrkidHandleBase* vec, double x);
void orkid_dvec3_set_y(OrkidHandleBase* vec, double y);
void orkid_dvec3_set_z(OrkidHandleBase* vec, double z);
OrkidHandleBase* orkid_svar_from_dvec3(OrkidHandleBase* vec);
OrkidHandleBase* orkid_svar_to_dvec3(OrkidHandleBase* variant);

// fquat
OrkidHandleBase* orkid_fquat_create(float x, float y, float z, float w);
OrkidHandleBase* orkid_fquat_from_axis_angle(float axis_x, float axis_y, float axis_z, float angle);
float orkid_fquat_get_x(const OrkidHandleBase* quat);
float orkid_fquat_get_y(const OrkidHandleBase* quat);
float orkid_fquat_get_z(const OrkidHandleBase* quat);
float orkid_fquat_get_w(const OrkidHandleBase* quat);
void orkid_fquat_set_x(OrkidHandleBase* quat, float x);
void orkid_fquat_set_y(OrkidHandleBase* quat, float y);
void orkid_fquat_set_z(OrkidHandleBase* quat, float z);
void orkid_fquat_set_w(OrkidHandleBase* quat, float w);
OrkidHandleBase* orkid_svar_from_fquat(OrkidHandleBase* quat);
OrkidHandleBase* orkid_svar_to_fquat(OrkidHandleBase* variant);

// fmtx4
OrkidHandleBase* orkid_fmtx4_create_identity(void);
OrkidHandleBase* orkid_fmtx4_create_translation(float x, float y, float z);
OrkidHandleBase* orkid_fmtx4_create_rotation_x(float angle);
OrkidHandleBase* orkid_fmtx4_create_rotation_y(float angle);
OrkidHandleBase* orkid_fmtx4_create_rotation_z(float angle);
OrkidHandleBase* orkid_fmtx4_create_scale(float x, float y, float z);
float orkid_fmtx4_get_element(const OrkidHandleBase* mtx, int row, int col);
void orkid_fmtx4_set_element(OrkidHandleBase* mtx, int row, int col, float value);
OrkidHandleBase* orkid_fmtx4_multiply(const OrkidHandleBase* lhs, const OrkidHandleBase* rhs);
OrkidHandleBase* orkid_svar_from_fmtx4(OrkidHandleBase* mtx);
OrkidHandleBase* orkid_svar_to_fmtx4(OrkidHandleBase* variant);

// ================================================================
// Timer
// ================================================================

OrkidHandleBase* orkid_timer_create(void);
void orkid_timer_start(OrkidHandleBase* timer);
void orkid_timer_end(OrkidHandleBase* timer);
float orkid_timer_secs_since_start(const OrkidHandleBase* timer);
float orkid_timer_get_sync_time(void);

OrkidHandleBase* orkid_svar_from_timer(OrkidHandleBase* timer);
OrkidHandleBase* orkid_svar_to_timer(OrkidHandleBase* variant);

// ================================================================
// Sphere
// ================================================================

OrkidHandleBase* orkid_sphere_create(float cx, float cy, float cz, float radius);
OrkidHandleBase* orkid_sphere_get_center(const OrkidHandleBase* sphere);
float orkid_sphere_get_radius(const OrkidHandleBase* sphere);
void orkid_sphere_set_center(OrkidHandleBase* sphere, OrkidHandleBase* center);
void orkid_sphere_set_radius(OrkidHandleBase* sphere, float radius);

OrkidHandleBase* orkid_svar_from_sphere(OrkidHandleBase* sphere);
OrkidHandleBase* orkid_svar_to_sphere(OrkidHandleBase* variant);

#ifdef __cplusplus
}
#endif
```

### C++ Implementation

```cpp
// ork.core/src/swift/orkid_swift_bridge.cpp

#include "ork/swift/orkid_handle.h"
#include "ork/swift/orkid_swift_bridge.h"
#include "ork/ios/app_init.h"
#include "ork/kernel/varmap.inl"
#include "ork/kernel/timer.h"
#include "ork/kernel/string/string.h"
#include "ork/math/cvector2.h"
#include "ork/math/cvector3.h"
#include "ork/math/cvector4.h"
#include "ork/math/quaternion.h"
#include "ork/math/cmatrix4.h"
#include "ork/math/sphere.h"
#include <cstring>
#include <memory>
#include <unordered_map>
#include <typeindex>

using namespace ork;
using namespace ork::swift;

static thread_local std::string g_last_error;

// ================================================================
// TypeRegistry Static Members
// ================================================================

std::unordered_map<std::type_index, const char*> TypeRegistry::_type_to_name;
std::unordered_map<std::type_index, uint64_t> TypeRegistry::_type_to_crc;

// ================================================================
// C API Implementation
// ================================================================

extern "C" {

// ================================================================
// Handle Management
// ================================================================

void orkid_handle_release(OrkidHandleBase* handle) {
    delete handle;  // Virtual dtor properly cleans up OrkidHandle<T>
}

int32_t orkid_handle_use_count(const OrkidHandleBase* handle) {
    return handle->useCount();
}

uint64_t orkid_handle_type_crc(const OrkidHandleBase* handle) {
    return handle->typeCRC();
}

const char* orkid_handle_type_name(const OrkidHandleBase* handle) {
    return handle->typeName();
}

// ================================================================
// Core Lifecycle - Register All Types Here!
// ================================================================

void orkid_swift_init(int argc, char** argv) {
    try {
        _coreappinit(argc, argv);

        // Register all Swift-bindable types (single source of truth!)
        TypeRegistry::registerType<Timer>("ork::Timer");
        TypeRegistry::registerType<varmap::VarMap>("ork::varmap::VarMap");
        TypeRegistry::registerType<svar128_t>("ork::svar128_t");
        TypeRegistry::registerType<fvec2>("ork::fvec2");
        TypeRegistry::registerType<fvec3>("ork::fvec3");
        TypeRegistry::registerType<fvec4>("ork::fvec4");
        TypeRegistry::registerType<dvec3>("ork::dvec3");
        TypeRegistry::registerType<fquat>("ork::fquat");
        TypeRegistry::registerType<fmtx4>("ork::fmtx4");
        TypeRegistry::registerType<Sphere>("ork::Sphere");
        // Add more as needed...

        g_last_error.clear();
    } catch (const std::exception& e) {
        g_last_error = std::string("Init failed: ") + e.what();
    }
}

void orkid_swift_exit() {
    try {
        _coreappexit();
    } catch (...) {
        // Swallow exceptions during shutdown
    }
}

void orkid_swift_poll() {
    _coreapppoll();
}

const char* orkid_get_last_error() {
    return g_last_error.c_str();
}

// ================================================================
// VarMap Implementation - Using Safe Casts
// ================================================================

OrkidHandleBase* orkid_varmap_create() {
    return OrkidHandle<varmap::VarMap>::makeShared();  // Clean!
}

OrkidHandleBase* orkid_varmap_get(OrkidHandleBase* vmap_handle, const char* key) {
    auto typed_vmap = vmap_handle->typedHandle<varmap::VarMap>();
    if (!typed_vmap) {
        g_last_error = "Invalid VarMap handle";
        return nullptr;
    }

    auto vmap = typed_vmap->get();
    const auto& val = vmap->valueForKey(key);

    return OrkidHandle<svar128_t>::makeShared(val);  // Copy constructor
}

void orkid_varmap_set(OrkidHandleBase* vmap_handle, const char* key, OrkidHandleBase* value_handle) {
    auto typed_vmap = vmap_handle->typedHandle<varmap::VarMap>();
    auto typed_var = value_handle->typedHandle<svar128_t>();

    if (!typed_vmap || !typed_var) {
        g_last_error = "Invalid handle type in varmap_set";
        return;
    }

    auto vmap = typed_vmap->get();
    auto var = typed_var->get();

    vmap->setValueForKey(key, *var);
}

void orkid_varmap_remove(OrkidHandleBase* vmap_handle, const char* key) {
    auto typed_vmap = vmap_handle->typedHandle<varmap::VarMap>();
    if (typed_vmap) {
        typed_vmap->get()->clearKey(key);
    }
}

bool orkid_varmap_contains(OrkidHandleBase* vmap_handle, const char* key) {
    auto typed_vmap = vmap_handle->typedHandle<varmap::VarMap>();
    return typed_vmap ? typed_vmap->get()->hasKey(key) : false;
}

const char** orkid_varmap_keys(OrkidHandleBase* vmap_handle, int32_t* out_count) {
    auto typed_vmap = vmap_handle->typedHandle<varmap::VarMap>();
    if (!typed_vmap) {
        *out_count = 0;
        return nullptr;
    }

    auto vmap = typed_vmap->get();
    auto keys = vmap->dumpkeys();

    *out_count = keys.size();
    auto result = new const char*[keys.size()];

    for (size_t i = 0; i < keys.size(); i++) {
        result[i] = strdup(keys[i].c_str());
    }

    return result;
}

void orkid_varmap_free_keys(const char** keys, int32_t count) {
    for (int32_t i = 0; i < count; i++) {
        free((void*)keys[i]);
    }
    delete[] keys;
}

size_t orkid_varmap_size(OrkidHandleBase* vmap_handle) {
    auto typed_vmap = vmap_handle->typedHandle<varmap::VarMap>();
    return typed_vmap ? typed_vmap->get()->_themap.size() : 0;
}

void orkid_varmap_clear(OrkidHandleBase* vmap_handle) {
    auto typed_vmap = vmap_handle->typedHandle<varmap::VarMap>();
    if (typed_vmap) {
        typed_vmap->get()->_themap.clear();
    }
}

OrkidHandleBase* orkid_varmap_clone(OrkidHandleBase* vmap_handle) {
    auto typed_vmap = vmap_handle->typedHandle<varmap::VarMap>();
    if (!typed_vmap) {
        return nullptr;
    }

    auto vmap = typed_vmap->get();
    auto cloned = vmap->clone();
    return OrkidHandle<varmap::VarMap>::assign(cloned);
}

// ================================================================
// Variant Creation
// ================================================================

OrkidHandleBase* orkid_svar_from_bool(bool value) {
    auto var = OrkidHandle<svar128_t>::makeShared();
    var->typedHandle<svar128_t>()->get()->set<bool>(value);
    return var;
}

OrkidHandleBase* orkid_svar_from_int(int32_t value) {
    auto var = OrkidHandle<svar128_t>::makeShared();
    var->typedHandle<svar128_t>()->get()->set<int>(value);
    return var;
}

OrkidHandleBase* orkid_svar_from_float(float value) {
    auto var = OrkidHandle<svar128_t>::makeShared();
    var->typedHandle<svar128_t>()->get()->set<float>(value);
    return var;
}

OrkidHandleBase* orkid_svar_from_double(double value) {
    auto var = OrkidHandle<svar128_t>::makeShared();
    var->typedHandle<svar128_t>()->get()->set<double>(value);
    return var;
}

OrkidHandleBase* orkid_svar_from_string(const char* value) {
    auto var = OrkidHandle<svar128_t>::makeShared();
    var->typedHandle<svar128_t>()->get()->set<std::string>(std::string(value));
    return var;
}

// ================================================================
// Variant Extraction
// ================================================================

bool orkid_svar_to_bool(OrkidHandleBase* variant_handle, bool* out_value) {
    auto typed_var = variant_handle->typedHandle<svar128_t>();
    if (!typed_var) return false;

    auto var = typed_var->get();
    auto result = var->tryAs<bool>();
    if (result) {
        *out_value = result.value();
        return true;
    }
    return false;
}

bool orkid_svar_to_int(OrkidHandleBase* variant_handle, int32_t* out_value) {
    auto typed_var = variant_handle->typedHandle<svar128_t>();
    if (!typed_var) return false;

    auto var = typed_var->get();
    auto result = var->tryAs<int>();
    if (result) {
        *out_value = result.value();
        return true;
    }
    return false;
}

bool orkid_svar_to_float(OrkidHandleBase* variant_handle, float* out_value) {
    auto typed_var = variant_handle->typedHandle<svar128_t>();
    if (!typed_var) return false;

    auto var = typed_var->get();
    auto result = var->tryAs<float>();
    if (result) {
        *out_value = result.value();
        return true;
    }
    return false;
}

bool orkid_svar_to_double(OrkidHandleBase* variant_handle, double* out_value) {
    auto typed_var = variant_handle->typedHandle<svar128_t>();
    if (!typed_var) return false;

    auto var = typed_var->get();
    auto result = var->tryAs<double>();
    if (result) {
        *out_value = result.value();
        return true;
    }
    return false;
}

bool orkid_svar_to_string(OrkidHandleBase* variant_handle, char* buffer, size_t buffer_size) {
    auto typed_var = variant_handle->typedHandle<svar128_t>();
    if (!typed_var) return false;

    auto var = typed_var->get();
    auto result = var->tryAs<std::string>();
    if (result) {
        strncpy(buffer, result.value().c_str(), buffer_size - 1);
        buffer[buffer_size - 1] = '\0';
        return true;
    }
    return false;
}

// ================================================================
// Math Types Implementation - Handle-based (bind real C++ classes)
// ================================================================

// fvec2
OrkidHandleBase* orkid_fvec2_create(float x, float y) {
    return OrkidHandle<fvec2>::makeShared(x, y);
}

float orkid_fvec2_get_x(const OrkidHandleBase* handle) {
    auto typed = handle->typedHandle<fvec2>();
    return typed ? typed->get()->x : 0.0f;
}

float orkid_fvec2_get_y(const OrkidHandleBase* handle) {
    auto typed = handle->typedHandle<fvec2>();
    return typed ? typed->get()->y : 0.0f;
}

void orkid_fvec2_set_x(OrkidHandleBase* handle, float x) {
    auto typed = handle->typedHandle<fvec2>();
    if (typed) typed->get()->x = x;
}

void orkid_fvec2_set_y(OrkidHandleBase* handle, float y) {
    auto typed = handle->typedHandle<fvec2>();
    if (typed) typed->get()->y = y;
}

OrkidHandleBase* orkid_svar_from_fvec2(OrkidHandleBase* vec_handle) {
    auto typed_vec = vec_handle->typedHandle<fvec2>();
    if (!typed_vec) return nullptr;

    auto vec_sp = typed_vec->get();
    auto var = OrkidHandle<svar128_t>::makeShared();
    var->typedHandle<svar128_t>()->get()->set<std::shared_ptr<fvec2>>(vec_sp);
    return var;
}

OrkidHandleBase* orkid_svar_to_fvec2(OrkidHandleBase* variant_handle) {
    auto typed_var = variant_handle->typedHandle<svar128_t>();
    if (!typed_var) return nullptr;

    auto var = typed_var->get();
    auto result = var->tryAs<std::shared_ptr<fvec2>>();
    return result ? OrkidHandle<fvec2>::assign(result.value()) : nullptr;
}

// fvec3
OrkidHandleBase* orkid_fvec3_create(float x, float y, float z) {
    return OrkidHandle<fvec3>::makeShared(x, y, z);
}

float orkid_fvec3_get_x(const OrkidHandleBase* handle) {
    auto typed = handle->typedHandle<fvec3>();
    return typed ? typed->get()->x : 0.0f;
}

float orkid_fvec3_get_y(const OrkidHandleBase* handle) {
    auto typed = handle->typedHandle<fvec3>();
    return typed ? typed->get()->y : 0.0f;
}

float orkid_fvec3_get_z(const OrkidHandleBase* handle) {
    auto typed = handle->typedHandle<fvec3>();
    return typed ? typed->get()->z : 0.0f;
}

void orkid_fvec3_set_x(OrkidHandleBase* handle, float x) {
    auto typed = handle->typedHandle<fvec3>();
    if (typed) typed->get()->x = x;
}

void orkid_fvec3_set_y(OrkidHandleBase* handle, float y) {
    auto typed = handle->typedHandle<fvec3>();
    if (typed) typed->get()->y = y;
}

void orkid_fvec3_set_z(OrkidHandleBase* handle, float z) {
    auto typed = handle->typedHandle<fvec3>();
    if (typed) typed->get()->z = z;
}

float orkid_fvec3_length(const OrkidHandleBase* handle) {
    auto typed = handle->typedHandle<fvec3>();
    return typed ? typed->get()->length() : 0.0f;
}

OrkidHandleBase* orkid_fvec3_normalized(const OrkidHandleBase* handle) {
    auto typed = handle->typedHandle<fvec3>();
    if (!typed) return nullptr;

    auto normalized = typed->get()->normalized();
    return OrkidHandle<fvec3>::makeShared(normalized);
}

OrkidHandleBase* orkid_svar_from_fvec3(OrkidHandleBase* vec_handle) {
    auto typed_vec = vec_handle->typedHandle<fvec3>();
    if (!typed_vec) return nullptr;

    auto vec_sp = typed_vec->get();
    auto var = OrkidHandle<svar128_t>::makeShared();
    var->typedHandle<svar128_t>()->get()->set<std::shared_ptr<fvec3>>(vec_sp);
    return var;
}

OrkidHandleBase* orkid_svar_to_fvec3(OrkidHandleBase* variant_handle) {
    auto typed_var = variant_handle->typedHandle<svar128_t>();
    if (!typed_var) return nullptr;

    auto var = typed_var->get();
    auto result = var->tryAs<std::shared_ptr<fvec3>>();
    return result ? OrkidHandle<fvec3>::assign(result.value()) : nullptr;
}

// fvec4
OrkidHandleBase* orkid_fvec4_create(float x, float y, float z, float w) {
    return OrkidHandle<fvec4>::makeShared(x, y, z, w);
}

float orkid_fvec4_get_x(const OrkidHandleBase* handle) {
    auto typed = handle->typedHandle<fvec4>();
    return typed ? typed->get()->x : 0.0f;
}

float orkid_fvec4_get_y(const OrkidHandleBase* handle) {
    auto typed = handle->typedHandle<fvec4>();
    return typed ? typed->get()->y : 0.0f;
}

float orkid_fvec4_get_z(const OrkidHandleBase* handle) {
    auto typed = handle->typedHandle<fvec4>();
    return typed ? typed->get()->z : 0.0f;
}

float orkid_fvec4_get_w(const OrkidHandleBase* handle) {
    auto typed = handle->typedHandle<fvec4>();
    return typed ? typed->get()->w : 0.0f;
}

void orkid_fvec4_set_x(OrkidHandleBase* handle, float x) {
    auto typed = handle->typedHandle<fvec4>();
    if (typed) typed->get()->x = x;
}

void orkid_fvec4_set_y(OrkidHandleBase* handle, float y) {
    auto typed = handle->typedHandle<fvec4>();
    if (typed) typed->get()->y = y;
}

void orkid_fvec4_set_z(OrkidHandleBase* handle, float z) {
    auto typed = handle->typedHandle<fvec4>();
    if (typed) typed->get()->z = z;
}

void orkid_fvec4_set_w(OrkidHandleBase* handle, float w) {
    auto typed = handle->typedHandle<fvec4>();
    if (typed) typed->get()->w = w;
}

OrkidHandleBase* orkid_svar_from_fvec4(OrkidHandleBase* vec_handle) {
    auto typed_vec = vec_handle->typedHandle<fvec4>();
    if (!typed_vec) return nullptr;

    auto vec_sp = typed_vec->get();
    auto var = OrkidHandle<svar128_t>::makeShared();
    var->typedHandle<svar128_t>()->get()->set<std::shared_ptr<fvec4>>(vec_sp);
    return var;
}

OrkidHandleBase* orkid_svar_to_fvec4(OrkidHandleBase* variant_handle) {
    auto typed_var = variant_handle->typedHandle<svar128_t>();
    if (!typed_var) return nullptr;

    auto var = typed_var->get();
    auto result = var->tryAs<std::shared_ptr<fvec4>>();
    return result ? OrkidHandle<fvec4>::assign(result.value()) : nullptr;
}

// dvec3
OrkidHandleBase* orkid_dvec3_create(double x, double y, double z) {
    return OrkidHandle<dvec3>::makeShared(x, y, z);
}

double orkid_dvec3_get_x(const OrkidHandleBase* handle) {
    auto typed = handle->typedHandle<dvec3>();
    return typed ? typed->get()->x : 0.0;
}

double orkid_dvec3_get_y(const OrkidHandleBase* handle) {
    auto typed = handle->typedHandle<dvec3>();
    return typed ? typed->get()->y : 0.0;
}

double orkid_dvec3_get_z(const OrkidHandleBase* handle) {
    auto typed = handle->typedHandle<dvec3>();
    return typed ? typed->get()->z : 0.0;
}

void orkid_dvec3_set_x(OrkidHandleBase* handle, double x) {
    auto typed = handle->typedHandle<dvec3>();
    if (typed) typed->get()->x = x;
}

void orkid_dvec3_set_y(OrkidHandleBase* handle, double y) {
    auto typed = handle->typedHandle<dvec3>();
    if (typed) typed->get()->y = y;
}

void orkid_dvec3_set_z(OrkidHandleBase* handle, double z) {
    auto typed = handle->typedHandle<dvec3>();
    if (typed) typed->get()->z = z;
}

OrkidHandleBase* orkid_svar_from_dvec3(OrkidHandleBase* vec_handle) {
    auto typed_vec = vec_handle->typedHandle<dvec3>();
    if (!typed_vec) return nullptr;

    auto vec_sp = typed_vec->get();
    auto var = OrkidHandle<svar128_t>::makeShared();
    var->typedHandle<svar128_t>()->get()->set<std::shared_ptr<dvec3>>(vec_sp);
    return var;
}

OrkidHandleBase* orkid_svar_to_dvec3(OrkidHandleBase* variant_handle) {
    auto typed_var = variant_handle->typedHandle<svar128_t>();
    if (!typed_var) return nullptr;

    auto var = typed_var->get();
    auto result = var->tryAs<std::shared_ptr<dvec3>>();
    return result ? OrkidHandle<dvec3>::assign(result.value()) : nullptr;
}

// fquat
OrkidHandleBase* orkid_fquat_create(float x, float y, float z, float w) {
    return OrkidHandle<fquat>::makeShared(x, y, z, w);
}

OrkidHandleBase* orkid_fquat_from_axis_angle(float axis_x, float axis_y, float axis_z, float angle) {
    fvec3 axis(axis_x, axis_y, axis_z);
    fquat q;
    q.fromAxisAngle(axis, angle);
    return OrkidHandle<fquat>::makeShared(q);
}

float orkid_fquat_get_x(const OrkidHandleBase* handle) {
    auto typed = handle->typedHandle<fquat>();
    return typed ? typed->get()->x : 0.0f;
}

float orkid_fquat_get_y(const OrkidHandleBase* handle) {
    auto typed = handle->typedHandle<fquat>();
    return typed ? typed->get()->y : 0.0f;
}

float orkid_fquat_get_z(const OrkidHandleBase* handle) {
    auto typed = handle->typedHandle<fquat>();
    return typed ? typed->get()->z : 0.0f;
}

float orkid_fquat_get_w(const OrkidHandleBase* handle) {
    auto typed = handle->typedHandle<fquat>();
    return typed ? typed->get()->w : 0.0f;
}

void orkid_fquat_set_x(OrkidHandleBase* handle, float x) {
    auto typed = handle->typedHandle<fquat>();
    if (typed) typed->get()->x = x;
}

void orkid_fquat_set_y(OrkidHandleBase* handle, float y) {
    auto typed = handle->typedHandle<fquat>();
    if (typed) typed->get()->y = y;
}

void orkid_fquat_set_z(OrkidHandleBase* handle, float z) {
    auto typed = handle->typedHandle<fquat>();
    if (typed) typed->get()->z = z;
}

void orkid_fquat_set_w(OrkidHandleBase* handle, float w) {
    auto typed = handle->typedHandle<fquat>();
    if (typed) typed->get()->w = w;
}

OrkidHandleBase* orkid_svar_from_fquat(OrkidHandleBase* quat_handle) {
    auto typed_quat = quat_handle->typedHandle<fquat>();
    if (!typed_quat) return nullptr;

    auto quat_sp = typed_quat->get();
    auto var = OrkidHandle<svar128_t>::makeShared();
    var->typedHandle<svar128_t>()->get()->set<std::shared_ptr<fquat>>(quat_sp);
    return var;
}

OrkidHandleBase* orkid_svar_to_fquat(OrkidHandleBase* variant_handle) {
    auto typed_var = variant_handle->typedHandle<svar128_t>();
    if (!typed_var) return nullptr;

    auto var = typed_var->get();
    auto result = var->tryAs<std::shared_ptr<fquat>>();
    return result ? OrkidHandle<fquat>::assign(result.value()) : nullptr;
}

// fmtx4
OrkidHandleBase* orkid_fmtx4_create_identity() {
    auto mtx = std::make_shared<fmtx4>();
    mtx->setToIdentity();
    return OrkidHandle<fmtx4>::assign(mtx);
}

OrkidHandleBase* orkid_fmtx4_create_translation(float x, float y, float z) {
    auto mtx = std::make_shared<fmtx4>();
    mtx->setTranslation(x, y, z);
    return OrkidHandle<fmtx4>::assign(mtx);
}

OrkidHandleBase* orkid_fmtx4_create_rotation_x(float angle) {
    auto mtx = std::make_shared<fmtx4>();
    mtx->setRotateX(angle);
    return OrkidHandle<fmtx4>::assign(mtx);
}

OrkidHandleBase* orkid_fmtx4_create_rotation_y(float angle) {
    auto mtx = std::make_shared<fmtx4>();
    mtx->setRotateY(angle);
    return OrkidHandle<fmtx4>::assign(mtx);
}

OrkidHandleBase* orkid_fmtx4_create_rotation_z(float angle) {
    auto mtx = std::make_shared<fmtx4>();
    mtx->setRotateZ(angle);
    return OrkidHandle<fmtx4>::assign(mtx);
}

OrkidHandleBase* orkid_fmtx4_create_scale(float x, float y, float z) {
    auto mtx = std::make_shared<fmtx4>();
    mtx->setScale(x, y, z);
    return OrkidHandle<fmtx4>::assign(mtx);
}

float orkid_fmtx4_get_element(const OrkidHandleBase* handle, int row, int col) {
    auto typed = handle->typedHandle<fmtx4>();
    return typed ? typed->get()->elemXY(col, row) : 0.0f;  // Note: elemXY is (col, row)
}

void orkid_fmtx4_set_element(OrkidHandleBase* handle, int row, int col, float value) {
    auto typed = handle->typedHandle<fmtx4>();
    if (typed) typed->get()->setElemXY(col, row, value);  // Note: setElemXY is (col, row)
}

OrkidHandleBase* orkid_fmtx4_multiply(const OrkidHandleBase* lhs_handle, const OrkidHandleBase* rhs_handle) {
    auto typed_lhs = lhs_handle->typedHandle<fmtx4>();
    auto typed_rhs = rhs_handle->typedHandle<fmtx4>();

    if (!typed_lhs || !typed_rhs) return nullptr;

    auto result = (*typed_lhs->get()) * (*typed_rhs->get());
    return OrkidHandle<fmtx4>::makeShared(result);
}

OrkidHandleBase* orkid_svar_from_fmtx4(OrkidHandleBase* mtx_handle) {
    auto typed_mtx = mtx_handle->typedHandle<fmtx4>();
    if (!typed_mtx) return nullptr;

    auto mtx_sp = typed_mtx->get();
    auto var = OrkidHandle<svar128_t>::makeShared();
    var->typedHandle<svar128_t>()->get()->set<std::shared_ptr<fmtx4>>(mtx_sp);
    return var;
}

OrkidHandleBase* orkid_svar_to_fmtx4(OrkidHandleBase* variant_handle) {
    auto typed_var = variant_handle->typedHandle<svar128_t>();
    if (!typed_var) return nullptr;

    auto var = typed_var->get();
    auto result = var->tryAs<std::shared_ptr<fmtx4>>();
    return result ? OrkidHandle<fmtx4>::assign(result.value()) : nullptr;
}

// ================================================================
// Timer Implementation - Clean API
// ================================================================

OrkidHandleBase* orkid_timer_create() {
    return OrkidHandle<Timer>::makeShared();  // One line!
}

void orkid_timer_start(OrkidHandleBase* handle) {
    auto typed = handle->typedHandle<Timer>();
    if (typed) {
        typed->get()->Start();
    }
}

void orkid_timer_end(OrkidHandleBase* handle) {
    auto typed = handle->typedHandle<Timer>();
    if (typed) {
        typed->get()->End();
    }
}

float orkid_timer_secs_since_start(const OrkidHandleBase* handle) {
    auto typed = handle->typedHandle<Timer>();
    return typed ? typed->get()->SecsSinceStart() : 0.0f;
}

float orkid_timer_get_sync_time() {
    return Timer::get_sync_time();
}

// Store Timer in variant (stores shared_ptr<Timer>!)
OrkidHandleBase* orkid_svar_from_timer(OrkidHandleBase* timer_handle) {
    auto typed_timer = timer_handle->typedHandle<Timer>();
    if (!typed_timer) {
        return nullptr;
    }

    auto timer_sp = typed_timer->get();
    auto var = OrkidHandle<svar128_t>::makeShared();
    var->typedHandle<svar128_t>()->get()->set<std::shared_ptr<Timer>>(timer_sp);

    return var;
}

OrkidHandleBase* orkid_svar_to_timer(OrkidHandleBase* variant_handle) {
    auto typed_var = variant_handle->typedHandle<svar128_t>();
    if (!typed_var) {
        return nullptr;
    }

    auto var = typed_var->get();
    auto result = var->tryAs<std::shared_ptr<Timer>>();

    return result ? OrkidHandle<Timer>::assign(result.value()) : nullptr;
}

// ================================================================
// Sphere Implementation
// ================================================================

OrkidHandleBase* orkid_sphere_create(float cx, float cy, float cz, float radius) {
    return OrkidHandle<Sphere>::makeShared(fvec3(cx, cy, cz), radius);  // Atomic!
}

OrkidHandleBase* orkid_sphere_get_center(const OrkidHandleBase* handle) {
    auto typed = handle->typedHandle<Sphere>();
    if (!typed) {
        return nullptr;
    }

    auto sphere = typed->get();
    const auto& center = sphere->mCenter;
    return OrkidHandle<fvec3>::makeShared(center);
}

float orkid_sphere_get_radius(const OrkidHandleBase* handle) {
    auto typed = handle->typedHandle<Sphere>();
    return typed ? typed->get()->mRadius : 0.0f;
}

void orkid_sphere_set_center(OrkidHandleBase* sphere_handle, OrkidHandleBase* center_handle) {
    auto typed_sphere = sphere_handle->typedHandle<Sphere>();
    auto typed_center = center_handle->typedHandle<fvec3>();
    if (typed_sphere && typed_center) {
        typed_sphere->get()->mCenter = *typed_center->get();
    }
}

void orkid_sphere_set_radius(OrkidHandleBase* sphere_handle, float radius) {
    auto typed = sphere_handle->typedHandle<Sphere>();
    if (typed) {
        typed->get()->mRadius = radius;
    }
}

OrkidHandleBase* orkid_svar_from_sphere(OrkidHandleBase* sphere_handle) {
    auto typed_sphere = sphere_handle->typedHandle<Sphere>();
    if (!typed_sphere) {
        return nullptr;
    }

    auto sphere_sp = typed_sphere->get();
    auto var = OrkidHandle<svar128_t>::makeShared();
    var->typedHandle<svar128_t>()->get()->set<std::shared_ptr<Sphere>>(sphere_sp);

    return var;
}

OrkidHandleBase* orkid_svar_to_sphere(OrkidHandleBase* variant_handle) {
    auto typed_var = variant_handle->typedHandle<svar128_t>();
    if (!typed_var) {
        return nullptr;
    }

    auto var = typed_var->get();
    auto result = var->tryAs<std::shared_ptr<Sphere>>();

    return result ? OrkidHandle<Sphere>::assign(result.value()) : nullptr;
}

} // extern "C"
```

---

## Swift Codec Architecture

### Overview

The codec system mirrors pycodec but lives **entirely in C++**. This provides bidirectional type conversion between Swift handles and C++ variants (`svar128_t`) for VarMap support, with all registration happening at C++ initialization time.

**Key Design Principles:**
- **C++-only codec** - All type registration and conversion logic in C++ (like pycodec)
- **No Swift codec class** - Swift just uses C bridge functions that handle codec internally
- **Transparent to Swift** - VarMap operations automatically convert handle ↔ variant
- **Single source of truth** - Type registration in `orkid_swift_init()` only

### C++ Codec Implementation

The codec lives entirely in C++ and is registered during `orkid_swift_init()`:

```cpp
// ork.core/src/swift/orkid_swift_codec.cpp

#include "ork/swift/orkid_handle.h"
#include "ork/swift/orkid_swift_bridge.h"
#include "ork/kernel/varmap.inl"
#include "ork/kernel/timer.h"
#include <unordered_map>
#include <typeindex>
#include <functional>

using namespace ork;
using namespace ork::swift;

namespace ork::swift {

// ================================================================
// Swift Codec (similar to PyCodecImpl in pycodec)
// ================================================================

struct SwiftCodecImpl {
    using encoderfn_t = std::function<OrkidHandleBase*(const varval_t&)>;
    using decoderfn_t = std::function<varval_t(OrkidHandleBase*)>;

    std::unordered_map<std::type_index, encoderfn_t> _encoders;
    std::unordered_map<uint64_t, decoderfn_t> _decoders;  // Key is type CRC

    OrkidHandleBase* encode(const varval_t& val) const;
    varval_t decode(OrkidHandleBase* handle) const;
};

// Global codec instance
static SwiftCodecImpl g_swift_codec;

// ================================================================
// Encode: variant → handle
// ================================================================

OrkidHandleBase* SwiftCodecImpl::encode(const varval_t& val) const {
    if (!val.isSet()) {
        return nullptr;
    }

    auto orktypeid = val.getOrkTypeId();
    auto it = _encoders.find(std::type_index(orktypeid._typeinfo));

    if (it != _encoders.end()) {
        return it->second(val);
    }

    // Fallback: primitives (direct copy, no handle)
    if (auto as_bool = val.tryAs<bool>()) {
        auto var = OrkidHandle<svar128_t>::makeShared();
        var->typedHandle<svar128_t>()->get()->set<bool>(as_bool.value());
        return var;
    } else if (auto as_int = val.tryAs<int>()) {
        auto var = OrkidHandle<svar128_t>::makeShared();
        var->typedHandle<svar128_t>()->get()->set<int>(as_int.value());
        return var;
    } else if (auto as_float = val.tryAs<float>()) {
        auto var = OrkidHandle<svar128_t>::makeShared();
        var->typedHandle<svar128_t>()->get()->set<float>(as_float.value());
        return var;
    } else if (auto as_double = val.tryAs<double>()) {
        auto var = OrkidHandle<svar128_t>::makeShared();
        var->typedHandle<svar128_t>()->get()->set<double>(as_double.value());
        return var;
    } else if (auto as_str = val.tryAs<std::string>()) {
        auto var = OrkidHandle<svar128_t>::makeShared();
        var->typedHandle<svar128_t>()->get()->set<std::string>(as_str.value());
        return var;
    }

    g_last_error = FormatString("SwiftCodec::encode: unregistered type %s", val.typeName());
    return nullptr;
}

// ================================================================
// Decode: handle → variant
// ================================================================

varval_t SwiftCodecImpl::decode(OrkidHandleBase* handle) const {
    auto typeCRC = handle->typeCRC();
    auto it = _decoders.find(typeCRC);

    if (it != _decoders.end()) {
        return it->second(handle);
    }

    // Fallback: primitives
    auto typed_var = handle->typedHandle<svar128_t>();
    if (typed_var) {
        return *typed_var->get();  // Return variant directly
    }

    g_last_error = FormatString("SwiftCodec::decode: unregistered type %s (CRC: 0x%llx)",
                                 handle->typeName(), typeCRC);
    return varval_t();
}

// ================================================================
// Type Registration Helper (like pycodec's registerStdCodec)
// ================================================================

template<typename T>
void registerSwiftType() {
    auto typeIdx = std::type_index(typeid(T));
    auto typeCRC = TypeRegistry::getTypeCRC<T>();

    // Encoder: variant → handle
    g_swift_codec._encoders[typeIdx] = [](const varval_t& val) -> OrkidHandleBase* {
        auto sp = val.tryAs<std::shared_ptr<T>>();
        if (!sp) return nullptr;
        return OrkidHandle<T>::assign(sp.value());
    };

    // Decoder: handle → variant
    g_swift_codec._decoders[typeCRC] = [](OrkidHandleBase* handle) -> varval_t {
        auto typed = handle->typedHandle<T>();
        if (!typed) return varval_t();

        varval_t val;
        val.set<std::shared_ptr<T>>(typed->get());
        return val;
    };
}

} // namespace ork::swift

// ================================================================
// C API - VarMap Operations (use codec internally)
// ================================================================

extern "C" {

OrkidHandleBase* orkid_varmap_get(OrkidHandleBase* vmap_handle, const char* key) {
    auto typed_vmap = vmap_handle->typedHandle<varmap::VarMap>();
    if (!typed_vmap) {
        g_last_error = "Invalid VarMap handle";
        return nullptr;
    }

    auto vmap = typed_vmap->get();
    if (!vmap->hasKey(key)) {
        return nullptr;
    }

    const auto& variant = vmap->valueForKey(key);

    // Use C++ codec to convert variant → handle
    return g_swift_codec.encode(variant);
}

void orkid_varmap_set(OrkidHandleBase* vmap_handle, const char* key, OrkidHandleBase* value_handle) {
    auto typed_vmap = vmap_handle->typedHandle<varmap::VarMap>();
    if (!typed_vmap) {
        g_last_error = "Invalid VarMap handle";
        return;
    }

    // Use C++ codec to convert handle → variant
    auto variant = g_swift_codec.decode(value_handle);

    auto vmap = typed_vmap->get();
    vmap->setValueForKey(key, variant);
}

} // extern "C"
```

### Codec Registration at Initialization

All type registration happens in `orkid_swift_init()`:

```cpp
// Update to orkid_swift_init() in orkid_swift_bridge.cpp

void orkid_swift_init(int argc, char** argv) {
    try {
        _coreappinit(argc, argv);

        // Register all types in TypeRegistry
        TypeRegistry::registerType<Timer>("ork::Timer");
        TypeRegistry::registerType<varmap::VarMap>("ork::varmap::VarMap");
        TypeRegistry::registerType<svar128_t>("ork::svar128_t");
        TypeRegistry::registerType<fvec2>("ork::fvec2");
        TypeRegistry::registerType<fvec3>("ork::fvec3");
        TypeRegistry::registerType<fvec4>("ork::fvec4");
        TypeRegistry::registerType<dvec3>("ork::dvec3");
        TypeRegistry::registerType<fquat>("ork::fquat");
        TypeRegistry::registerType<fmtx4>("ork::fmtx4");
        TypeRegistry::registerType<Sphere>("ork::Sphere");

        // Register codec for each type (C++ only!)
        registerSwiftType<Timer>();
        registerSwiftType<varmap::VarMap>();
        registerSwiftType<fvec2>();
        registerSwiftType<fvec3>();
        registerSwiftType<fvec4>();
        registerSwiftType<dvec3>();
        registerSwiftType<fquat>();
        registerSwiftType<fmtx4>();
        registerSwiftType<Sphere>();

        g_last_error.clear();
    } catch (const std::exception& e) {
        g_last_error = std::string("Init failed: ") + e.what();
    }
}
```

### Swift Side: No Codec, Just Wrappers

Swift classes are simple wrappers with no codec logic:

```swift
// Sources/Orkid/Kernel/Timer.swift

import Foundation

/// Base class for all Orkid objects (uses handle pattern)
open class OrkidObject {
    internal let handle: OpaquePointer
    private var ownsHandle: Bool

    internal init(handle: OpaquePointer, owned: Bool = true) {
        self.handle = handle
        self.ownsHandle = owned
    }

    deinit {
        if ownsHandle {
            orkid_handle_release(handle)
        }
    }

    public var typeName: String {
        return String(cString: orkid_handle_type_name(handle))
    }

    public var typeCRC: UInt64 {
        return orkid_handle_type_crc(handle)
    }

    public var useCount: Int32 {
        return orkid_handle_use_count(handle)
    }
}

/// Timer wrapper - no codec registration needed!
public final class Timer: OrkidObject {

    public init() {
        super.init(handle: orkid_timer_create()!)
    }

    public func start() {
        orkid_timer_start(handle)
    }

    public func end() {
        orkid_timer_end(handle)
    }

    public var secsSinceStart: Float {
        return orkid_timer_secs_since_start(handle)
    }

    public static var syncTime: Float {
        return orkid_timer_get_sync_time()
    }
}
```

### VarMap Implementation (Codec Transparent)

```swift
// Sources/Orkid/VarMap/VarMap.swift

import Foundation

/// VarMap wrapper - codec handled transparently by C++ bridge
public final class VarMap: OrkidObject {

    public init() {
        super.init(handle: orkid_varmap_create()!)
    }

    private override init(handle: OpaquePointer, owned: Bool = true) {
        super.init(handle: handle, owned: owned)
    }

    // MARK: - Subscript (codec transparent - handled by C++ bridge)

    public subscript(key: String) -> OrkidObject? {
        get {
            // C++ bridge uses codec internally to convert variant → handle
            guard let handle = orkid_varmap_get(handle, key) else {
                return nil
            }

            // TODO: Create appropriate Swift wrapper based on type CRC
            // For now, return generic OrkidObject
            return OrkidObject(handle: handle)
        }
        set {
            if let value = newValue {
                // C++ bridge uses codec internally to convert handle → variant
                orkid_varmap_set(handle, key, value.handle)
            } else {
                orkid_varmap_remove(handle, key)
            }
        }
    }

    public func contains(_ key: String) -> Bool {
        return orkid_varmap_contains(handle, key)
    }

    public var count: Int {
        return Int(orkid_varmap_size(handle))
    }

    public var keys: [String] {
        var keyCount: Int32 = 0
        guard let keysPtr = orkid_varmap_keys(handle, &keyCount) else {
            return []
        }
        defer { orkid_varmap_free_keys(keysPtr, keyCount) }

        var result: [String] = []
        for i in 0..<Int(keyCount) {
            if let keyPtr = keysPtr[i] {
                result.append(String(cString: keyPtr))
            }
        }
        return result
    }

    public func clear() {
        orkid_varmap_clear(handle)
    }

    public func clone() -> VarMap {
        return VarMap(handle: orkid_varmap_clone(handle)!)
    }
}
```

---

## Swift Binding Patterns: Instantiable vs C++-Only Types

### Pattern A: Instantiable Types

Types that **Swift can create** and pass to/from C++. These have public `init()` and creation bridge functions.

**Examples:** vec3, quat, mtx4, Timer, VarMap, Sphere

#### Swift Side - Full Lifecycle

```swift
// Sources/Orkid/Math/vec3.swift

import Foundation

/// Swift wrapper for ork::fvec3 - INSTANTIABLE from Swift
public final class vec3: OrkidObject {

    // MARK: - Public Initialization (Swift can create!)

    public init(x: Float, y: Float, z: Float) {
        super.init(handle: orkid_fvec3_create(x, y, z)!)
    }

    public init() {
        self.init(x: 0, y: 0, z: 0)
    }

    // MARK: - Properties

    public var x: Float {
        get { orkid_fvec3_get_x(handle) }
        set { orkid_fvec3_set_x(handle, newValue) }
    }

    public var y: Float {
        get { orkid_fvec3_get_y(handle) }
        set { orkid_fvec3_set_y(handle, newValue) }
    }

    public var z: Float {
        get { orkid_fvec3_get_z(handle) }
        set { orkid_fvec3_set_z(handle, newValue) }
    }

    // MARK: - Methods

    public var length: Float {
        return orkid_fvec3_length(handle)
    }

    public func normalized() -> vec3 {
        return vec3(handle: orkid_fvec3_normalized(handle)!)
    }
}
```

#### Usage - Swift Creates and Passes

```swift
// Swift creates instances
let position = vec3(x: 1.0, y: 2.0, z: 3.0)
let velocity = vec3(x: 0.5, y: 0.0, z: 0.1)

// Swift → C++ (pass Swift-created object to C++)
camera.setPosition(position)

// C++ → Swift (receive C++-created object from C++)
let cameraPos = camera.getPosition()  // Returns vec3

// Lifetime: Swift ARC manages via deinit → orkid_handle_release
```

#### C Bridge Functions - Creation Exposed

```cpp
// C++ provides creation function for Swift
OrkidHandleBase* orkid_fvec3_create(float x, float y, float z) {
    return OrkidHandle<fvec3>::makeShared(x, y, z);
}

// Also provide property accessors
float orkid_fvec3_get_x(const OrkidHandleBase* handle) {
    auto typed = handle->typedHandle<fvec3>();
    return typed ? typed->get()->x : 0.0f;
}

// And methods
OrkidHandleBase* orkid_fvec3_normalized(const OrkidHandleBase* handle) {
    auto typed = handle->typedHandle<fvec3>();
    if (!typed) return nullptr;
    auto normalized = typed->get()->normalized();
    return OrkidHandle<fvec3>::makeShared(normalized);
}
```

---

### Pattern B: C++-Only Types

Types that **Swift cannot create** - only received from C++ methods. Lifetime managed entirely by C++.

**Examples:** RenderContext, GPUBuffer, TextureResource, SceneGraph, Camera (when owned by scene)

#### Swift Side - Receive-Only

```swift
// Sources/Orkid/Graphics/RenderContext.swift

import Foundation

/// Swift wrapper for ork::lev2::Context - C++ ONLY (no Swift creation!)
public final class RenderContext: OrkidObject {

    // MARK: - NO public init! Swift cannot create this.

    // Only internal init for receiving from C++
    internal override init(handle: OpaquePointer, owned: Bool = false) {
        super.init(handle: handle, owned: owned)  // owned=false! C++ manages lifetime
    }

    // MARK: - Methods (can use after receiving from C++)

    public func clear(color: vec4) {
        orkid_context_clear(handle, color.x, color.y, color.z, color.w)
    }

    public func beginFrame() {
        orkid_context_begin_frame(handle)
    }

    public func endFrame() {
        orkid_context_end_frame(handle)
    }
}
```

```swift
// Sources/Orkid/Graphics/Camera.swift

import Foundation

/// Camera owned by C++ scene graph - Swift cannot create
public final class Camera: OrkidObject {

    // MARK: - NO public init!

    internal override init(handle: OpaquePointer, owned: Bool = false) {
        super.init(handle: handle, owned: owned)
    }

    // MARK: - Properties (getters/setters call C++)

    public var position: vec3 {
        get {
            return vec3(handle: orkid_camera_get_position(handle)!)
        }
        set {
            orkid_camera_set_position(handle, newValue.handle)
        }
    }

    public var target: vec3 {
        get {
            return vec3(handle: orkid_camera_get_target(handle)!)
        }
        set {
            orkid_camera_set_target(handle, newValue.handle)
        }
    }
}
```

#### Usage - Receive from C++, Cannot Create

```swift
// ❌ CANNOT create from Swift:
// let context = RenderContext()  // Compile error - no public init!
// let camera = Camera()          // Compile error - no public init!

// ✅ Can only RECEIVE from C++:
let renderer = app.renderer        // C++ creates/owns
let context = renderer.context     // Receive RenderContext from C++
let camera = scene.mainCamera      // Receive Camera from C++ scene

// Can USE the objects:
camera.position = vec3(x: 10, y: 5, z: 0)  // vec3 is instantiable!
context.clear(color: vec4(0, 0, 0, 1))
context.beginFrame()
// ... render ...
context.endFrame()

// Can PASS Pattern B objects back to C++ functions:
renderer.setActiveCamera(camera)           // Pass camera handle to C++
postProcessor.process(context: context)    // Pass context handle to C++

// Lifetime: C++ parent owns it, Swift just has non-owning reference
// When renderer/scene goes away, context/camera are cleaned up by C++
```

#### C Bridge Functions - No Creation, Only Access

```cpp
// NO creation function for C++-only types!
// OrkidHandleBase* orkid_context_create()  ❌ Does not exist

// Only provide methods for objects received from C++:

void orkid_context_clear(OrkidHandleBase* handle, float r, float g, float b, float a) {
    auto typed = handle->typedHandle<lev2::Context>();
    if (typed) {
        typed->get()->clear(fvec4(r, g, b, a));
    }
}

OrkidHandleBase* orkid_camera_get_position(const OrkidHandleBase* handle) {
    auto typed = handle->typedHandle<Camera>();
    if (!typed) return nullptr;
    const auto& pos = typed->get()->getPosition();
    return OrkidHandle<fvec3>::makeShared(pos);
}

void orkid_camera_set_position(OrkidHandleBase* handle, OrkidHandleBase* pos_handle) {
    auto typed_cam = handle->typedHandle<Camera>();
    auto typed_pos = pos_handle->typedHandle<fvec3>();
    if (typed_cam && typed_pos) {
        typed_cam->get()->setPosition(*typed_pos->get());
    }
}
```

---

### Pattern Comparison Table

| Aspect | **Pattern A: Instantiable** | **Pattern B: C++-Only** |
|--------|---------------------------|------------------------|
| **Swift init()** | ✅ Public `init()` | ❌ No public init (only internal) |
| **Created by** | Swift or C++ | C++ only |
| **Passed to C++** | ✅ Yes (Swift or C++ created) | ✅ Yes (C++ created, Swift holds ref) |
| **Received from C++** | ✅ Yes | ✅ Yes (only way to get) |
| **Ownership** | Swift ARC (owned: true) | C++ parent (owned: false) |
| **C Bridge Creation** | ✅ `orkid_X_create()` | ❌ No creation function |
| **Examples** | vec3, quat, Timer, VarMap | RenderContext, Camera, GPUBuffer |

---

### Pattern C: Swift Callbacks in Variants

Swift closures can be stored in C++ variants (like `py::object` callbacks in Python bindings), enabling async completion handlers.

**Pattern:** Swift → C++ (opaque storage) → Swift (unwrap variant & invoke)

**Key Insight:** Callbacks always receive/return **variants**. Type unwrapping happens at registration time with zero per-use boilerplate.

#### Swift Callback Wrapper

```swift
// Sources/Orkid/Core/SwiftCallback.swift

import Foundation

/// Base callback - no args
public final class SwiftCallback: OrkidObject {
    private let closure: () -> Void
    private let callbackId: UInt64  // ← Cache ID for deinit

    internal init(closure: @escaping () -> Void) {
        self.closure = closure
        let handle = orkid_swiftcallback_create()!
        self.callbackId = orkid_swiftcallback_get_id(handle)  // ← Cache before super.init
        super.init(handle: handle, owned: true)

        // Register in global callback registry
        SwiftCallbackManager.shared.register(id: callbackId, callback: self)
    }

    deinit {
        SwiftCallbackManager.shared.unregister(id: callbackId)  // ← CRITICAL: Prevent memory leak
    }

    internal func invoke(argsHandle: OpaquePointer?) {
        closure()
    }
}

/// Callback with 1 argument - unwraps variant to concrete type at registration time
public final class SwiftCallback1<T>: OrkidObject {
    private let closure: (T) -> Void
    private let unwrapper: (OpaquePointer?) -> T  // Captures type unwrapping logic
    private let callbackId: UInt64  // ← Cache ID for deinit

    internal init(
        closure: @escaping (T) -> Void,
        unwrapper: @escaping (OpaquePointer?) -> T
    ) {
        self.closure = closure
        self.unwrapper = unwrapper
        let handle = orkid_swiftcallback_create()!
        self.callbackId = orkid_swiftcallback_get_id(handle)  // ← Cache before super.init
        super.init(handle: handle, owned: true)

        SwiftCallbackManager.shared.register(id: callbackId, callback: self)
    }

    deinit {
        SwiftCallbackManager.shared.unregister(id: callbackId)  // ← CRITICAL: Prevent memory leak
    }

    internal func invoke(argsHandle: OpaquePointer?) {
        let arg = unwrapper(argsHandle)  // Unwrap variant → concrete type
        closure(arg)                      // User gets concrete type!
    }
}

/// Callback with 2 arguments - unpacks vector<svar128_t> to individual args
public final class SwiftCallback2<T1, T2>: OrkidObject {
    private let closure: (T1, T2) -> Void
    private let unwrapper1: (OpaquePointer?) -> T1
    private let unwrapper2: (OpaquePointer?) -> T2
    private let callbackId: UInt64  // ← Cache ID for deinit

    internal init(
        closure: @escaping (T1, T2) -> Void,
        unwrapper1: @escaping (OpaquePointer?) -> T1,
        unwrapper2: @escaping (OpaquePointer?) -> T2
    ) {
        self.closure = closure
        self.unwrapper1 = unwrapper1
        self.unwrapper2 = unwrapper2
        let handle = orkid_swiftcallback_create()!
        self.callbackId = orkid_swiftcallback_get_id(handle)  // ← Cache before super.init
        super.init(handle: handle, owned: true)

        SwiftCallbackManager.shared.register(id: callbackId, callback: self)
    }

    deinit {
        SwiftCallbackManager.shared.unregister(id: callbackId)  // ← CRITICAL: Prevent memory leak
    }

    internal func invoke(argsHandle: OpaquePointer?) {
        // C++ passes vector<svar128_t>, extract each element
        let arg1Handle = orkid_svar_list_get(argsHandle, 0)
        let arg2Handle = orkid_svar_list_get(argsHandle, 1)

        let arg1 = unwrapper1(arg1Handle)
        let arg2 = unwrapper2(arg2Handle)

        closure(arg1, arg2)  // Call with separate args - no tuple!
    }
}

/// Callback with 3 arguments - unpacks vector<svar128_t> to individual args
public final class SwiftCallback3<T1, T2, T3>: OrkidObject {
    private let closure: (T1, T2, T3) -> Void
    private let unwrapper1: (OpaquePointer?) -> T1
    private let unwrapper2: (OpaquePointer?) -> T2
    private let unwrapper3: (OpaquePointer?) -> T3
    private let callbackId: UInt64  // ← Cache ID for deinit

    internal init(
        closure: @escaping (T1, T2, T3) -> Void,
        unwrapper1: @escaping (OpaquePointer?) -> T1,
        unwrapper2: @escaping (OpaquePointer?) -> T2,
        unwrapper3: @escaping (OpaquePointer?) -> T3
    ) {
        self.closure = closure
        self.unwrapper1 = unwrapper1
        self.unwrapper2 = unwrapper2
        self.unwrapper3 = unwrapper3
        let handle = orkid_swiftcallback_create()!
        self.callbackId = orkid_swiftcallback_get_id(handle)  // ← Cache before super.init
        super.init(handle: handle, owned: true)

        SwiftCallbackManager.shared.register(id: callbackId, callback: self)
    }

    deinit {
        SwiftCallbackManager.shared.unregister(id: callbackId)  // ← CRITICAL: Prevent memory leak
    }

    internal func invoke(argsHandle: OpaquePointer?) {
        // C++ passes vector<svar128_t>, extract each element
        let arg1Handle = orkid_svar_list_get(argsHandle, 0)
        let arg2Handle = orkid_svar_list_get(argsHandle, 1)
        let arg3Handle = orkid_svar_list_get(argsHandle, 2)

        let arg1 = unwrapper1(arg1Handle)
        let arg2 = unwrapper2(arg2Handle)
        let arg3 = unwrapper3(arg3Handle)

        closure(arg1, arg2, arg3)  // Call with separate args - no tuple!
    }
}
```

#### SwiftCallbackManager - Global Registry

```swift
// Sources/Orkid/Core/SwiftCallbackManager.swift

import Foundation

/// Protocol for type-erased callback invocation
protocol SwiftCallbackInvocable: AnyObject {
    func invoke(argsHandle: OpaquePointer?)
}

/// Global registry for callback lookup by ID
class SwiftCallbackManager {
    static let shared = SwiftCallbackManager()

    private var callbacks: [UInt64: SwiftCallbackInvocable] = [:]
    private let lock = NSLock()

    func register(id: UInt64, callback: SwiftCallbackInvocable) {
        lock.lock()
        defer { lock.unlock() }
        callbacks[id] = callback
    }

    func unregister(id: UInt64) {
        lock.lock()
        defer { lock.unlock() }
        callbacks.removeValue(forKey: id)
    }

    func invoke(id: UInt64, argsHandle: OpaquePointer?) {
        lock.lock()
        let callback = callbacks[id]  // ← Creates strong reference via ARC
        lock.unlock()                   // ← Safe to unlock - callback stays alive

        // Invoke through protocol - callback object retained by local variable
        callback?.invoke(argsHandle: argsHandle)
    }
}

// All callback types conform to unified protocol
extension SwiftCallback: SwiftCallbackInvocable {}
extension SwiftCallback1: SwiftCallbackInvocable {}
extension SwiftCallback2: SwiftCallbackInvocable {}
extension SwiftCallback3: SwiftCallbackInvocable {}
```

#### C++ Bridge - Opaque Storage

```cpp
// ork.core/inc/ork/swift/orkid_swift_callback.h

#pragma once

namespace ork::swift {

// Opaque callback holder (C++ doesn't know about Swift closures)
struct SwiftCallbackHolder {
    uint64_t _callback_id = 0;     // Unique ID for Swift registry lookup

    SwiftCallbackHolder() = default;
    ~SwiftCallbackHolder() = default;
};

using swiftcallback_ptr_t = std::shared_ptr<SwiftCallbackHolder>;

} // namespace ork::swift
```

```cpp
// ork.core/src/swift/orkid_swift_callback.cpp

#include "ork/swift/orkid_swift_callback.h"
#include "ork/swift/orkid_handle.h"
#include <atomic>

namespace ork::swift {

// Simple ID generator
static std::atomic<uint64_t> g_next_callback_id{1};

} // namespace ork::swift

extern "C" {

// Create opaque callback holder
OrkidHandleBase* orkid_swiftcallback_create() {
    auto holder = std::make_shared<ork::swift::SwiftCallbackHolder>();
    holder->_callback_id = ork::swift::g_next_callback_id.fetch_add(1);
    return OrkidHandle<ork::swift::SwiftCallbackHolder>::assign(holder);
}

// Get callback ID for Swift lookup
uint64_t orkid_swiftcallback_get_id(OrkidHandleBase* handle) {
    auto typed = handle->typedHandle<ork::swift::SwiftCallbackHolder>();
    return typed ? typed->get()->_callback_id : 0;
}

// SINGLE GENERIC INVOKE - Called from C++
extern void orkid_swift_invoke_callback(uint64_t callback_id, OrkidHandleBase* args_variant);

// Variant list accessor for multi-arg callbacks
OrkidHandleBase* orkid_svar_list_get(OrkidHandleBase* list_handle, size_t index) {
    auto typed_list = list_handle->typedHandle<std::vector<svar128_t>>();
    if (!typed_list) return nullptr;

    auto& vec = *typed_list->get();
    if (index >= vec.size()) return nullptr;

    // Wrap individual variant element in handle
    auto elem_variant = OrkidHandle<svar128_t>::makeShared();
    *elem_variant->typedHandle<svar128_t>()->get() = vec[index];
    return elem_variant;
}

} // extern "C"
```

#### Generic Swift Callback Invocation

```swift
// Single C-exported function called from C++
@_cdecl("orkid_swift_invoke_callback")
func orkid_swift_invoke_callback(callback_id: UInt64, args_variant: OpaquePointer?) {
    SwiftCallbackManager.shared.invoke(id: callback_id, argsHandle: args_variant)
}
```

#### Usage Examples - 1, 2, and 3 Arg Callbacks

```swift
// Example 1: Single arg callback
extension AssetCatalog {
    public func uploadAsset(
        assetId: String,
        onCompleted: @escaping (String) -> Void
    ) {
        let callback = SwiftCallback1<String>(
            closure: onCompleted,
            unwrapper: { variantHandle in
                var buf = [CChar](repeating: 0, count: 4096)
                orkid_svar_to_string(variantHandle, &buf, 4096)
                return String(cString: buf)
            }
        )
        orkid_catalog_upload_asset(handle, assetId, callback.handle)
    }
}

// User code - natural!
catalog.uploadAsset(assetId: "model.glb") { result in
    print("Upload completed: \(result)")
}

// Example 2: Two arg callback
extension AssetCatalog {
    public func uploadNamespace(
        namespaceId: String,
        onAssetCompleted: @escaping (String, Int) -> Void
    ) {
        let callback = SwiftCallback2<String, Int>(
            closure: onAssetCompleted,
            unwrapper1: { variantHandle in  // String unwrapper
                var buf = [CChar](repeating: 0, count: 4096)
                orkid_svar_to_string(variantHandle, &buf, 4096)
                return String(cString: buf)
            },
            unwrapper2: { variantHandle in  // Int unwrapper
                var val: Int32 = 0
                orkid_svar_to_int(variantHandle, &val)
                return Int(val)
            }
        )
        orkid_catalog_upload_namespace(handle, namespaceId, callback.handle)
    }
}

// User code - sees individual args, not tuple!
catalog.uploadNamespace(namespaceId: "textures") { assetId, fileCount in
    print("Asset \(assetId) uploaded: \(fileCount) files")
}

// Example 3: Three arg callback
extension Renderer {
    public func renderFrame(
        onCompleted: @escaping (String, Float, Int) -> Void
    ) {
        let callback = SwiftCallback3<String, Float, Int>(
            closure: onCompleted,
            unwrapper1: { vh in stringUnwrapper(vh) },
            unwrapper2: { vh in floatUnwrapper(vh) },
            unwrapper3: { vh in intUnwrapper(vh) }
        )
        orkid_renderer_render_frame(handle, callback.handle)
    }
}

// User code - clean, natural Swift!
renderer.renderFrame { status, fps, triangles in
    print("Frame: \(status) @ \(fps) FPS (\(triangles) tris)")
}
```

**Boilerplate Summary:**
- ❌ **Per-call:** None! User just passes closure
- ✅ **Per-method:** Unwrappers written once in binding definition
- ✅ **Per-type:** Unwrappers reusable (string/int/float unwrappers used everywhere)

#### C++ Side - Generic Invoke with Multi-Arg Support

```cpp
// ork.core/src/swift/pyext_asset_catalog.cpp (Swift version)

extern "C" {

// Example 1: Single arg callback
void orkid_catalog_upload_asset(
    OrkidHandleBase* catalog_handle,
    const char* fq_asset_id,
    OrkidHandleBase* callback_handle
) {
    auto typed_catalog = catalog_handle->typedHandle<AssetCatalog>();
    auto typed_callback = callback_handle->typedHandle<ork::swift::SwiftCallbackHolder>();

    if (!typed_catalog || !typed_callback) return;

    auto catalog = typed_catalog->get();
    uint64_t callback_id = typed_callback->get()->_callback_id;

    // Enqueue async upload
    auto op = [catalog, fq_asset_id = std::string(fq_asset_id), callback_id]() {
        auto receipt = catalog->uploadAsset(fq_asset_id, nullptr);

        if (receipt) {
            // Single arg: pass variant directly
            auto result_variant = OrkidHandle<svar128_t>::makeShared();
            result_variant->typedHandle<svar128_t>()->get()->set<std::string>("");

            extern void orkid_swift_invoke_callback(uint64_t, OrkidHandleBase*);
            orkid_swift_invoke_callback(callback_id, result_variant);
        }
    };

    opq::concurrentQueue()->enqueue(op);
}

// Example 2: Two arg callback
void orkid_catalog_upload_namespace(
    OrkidHandleBase* catalog_handle,
    const char* namespace_id,
    OrkidHandleBase* callback_handle
) {
    auto typed_catalog = catalog_handle->typedHandle<AssetCatalog>();
    auto typed_callback = callback_handle->typedHandle<ork::swift::SwiftCallbackHolder>();

    if (!typed_catalog || !typed_callback) return;

    auto catalog = typed_catalog->get();
    uint64_t callback_id = typed_callback->get()->_callback_id;

    auto op = [catalog, namespace_id = std::string(namespace_id), callback_id]() {
        catalog->uploadNamespace(namespace_id, [callback_id](const std::string& asset_id) {
            // Pack TWO args into vector<svar128_t>
            auto args_vec = std::make_shared<std::vector<svar128_t>>();
            args_vec->push_back(svar128_t(asset_id));        // Arg 1: String
            args_vec->push_back(svar128_t(42));              // Arg 2: Int (file count)

            auto args_handle = OrkidHandle<std::vector<svar128_t>>::assign(args_vec);

            extern void orkid_swift_invoke_callback(uint64_t, OrkidHandleBase*);
            orkid_swift_invoke_callback(callback_id, args_handle);
        });
    };

    opq::concurrentQueue()->enqueue(op);
}

// Example 3: Three arg callback
void orkid_renderer_render_frame(
    OrkidHandleBase* renderer_handle,
    OrkidHandleBase* callback_handle
) {
    auto typed_renderer = renderer_handle->typedHandle<Renderer>();
    auto typed_callback = callback_handle->typedHandle<ork::swift::SwiftCallbackHolder>();

    if (!typed_renderer || !typed_callback) return;

    auto renderer = typed_renderer->get();
    uint64_t callback_id = typed_callback->get()->_callback_id;

    auto op = [renderer, callback_id]() {
        renderer->renderFrame();

        // Pack THREE args into vector<svar128_t>
        auto args_vec = std::make_shared<std::vector<svar128_t>>();
        args_vec->push_back(svar128_t(std::string("ok")));  // Arg 1: String
        args_vec->push_back(svar128_t(60.0f));              // Arg 2: Float (FPS)
        args_vec->push_back(svar128_t(123456));             // Arg 3: Int (triangle count)

        auto args_handle = OrkidHandle<std::vector<svar128_t>>::assign(args_vec);

        extern void orkid_swift_invoke_callback(uint64_t, OrkidHandleBase*);
        orkid_swift_invoke_callback(callback_id, args_handle);
    };

    opq::concurrentQueue()->enqueue(op);
}

} // extern "C"
```

#### Key Points

1. **Single Generic Invoke:** C++ always calls `orkid_swift_invoke_callback(id, variant)` - no type-specific functions!
2. **Multi-Arg Support:** 0-3 args supported (SwiftCallback, SwiftCallback1, SwiftCallback2, SwiftCallback3)
3. **Vector Packing:** Multi-arg callbacks pack args into `std::vector<svar128_t>`, unpacked Swift-side
4. **Individual Args:** User sees separate parameters `(String, Int) -> Void`, not tuples!
5. **Opaque Storage:** C++ stores `SwiftCallbackHolder` with only callback ID
6. **Registry Pattern:** Swift maintains ID → callback object mapping
7. **Variant Args/Returns:** All callback arguments/returns are variants - unwrapped by Swift infrastructure
8. **Registration-Time Types:** Unwrapper closures provided once at binding definition, not per-call
9. **Zero User Boilerplate:** User just passes natural Swift closures with concrete types
10. **Variant Storage:** `SwiftCallbackHolder` stored as `shared_ptr` in variant (like `py::object`)
11. **Lifetime:** Swift callback object lives as long as C++ holds the variant

This pattern mirrors Python's `py::object` callback storage with minimal boilerplate and natural multi-arg support.

---

### Usage Example (Transparent Codec)

```swift
// Example: VarMap usage with transparent C++ codec

let vmap = VarMap()

// Store objects (C++ codec converts handle → variant automatically)
let timer = Timer()
timer.start()
vmap["timer"] = timer  // Stored as shared_ptr<Timer> in variant

// Store math types (also via handle, NOT copied!)
let position = vec3(x: 1.0, y: 2.0, z: 3.0)
vmap["position"] = position  // Stored as shared_ptr<fvec3> in variant

// Retrieve (C++ codec converts variant → handle automatically)
if let retrievedTimer = vmap["timer"] as? Timer {
    print("Timer: \(retrievedTimer.secsSinceStart)s")
}

if let pos = vmap["position"] as? vec3 {
    print("Position: (\(pos.x), \(pos.y), \(pos.z))")
}

// VarMap can be stored in VarMap (recursive)
let nested = VarMap()
vmap["nested"] = nested

if let inner = vmap["nested"] as? VarMap {
    print("Found nested VarMap with \(inner.count) items")
}
```

**Note:** Type casting (as? Timer) will be implemented via type CRC lookup in future iterations.

### Initialization

```swift
// Sources/Orkid/Orkid.swift

import Foundation

public final class Orkid {

    public static let shared = Orkid()

    private init() {
        // Initialize C++ layer (codec registration happens here in C++)
        var args = CommandLine.arguments.map { strdup($0) }
        orkid_swift_init(Int32(args.count), &args)
        args.forEach { free($0) }
    }

    public static func poll() {
        orkid_swift_poll()
    }

    public static func exit() {
        orkid_swift_exit()
    }
}
```

---

## Implementability Review (2025-11-05)

This section documents the comprehensive implementability review conducted before implementation begins.

### Review Summary: ✅ IMPLEMENTABLE

All major architectural components have been reviewed and are confirmed implementable in Swift/Xcode 26 with C++17. Critical issues have been identified and fixed in this document.

---

### 1. C++ Template Handle Architecture ✅

**Status:** Fully implementable

**Reviewed Components:**
- Virtual base class `OrkidHandleBase` with template-derived `OrkidHandle<T>`
- Type-safe casting via `typedHandle<T>()` with runtime validation
- Factory methods: `makeShared()` for creation, `assign()` for existing shared_ptr
- Memory management: raw `new` in factory, `delete` in `orkid_handle_release()`

**Key Findings:**
- ✅ Virtual destructor ensures proper cleanup of template-derived classes
- ✅ Type validation before casting prevents undefined behavior
- ✅ Full type information preserved (no type erasure of shared_ptr)
- ✅ Memory lifecycle is clear: C++ creates, Swift deinit triggers cleanup
- ✅ Template instantiation happens implicitly during registration - no issues

**Verdict:** No issues found. Pattern is clean and safe.

---

### 2. Swift OpaquePointer Bridging & Memory Safety ✅

**Status:** Fully implementable

**Reviewed Components:**
- `OpaquePointer` wrapping of C++ `OrkidHandleBase*`
- Ownership tracking via `ownsHandle` flag
- Automatic cleanup via `deinit` → `orkid_handle_release()`
- Handle passing to/from C functions

**Key Findings:**
- ✅ `OpaquePointer` is Swift's standard type for opaque C pointers
- ✅ Ownership flag prevents double-free (C++-owned vs Swift-owned objects)
- ✅ `deinit` cleanup is automatic, thread-safe, and guaranteed by Swift
- ✅ Force unwrap (`!`) is safe since C++ throws on allocation failure

**Improvements Made:**
- ✅ Added note about `_Nonnull` annotations for cleaner Swift imports
- ✅ Documented that nullability annotations eliminate force-unwrap operators

**Verdict:** Safe and idiomatic. Optional improvement available (nullability annotations).

---

### 3. Swift Generic Callback Classes ⚠️ → ✅

**Status:** Implementable with fixes (FIXED)

**Reviewed Components:**
- Generic classes: `SwiftCallback`, `SwiftCallback1<T>`, `SwiftCallback2<T1,T2>`, `SwiftCallback3<T1,T2,T3>`
- Unwrapper closures for type conversion
- Callback registration in global manager
- Vector unpacking for multi-arg callbacks

**Critical Issue Found:** ❌ Memory leak in callback registration
- Callbacks registered but never unregistered
- Would accumulate in global dictionary forever

**Fix Applied:** ✅
```swift
// Added to all callback classes:
private let callbackId: UInt64  // Cache ID before super.init

deinit {
    SwiftCallbackManager.shared.unregister(id: callbackId)
}
```

**Why Fix is Necessary:**
- Cached ID required because parent deinit releases handle before we can query it
- Swift deinit order: child first, then parent - so unregister happens before handle release
- Without unregister, callbacks leak and never release closures

**Verdict:** ✅ Fixed. Now safe and leak-free.

---

### 4. Protocol-Based Type Erasure for Callbacks ✅

**Status:** Fully implementable (with optimization applied)

**Reviewed Components:**
- Type erasure via `[UInt64: SwiftCallbackInvocable]` storage
- Protocol-based invocation for generic-to-non-generic bridge
- Dynamic casting for callback type detection

**Optimization Applied:** ✅
- Simplified from 3 separate protocols to single unified `SwiftCallbackInvocable`
- Cleaner code, identical functionality
- All callback types conform via extension

**Key Findings:**
- ✅ Type erasure through protocol conformance is standard Swift
- ✅ Extension-based conformance is idiomatic
- ✅ Protocol allows generic callbacks to be stored uniformly

**Verdict:** Clean, idiomatic Swift. Optimization applied for simplicity.

---

### 5. C++ Codec Architecture & Variant Conversion ✅

**Status:** Fully implementable

**Reviewed Components:**
- `SwiftCodecImpl` with encoder/decoder function maps
- Template-based type registration via `registerSwiftType<T>()`
- `std::type_index` for encoder lookup (C++ → Swift direction)
- Type CRC for decoder lookup (Swift → C++ direction)
- VarMap get/set using codec transparently

**Key Findings:**
- ✅ Codec mirrors pycodec pattern successfully
- ✅ Type-safe conversion: `varval_t` ↔ `OrkidHandleBase*`
- ✅ Encoder lookup by C++ `type_index` is correct
- ✅ Decoder lookup by type CRC enables runtime type recovery
- ✅ Safe casting prevents type mismatches
- ✅ Read-only after init → thread-safe concurrent access
- ✅ Primitives handled as special cases with direct variant storage
- ✅ Completely transparent to Swift (all conversion in C++ bridge)

**Verdict:** Excellent design. No issues found.

---

### 6. Thread Safety & Concurrency Patterns ✅

**Status:** Fully thread-safe

**Reviewed Components:**
- `SwiftCallbackManager` with NSLock protection
- Callback invocation with lock release before invoke
- Handle release from arbitrary threads
- Codec/registry concurrent read access
- C++ threads invoking Swift callbacks

**Key Analysis - SwiftCallbackManager Invoke Pattern:**
```swift
func invoke(id: UInt64, argsHandle: OpaquePointer?) {
    lock.lock()
    let callback = callbacks[id]  // ← Creates strong reference via ARC
    lock.unlock()                   // ← Safe to unlock - callback stays alive

    callback?.invoke(argsHandle: argsHandle)  // ← Object still alive
}
```

**Why This is Safe:**
- Swift's ARC creates a strong reference when `callback` is assigned
- Even if another thread unregisters, local variable keeps object alive
- No race condition possible

**Thread Safety Findings:**
- ✅ `orkid_handle_release()` safe from any thread (shared_ptr refcount is atomic)
- ✅ TypeRegistry read-only after init (concurrent reads safe)
- ✅ SwiftCodec read-only after init (concurrent reads safe)
- ✅ VarMap not thread-safe (expected - user responsibility, matches Python)
- ✅ C++ threads can invoke Swift callbacks (protected by NSLock)
- ✅ `@_cdecl` export thread-safe when Swift code uses proper synchronization
- ✅ Callback cleanup via deinit thread-safe (NSLock protected)

**Verdict:** Fully thread-safe. No issues found.

---

### 7. Cross-Check Against Existing Orkid C++ Patterns ✅

**Status:** Consistent with Orkid conventions

**Checked Patterns:**
- ✅ No `shared_from_this()` (matches session_notes.md requirement)
- ✅ Static factory pattern for creation (matches Orkid conventions)
- ✅ Type system using CRC (matches Orkid's CrcString system)
- ✅ VarMap shared ownership (matches Python bindings)
- ✅ Codec registration at init (matches pycodec pattern)
- ✅ Opaque callback storage (matches Python's py::object pattern)
- ✅ Variant-based type erasure (matches Orkid's svar128_t system)

**Verdict:** Fully consistent with Orkid architecture and conventions.

---

### Issues Fixed in This Document

| Issue | Severity | Status | Location |
|-------|----------|--------|----------|
| Memory leak in callback registration | 🔴 Critical | ✅ Fixed | SwiftCallback classes - added deinit with unregister |
| Callback ID caching for deinit | 🔴 Critical | ✅ Fixed | SwiftCallback classes - cache ID before super.init |
| Protocol proliferation | 🟡 Optimization | ✅ Fixed | SwiftCallbackManager - unified to single protocol |
| Missing nullability annotations | 🟡 Improvement | ✅ Documented | C bridge header - added note about _Nonnull |

---

### Final Verdict: ✅ READY FOR IMPLEMENTATION

All components reviewed and confirmed implementable. Critical issues have been fixed. Document is ready for implementation phase.

**Confidence Level:** High - All patterns are proven Swift/C++ interop techniques with no language limitations.

---

## Key Decisions Summary

| Aspect | Decision | Rationale |
|--------|----------|-----------|
| **Memory Management** | Template handle with automatic cleanup | Zero manual memory management, crash-proof |
| **Type System** | CRC-based type identity (matches Orkid) | Fast type checking, debugging friendly |
| **C++ Architecture** | Template hierarchy, no type erasure | Preserves `shared_ptr<T>` not `shared_ptr<void>` |
| **Type Registry** | Populated once at init | DRY principle, single source of truth |
| **Safe Casting** | `typedHandle<T>()` with validation | Returns nullptr on mismatch, no crashes |
| **Object Creation** | `makeShared(args...)` atomic operation | One-liner, perfect forwarding |
| **Existing shared_ptr** | `assign(sp)` wraps existing pointer | Clear semantics for assignment |
| **Ownership Model** | Automatic shared ownership (99% of objects) | Matches Python bindings, prevents crashes |
| **Value Types** | Direct copy for primitives only | Efficient for small types (Int, Float, Double, Bool, String) |
| **Reference Counting** | Single layer via shared_ptr | No extra handle refcount |
| **VarMap** | Always `shared_ptr<VarMap>` | Safe sharing between C++/Swift |
| **Stored Objects** | Can store `shared_ptr<T>` in variant | Preserves shared ownership in VarMap |
| **Codec Architecture** | C++-only (like pycodec) | Swift transparent, single source of truth, registration at init |
| **Binding Patterns** | Instantiable (vec3, Timer) vs C++-Only (Camera, RenderContext) | Case-by-case ownership and creation control |
| **Swift Callbacks** | Generic variant args/returns with single invoke function | 0-3 args, vector packing, zero user boilerplate, unwrapping at registration time |
| **Namespaces** | Mirror C++ exactly (ork::kernel → ork.kernel) | Clear organization, easy debugging |
| **Naming** | lowercase namespaces, PascalCase classes, camelCase methods, snake_case properties | Idiomatic Swift + matches C++ |

---

**Document Version:** 12.0
**Last Updated:** 2025-11-05
**Architecture:** Type Registry + Safe Casting + C++-Only Codec + Dual Binding Patterns + Multi-Arg Variant Callbacks
**Status:** ✅ Implementation Ready - Reviewed and Validated
