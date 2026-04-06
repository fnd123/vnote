# Agent Guidelines for vxcore

## Project Overview

vxcore is a cross-platform C/C++ library providing notebook management functionality for VNote. It offers a stable C ABI for embedding in desktop (Qt), iOS (Swift), and Android (Kotlin) applications.

**Key Features**: Notebook management, folder/file operations, tag system, full-text search, JSON-based configuration.

## Build Commands

```bash
# Configure
cmake -B build -DVXCORE_BUILD_TESTS=ON

# Build
cmake --build build

# Run all tests
ctest --test-dir build -C Debug     # Windows
ctest --test-dir build              # Unix

# Run single test module
ctest --test-dir build -C Debug -R test_core
ctest --test-dir build -C Debug -R test_notebook
ctest --test-dir build -C Debug -R test_folder
ctest --test-dir build -C Debug -R test_search
ctest --test-dir build -C Debug -R test_db

# Run with verbose output
ctest --test-dir build -C Debug -V
```

## Test Structure

- Each module has its own test executable: `test_core`, `test_notebook`, `test_folder`, `test_search`, `test_db`, etc.
- Individual test cases tracked within each module's `main()`
- Test macros defined in `tests/test_utils.h`: `ASSERT`, `ASSERT_EQ`, `ASSERT_NE`, `ASSERT_TRUE`, `ASSERT_FALSE`, `ASSERT_NULL`, `ASSERT_NOT_NULL`, `RUN_TEST`
- **Test Mode**: All tests call `vxcore_set_test_mode(1)` to isolate test data
  - Enabled: Uses `%TEMP%\vxcore_test` (Windows) or `/tmp/vxcore_test` (Unix)
  - Disabled: Uses real AppData paths (`%APPDATA%\VNote`, `%LOCALAPPDATA%\VNote`)
  - CLI tools do NOT enable test mode (use production paths)

## Code Style

- **Language**: C++17 with C ABI for public API
- **Formatting**: Use `.clang-format` based on Google style (indent 2 spaces, 100 col limit, pointer alignment right)
- **Headers**: Sort includes; public API in `include/vxcore/`, implementation in `src/`
- **File Format**: Unix line endings

### Naming Conventions (Google C++ Style Guide)

| Element | Convention | Examples |
|---------|------------|----------|
| C API functions | `snake_case` | `vxcore_context_create`, `vxcore_notebook_open` |
| Types (classes/structs/enums) | `PascalCase` | `NotebookManager`, `VxCoreError`, `FolderConfig` |
| Functions/Methods | `PascalCase` | `GetNotebook()`, `CreateFolder()`, `ToJson()` |
| Variables (local/global) | `snake_case` | `notebook_id`, `root_folder`, `out_config_json` |
| Class data members | `snake_case_` (trailing underscore) | `config_`, `notebooks_`, `folder_manager_` |
| Struct data members | `snake_case` (no trailing underscore) | `assets_folder`, `created_utc`, `parent_id` |
| Constants | `kPascalCase` | `kMaxPathLength`, `kConfigFileName` |
| Enumerators | `kPascalCase` | `kSuccess`, `kTrace`, `kDebug` |
| Macros | `UPPER_CASE` | `VXCORE_API`, `RUN_TEST`, `VXCORE_LOG_INFO` |
| Namespaces | `snake_case` | `vxcore`, `vxcore::db` |

### JSON Conventions

- **JSON keys**: Always use `camelCase` (e.g., `createdUtc`, `modifiedUtc`, `assetsFolder`, `rootFolder`)
- **C++ struct members**: Use `snake_case` (e.g., `created_utc`, `modified_utc`, `assets_folder`)
- **Rationale**: User-facing files (config.json, session.json) follow JavaScript conventions

> **⚠ JSON Key Mismatch Warning**
>
> When sending or receiving JSON to/from vxcore, you **must** use the exact camelCase keys defined in the corresponding `FromJson()` / `ToJson()` methods. Do **not** invent similar-sounding names.
>
> **Real-world example**: Qt code sent `pathPatterns` but vxcore parses `filePatterns` — the field was silently ignored and search filters stopped working.
>
> Rules:
> 1. Before writing or changing any JSON key on the Qt/controller side, open the relevant vxcore `FromJson()` / `ToJson()` source and verify the exact key string.
> 2. Never assume a key name from context; always confirm against the parser code.
> 3. When renaming a JSON key in vxcore, update **all** callers (Qt services, CLI, tests) in the same change.
> 4. Update tests alongside any JSON contract change to catch mismatches at build time.

### Error Handling

- Return `VxCoreError` codes from C API functions
- Check null pointers; use `(void)param` for unused parameters
- Caller frees strings with `vxcore_string_free()`
- Context owns handles; destroy context to free all resources

### Platform

- Use `VXCORE_API` macro for exports; Windows DLL-safe
- No Qt dependencies in core library
- Namespaces: C++ code in `namespace vxcore`; close with `// namespace vxcore`

## Folder Structure

```
vxcore/
├── include/vxcore/          # Public C API headers
│   ├── vxcore.h             # Main API (context, notebook, folder, file, tag, search)
│   ├── vxcore_types.h       # Error codes, handles, version struct
│   └── vxcore_log.h         # Logging API
│
├── src/
│   ├── api/                 # C API implementation layer
│   │   ├── vxcore_api.cpp           # Context, version, test mode
│   │   ├── vxcore_notebook_api.cpp  # Notebook operations
│   │   ├── vxcore_folder_api.cpp    # Folder/file operations
│   │   ├── vxcore_tag_api.cpp       # Tag operations
│   │   ├── vxcore_search_api.cpp    # Search operations
│   │   ├── api_utils.h              # API helper utilities
│   │   ├── error_handler.h/.cpp     # Error handling utilities
│   │   └── handle_manager.h/.cpp    # Handle management
│   │
│   ├── core/                # Core C++ business logic
│   │   ├── context.h                # VxCoreContext: owns ConfigManager + NotebookManager
│   │   ├── config_manager.h/.cpp    # VxCoreConfig + VxCoreSessionConfig management
│   │   ├── vxcore_config.h/.cpp     # Application config (vxcore.json)
│   │   ├── vxcore_session_config.h/.cpp  # Session config (session.json)
│   │   ├── notebook_manager.h/.cpp  # Notebook lifecycle (create, open, close, list)
│   │   ├── notebook.h/.cpp          # Notebook base class, NotebookConfig, NotebookRecord, TagNode
│   │   ├── bundled_notebook.h/.cpp  # Bundled notebook (config in vx_notebook/config.json)
│   │   ├── raw_notebook.h/.cpp      # Raw notebook (config in session only)
│   │   ├── folder_manager.h         # Abstract folder/file operations interface
│   │   ├── bundled_folder_manager.h/.cpp  # Folder ops for bundled notebooks
│   │   ├── raw_folder_manager.h/.cpp      # Folder ops for raw notebooks
│   │   └── folder.h/.cpp            # FolderConfig, FolderRecord, FileRecord
│   │
│   ├── db/                  # SQLite database layer
│   │   ├── db_manager.h/.cpp        # Database lifecycle, schema, transactions
│   │   ├── db_schema.h              # Schema definitions (tables, indexes, FTS5)
│   │   ├── file_db.h/.cpp           # File/folder CRUD operations
│   │   ├── tag_db.h/.cpp            # Tag CRUD and queries
│   │   └── sync_manager.h/.cpp      # Filesystem ↔ database synchronization
│   │
│   ├── search/              # Search subsystem
│   │   ├── search_manager.h/.cpp    # Search orchestration
│   │   ├── search_backend.h         # ISearchBackend interface
│   │   ├── simple_search_backend.h/.cpp  # Built-in search implementation
│   │   ├── rg_search_backend.h/.cpp      # Ripgrep integration
│   │   ├── search_query.h/.cpp      # SearchQuery, SearchScope, SearchOption
│   │   └── search_file_info.h/.cpp  # SearchFileInfo struct
│   │
│   ├── platform/            # Platform-specific code
│   │   ├── path_provider.h/.cpp     # AppData, LocalData paths per platform
│   │   └── process_utils.h/.cpp     # External process execution (for rg)
│   │
│   └── utils/               # General utilities
│       ├── utils.h/.cpp             # GenerateUUID(), GetCurrentTimestampMillis(), HasFlag()
│       ├── file_utils.h/.cpp        # CleanPath(), ConcatenatePaths(), LoadJsonFile()
│       ├── string_utils.h/.cpp      # ToLowerString(), MatchesPattern(), exclude patterns
│       └── logger.h/.cpp            # VXCORE_LOG_* macros, Logger singleton
│
├── cli/                     # Command-line interface
│   ├── main.cpp             # Entry point, command dispatch
│   ├── args.h/.cpp          # Argument parsing
│   ├── notebook_cmd.h/.cpp  # notebook subcommand
│   ├── tag_cmd.h/.cpp       # tag subcommand
│   ├── config_cmd.h/.cpp    # config subcommand
│   └── json_helpers.h/.cpp  # JSON formatting utilities
│
├── tests/                   # Test executables
│   ├── test_utils.h         # Test macros and helpers
│   ├── test_core.cpp        # Context, config tests
│   ├── test_notebook.cpp    # Notebook CRUD tests
│   ├── test_folder.cpp      # Folder/file operation tests
│   ├── test_search.cpp      # Search tests
│   ├── test_db.cpp          # Database layer tests
│   └── ...
│
├── third_party/             # External dependencies (DO NOT MODIFY)
│   ├── nlohmann/json.hpp    # JSON library
│   └── sqlite/              # SQLite amalgamation
│
└── data/
    └── vxcore.json          # Default configuration template
```

## Architecture Notes

### Layered Architecture

```
┌─────────────────────────────────────────────────┐
│  C API Layer (include/vxcore/, src/api/)        │  ← Stable ABI for FFI
├─────────────────────────────────────────────────┤
│  Core Layer (src/core/)                         │  ← Business logic
├─────────────────────────────────────────────────┤
│  Database Layer (src/db/)                       │  ← SQLite metadata
├──────────────────────┬──────────────────────────┤
│  Platform (src/platform/)  │  Utils (src/utils/)│  ← Cross-cutting concerns
└──────────────────────┴──────────────────────────┘
```

### Key Design Patterns

1. **Context-based API**: All operations go through `VxCoreContextHandle`
2. **Opaque handles**: C API uses opaque pointers for type safety
3. **JSON boundaries**: All complex data crosses C ABI as JSON strings
4. **Two notebook types**:
   - **Bundled**: Config stored in `<root>/vx_notebook/config.json`, metadata in `<root>/vx_notebook/`
   - **Raw**: Config stored in session config only, metadata in local data folder
5. **FolderManager abstraction**: `BundledFolderManager` and `RawFolderManager` implement different storage strategies
6. **Search backends**: Pluggable via `ISearchBackend` interface (ripgrep, simple built-in)

### Thread Safety

- `NotebookManager` operations are NOT thread-safe; caller must synchronize
- `DbManager`, `FileDb`, `TagDb` are NOT thread-safe; designed for single-threaded use per notebook
- `Logger` is thread-safe (uses mutex)

### Data Flow

```
User Code
    │
    ▼
C API (vxcore_*)
    │
    ▼
VxCoreContext
    ├── ConfigManager (VxCoreConfig, VxCoreSessionConfig)
    └── NotebookManager
            └── Notebook (Bundled/Raw)
                    ├── NotebookConfig (id, name, tags, metadata)
                    ├── FolderManager (folder/file operations)
                    │       └── DbManager + FileDb + TagDb
                    └── SearchManager (search operations)
                            └── ISearchBackend (rg/simple)
```

## Important Implementation Details

### Notebook Types

| Aspect | Bundled | Raw |
|--------|---------|-----|
| Config location | `<root>/vx_notebook/config.json` | Session config only |
| Metadata folder | `<root>/vx_notebook/` | `<local_data>/notebooks/<id>/` |
| Database location | `<metadata_folder>/metadata.db` | `<metadata_folder>/metadata.db` |
| Portable | Yes (self-contained) | No (external metadata) |

### Path Handling

- Always use `CleanPath()` or `CleanFsPath()` to normalize paths
- Paths use forward slashes internally (even on Windows)
- Relative paths within notebook are relative to notebook root
- Use `ConcatenatePaths()` to join path components

### Utility Functions (check before implementing)

- `src/utils/utils.h`: `GenerateUUID()`, `GetCurrentTimestampMillis()`, `HasFlag()`
- `src/utils/file_utils.h`: `CleanPath()`, `LoadJsonFile()`, `ConcatenatePaths()`, `SplitPath()`, `RelativePath()`
- `src/utils/string_utils.h`: `ToLowerString()`, `MatchesPattern()`, `MatchesPatterns()`
- `src/utils/logger.h`: `VXCORE_LOG_TRACE/DEBUG/INFO/WARN/ERROR/FATAL`

### Logging

```cpp
#include "utils/logger.h"

VXCORE_LOG_INFO("Opening notebook: %s", path.c_str());
VXCORE_LOG_ERROR("Failed to open database: %s", db_path.c_str());
```

## Common Patterns

### Adding a New C API Function

1. Declare in `include/vxcore/vxcore.h`
2. Implement in appropriate `src/api/vxcore_*_api.cpp`
3. Add tests in `tests/test_*.cpp`

### Adding a New Core Feature

1. Implement in `src/core/`
2. Expose through existing managers or create new manager
3. Add C API wrapper in `src/api/`
4. Add tests

### Working with JSON

```cpp
#include <nlohmann/json.hpp>

// Parsing
nlohmann::json j = nlohmann::json::parse(json_string);
auto config = NotebookConfig::FromJson(j);

// Serializing
nlohmann::json j = config.ToJson();
std::string json_string = j.dump();
```

### Error Handling Pattern

```cpp
VxCoreError MyFunction(const char* input, char** output) {
  if (!input || !output) {
    return VXCORE_ERR_NULL_POINTER;
  }

  try {
    // ... implementation ...
    *output = strdup(result.c_str());
    return VXCORE_OK;
  } catch (const std::exception& e) {
    VXCORE_LOG_ERROR("MyFunction failed: %s", e.what());
    return VXCORE_ERR_UNKNOWN;
  }
}
```
