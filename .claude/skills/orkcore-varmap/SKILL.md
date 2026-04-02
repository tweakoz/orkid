---
name: orkcore-varmap
description: Answer questions about orkid's VarMap dynamic key-value container, svar128_t variant type, Python attribute access, nesting patterns, and how VarMap is used across the codebase (PropertySheet data, FSM vars, widget uservars, annotations). Use when the user asks about VarMap, dynamic properties, or the variant type system.
user-invocable: false
---

# Orkid VarMap Reference

When answering questions about VarMap in orkid, consult these files.

## Key Files

| Component | Location |
|-----------|----------|
| VarMap Template | `ork.core/inc/ork/kernel/varmap.inl` |
| VarMap Impl | `ork.core/src/kernel/varmap.cpp` |
| svar128_t (variant) | `ork.core/inc/ork/kernel/svariant.h` |
| Python Bindings | `ork.core/inc/ork/python/common_bindings/pyext_varmap.inl` |

## Core Concept

`VarMap` is a `std::map<std::string, svar128_t>` — a dynamic key-value store where values are 128-byte stack-allocated variants. Used throughout orkid as a universal data container.

## Python API

```python
from orkengine.core import VarMap

vm = VarMap()

# Attribute access (most common)
vm.name = "hello"           # __setattr__ → setValueForKey
val = vm.name                # __getattr__ → valueForKey
"name" in vm                 # __contains__
len(vm)                      # __len__

# Dotted keys (for annotations)
setattr(vm, "editor.filebase", "<assetcache>")

# Dictionary-style
vm["key"]                    # __getitem__ (raises on missing)

# Utilities
vm.keys()                    # List of all keys
vm.clone()                   # Deep copy
vm.dumpToString()            # Formatted dump with types
```

## Supported Value Types

Anything that fits in `svar128_t` (128 bytes):
- **Primitives:** `bool`, `int`, `float`, `double`, `str`
- **Math:** `fvec2`, `fvec3`, `fvec4`, `fmtx3`, `fmtx4`, `fquat`
- **Pointers:** `std::shared_ptr<T>` for any type
- **Nested:** `VarMap` (for hierarchical data)
- **Other:** `CrcString`, `datablock_ptr_t`, etc.

## C++ API

```cpp
auto vm = std::make_shared<varmap::VarMap>();

// Set/get
vm->setValueForKey("name", val);        // Set svar128_t value
vm->valueForKey("name");                // Get (returns nil if missing)
vm->typedValueForKey<float>("speed");   // Type-safe get (attempt_cast)
vm->hasKey("name");                     // Check existence

// Typed helpers
vm->set<float>("speed", 1.5f);
vm->makeValueForKey<std::string>("label", "hello");  // Construct in-place
vm->makeSharedForKey<MyClass>("obj", args...);        // Construct shared_ptr

// Number coercion
vm->tryKeyAsNumber("value");            // Float from float/double/int
vm->tryKeyAsInteger("count");           // Int from int/float/double

// Utility
vm->clone();                            // Deep copy
vm->mergeVars(other_vm);                // Merge other into this
vm->hash();                             // CRC64 hash
vm->dumpkeys();                         // Vector of all keys
```

## Where VarMap Is Used

| Context | Accessor | Purpose |
|---------|----------|---------|
| **PropertySheet data** | `propsheet.data = vm` | Form editor backing store |
| **PropertySheet annotations** | `model.setAnnotations(key, vm)` | Editor hints (type, filebase) |
| **FSM instance vars** | `inst.vars.counter = 0` | Per-FSM-instance state |
| **Widget uservars** | `widget.uservars.my_data = obj` | Arbitrary data on widgets |
| **Subsystem vars** | `subsystem._vars` | Subsystem configuration |
| **DataTable** | `datatable["key"] = val` | ECS component data exchange |
| **Asset loading** | `LoadRequest(vars)` | Asset-specific parameters |
| **Dataflow graph** | `graph._vars` | Graph runtime state |
| **Scene params** | `Scene(params_varmap)` | Scene initialization |

## Nesting Pattern (Hierarchical PropertySheet)

```python
vm = VarMap()

# Flat keys (PropertySheet renders as rows)
vm.namespace = "game"
vm.source_dir = "/path/to/files"

# Nested VarMap (PropertySheet renders as group)
transform = VarMap()
transform.position_x = 0.0
transform.position_y = 0.0
transform.position_z = 0.0
vm.Transform = transform   # Group header in PropertySheet
```

## Annotation Pattern

```python
model = propsheet.model
annot = VarMap()
annot.type = tokens.Color           # CrcString for custom editor
model.setAnnotations("diffuse", annot)

# Dotted keys for built-in annotations
annot2 = VarMap()
setattr(annot2, "editor.browsetype", "folder")
setattr(annot2, "editor.filebase", "<assetcache>")
model.setAnnotations("source_dir", annot2)
```

## svar128_t Variant Type

128-byte stack buffer with runtime type tracking:
```cpp
svar128_t val;
val.set<float>(1.5f);           // Store
float f = val.get<float>();     // Retrieve (asserts type match)
auto opt = val.tryAs<float>();  // Safe retrieve (returns attempt_cast)
bool is_f = val.isA<float>();   // Type check
val.make<std::string>("hello"); // Construct in place
val.clear();                    // Destroy and reset
```

Size variants: `svar64_t` (64B), `svar128_t` (128B), `svar160_t` (160B), `svar256_t` (256B)

## How to Answer

1. For VarMap API: read `varmap.inl` for all methods
2. For Python access: check `pyext_varmap.inl` for binding details
3. For variant internals: read `svariant.h` for `static_variant<N>`
4. Remember: Python `vm.key = val` maps to `setValueForKey` via type codec
5. Dotted keys require `setattr(vm, "dotted.key", val)` in Python
