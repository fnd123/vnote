#include "api/api_utils.h"
#include "core/buffer_manager.h"
#include "core/context.h"
#include "core/folder_manager.h"
#include "core/notebook_manager.h"
#include "vxcore/vxcore.h"
#include "vxcore/vxcore_types.h"
#include <nlohmann/json.hpp>

// Helper function to detect whether a path refers to a file or folder
static VxCoreError DetectNodeType(vxcore::Notebook *notebook, const std::string &path,
                                  vxcore::NodeType &out_type) {
  vxcore::FolderManager *folder_manager = notebook->GetFolderManager();
  if (!folder_manager) {
    return VXCORE_ERR_INVALID_STATE;
  }

  // Try file first (files are more common)
  std::string file_info_json;
  VxCoreError err = folder_manager->GetFileInfo(path, file_info_json);
  if (err == VXCORE_OK) {
    out_type = vxcore::NodeType::File;
    return VXCORE_OK;
  }

  // Try folder
  std::string folder_config_json;
  err = folder_manager->GetFolderConfig(path, folder_config_json);
  if (err == VXCORE_OK) {
    out_type = vxcore::NodeType::Folder;
    return VXCORE_OK;
  }

  return VXCORE_ERR_NOT_FOUND;
}

VXCORE_API VxCoreError vxcore_node_get_config(VxCoreContextHandle context, const char *notebook_id,
                                              const char *node_path, char **out_config_json) {
  if (!context || !notebook_id || !node_path || !out_config_json) {
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

    // Detect node type
    vxcore::NodeType node_type;
    VxCoreError err = DetectNodeType(notebook, node_path, node_type);
    if (err != VXCORE_OK) {
      return err;
    }

    // Get config with type information
    std::string config_json;
    if (node_type == vxcore::NodeType::File) {
      err = folder_manager->GetFileInfo(node_path, config_json);
      if (err != VXCORE_OK) {
        return err;
      }
      // Parse and add type field
      nlohmann::json j = nlohmann::json::parse(config_json);
      j["type"] = "file";
      config_json = j.dump();
    } else {
      err = folder_manager->GetFolderConfig(node_path, config_json);
      if (err != VXCORE_OK) {
        return err;
      }
      // Parse and add type field
      nlohmann::json j = nlohmann::json::parse(config_json);
      j["type"] = "folder";
      config_json = j.dump();
    }

    *out_config_json = vxcore_strdup(config_json.c_str());
    return VXCORE_OK;
  } catch (const std::exception &e) {
    ctx->last_error = std::string("Exception: ") + e.what();
    return VXCORE_ERR_UNKNOWN;
  }
}

VXCORE_API VxCoreError vxcore_node_delete(VxCoreContextHandle context, const char *notebook_id,
                                          const char *node_path) {
  if (!context || !notebook_id || !node_path) {
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

    // Detect node type
    vxcore::NodeType node_type;
    VxCoreError err = DetectNodeType(notebook, node_path, node_type);
    if (err != VXCORE_OK) {
      return err;
    }

    // Delete based on type
    if (node_type == vxcore::NodeType::File) {
      return folder_manager->DeleteFile(node_path);
    } else {
      return folder_manager->DeleteFolder(node_path);
    }
  } catch (const std::exception &e) {
    ctx->last_error = std::string("Exception: ") + e.what();
    return VXCORE_ERR_UNKNOWN;
  }
}

VXCORE_API VxCoreError vxcore_node_rename(VxCoreContextHandle context, const char *notebook_id,
                                          const char *node_path, const char *new_name) {
  if (!context || !notebook_id || !node_path || !new_name) {
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

    // Detect node type
    vxcore::NodeType node_type;
    VxCoreError err = DetectNodeType(notebook, node_path, node_type);
    if (err != VXCORE_OK) {
      return err;
    }

    // Rename based on type
    if (node_type == vxcore::NodeType::File) {
      err = folder_manager->RenameFile(node_path, new_name);
    } else {
      err = folder_manager->RenameFolder(node_path, new_name);
    }

    if (err != VXCORE_OK) {
      return err;
    }

    // Update open buffer paths to reflect the rename.
    if (ctx->buffer_manager) {
      bool is_folder = (node_type == vxcore::NodeType::Folder);
      std::string old_path = node_path;

      // Derive the new path: replace the last component with new_name.
      std::string new_path;
      size_t last_slash = old_path.rfind('/');
      if (last_slash != std::string::npos) {
        new_path = old_path.substr(0, last_slash + 1) + new_name;
      } else {
        new_path = new_name;
      }

      ctx->buffer_manager->UpdatePathsAfterRename(notebook_id, old_path, new_path, is_folder);
    }

    return VXCORE_OK;
  } catch (const std::exception &e) {
    ctx->last_error = std::string("Exception: ") + e.what();
    return VXCORE_ERR_UNKNOWN;
  }
}

VXCORE_API VxCoreError vxcore_node_move(VxCoreContextHandle context, const char *notebook_id,
                                        const char *src_path, const char *dest_parent_path) {
  if (!context || !notebook_id || !src_path || !dest_parent_path) {
    return VXCORE_ERR_INVALID_PARAM;
  }

  // Convert empty dest_parent_path to "." for root folder
  std::string dest_path = dest_parent_path;
  if (dest_path.empty()) {
    dest_path = ".";
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

    // Detect node type
    vxcore::NodeType node_type;
    VxCoreError err = DetectNodeType(notebook, src_path, node_type);
    if (err != VXCORE_OK) {
      return err;
    }

    // Move based on type
    if (node_type == vxcore::NodeType::File) {
      return folder_manager->MoveFile(src_path, dest_path);
    } else {
      return folder_manager->MoveFolder(src_path, dest_path);
    }
  } catch (const std::exception &e) {
    ctx->last_error = std::string("Exception: ") + e.what();
    return VXCORE_ERR_UNKNOWN;
  }
}

VXCORE_API VxCoreError vxcore_node_copy(VxCoreContextHandle context, const char *notebook_id,
                                        const char *src_path, const char *dest_parent_path,
                                        const char *new_name, char **out_node_id) {
  if (!context || !notebook_id || !src_path || !dest_parent_path || !out_node_id) {
    return VXCORE_ERR_INVALID_PARAM;
  }

  // Convert empty dest_parent_path to "." for root folder
  std::string dest_path = dest_parent_path;
  if (dest_path.empty()) {
    dest_path = ".";
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

    // Detect node type
    vxcore::NodeType node_type;
    VxCoreError err = DetectNodeType(notebook, src_path, node_type);
    if (err != VXCORE_OK) {
      return err;
    }

    std::string target_name = new_name ? new_name : "";
    std::string node_id;

    // Copy based on type
    if (node_type == vxcore::NodeType::File) {
      err = folder_manager->CopyFile(src_path, dest_path, target_name, node_id);
    } else {
      err = folder_manager->CopyFolder(src_path, dest_path, target_name, node_id);
    }

    if (err != VXCORE_OK) {
      return err;
    }

    *out_node_id = vxcore_strdup(node_id.c_str());
    return VXCORE_OK;
  } catch (const std::exception &e) {
    ctx->last_error = std::string("Exception: ") + e.what();
    return VXCORE_ERR_UNKNOWN;
  }
}

VXCORE_API VxCoreError vxcore_node_get_metadata(VxCoreContextHandle context,
                                                const char *notebook_id, const char *node_path,
                                                char **out_metadata_json) {
  if (!context || !notebook_id || !node_path || !out_metadata_json) {
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

    // Detect node type
    vxcore::NodeType node_type;
    VxCoreError err = DetectNodeType(notebook, node_path, node_type);
    if (err != VXCORE_OK) {
      return err;
    }

    std::string metadata_json;

    // Get metadata based on type
    if (node_type == vxcore::NodeType::File) {
      err = folder_manager->GetFileMetadata(node_path, metadata_json);
    } else {
      err = folder_manager->GetFolderMetadata(node_path, metadata_json);
    }

    if (err != VXCORE_OK) {
      return err;
    }

    *out_metadata_json = vxcore_strdup(metadata_json.c_str());
    return VXCORE_OK;
  } catch (const std::exception &e) {
    ctx->last_error = std::string("Exception: ") + e.what();
    return VXCORE_ERR_UNKNOWN;
  }
}

VXCORE_API VxCoreError vxcore_node_update_metadata(VxCoreContextHandle context,
                                                   const char *notebook_id, const char *node_path,
                                                   const char *metadata_json) {
  if (!context || !notebook_id || !node_path || !metadata_json) {
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

    // Detect node type
    vxcore::NodeType node_type;
    VxCoreError err = DetectNodeType(notebook, node_path, node_type);
    if (err != VXCORE_OK) {
      return err;
    }

    // Update metadata based on type
    if (node_type == vxcore::NodeType::File) {
      return folder_manager->UpdateFileMetadata(node_path, metadata_json);
    } else {
      return folder_manager->UpdateFolderMetadata(node_path, metadata_json);
    }
  } catch (const std::exception &e) {
    ctx->last_error = std::string("Exception: ") + e.what();
    return VXCORE_ERR_UNKNOWN;
  }
}

VXCORE_API VxCoreError vxcore_node_index(VxCoreContextHandle context, const char *notebook_id,
                                         const char *node_path) {
  if (!context || !notebook_id || !node_path) {
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

    return folder_manager->IndexNode(node_path);
  } catch (const std::exception &e) {
    ctx->last_error = std::string("Exception: ") + e.what();
    return VXCORE_ERR_UNKNOWN;
  }
}

VXCORE_API VxCoreError vxcore_node_unindex(VxCoreContextHandle context, const char *notebook_id,
                                           const char *node_path) {
  if (!context || !notebook_id || !node_path) {
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

    return folder_manager->UnindexNode(node_path);
  } catch (const std::exception &e) {
    ctx->last_error = std::string("Exception: ") + e.what();
    return VXCORE_ERR_UNKNOWN;
  }
}