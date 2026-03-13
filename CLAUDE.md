# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

vxcore is a cross-platform C/C++ library providing notebook management for VNote. It exposes a stable **C ABI** (via `extern "C"`) for embedding in desktop (Qt), iOS (Swift), and Android (Kotlin) applications. Internally implemented in C++17. No Qt dependency.

## Build Commands

```bash
# Configure (standalone, with tests enabled)
cmake -B build -DVXCORE_BUILD_TESTS=ON

# Build
cmake --build build

# Run all tests
ctest --test-dir build -C Debug     # Windows
ctest --test-dir build              # Unix

# Run a single test module
ctest --test-dir build -C Debug -R test_core

# Run with verbose output
ctest --test-dir build -C Debug -V
```

### CMake Options

| Option | Default | Description |
|--------|---------|-------------|
| `VXCORE_BUILD_SHARED` | ON | Build shared library (DLL/SO) |
| `VXCORE_BUILD_CLI` | ON | Build `vxcore_cli` command-line tool |
| `VXCORE_BUILD_TESTS` | ON | Build test executables |
| `VXCORE_INSTALL` | ON | Enable install targets |

**When built as a VNote submodule**, the parent CMake sets `VXCORE_BUILD_TESTS=OFF`, `VXCORE_BUILD_CLI=OFF`, and `VXCORE_BUILD_SHARED=OFF` (static lib). Build vxcore standalone for testing.

## Testing

Custom test framework in `tests/test_utils.h`. Tests are plain C++ executables returning 0 on success.

**Test mode:** All tests must call `vxcore_set_test_mode(1)` before `vxcore_context_create()` to redirect data to temp directories instead of real AppData paths.

### Test Pattern

```cpp
#include "test_utils.h"
#include "vxcore/vxcore.h"

int test_something() {
  std::cout << "  Running test_something..." << std::endl;
  // ... test logic using ASSERT, ASSERT_EQ, ASSERT_NE, etc.
  ASSERT_EQ(err, VXCORE_OK);
  std::cout << "  ✓ test_something passed" << std::endl;
  return 0;
}

int main() {
  vxcore_set_test_mode(1);
  vxcore_clear_test_directory();

  RUN_TEST(test_something);

  std::cout << "All tests passed!" << std::endl;
  return 0;
}
```

### Adding a New Test

For tests that link the full vxcore library, use the helper in `tests/CMakeLists.txt`:

```cmake
add_vxcore_test(test_mymodule)
```

For tests that link only specific source files (unit-level isolation), add a manual `add_executable` block (see `test_db`, `test_rg_search_backend` in `tests/CMakeLists.txt` for examples).

### Available Test Macros

`ASSERT(cond)`, `ASSERT_EQ(a, b)`, `ASSERT_NE(a, b)`, `ASSERT_TRUE(cond)`, `ASSERT_FALSE(cond)`, `ASSERT_NULL(ptr)`, `ASSERT_NOT_NULL(ptr)`, `RUN_TEST(func)`.

### Test Modules

`test_core`, `test_notebook`, `test_folder`, `test_node`, `test_search`, `test_tag_sync`, `test_filetype`, `test_workspace`, `test_buffer`, `test_file_utils`, `test_process_utils`, `test_rg_search_backend`, `test_simple_search_backend`, `test_db`, `test_metadata_store`, `test_folder_manager`.

## Architecture

### Layered Design

```
C API Layer (include/vxcore/, src/api/)       ← Stable ABI, extern "C"
    │
Core Layer (src/core/)                        ← C++ business logic
    │
Database Layer (src/db/)                      ← SQLite metadata store
    │
Platform (src/platform/) + Utils (src/utils/) ← Cross-cutting concerns
```

### Data Flow

```
C API (vxcore_*)
    → VxCoreContext
        ├── ConfigManager (VxCoreConfig + VxCoreSessionConfig)
        └── NotebookManager
                └── Notebook (Bundled or Raw)
                        ├── FolderManager (folder/file CRUD)
                        │       └── DbManager + FileDb + TagDb
                        ├── BufferManager (open file handles)
                        ├── WorkspaceManager (split pane state)
                        └── SearchManager → ISearchBackend (rg / simple)
```

### Key Design Patterns

1. **Context-based API:** All operations go through `VxCoreContextHandle` (opaque pointer)
2. **JSON boundaries:** All complex data crosses the C ABI as JSON strings (using nlohmann/json internally)
3. **Caller-frees strings:** C API returns `char*` that the caller must free with `vxcore_string_free()`
4. **Unified Node API:** `vxcore_node_*` functions work on both files and folders, returning a `"type"` field in JSON
5. **Two notebook types:**
   - **Bundled:** Self-contained, config in `<root>/vx_notebook/config.json`, metadata in `<root>/vx_notebook/`
   - **Raw:** Config in session only, metadata in `<local_data>/notebooks/<id>/`
6. **FolderManager abstraction:** `BundledFolderManager` and `RawFolderManager` implement different storage strategies behind a common interface
7. **Search backends:** Pluggable via `ISearchBackend` (ripgrep for performance, simple built-in as fallback)
8. **Buffer providers:** `StandardBufferProvider` for notebook files, `ExternalBufferProvider` for standalone files

### Public Headers

| Header | Contents |
|--------|----------|
| `vxcore.h` | All C API functions (context, notebook, folder, file, node, tag, search, buffer, workspace) |
| `vxcore_types.h` | Error codes (`VxCoreError`), handles (`VxCoreContextHandle`), enums, version struct |
| `vxcore_events.h` | Event types and callback registration |
| `vxcore_log.h` | Log level control and file/console output |

### Thread Safety

- `NotebookManager`, `DbManager`, `FileDb`, `TagDb`: **NOT** thread-safe; designed for single-threaded use per notebook
- `Logger`: Thread-safe (uses mutex)

## Code Style

Google C++ Style Guide base, enforced by `.clang-format`. Key conventions:

- **C++17**, 2-space indent, 100-char line limit, pointer alignment right
- **C API functions:** `vxcore_module_action` snake_case (e.g., `vxcore_notebook_open`)
- **C++ types:** `PascalCase` (e.g., `NotebookManager`, `FolderConfig`)
- **C++ methods:** `PascalCase` (e.g., `GetNotebook()`, `CreateFolder()`)
- **Variables:** `snake_case` (e.g., `notebook_id`, `root_folder`)
- **Class members:** `snake_case_` with trailing underscore (e.g., `config_`, `notebooks_`)
- **Struct members:** `snake_case` without trailing underscore (e.g., `assets_folder`, `created_utc`)
- **Constants/Enumerators:** `kPascalCase` (e.g., `kMaxPathLength`, `kSuccess`)
- **Macros:** `UPPER_CASE` (e.g., `VXCORE_API`, `RUN_TEST`)
- **JSON keys:** `camelCase` (e.g., `createdUtc`, `assetsFolder`) — user-facing files follow JavaScript conventions

### Error Handling

```cpp
VxCoreError MyFunction(const char *input, char **output) {
  if (!input || !output) return VXCORE_ERR_NULL_POINTER;
  try {
    // ...
    *output = strdup(result.c_str());
    return VXCORE_OK;
  } catch (const std::exception &e) {
    VXCORE_LOG_ERROR("MyFunction failed: %s", e.what());
    return VXCORE_ERR_UNKNOWN;
  }
}
```

### Logging

```cpp
#include "utils/logger.h"
VXCORE_LOG_INFO("Opening notebook: %s", path.c_str());
VXCORE_LOG_ERROR("Failed: %s", err_msg.c_str());
```

## Third-Party Dependencies

- **nlohmann/json** — JSON parsing/serialization (`third_party/nlohmann/`)
- **SQLite** — Embedded database via amalgamation (`third_party/sqlite/`)

Do NOT modify files in `third_party/`.

## Adding a New C API Function

1. Declare in `include/vxcore/vxcore.h` with `VXCORE_API` prefix
2. Implement in appropriate `src/api/vxcore_*_api.cpp`
3. Add tests in `tests/test_*.cpp`
4. Use `vxcore_string_free()` convention for returned strings

## Path Handling

- Always use `CleanPath()` or `CleanFsPath()` from `src/utils/file_utils.h` to normalize
- Paths use forward slashes internally (even on Windows)
- Relative paths within notebooks are relative to notebook root
- Use `ConcatenatePaths()` to join path components
