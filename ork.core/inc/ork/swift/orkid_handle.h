////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <memory>
#include <typeindex>
#include <unordered_map>
#include <ork/util/crc.h>

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
// Templated Handle (forward declaration)
// ================================================================

template<typename T>
class OrkidHandle;

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
    virtual OrkidHandleBase* share() const = 0;  // Create new handle sharing same shared_ptr

    /// Safe typed cast with validation (returns nullptr if invalid)
    /// Defined after OrkidHandle<T> is complete
    template<typename T>
    OrkidHandle<T>* typedHandle();

    /// Safe typed cast (const version)
    template<typename T>
    const OrkidHandle<T>* typedHandle() const;
};

// ================================================================
// Templated Handle Implementation
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

    OrkidHandleBase* share() const override {
        return new OrkidHandle<T>(_ptr);  // New handle, same shared_ptr
    }

private:
    OrkidHandle(std::shared_ptr<T> sp) : _ptr(sp) {}

    std::shared_ptr<T> _ptr;    // Real type preserved!
};

// ================================================================
// OrkidHandleBase Template Method Implementations
// (Must be defined after OrkidHandle<T> is complete)
// ================================================================

template<typename T>
OrkidHandle<T>* OrkidHandleBase::typedHandle() {
    if (typeIndex() != std::type_index(typeid(T))) {
        return nullptr;  // Type mismatch - safe!
    }
    return static_cast<OrkidHandle<T>*>(this);
}

template<typename T>
const OrkidHandle<T>* OrkidHandleBase::typedHandle() const {
    if (typeIndex() != std::type_index(typeid(T))) {
        return nullptr;
    }
    return static_cast<const OrkidHandle<T>*>(this);
}

} // namespace ork::swift
