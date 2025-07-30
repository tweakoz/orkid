# Orkid Engine - LLM Interaction Guide

## Document Purpose & Evolution

This living document guides LLMs in working with the Orkid game engine codebase. It captures architectural patterns, coding conventions, and interaction preferences learned through collaborative development sessions.

### Self-Improvement Protocol

When working with this codebase, the LLM should:
1. **Capture New Patterns**: When discovering recurring generic patterns not documented here, request to user to add them here
2. **Refine Existing Guidance**: If instructions prove ambiguous or incomplete, clarify them
3. **Document Gotchas**: When encountering non-obvious issues, document them for future sessions
4. **Update Examples**: Replace outdated examples with current, working code snippets
5. **Track Design Decisions**: When architectural decisions are made, record the reasoning

> **Meta-Rule**: This document should grow more valuable with each session. Try when applicable to leave it better than you found it.

## Critical Design Patterns - Prime Directives

### 1. NO shared_from_this Pattern (NEVER VIOLATE)

This is THE fundamental pattern across the entire Orkid codebase. When forming hierarchies with `shared_ptr`:

```cpp
// CORRECT - Static factory pattern
class Parent;
class Child;

using parent_ptr_t = std::shared_ptr<Parent>;
using child_ptr_t = std::shared_ptr<Child>;

class Parent {
public:
    // Static factory receives parent as shared_ptr
    static child_ptr_t createChild(parent_ptr_t self, /* other args */) {
        auto child = std::make_shared<Child>();
        child->_parent = self;  // Child stores weak_ptr
        return child;
    }
};

class Child {
    std::weak_ptr<Parent> _parent;  // ALWAYS weak_ptr to prevent cycles
};
```

**Why This Matters:**
- Prevents circular reference memory leaks
- Makes ownership explicit and debuggable
- Follows RAII principles correctly
- Enables clear parent-child relationships

**Apply This Pattern To:**
- Any parent-child relationship
- Factory methods
- Builder patterns
- Component systems
- Scene graphs

### 2. Type System Conventions

All types follow this pattern in `types.h`:

```cpp
// Forward declaration
struct MyType;

// Pointer type aliases
using mytype_ptr_t = std::shared_ptr<MyType>;
using mytype_constptr_t = std::shared_ptr<const MyType>;
using mytype_wkptr_t = std::weak_ptr<MyType>;  // Only if needed

// Collection type aliases  
using mytype_vect_t = std::vector<mytype_ptr_t>;
using mytype_set_t = std::set<mytype_ptr_t>;
```

**Rules:**
- ALWAYS use type aliases, never raw `std::shared_ptr<X>`
- Forward declare in appropriate `types.h`
- Keep pointer suffix consistent: `_ptr_t`, `_constptr_t`, `_wkptr_t`
- Collection types use `_vect_t`, `_set_t`, `_map_t` suffixes

### 3. Builder & Factory Patterns

Parents create children, children never create themselves:

```cpp
// CORRECT
auto child = Parent::createChild(parent_ptr, id, version);
auto grandchild = Child::createGrandchild(child_ptr, id, priority);

// WRONG  
auto child = std::make_shared<Child>(id, version);
auto grandchild = child->createGrandchild(id, priority);  // Non-static factory
```

### 4. Flyweight Pattern Usage

For objects that should have single instances (namespaces, requests, resource handles, etc.):

```cpp
// Merge pattern - get existing or create new
resource_ptr_t mergeResource(const std::string& id) {
    auto it = _resources.find(id);
    if (it != _resources.end()) {
        return it->second;  // Return existing
    }
    // Create new with proper hierarchy
    auto resource = std::make_shared<Resource>(id);
    _resources[id] = resource;
    return resource;
}
```

## Code Organization

### Directory Structure

| Component | Location | Purpose |
|-----------|----------|---------|
| Source files | `ork.core/src` | Implementation files |
| Headers | `ork.core/inc/ork/` | Public headers |
| Python bindings | `ork.core/pyext/` | pybind11 bindings |
| Python tests | `ork.core/pyext/tests/` | Python unit tests |
| C++ tests | `ork.core/tests/` | UnitTest++ tests |
| Documentation | `ork.dox/` | System documentation |
| Data files | `ork.data/` | Runtime data, configs |

### Module Organization

Each subsystem follows this pattern:
```
ork.core/inc/ork/subsystem/
├── types.h          # Forward declarations & type aliases
├── main_class.h     # Primary interface
├── support_class.h  # Supporting classes
└── implementation.h # Implementation details (if needed)

ork.core/src/subsystem/
├── main_class.cpp
├── support_class.cpp  
└── subsystem_specific.cpp
```

## Development Workflow

### Building

```bash
# Standard build (Release mode, parallel)
ork.build.py

# The build system handles:
# - CMake configuration
# - Dependency management  
# - Installation to <stage>
# - Python module deployment
```

**Build System Notes:**
- Custom system provided by obt package
- Don't timeout builds - they can be slow if core headers changed (>300s)
- Build artifacts go to `<stage>/builds/orkid/.build`
- Installs to `<stage>` , aka ${OBT_STAGE}

### Testing

#### C++ Tests
```bash
# Run tests matching prefix
ork.test.core.exe TestPrefix!

# Examples
ork.test.core.exe Texture!    # All texture tests
ork.test.core.exe Shader!     # Just shader tests
```

**Test Rules:**
- Wildcard is `!` (not `*`) to avoid shell expansion
- Wildcard only at end
- No additional arguments
- Test binary is in `$PATH`

#### Python Tests
```bash
cd ork.core/pyext/tests
./test_name.py
```

**Python Requirements:**
- MUST use `#!/usr/bin/env ork.python` shebang
- Uses custom Python environment
- Tests should be incremental
- Verify each step before proceeding

## Coding Standards

### General Principles

1. **No Faking**: Real implementations only. If stuck, defer to user.
2. **Direct Style**: No fluff, straight to the point
3. **Full Understanding**: Read entire subsystems before modifying
4. **Incremental Development**: Each change must work before proceeding

### Command Naming Convention - Reverse DNS Notation

All Python commands in obt and orkid follow reverse DNS notation (generic→specific):

```bash
# Pattern: ork.category.subcategory.specific.py
ork.build.py                    # Generic build command
ork.test.core.exe              # Test runner for core
ork.llm.read.log.py            # LLM log reader
ork.asset.catalog.fetch.py     # Asset catalog fetch tool
ork.asset.catalog.list.py      # Asset catalog listing
```

**Why This Matters:**
- **Cognitive Load Reduction**: Commands group naturally in filesystem/tab completion
- **Discoverability**: `ork.<TAB>` shows all orkid commands
- **Hierarchy Clear**: `ork.asset.<TAB>` shows all asset-related tools
- **Consistency**: Same pattern as C++ namespaces (`ork::subsystem::class`)
- **Muscle Memory**: Predictable patterns reduce mental overhead

**Apply This Pattern To:**
- New command-line tools
- Python scripts in obt.project/bin
- Any user-facing commands

### C++ Conventions

```cpp
// Member variables with underscore prefix
struct MyStruct; // fwd decl (above definition or in types.h)
using mystruct_ptr_t = std::shared_ptr<MyStruct>; 
using mystruct_wkptr_t = std::weak_ptr<MyStruct>;  // only when really needed

struct MyStruct {
  // all public 
  // methods up top
  void publicMethod();                                       // Camel case for methods
  void _internalMethodWithPublicInterfaceExposure();         // _ denotes internal on methods
  virtual ~MyStruct();                                       // only when subclassed and used polymorphically                      
  static void noSharedFromThis(mystruct_ptr_t self);         // when class needs a shared_ptr of itself   
  static void noSharedFromThis2(mystruct_ptr_t self,int arg);// .. and needs other args
  // variables down low  
  int _member_variable = 0;                                  // prefer pod init in header
  int _member_variable2 = 0;                                 // member variables use lowercase and _ for spacing
  static constexpr int _static_const = 42;                   // use constexpr when appropriate
  static constexpr size_t _static_some_size_or_length = 42;  // use appropiate type for job

};


// Type aliases over raw types
using path_list_t = std::vector<path_t>;      // GOOD (can go in subsystem's types.h if it exists and publically used), reduces cognitive burden when reading...
std::vector<file::Path> paths;                // AVOID
path_list_t paths;                            // GOOD, reduces cognitive burden when reading...

// Namespace usage
namespace ork::subsystem {  // Nested namespaces
    // Implementation
}

using namespace X; // only in cpp files, not headers

namespace ork::subsystem {  // in header
   namespace nsalias = ::some::really::really::long:namespace;  // GOOD
   namespace nsalias2 = ::shortns;  // BAD
   struct X {
    X(nsalias::some_ptr_t s);  // GOOD
    X(nsalias2::some_ptr_t s); // BAD
   };
}

```

### Python Binding Conventions

```cpp
// In pyext_*.cpp files
py::class_<MyStruct, mystruct_ptr_t>(module, "MyStruct")
    .def(py::init<>())
    .def_property_readonly("member_variable", [](mystruct_ptr_t obj) -> int { // member variables use lowercase and _ for spacing
        return obj->_member_variable;
    })
    .def("publicMethod", [](mystruct_ptr_t obj) { // camelCase for method names
        obj->publicMethod();
    })
    .def("publicMethod2", [](mystruct_ptr_t obj, int i) { // no shared_from_this
        MyStruct::noSharedFromThis2(obj,i);
    })
    .def("method", &MyStruct::method, py::arg("param"))
    .def("__repr__", [](mystruct_ptr_t obj) {
        return FormatString("MyStruct(%p)", obj.get());
    });

// Path conversions (takes a variety of types convertable to string as a path)
.def("load", [](MyStruct* self, py::object path) {
    auto as_pystr = py::cast<py::str>(path); // as if str(x) were called
    auto as_str = as_pystr.cast<std::string>();
    return self->load(file::Path(as_str));
})
```

### Error Handling Philosophy

- **Mixed approach**: Some methods return nullptr or error state, others OrkAssert, fail early when appropriate
- **Document expectations**: Make failure modes clear
- **Python bindings**: Convert to exceptions where appropriate
- **Resource management**: RAII everywhere, no manual cleanup

## Architecture Principles

### Thread Safety

- **Generation-based versioning**: Increment generation on state changes
- **Lock-free reads**: Where possible using atomic operations  
- **Clear ownership**: One writer, many readers pattern
- **Immutable data**: Prefer immutable structures for shared state

### I/O Preferences

- **Real I/O**: Use stdio, POSIX, boost::filesystem
- **No abstractions**: Avoid FileEnv wrapper until it is refactored.
- **Direct operations**: `fopen`, `fread`, `fwrite` for C-style
- **Boost for paths**: `boost::filesystem` for path manipulation

### Memory Management

- **RAII everywhere**: Constructors acquire, destructors release
- **Smart pointers**: `shared_ptr` for shared ownership, `unique_ptr` for single
- **Weak pointers**: Break cycles, especially parent-child
- **No raw new/delete**: Always use make_shared or make_unique, except in special cases.

## Platform Support

### Target Platforms
- macOS Darwin (primary development)
- Modern Linux (Ubuntu 20.04+, RHEL 8+)
- Windows (experimental)

### Platform Abstractions
- Use POSIX when unsure
- Engine provides cross-platform abstractions
- Prefer standard library over platform-specific code
- Test on multiple platforms regularly

## Documentation Standards

### Code Documentation
```cpp
// File header
////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

// struct documentation
////////////////////////////////////////////////////////////////////////////////
// Brief description of struct purpose
//
// Detailed explanation including:
// - Key responsibilities
// - Usage patterns  
// - Thread safety guarantees
// - Example code if non-obvious
////////////////////////////////////////////////////////////////////////////////
struct MyStruct {
```

### Maintaining Project Documentation

**Key Documents:**
- `ork.dox/*.md` - System documentation
- `README.md` - Project overview
- This file - LLM interaction guide

**When to Update:**
- New patterns discovered
- API changes
- Architectural decisions
- Build process changes

## Common Pitfalls & Solutions

### Pitfall: Using shared_from_this
**Solution**: Use static factory pattern with parent passed as parameter

### Pitfall: Raw pointer types in APIs
**Solution**: Always use type aliases (`_ptr_t` suffixes)

### Pitfall: Circular dependencies
**Solution**: Forward declarations in types.h, weak_ptr for back-references

### Pitfall: Platform-specific code
**Solution**: Use engine abstractions or POSIX APIs

### Pitfall: Manual memory management  
**Solution**: RAII and smart pointers everywhere

## Session Management

### Working with LLMs

1. **Start each session**: Review this document for context
2. **During session**: Update patterns as discovered
3. **End of session**: Document any new findings here
4. **Complex tasks**: Break into smaller, verifiable steps

### Information Preservation

When discovering important patterns or conventions:
1. Add to relevant section above
2. Include concrete example
3. Explain why it matters
4. Note any exceptions

### LLM Log Search (for pattern discovery)

Use `ork.llm.read.log.py` to find previous solutions:

```bash
# List all available sessions
ork.llm.read.log.py --list

# Find recent work
ork.llm.read.log.py --recent

# Search for patterns in a specific file
ork.llm.read.log.py -s "pattern_name" [session-uuid].jsonl

# Time-based search across all files
ork.llm.read.log.py --show "14:00" "16:00" -s "shared_ptr"
```

## Quick Reference Card

### Must Remember
- ❌ NEVER use `shared_from_this`
- ✅ ALWAYS use type aliases (`_ptr_t`)
- ✅ ALWAYS use static factory pattern
- ✅ Parents create children
- ✅ Children store `weak_ptr` to parent
- ✅ Forward declare in types.h
- ✅ One line must work before next

### Build & Test
```bash
ork.build.py                    # Build everything
ork.test.core.exe TestName!     # Run C++ tests  
./test_name.py                  # Run Python tests (from test dir)
```

### Python Requirements
- Shebang: `#!/usr/bin/env ork.python`
- Custom environment, not system Python
- Use `py::arg()` for all parameters
- Convert paths from py::object to file::Path

---

*This is a living document. Each interaction should improve it.*