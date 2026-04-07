#include <nlohmann/json.hpp>

#include "api/api_utils.h"
#include "core/context.h"
#include "core/folder.h"
#include "core/folder_manager.h"
#include "core/metadata_store.h"
#include "core/notebook_manager.h"
#include "utils/file_utils.h"
#include "vxcore/vxcore.h"

VXCORE_API VxCoreError vxcore_folder_create(VxCoreContextHandle context, const char *notebook_id,
                                            const char *parent_path, const char *folder_name,
                                            char **out_folder_id) {
  if (!context || !notebook_id || !folder_name || !out_folder_id) {
    return VXCORE_ERR_INVALID_PARAM;
  }

  vxcore::VxCoreContext *ctx = reinterpret_cast<vxcore::VxCoreContext *>(context);

  try {
    vxcore::Notebook *notebook = ctx->notebook_manager->GetNotebook(notebook_id);
    if (!notebook) {
      ctx->last_error = "Notebook not found";
      return VXCORE_ERR_NOT_FOUND;
    }

    vxcore::FolderManager *folder_manager = notebook->GetFolderManager();
    if (!folder_manager) {
      ctx->last_error = "FolderManager not available";
      return VXCORE_ERR_INVALID_STATE;
    }

    std::string folder_id;
    std::string parent = parent_path ? parent_path : ".";

    VxCoreError error = folder_manager->CreateFolder(parent, folder_name, folder_id);
    if (error != VXCORE_OK) {
      return error;
    }

    *out_folder_id = vxcore_strdup(folder_id.c_str());
    return VXCORE_OK;
  } catch (const std::exception &e) {
    ctx->last_error = std::string("Exception: ") + e.what();
    return VXCORE_ERR_UNKNOWN;
  }
}

VXCORE_API VxCoreError vxcore_folder_create_path(VxCoreContextHandle context,
                                                 const char *notebook_id, const char *folder_path,
                                                 char **out_folder_id) {
  if (!context || !notebook_id || !folder_path || !out_folder_id) {
    return VXCORE_ERR_INVALID_PARAM;
  }

  vxcore::VxCoreContext *ctx = reinterpret_cast<vxcore::VxCoreContext *>(context);

  try {
    vxcore::Notebook *notebook = ctx->notebook_manager->GetNotebook(notebook_id);
    if (!notebook) {
      ctx->last_error = "Notebook not found";
      return VXCORE_ERR_NOT_FOUND;
    }

    std::string folder_id;
    VxCoreError error = notebook->GetFolderManager()->CreateFolderPath(folder_path, folder_id);
    if (error != VXCORE_OK) {
      return error;
    }

    *out_folder_id = vxcore_strdup(folder_id.c_str());
    return VXCORE_OK;
  } catch (const std::exception &e) {
    ctx->last_error = std::string("Exception: ") + e.what();
    return VXCORE_ERR_UNKNOWN;
  }
}

VXCORE_API VxCoreError vxcore_file_create(VxCoreContextHandle context, const char *notebook_id,
                                          const char *folder_path, const char *file_name,
                                          char **out_file_id) {
  if (!context || !notebook_id || !folder_path || !file_name || !out_file_id) {
    return VXCORE_ERR_INVALID_PARAM;
  }

  vxcore::VxCoreContext *ctx = reinterpret_cast<vxcore::VxCoreContext *>(context);

  try {
    vxcore::Notebook *notebook = ctx->notebook_manager->GetNotebook(notebook_id);
    if (!notebook) {
      ctx->last_error = "Notebook not found";
      return VXCORE_ERR_NOT_FOUND;
    }

    vxcore::FolderManager *folder_manager = notebook->GetFolderManager();
    if (!folder_manager) {
      ctx->last_error = "FolderManager not available";
      return VXCORE_ERR_INVALID_STATE;
    }
    std::string file_id;

    VxCoreError error = folder_manager->CreateFile(folder_path, file_name, file_id);
    if (error != VXCORE_OK) {
      return error;
    }

    *out_file_id = vxcore_strdup(file_id.c_str());
    return VXCORE_OK;
  } catch (const std::exception &e) {
    ctx->last_error = std::string("Exception: ") + e.what();
    return VXCORE_ERR_UNKNOWN;
  }
}

VXCORE_API VxCoreError vxcore_file_update_tags(VxCoreContextHandle context, const char *notebook_id,
                                               const char *file_path, const char *tags_json) {
  if (!context || !notebook_id || !file_path || !tags_json) {
    return VXCORE_ERR_INVALID_PARAM;
  }

  vxcore::VxCoreContext *ctx = reinterpret_cast<vxcore::VxCoreContext *>(context);

  try {
    vxcore::Notebook *notebook = ctx->notebook_manager->GetNotebook(notebook_id);
    if (!notebook) {
      ctx->last_error = "Notebook not found";
      return VXCORE_ERR_NOT_FOUND;
    }

    vxcore::FolderManager *folder_manager = notebook->GetFolderManager();
    if (!folder_manager) {
      ctx->last_error = "FolderManager not available";
      return VXCORE_ERR_INVALID_STATE;
    }
    return folder_manager->UpdateFileTags(file_path, tags_json);
  } catch (const std::exception &e) {
    ctx->last_error = std::string("Exception: ") + e.what();
    return VXCORE_ERR_UNKNOWN;
  }
}

VXCORE_API VxCoreError vxcore_file_tag(VxCoreContextHandle context, const char *notebook_id,
                                       const char *file_path, const char *tag_name) {
  if (!context || !notebook_id || !file_path || !tag_name) {
    return VXCORE_ERR_INVALID_PARAM;
  }

  vxcore::VxCoreContext *ctx = reinterpret_cast<vxcore::VxCoreContext *>(context);

  try {
    vxcore::Notebook *notebook = ctx->notebook_manager->GetNotebook(notebook_id);
    if (!notebook) {
      ctx->last_error = "Notebook not found";
      return VXCORE_ERR_NOT_FOUND;
    }

    vxcore::FolderManager *folder_manager = notebook->GetFolderManager();
    if (!folder_manager) {
      ctx->last_error = "FolderManager not available";
      return VXCORE_ERR_INVALID_STATE;
    }
    return folder_manager->TagFile(file_path, tag_name);
  } catch (const std::exception &e) {
    ctx->last_error = std::string("Exception: ") + e.what();
    return VXCORE_ERR_UNKNOWN;
  }
}

VXCORE_API VxCoreError vxcore_file_untag(VxCoreContextHandle context, const char *notebook_id,
                                         const char *file_path, const char *tag_name) {
  if (!context || !notebook_id || !file_path || !tag_name) {
    return VXCORE_ERR_INVALID_PARAM;
  }

  vxcore::VxCoreContext *ctx = reinterpret_cast<vxcore::VxCoreContext *>(context);

  try {
    vxcore::Notebook *notebook = ctx->notebook_manager->GetNotebook(notebook_id);
    if (!notebook) {
      ctx->last_error = "Notebook not found";
      return VXCORE_ERR_NOT_FOUND;
    }

    vxcore::FolderManager *folder_manager = notebook->GetFolderManager();
    if (!folder_manager) {
      ctx->last_error = "FolderManager not available";
      return VXCORE_ERR_INVALID_STATE;
    }
    return folder_manager->UntagFile(file_path, tag_name);
  } catch (const std::exception &e) {
    ctx->last_error = std::string("Exception: ") + e.what();
    return VXCORE_ERR_UNKNOWN;
  }
}

VXCORE_API VxCoreError vxcore_folder_list_children(VxCoreContextHandle context,
                                                   const char *notebook_id, const char *folder_path,
                                                   char **out_children_json) {
  if (!context || !notebook_id || !out_children_json) {
    return VXCORE_ERR_INVALID_PARAM;
  }

  vxcore::VxCoreContext *ctx = reinterpret_cast<vxcore::VxCoreContext *>(context);

  try {
    vxcore::Notebook *notebook = ctx->notebook_manager->GetNotebook(notebook_id);
    if (!notebook) {
      ctx->last_error = "Notebook not found";
      return VXCORE_ERR_NOT_FOUND;
    }

    vxcore::FolderManager *folder_manager = notebook->GetFolderManager();
    if (!folder_manager) {
      ctx->last_error = "FolderManager not available";
      return VXCORE_ERR_INVALID_STATE;
    }

    std::string path = folder_path ? folder_path : ".";
    vxcore::FolderManager::FolderContents contents;

    VxCoreError error = folder_manager->ListFolderContents(path, true, contents);
    if (error != VXCORE_OK) {
      return error;
    }

    // Build JSON response
    nlohmann::json result;
    nlohmann::json files_json = nlohmann::json::array();
    nlohmann::json folders_json = nlohmann::json::array();

    for (const auto &file : contents.files) {
      files_json.push_back(file.ToJsonWithType());
    }

    for (const auto &folder : contents.folders) {
      folders_json.push_back(folder.ToJson());
    }

    result["files"] = files_json;
    result["folders"] = folders_json;

    *out_children_json = vxcore_strdup(result.dump().c_str());
    return VXCORE_OK;
  } catch (const std::exception &e) {
    ctx->last_error = std::string("Exception: ") + e.what();
    return VXCORE_ERR_UNKNOWN;
  }
}

VXCORE_API VxCoreError vxcore_folder_list_external(VxCoreContextHandle context,
                                                   const char *notebook_id, const char *folder_path,
                                                   char **out_external_json) {
  if (!context || !notebook_id || !out_external_json) {
    return VXCORE_ERR_INVALID_PARAM;
  }

  vxcore::VxCoreContext *ctx = reinterpret_cast<vxcore::VxCoreContext *>(context);

  try {
    vxcore::Notebook *notebook = ctx->notebook_manager->GetNotebook(notebook_id);
    if (!notebook) {
      ctx->last_error = "Notebook not found";
      return VXCORE_ERR_NOT_FOUND;
    }

    vxcore::FolderManager *folder_manager = notebook->GetFolderManager();
    if (!folder_manager) {
      ctx->last_error = "FolderManager not available";
      return VXCORE_ERR_INVALID_STATE;
    }

    std::string path = folder_path ? folder_path : ".";
    vxcore::FolderManager::FolderContents contents;

    VxCoreError error = folder_manager->ListExternalNodes(path, contents);
    if (error != VXCORE_OK) {
      return error;
    }

    // Build JSON response
    nlohmann::json result;
    nlohmann::json files_json = nlohmann::json::array();
    nlohmann::json folders_json = nlohmann::json::array();

    for (const auto &file : contents.files) {
      // External files only have name (no ID)
      nlohmann::json file_json;
      file_json["name"] = file.name;
      file_json["type"] = "file";
      files_json.push_back(file_json);
    }

    for (const auto &folder : contents.folders) {
      // External folders only have name (no ID)
      nlohmann::json folder_json;
      folder_json["name"] = folder.name;
      folder_json["type"] = "folder";
      folders_json.push_back(folder_json);
    }

    result["files"] = files_json;
    result["folders"] = folders_json;

    *out_external_json = vxcore_strdup(result.dump().c_str());
    return VXCORE_OK;
  } catch (const std::exception &e) {
    ctx->last_error = std::string("Exception: ") + e.what();
    return VXCORE_ERR_UNKNOWN;
  }
}

VXCORE_API VxCoreError vxcore_file_import(VxCoreContextHandle context, const char *notebook_id,
                                          const char *folder_path, const char *external_file_path,
                                          char **out_file_id) {
  if (!context || !notebook_id || !folder_path || !external_file_path || !out_file_id) {
    return VXCORE_ERR_INVALID_PARAM;
  }

  vxcore::VxCoreContext *ctx = reinterpret_cast<vxcore::VxCoreContext *>(context);

  try {
    vxcore::Notebook *notebook = ctx->notebook_manager->GetNotebook(notebook_id);
    if (!notebook) {
      ctx->last_error = "Notebook not found";
      return VXCORE_ERR_NOT_FOUND;
    }

    vxcore::FolderManager *folder_manager = notebook->GetFolderManager();
    if (!folder_manager) {
      ctx->last_error = "FolderManager not available";
      return VXCORE_ERR_INVALID_STATE;
    }

    std::string file_id;
    VxCoreError error = folder_manager->ImportFile(folder_path, external_file_path, file_id);
    if (error != VXCORE_OK) {
      return error;
    }

    *out_file_id = vxcore_strdup(file_id.c_str());
    return VXCORE_OK;
  } catch (const std::exception &e) {
    ctx->last_error = std::string("Exception: ") + e.what();
    return VXCORE_ERR_UNKNOWN;
  }
}

VXCORE_API VxCoreError vxcore_folder_import(VxCoreContextHandle context, const char *notebook_id,
                                            const char *dest_folder_path,
                                            const char *external_folder_path,
                                            const char *suffix_allowlist, char **out_folder_id) {
  if (!context || !notebook_id || !dest_folder_path || !external_folder_path || !out_folder_id) {
    return VXCORE_ERR_NULL_POINTER;
  }

  vxcore::VxCoreContext *ctx = reinterpret_cast<vxcore::VxCoreContext *>(context);

  try {
    vxcore::Notebook *notebook = ctx->notebook_manager->GetNotebook(notebook_id);
    if (!notebook) {
      ctx->last_error = "Notebook not found";
      return VXCORE_ERR_NOT_FOUND;
    }

    vxcore::FolderManager *folder_manager = notebook->GetFolderManager();
    if (!folder_manager) {
      ctx->last_error = "FolderManager not available";
      return VXCORE_ERR_INVALID_STATE;
    }

    std::string folder_id;
    std::string allowlist = suffix_allowlist ? suffix_allowlist : "";
    VxCoreError error =
        folder_manager->ImportFolder(dest_folder_path, external_folder_path, allowlist, folder_id);
    if (error != VXCORE_OK) {
      return error;
    }

    *out_folder_id = vxcore_strdup(folder_id.c_str());
    return VXCORE_OK;
  } catch (const std::exception &e) {
    ctx->last_error = std::string("Exception: ") + e.what();
    return VXCORE_ERR_UNKNOWN;
  }
}

VXCORE_API VxCoreError vxcore_node_get_path_by_id(VxCoreContextHandle context,
                                                  const char *notebook_id, const char *node_id,
                                                  char **out_path) {
  if (!context || !notebook_id || !node_id || !out_path) {
    return VXCORE_ERR_INVALID_PARAM;
  }

  *out_path = nullptr;  // Initialize to nullptr

  vxcore::VxCoreContext *ctx = reinterpret_cast<vxcore::VxCoreContext *>(context);

  try {
    vxcore::Notebook *notebook = ctx->notebook_manager->GetNotebook(notebook_id);
    if (!notebook) {
      ctx->last_error = "Notebook not found";
      return VXCORE_ERR_NOT_FOUND;
    }

    vxcore::MetadataStore *store = notebook->GetMetadataStore();
    if (!store) {
      ctx->last_error = "MetadataStore not available";
      return VXCORE_ERR_INVALID_STATE;
    }

    std::string path = store->GetNodePathById(node_id);
    if (path.empty()) {
      ctx->last_error = "Node not found: " + std::string(node_id);
      return VXCORE_ERR_NOT_FOUND;
    }

    *out_path = vxcore_strdup(path.c_str());
    return VXCORE_OK;
  } catch (const std::exception &e) {
    ctx->last_error = std::string("Exception: ") + e.what();
    return VXCORE_ERR_UNKNOWN;
  }
}

VXCORE_API VxCoreError vxcore_node_resolve_by_id(VxCoreContextHandle context, const char *node_id,
                                                 char **out_notebook_id, char **out_relative_path) {
  if (!context || !node_id || !out_notebook_id || !out_relative_path) {
    return VXCORE_ERR_NULL_POINTER;
  }

  *out_notebook_id = nullptr;
  *out_relative_path = nullptr;

  auto *ctx = reinterpret_cast<vxcore::VxCoreContext *>(context);

  try {
    std::string notebook_id;
    std::string relative_path;
    VxCoreError err = ctx->notebook_manager->ResolveNodeById(node_id, notebook_id, relative_path);

    if (err != VXCORE_OK) {
      ctx->last_error = "Node not found in any open notebook";
      return err;
    }

    char *id_copy = vxcore_strdup(notebook_id.c_str());
    if (!id_copy) {
      return VXCORE_ERR_OUT_OF_MEMORY;
    }

    char *path_copy = vxcore_strdup(relative_path.c_str());
    if (!path_copy) {
      free(id_copy);
      return VXCORE_ERR_OUT_OF_MEMORY;
    }

    *out_notebook_id = id_copy;
    *out_relative_path = path_copy;
    return VXCORE_OK;
  } catch (...) {
    ctx->last_error = "Unknown error resolving node by ID";
    return VXCORE_ERR_UNKNOWN;
  }
}

VXCORE_API VxCoreError vxcore_folder_get_available_name(VxCoreContextHandle context,
                                                        const char *notebook_id,
                                                        const char *folder_path,
                                                        const char *new_name,
                                                        char **out_available_name) {
  if (!context || !notebook_id || !new_name || !out_available_name) {
    return VXCORE_ERR_INVALID_PARAM;
  }

  *out_available_name = nullptr;

  vxcore::VxCoreContext *ctx = reinterpret_cast<vxcore::VxCoreContext *>(context);

  try {
    vxcore::Notebook *notebook = ctx->notebook_manager->GetNotebook(notebook_id);
    if (!notebook) {
      ctx->last_error = "Notebook not found";
      return VXCORE_ERR_NOT_FOUND;
    }

    vxcore::FolderManager *folder_manager = notebook->GetFolderManager();
    if (!folder_manager) {
      ctx->last_error = "FolderManager not available";
      return VXCORE_ERR_INVALID_STATE;
    }

    std::string path = folder_path ? folder_path : ".";
    std::string available_name;

    VxCoreError error = folder_manager->GetAvailableName(path, new_name, available_name);
    if (error != VXCORE_OK) {
      return error;
    }

    *out_available_name = vxcore_strdup(available_name.c_str());
    return VXCORE_OK;
  } catch (const std::exception &e) {
    ctx->last_error = std::string("Exception: ") + e.what();
    return VXCORE_ERR_UNKNOWN;
  }
}

VXCORE_API VxCoreError vxcore_file_peek(VxCoreContextHandle context, const char *notebook_id,
                                        const char *file_path, size_t max_bytes,
                                        char **out_content) {
  if (!context || !notebook_id || !file_path || !out_content) {
    return VXCORE_ERR_INVALID_PARAM;
  }

  *out_content = nullptr;

  vxcore::VxCoreContext *ctx = reinterpret_cast<vxcore::VxCoreContext *>(context);

  try {
    vxcore::Notebook *notebook = ctx->notebook_manager->GetNotebook(notebook_id);
    if (!notebook) {
      ctx->last_error = "Notebook not found";
      return VXCORE_ERR_NOT_FOUND;
    }

    std::filesystem::path abs_path = std::filesystem::path(notebook->GetRootFolder()) / file_path;
    if (!std::filesystem::exists(abs_path) || !std::filesystem::is_regular_file(abs_path)) {
      ctx->last_error = "File not found: " + std::string(file_path);
      return VXCORE_ERR_NOT_FOUND;
    }

    std::string content;
    VxCoreError error = vxcore::ReadFileHead(abs_path, max_bytes, content);
    if (error != VXCORE_OK) {
      ctx->last_error = "Failed to read file";
      return error;
    }

    *out_content = vxcore_strdup(content.c_str());
    return VXCORE_OK;
  } catch (const std::exception &e) {
    ctx->last_error = std::string("Exception: ") + e.what();
    return VXCORE_ERR_UNKNOWN;
  }
}