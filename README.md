# vxcore
vxcore is a cross-platform C/C++ library providing notebook management functionality for VNote. It offers a stable C ABI for embedding in desktop (Qt), iOS (Swift), and Android (Kotlin) applications.

## Features
- **Notebook Management**: Create, open, close notebooks (bundled & raw types)
- **Unified Node Operations**: Work with files and folders using consistent `(notebook_id, path)` API
- **Folder Operations**: Create, delete, rename, move, copy folders
- **File Operations**: Create, delete, rename, move, copy files with metadata
- **Tag System**: Create tags at notebook level, apply/remove tags on files
- **Configuration**: JSON-based configuration with session persistence
- **Search**: Integration with external search tools (future)

## API Usage

### Unified Node API

The library provides unified APIs for working with both files and folders using a consistent `(notebook_id, node_path)` interface. The returned JSON includes a `"type"` field (`"file"` or `"folder"`) for automatic type identification.

```c
// Get config for any node (file or folder)
char *config_json = NULL;
VxCoreError err = vxcore_node_get_config(context, notebook_id, "path/to/node", &config_json);
if (err == VXCORE_OK) {
    // Parse JSON to check type
    // { "type": "file", "name": "...", ... } or { "type": "folder", "name": "...", ... }
    vxcore_string_free(config_json);
}

// Delete any node (automatically detects type)
vxcore_node_delete(context, notebook_id, "path/to/node");

// Rename any node
vxcore_node_rename(context, notebook_id, "old/path", "new_name");

// Move any node to different parent
vxcore_node_move(context, notebook_id, "src/path", "dest/parent");

// Copy any node with optional new name
char *copy_id = NULL;
vxcore_node_copy(context, notebook_id, "src/path", "dest/parent", "new_name", &copy_id);
vxcore_string_free(copy_id);

// Get/update metadata for any node
char *metadata_json = NULL;
vxcore_node_get_metadata(context, notebook_id, "path/to/node", &metadata_json);
vxcore_node_update_metadata(context, notebook_id, "path/to/node", "{\"key\": \"value\"}");
vxcore_string_free(metadata_json);
```

**Unified APIs:**
- `vxcore_node_get_config` - Get node config with type indicator
- `vxcore_node_delete` - Delete file or folder
- `vxcore_node_rename` - Rename file or folder
- `vxcore_node_move` - Move file or folder to different parent
- `vxcore_node_copy` - Copy file or folder with optional new name
- `vxcore_node_get_metadata` - Get metadata for file or folder
- `vxcore_node_update_metadata` - Update metadata for file or folder

**Create Operations:** For creating folders and files, use `vxcore_folder_create`, `vxcore_folder_create_path`, and `vxcore_file_create`. For file-specific tag operations, use `vxcore_file_update_tags`, `vxcore_file_tag`, and `vxcore_file_untag`.


## Build

### Configure
```bash
cmake -B build -DVXCORE_BUILD_TESTS=ON
```

### Build
```bash
cmake --build build
```

### Run Tests
```bash
# All tests
ctest --test-dir build -C Debug  # Windows
ctest --test-dir build           # Unix

# Single module
ctest --test-dir build -C Debug -R test_core
```

## License
See LICENSE file
