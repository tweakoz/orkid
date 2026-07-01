# Orkid C++ Code Search and Analysis Tools

This document describes the Orkid-specific C++ code analysis tools that provide advanced search, navigation, and exploration capabilities for the Orkid codebase.

## Overview

These tools leverage a SQLite database containing parsed C++ entities (classes, structs, enums, typedefs) and their relationships. The database must be built before using these tools.

## Building the Database

Before using any search tools, build the C++ entity database:

```bash
# Build database for specific modules
ork.cpp.db.build.py -m gfx        # Build only gfx module
ork.cpp.db.build.py -m core lev2  # Build multiple modules

# The database is stored at:
# ~/.staging-aug18/cpp_db_v2_orkid.db
```

## Core Search Tools

### ork.cpp.db.search.py

Advanced entity search with multiple filters and output formats.

#### Basic Usage

```bash
# Search for classes/structs by name
ork.cpp.db.search.py Camera           # Find entities with "Camera" in the name
ork.cpp.db.search.py "^Camera$"       # Exact match using regex
ork.cpp.db.search.py "Camera.*Data"   # Regex pattern matching

# Search by entity type
ork.cpp.db.search.py -t class UiCamera        # Only classes
ork.cpp.db.search.py -t struct CameraData     # Only structs
ork.cpp.db.search.py -t enum EBufferFormat    # Only enums
ork.cpp.db.search.py -t typedef ptr           # Only typedefs
ork.cpp.db.search.py -t namespace lev2        # Find namespaces
ork.cpp.db.search.py -t objects Camera        # Both classes and structs

# Search within specific namespace
ork.cpp.db.search.py -n ork::lev2 Camera      # Search in ork::lev2 namespace
ork.cpp.db.search.py -n ork::ecs Component    # Search in ork::ecs namespace

# Output formats
ork.cpp.db.search.py Camera --json            # JSON output for scripting
ork.cpp.db.search.py Camera --files           # Show only file paths
ork.cpp.db.search.py Camera --summary         # Minimal summary view
```

#### Advanced Features

```bash
# Combine filters
ork.cpp.db.search.py -t class -n ork::lev2 Context

# Case-insensitive search
ork.cpp.db.search.py -i camera

# Show all entities of a type
ork.cpp.db.search.py -t enum 

# Limit results
ork.cpp.db.search.py Camera --limit 10
```

#### Output Format

The default columnar output shows:
- **Name**: Entity name (yellow)
- **Namespace**: C++ namespace (magenta)
- **Type**: Entity type (green)
- **Access**: Public/private/protected for members
- **Signature**: Method signatures or typedef definitions
- **Location**: File path and line number

### ork.cpp.members.py

Display members of a specific class or struct.

#### Usage

```bash
# Show members of a class
ork.cpp.members.py CameraData              # find (implicit)
ork.cpp.members.py ork::lev2::Context      # explicit

# Output formats
ork.cpp.members.py CameraData --json       # JSON format
ork.cpp.members.py CameraData              # standard
```

#### Features

- Shows all public, protected, and private members
- Displays member types (method, field, constructor, etc.)
- Shows method signatures with parameter types
- Indicates const, noexcept, override, virtual qualifiers
- Shows default values for fields
- Displays typedef aliases with resolved types

### ork.cpp.references.py

Find all references to an entity or member.

#### Usage

```bash
# Find references to a class
ork.cpp.references.py ork::lev2::Context

# Find references to a specific member
ork.cpp.references.py "ork::lev2::CameraMatrices::_frustum"

# Filter by access type (not yet implemented in current version)
# ork.cpp.references.py Camera::setView --type call    # Method calls only
# ork.cpp.references.py _frustum --type read           # Read accesses
# ork.cpp.references.py _frustum --type write          # Write accesses

# Output formats
ork.cpp.references.py "ork::lev2::Context" --json                  # JSON for AI/scripting
ork.cpp.references.py "ork::lev2::Context" --limit 10              # Limit results
```

#### JSON Output Mode

The `--json` flag provides structured output optimized for programmatic parsing:

```json
{
  "entity": "ork::lev2::Context",
  "total_references": 42,
  "references": [
    {
      "file": "/path/to/file.cpp",
      "line": 123,
      "type": "call",
      "context": "ctx->beginFrame();"
    }
  ]
}
```

### ork.cpp.inhtree.py

Display inheritance hierarchy for classes.

#### Usage

```bash
# Show inheritance tree
ork.cpp.inhtree.py ork::lev2::Context

# Note: --derived flag not currently implemented
# The tool shows both base and derived classes by default

# JSON output
ork.cpp.inhtree.py ork::lev2::Context --json
```

#### Output

Shows the complete inheritance hierarchy with:
- Base classes (upward tree)
- Derived classes (downward tree)
- Multiple inheritance paths
- Virtual inheritance indicators

## Enum Tools

### ork.cpp.enums.py

Specialized tool for exploring enum definitions and values.

#### Basic Usage

```bash
# Display enum values
ork.cpp.enums.py EBufferFormat         # Show specific enum
ork.cpp.enums.py "ork::lev2::*"       # Pattern matching
ork.cpp.enums.py                  # Show all enums

# Search by hash value (for CrcEnum enums)
ork.cpp.enums.py --hash 0xE15695B7    # Search by hex hash
ork.cpp.enums.py --hash 3780548023    # Search by decimal hash
ork.cpp.enums.py --hash 0xe15695b7    # Lowercase hex works too

# Output formats
ork.cpp.enums.py EBufferFormat --json  # JSON output
```

#### Features

- **CrcEnum Detection**: Automatically detects enums using the CrcEnum macro
- **Hash Computation**: Computes and displays CRC32 hash values for CrcEnum values
- **Hash Search**: Find which enum value corresponds to a hash value
- **Pattern Matching**: Support for wildcard patterns
- **Type Information**: Shows enum class vs regular enum, base types

#### Output Format

Columnar display with:
- **Name**: Enum value name (yellow)
- **Namespace**: C++ namespace (magenta)
- **Type**: enum/enum class and base type (green)
- **Value**: Actual value or CrcEnum notation (cyan)
- **Hash**: For CrcEnum - hex (orange) and decimal (darker orange)

Example output:
```
ork::lev2::EBufferFormat
Name          Namespace    Type         Value              Hash
R8            ork::lev2    enum class   CrcEnum(R8)       0xE15695B7 (3780548023)
Y16UI         ork::lev2    enum class   CrcEnum(Y16UI)    0x539C0C64 (1402735716)
```

## Utility Tools

### ork.crcstring.py

Compute CRC32 hash for strings (validates CrcEnum values).

```bash
ork.crcstring.py R8
# Output: str(R8) -> CrcString(0xe15695b7:3780548023)
```

## Use Cases

### 1. Understanding Class Hierarchies

```bash
# Find all renderer implementations
ork.cpp.db.search.py Renderer
ork.cpp.inhtree.py ork::lev2::IRenderer

# Explore a specific renderer
ork.cpp.members.py ork::lev2::IRenderer
```

### 2. Tracking Field Usage

```bash
# Find all accesses to a camera's frustum
ork.cpp.references.py "ork::lev2::CameraMatrices::_frustum"

# Find references to display modes
ork.cpp.references.py "ork::lev2::Context::mDisplayModes"
```

### 3. Exploring Enums

```bash
# Find all buffer format enums
ork.cpp.db.search.py -t enum Buffer

# See enum values
ork.cpp.enums.py EBufferFormat

# Reverse lookup a hash from logs (supports hex and decimal)
ork.cpp.enums.py --hash 0xE15695B7
ork.cpp.enums.py --hash 3780548023
ork.cpp.enums.py --hash 0xe15695b7  # lowercase hex also works
```

### 4. Finding Type Definitions

```bash
# Find all smart pointer typedefs
ork.cpp.db.search.py -t typedef "_ptr"

# See what a typedef resolves to
ork.cpp.db.search.py -t typedef camera_node_ptr_t
```

### 5. Namespace Exploration

```bash
# Find all namespaces
ork.cpp.db.search.py -t namespace 

# Find everything in a namespace
ork.cpp.db.search.py -n ork::lev2 
```

### 6. API Discovery

```bash
# Find all methods named "render"
ork.cpp.db.search.py "::render$"

# Find all begin/end method pairs
ork.cpp.db.search.py "^begin" --json | jq '.entities[].canonical_name'
ork.cpp.db.search.py "^end" --json | jq '.entities[].canonical_name'
```

## Implementation Notes

### Database Schema

The tools use a SQLite database with tables for:
- `entities`: Classes, structs, enums, typedefs, namespaces
- `entity_members`: Methods, fields, enum values
- `entity_locations`: File locations and line numbers
- `entity_accesses`: Read/write/call references
- `base_classes`: Inheritance relationships

### Line Number Accuracy

Line numbers are remapped from trimmed source to original source files, ensuring accurate navigation to code locations.

### CrcEnum Handling

The CrcEnum macro is specially handled:
- Parser detects CrcEnum pattern in enum definitions
- Hash values are computed using Python's zlib.crc32()
- Values match Orkid's runtime CRC32 computation

### Type Resolution

- Typedef chains are resolved to show final types
- Template parameters are preserved
- Nested types are fully qualified

## Performance Tips

1. **Build Incrementally**: Build only the modules you need
2. **Use Patterns Wisely**: Exact matches are faster than wildcards
3. **Limit Results**: Use `--limit` for large result sets
4. **JSON for Scripting**: JSON output is optimized for parsing
5. **Cache Database**: The database persists between sessions

## Troubleshooting

### Database Not Found
```
Orkid database not found!
Expected at: ~/.staging-aug18/cpp_db_v2_orkid.db
Run 'ork.cpp.db.build.py -m <modules>' to build it first
```

**Solution**: Build the database with required modules.

### No Results Found

- Check spelling and namespace qualification
- Try with wildcards: `*Camera*`
- Verify the module was included in database build

### Stale Database

If code has changed significantly:
```bash
ork.cpp.db.build.py -m <modules>  # Rebuild affected modules
```

## Advanced Examples

### Finding Unused Code

```bash
# Find classes with no references
for class in $(ork.cpp.db.search.py -t class --json | jq -r '.entities[].canonical_name'); do
  refs=$(ork.cpp.references.py "$class" --json | jq '.total_references')
  if [ "$refs" = "0" ]; then
    echo "Unused: $class"
  fi
done
```

### Generating Documentation

```bash
# Export all class information
ork.cpp.db.search.py -t class --json > classes.json

# Extract public API
ork.cpp.members.py ClassName --json | jq '.members[] | select(.access_level == "PUBLIC")'
```

### Cross-Reference Analysis

```bash
# Find all classes that use a specific type
ork.cpp.references.py "shared_ptr<Camera>" --type read --json | \
  jq -r '.references[].file' | sort -u
```

## Related Documentation

- `cpp_database_v2.py`: Database implementation
- `cpp_parser_descent.py`: C++ parsing logic
- `cpp_entities_v2.py`: Entity type definitions
- Tree-sitter C++ grammar documentation