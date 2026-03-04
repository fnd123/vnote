#ifndef VXCORE_H
#define VXCORE_H

#include "vxcore_types.h"

#ifdef __cplusplus
extern "C" {
#endif

VXCORE_API VxCoreVersion vxcore_get_version(void);

VXCORE_API const char *vxcore_get_version_string(void);

VXCORE_API const char *vxcore_error_message(VxCoreError error);

VXCORE_API void vxcore_set_test_mode(int enabled);

VXCORE_API void vxcore_set_app_info(const char *org_name, const char *app_name);

VXCORE_API void vxcore_get_execution_file_path(char **out_path);

VXCORE_API void vxcore_get_execution_folder_path(char **out_path);

VXCORE_API VxCoreError vxcore_context_create(const char *config_json,
                                             VxCoreContextHandle *out_context);

VXCORE_API void vxcore_context_destroy(VxCoreContextHandle context);

VXCORE_API VxCoreError vxcore_context_get_last_error(VxCoreContextHandle context,
                                                     const char **out_message);

VXCORE_API VxCoreError vxcore_context_get_data_path(VxCoreContextHandle context,
                                                    VxCoreDataLocation location, char **out_path);

VXCORE_API VxCoreError vxcore_context_get_config_path(VxCoreContextHandle context, char **out_path);

VXCORE_API VxCoreError vxcore_context_get_session_config_path(VxCoreContextHandle context,
                                                              char **out_path);

VXCORE_API VxCoreError vxcore_context_get_config(VxCoreContextHandle context, char **out_json);

VXCORE_API VxCoreError vxcore_context_get_session_config(VxCoreContextHandle context,
                                                         char **out_json);

VXCORE_API VxCoreError vxcore_context_get_config_by_name(VxCoreContextHandle context,
                                                         VxCoreDataLocation location,
                                                         const char *base_name, char **out_json);

VXCORE_API VxCoreError vxcore_context_get_config_by_name_with_defaults(VxCoreContextHandle context,
                                                                       VxCoreDataLocation location,
                                                                       const char *base_name,
                                                                       const char *default_json,
                                                                       char **out_json);

VXCORE_API VxCoreError vxcore_context_update_config_by_name(VxCoreContextHandle context,
                                                            VxCoreDataLocation location,
                                                            const char *base_name,
                                                            const char *json);

VXCORE_API VxCoreError vxcore_notebook_create(VxCoreContextHandle context, const char *path,
                                              const char *config_json, VxCoreNotebookType type,
                                              char **out_notebook_id);

VXCORE_API VxCoreError vxcore_notebook_open(VxCoreContextHandle context, const char *path,
                                            char **out_notebook_id);

VXCORE_API VxCoreError vxcore_notebook_close(VxCoreContextHandle context, const char *notebook_id);

VXCORE_API VxCoreError vxcore_notebook_list(VxCoreContextHandle context, char **out_notebooks_json);

VXCORE_API VxCoreError vxcore_notebook_get_config(VxCoreContextHandle context,
                                                  const char *notebook_id, char **out_config_json);

VXCORE_API VxCoreError vxcore_notebook_update_config(VxCoreContextHandle context,
                                                     const char *notebook_id,
                                                     const char *config_json);

// Rebuilds the metadata cache for the notebook from ground truth (vx.json files).
// Use this when the cache seems out of sync or corrupted.
// The cache uses lazy sync by default - this forces a full rebuild.
VXCORE_API VxCoreError vxcore_notebook_rebuild_cache(VxCoreContextHandle context,
                                                     const char *notebook_id);

// ============ Recycle Bin Operations (Bundled Notebooks Only) ============
// Get the path to the recycle bin folder.
// Returns empty string for raw notebooks (not supported).
VXCORE_API VxCoreError vxcore_notebook_get_recycle_bin_path(VxCoreContextHandle context,
                                                            const char *notebook_id,
                                                            char **out_path);

// Empty the recycle bin by deleting all files and folders in it.
// Returns VXCORE_ERR_UNSUPPORTED for raw notebooks.
VXCORE_API VxCoreError vxcore_notebook_empty_recycle_bin(VxCoreContextHandle context,
                                                         const char *notebook_id);

// ============ Folder Operations ============
VXCORE_API VxCoreError vxcore_folder_create(VxCoreContextHandle context, const char *notebook_id,
                                            const char *parent_path, const char *folder_name,
                                            char **out_folder_id);

VXCORE_API VxCoreError vxcore_folder_create_path(VxCoreContextHandle context,
                                                 const char *notebook_id, const char *folder_path,
                                                 char **out_folder_id);

// List direct children of a folder (files and subfolders)
// folder_path: Path relative to notebook root ("" or "." for root)
// Output JSON: {"files": [...], "folders": [...]}
VXCORE_API VxCoreError vxcore_folder_list_children(VxCoreContextHandle context,
                                                   const char *notebook_id, const char *folder_path,
                                                   char **out_children_json);

// List external (unindexed) nodes in a folder.
// External nodes exist on filesystem but are not tracked in metadata.
// folder_path: Path relative to notebook root ("" or "." for root)
// Output JSON: {"files": [...], "folders": [...]}
// Each entry contains only "name" field (no ID since not indexed).
VXCORE_API VxCoreError vxcore_folder_list_external(VxCoreContextHandle context,
                                                   const char *notebook_id, const char *folder_path,
                                                   char **out_external_json);

// Get an available name for a new node (file or folder) in a folder.
// If new_name is available, out_available_name will be set to new_name.
// If new_name exists, tries new_name_1, new_name_2, etc. until an available name is found.
// For files with extensions, the suffix is inserted before the extension (e.g., file_1.txt).
// folder_path: Path relative to notebook root ("" or "." for root)
// new_name: Desired name for the new node
// out_available_name: Receives the available name (caller must free with vxcore_string_free)
VXCORE_API VxCoreError vxcore_folder_get_available_name(VxCoreContextHandle context,
                                                        const char *notebook_id,
                                                        const char *folder_path,
                                                        const char *new_name,
                                                        char **out_available_name);
// ============ File Operations ============
VXCORE_API VxCoreError vxcore_file_create(VxCoreContextHandle context, const char *notebook_id,
                                          const char *folder_path, const char *file_name,
                                          char **out_file_id);

// Import an external file into a notebook folder.
// The file is copied (not moved) to the notebook.
// If a file with the same name exists, a unique name is generated (e.g., file_1.txt).
// out_file_id: receives the ID of the imported file (caller must free with vxcore_string_free)
VXCORE_API VxCoreError vxcore_file_import(VxCoreContextHandle context, const char *notebook_id,
                                          const char *folder_path, const char *external_file_path,
                                          char **out_file_id);

// Import an external folder into a notebook folder recursively.
// The folder and all its contents are copied (not moved) to the notebook.
// All files and subfolders are indexed into the notebook's metadata system.
// If a folder with the same name exists, a unique name is generated (e.g., folder_1).
// out_folder_id: receives the ID of the imported root folder (caller must free with
// vxcore_string_free) external_folder_path: must be an absolute path to a folder outside the
// notebook root suffix_allowlist: semicolon-separated list of file extensions to import (e.g.,
// "txt;md;json"),
//                   or NULL/empty to import all files
VXCORE_API VxCoreError vxcore_folder_import(VxCoreContextHandle context, const char *notebook_id,
                                            const char *dest_folder_path,
                                            const char *external_folder_path,
                                            const char *suffix_allowlist, char **out_folder_id);

VXCORE_API VxCoreError vxcore_file_update_tags(VxCoreContextHandle context, const char *notebook_id,
                                               const char *file_path, const char *tags_json);

VXCORE_API VxCoreError vxcore_file_tag(VxCoreContextHandle context, const char *notebook_id,
                                       const char *file_path, const char *tag_name);

VXCORE_API VxCoreError vxcore_file_untag(VxCoreContextHandle context, const char *notebook_id,
                                         const char *file_path, const char *tag_name);

// Peek at the first N bytes of a file's content.
// file_path: Path relative to notebook root
// max_bytes: Maximum number of bytes to read
// out_content: receives the content (caller must free with vxcore_string_free)
// Returns VXCORE_ERR_NOT_FOUND if file doesn't exist.
VXCORE_API VxCoreError vxcore_file_peek(VxCoreContextHandle context, const char *notebook_id,
                                        const char *file_path, size_t max_bytes,
                                        char **out_content);

// ============ Node Operations (Unified File/Folder API) ============
// These APIs work with both files and folders using the same (notebook_id, node_path) pair.
// The returned JSON includes a "type" field ("file" or "folder") for caller identification.

// Get node config (unified version of folder_get_config and file_get_info)
VXCORE_API VxCoreError vxcore_node_get_config(VxCoreContextHandle context, const char *notebook_id,
                                              const char *node_path, char **out_config_json);

// Delete node (file or folder)
VXCORE_API VxCoreError vxcore_node_delete(VxCoreContextHandle context, const char *notebook_id,
                                          const char *node_path);

// Rename node (file or folder)
VXCORE_API VxCoreError vxcore_node_rename(VxCoreContextHandle context, const char *notebook_id,
                                          const char *node_path, const char *new_name);

// Move node to a different parent folder
VXCORE_API VxCoreError vxcore_node_move(VxCoreContextHandle context, const char *notebook_id,
                                        const char *src_path, const char *dest_parent_path);

// Copy node to a different parent folder with optional new name
VXCORE_API VxCoreError vxcore_node_copy(VxCoreContextHandle context, const char *notebook_id,
                                        const char *src_path, const char *dest_parent_path,
                                        const char *new_name, char **out_node_id);

// Get node metadata
VXCORE_API VxCoreError vxcore_node_get_metadata(VxCoreContextHandle context,
                                                const char *notebook_id, const char *node_path,
                                                char **out_metadata_json);

// Update node metadata
VXCORE_API VxCoreError vxcore_node_update_metadata(VxCoreContextHandle context,
                                                   const char *notebook_id, const char *node_path,
                                                   const char *metadata_json);

// Index a filesystem node (file or folder) into the metadata store.
// The node must exist on filesystem but not be tracked in metadata.
// node_path: Path relative to notebook root (e.g., "folder/file.md")
// Returns VXCORE_ERR_NOT_FOUND if node doesn't exist on filesystem.
// Returns VXCORE_ERR_ALREADY_EXISTS if node is already indexed.
VXCORE_API VxCoreError vxcore_node_index(VxCoreContextHandle context, const char *notebook_id,
                                         const char *node_path);

// Remove a node (file or folder) from the metadata index.
// The filesystem is NOT modified - only metadata is removed.
// node_path: Path relative to notebook root (e.g., "folder/file.md")
// Returns VXCORE_ERR_NOT_FOUND if node is not in metadata.
VXCORE_API VxCoreError vxcore_node_unindex(VxCoreContextHandle context, const char *notebook_id,
                                           const char *node_path);

// Get relative path of a node (file or folder) by its ID.
// node_id: The UUID of the node
// out_path: receives the relative path (caller must free with vxcore_string_free)
// Returns VXCORE_ERR_NOT_FOUND if node is not found in metadata.
VXCORE_API VxCoreError vxcore_node_get_path_by_id(VxCoreContextHandle context,
                                                  const char *notebook_id, const char *node_id,
                                                  char **out_path);

// ============ File Type Operations ============

// Returns JSON array of all file types.
// Caller must free the returned string with vxcore_string_free().
VXCORE_API VxCoreError vxcore_filetype_list(VxCoreContextHandle context, char **out_json);

// Get file type by suffix (case-insensitive).
// Returns Others type if suffix not found.
// Caller must free the returned string with vxcore_string_free().
VXCORE_API VxCoreError vxcore_filetype_get_by_suffix(VxCoreContextHandle context,
                                                     const char *suffix, char **out_json);

// Get file type by exact name match.
// Returns VXCORE_ERR_NOT_FOUND if name not found.
// Caller must free the returned string with vxcore_string_free().
VXCORE_API VxCoreError vxcore_filetype_get_by_name(VxCoreContextHandle context, const char *name,
                                                   char **out_json);

// Set the entire file types configuration.
// Replaces all file types with the provided JSON array.
// filetype_json: JSON array of file type objects, each with:
//   - "name" (string, required, must be unique)
//   - "suffixes" (array of strings)
//   - "isNewable" (boolean)
//   - "displayName" (string)
//   - "metadata" (object, optional)
// Returns VXCORE_ERR_JSON_PARSE if JSON is invalid.
// Returns VXCORE_ERR_INVALID_PARAM if validation fails (empty name, duplicate names).
VXCORE_API VxCoreError vxcore_filetype_set(VxCoreContextHandle context, const char *filetype_json);

// ============ Tag Operations ============

VXCORE_API VxCoreError vxcore_tag_create(VxCoreContextHandle context, const char *notebook_id,
                                         const char *tag_name);

VXCORE_API VxCoreError vxcore_tag_create_path(VxCoreContextHandle context, const char *notebook_id,
                                              const char *tag_path);

VXCORE_API VxCoreError vxcore_tag_delete(VxCoreContextHandle context, const char *notebook_id,
                                         const char *tag_name);

VXCORE_API VxCoreError vxcore_tag_list(VxCoreContextHandle context, const char *notebook_id,
                                       char **out_tags_json);

VXCORE_API VxCoreError vxcore_tag_move(VxCoreContextHandle context, const char *notebook_id,
                                       const char *tag_name, const char *parent_tag);

VXCORE_API VxCoreError vxcore_search_files(VxCoreContextHandle context, const char *notebook_id,
                                           const char *query_json, const char *input_files_json,
                                           char **out_results_json);

VXCORE_API VxCoreError vxcore_search_content(VxCoreContextHandle context, const char *notebook_id,
                                             const char *query_json, const char *input_files_json,
                                             char **out_results_json);

VXCORE_API VxCoreError vxcore_search_by_tags(VxCoreContextHandle context, const char *notebook_id,
                                             const char *query_json, const char *input_files_json,
                                             char **out_results_json);

// Resolve an absolute path to its containing notebook.
// out_notebook_id: receives the notebook ID (caller must free with vxcore_string_free)
// out_relative_path: receives the relative path within notebook (caller must free with
// vxcore_string_free) Returns VXCORE_ERR_NOT_FOUND if path is not within any open notebook.
VXCORE_API VxCoreError vxcore_path_resolve(VxCoreContextHandle context, const char *absolute_path,
                                           char **out_notebook_id, char **out_relative_path);
VXCORE_API void vxcore_string_free(char *str);

#ifdef __cplusplus
}
#endif

#endif
