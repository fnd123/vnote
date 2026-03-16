#include <stdlib.h>
#include <string.h>

#include <nlohmann/json.hpp>

#include "api/api_utils.h"
#include "core/buffer_manager.h"
#include "core/buffer_provider.h"
#include "core/context.h"
#include "utils/base64.h"
#include "vxcore/vxcore.h"

VXCORE_API VxCoreError vxcore_buffer_open(VxCoreContextHandle context, const char *notebook_id,
                                          const char *file_path, char **out_id) {
  if (!context || !file_path || !out_id) {
    return VXCORE_ERR_NULL_POINTER;
  }
  // notebook_id can be NULL for external files

  auto *ctx = reinterpret_cast<vxcore::VxCoreContext *>(context);
  if (!ctx->buffer_manager) {
    return VXCORE_ERR_NOT_INITIALIZED;
  }

  try {
    std::string nb_id = notebook_id ? notebook_id : "";
    std::string id = ctx->buffer_manager->OpenBuffer(nb_id, file_path);
    *out_id = vxcore_strdup(id.c_str());
    vxcore::PersistSession(ctx);
    return VXCORE_OK;
  } catch (const std::exception &e) {
    ctx->last_error = std::string("Exception: ") + e.what();
    return VXCORE_ERR_UNKNOWN;
  }
}

VXCORE_API VxCoreError vxcore_buffer_close(VxCoreContextHandle context, const char *id) {
  if (!context || !id) {
    return VXCORE_ERR_NULL_POINTER;
  }

  auto *ctx = reinterpret_cast<vxcore::VxCoreContext *>(context);
  if (!ctx->buffer_manager) {
    return VXCORE_ERR_NOT_INITIALIZED;
  }

  try {
    bool success = ctx->buffer_manager->CloseBuffer(id);
    if (!success) {
      ctx->last_error = "Buffer not found";
      return VXCORE_ERR_BUFFER_NOT_FOUND;
    }
    vxcore::PersistSession(ctx);
    return VXCORE_OK;
  } catch (const std::exception &e) {
    ctx->last_error = std::string("Exception: ") + e.what();
    return VXCORE_ERR_UNKNOWN;
  }
}

VXCORE_API VxCoreError vxcore_buffer_get(VxCoreContextHandle context, const char *id,
                                         char **out_json) {
  if (!context || !id || !out_json) {
    return VXCORE_ERR_NULL_POINTER;
  }

  auto *ctx = reinterpret_cast<vxcore::VxCoreContext *>(context);
  if (!ctx->buffer_manager) {
    return VXCORE_ERR_NOT_INITIALIZED;
  }

  try {
    auto *buffer = ctx->buffer_manager->GetBuffer(id);
    if (!buffer) {
      ctx->last_error = "Buffer not found";
      return VXCORE_ERR_BUFFER_NOT_FOUND;
    }

    // Build JSON manually (Buffer is now a class, not struct with ToJson)
    nlohmann::json json = nlohmann::json::object();
    json["id"] = buffer->GetId();
    json["notebookId"] = buffer->GetNotebookId();
    json["filePath"] = buffer->GetFilePath();
    json["revision"] = buffer->GetRevision();
    json["modified"] = buffer->IsModified();
    json["state"] = static_cast<int>(buffer->GetState());
    json["metadata"] = buffer->GetMetadata();
    json["lastModifiedTime"] = buffer->GetLastModifiedTime();
    json["contentLoaded"] = buffer->IsContentLoaded();
    // Note: content field omitted (retrieved separately via get_content APIs)

    std::string json_str = json.dump();
    *out_json = vxcore_strdup(json_str.c_str());
    return VXCORE_OK;
  } catch (const nlohmann::json::exception &e) {
    ctx->last_error = std::string("JSON error: ") + e.what();
    return VXCORE_ERR_JSON_SERIALIZE;
  } catch (const std::exception &e) {
    ctx->last_error = std::string("Exception: ") + e.what();
    return VXCORE_ERR_UNKNOWN;
  }
}

VXCORE_API VxCoreError vxcore_buffer_list(VxCoreContextHandle context, char **out_json) {
  if (!context || !out_json) {
    return VXCORE_ERR_NULL_POINTER;
  }

  auto *ctx = reinterpret_cast<vxcore::VxCoreContext *>(context);
  if (!ctx->buffer_manager) {
    return VXCORE_ERR_NOT_INITIALIZED;
  }

  try {
    auto buffers = ctx->buffer_manager->ListBuffers();
    nlohmann::json json = nlohmann::json::array();
    for (const auto *buf : buffers) {
      nlohmann::json buf_json = nlohmann::json::object();
      buf_json["id"] = buf->GetId();
      buf_json["notebookId"] = buf->GetNotebookId();
      buf_json["filePath"] = buf->GetFilePath();
      buf_json["revision"] = buf->GetRevision();
      buf_json["modified"] = buf->IsModified();
      buf_json["state"] = static_cast<int>(buf->GetState());
      buf_json["metadata"] = buf->GetMetadata();
      buf_json["lastModifiedTime"] = buf->GetLastModifiedTime();
      buf_json["contentLoaded"] = buf->IsContentLoaded();
      json.push_back(buf_json);
    }
    std::string json_str = json.dump();
    *out_json = vxcore_strdup(json_str.c_str());
    return VXCORE_OK;
  } catch (const nlohmann::json::exception &e) {
    ctx->last_error = std::string("JSON error: ") + e.what();
    return VXCORE_ERR_JSON_SERIALIZE;
  } catch (const std::exception &e) {
    ctx->last_error = std::string("Exception: ") + e.what();
    return VXCORE_ERR_UNKNOWN;
  }
}

VXCORE_API VxCoreError vxcore_buffer_save(VxCoreContextHandle context, const char *id) {
  if (!context || !id) {
    return VXCORE_ERR_NULL_POINTER;
  }

  auto *ctx = reinterpret_cast<vxcore::VxCoreContext *>(context);
  if (!ctx->buffer_manager) {
    return VXCORE_ERR_NOT_INITIALIZED;
  }

  try {
    VxCoreError err = ctx->buffer_manager->SaveBuffer(id);
    if (err != VXCORE_OK) {
      ctx->last_error = "Failed to save buffer";
    }
    return err;
  } catch (const std::exception &e) {
    ctx->last_error = std::string("Exception: ") + e.what();
    return VXCORE_ERR_UNKNOWN;
  }
}

VXCORE_API VxCoreError vxcore_buffer_reload(VxCoreContextHandle context, const char *id) {
  if (!context || !id) {
    return VXCORE_ERR_NULL_POINTER;
  }

  auto *ctx = reinterpret_cast<vxcore::VxCoreContext *>(context);
  if (!ctx->buffer_manager) {
    return VXCORE_ERR_NOT_INITIALIZED;
  }

  try {
    VxCoreError err = ctx->buffer_manager->ReloadBuffer(id);
    if (err != VXCORE_OK) {
      ctx->last_error = "Failed to reload buffer";
    }
    return err;
  } catch (const std::exception &e) {
    ctx->last_error = std::string("Exception: ") + e.what();
    return VXCORE_ERR_UNKNOWN;
  }
}

VXCORE_API VxCoreError vxcore_buffer_get_content(VxCoreContextHandle context, const char *id,
                                                 char **out_json) {
  if (!context || !id || !out_json) {
    return VXCORE_ERR_NULL_POINTER;
  }

  auto *ctx = reinterpret_cast<vxcore::VxCoreContext *>(context);
  if (!ctx->buffer_manager) {
    return VXCORE_ERR_NOT_INITIALIZED;
  }

  try {
    const void *data;
    size_t size;
    VxCoreError err = ctx->buffer_manager->GetBufferContent(id, &data, &size);
    if (err != VXCORE_OK) {
      ctx->last_error = "Failed to get buffer content";
      return err;
    }

    // Convert to base64 string for JSON transport
    std::string encoded = vxcore::Base64Encode(static_cast<const uint8_t *>(data), size);

    nlohmann::json json = nlohmann::json::object();
    json["content"] = encoded;
    json["size"] = size;
    std::string json_str = json.dump();
    *out_json = vxcore_strdup(json_str.c_str());
    return VXCORE_OK;
  } catch (const nlohmann::json::exception &e) {
    ctx->last_error = std::string("JSON error: ") + e.what();
    return VXCORE_ERR_JSON_SERIALIZE;
  } catch (const std::exception &e) {
    ctx->last_error = std::string("Exception: ") + e.what();
    return VXCORE_ERR_UNKNOWN;
  }
}

VXCORE_API VxCoreError vxcore_buffer_set_content(VxCoreContextHandle context, const char *id,
                                                 const char *content_json) {
  if (!context || !id || !content_json) {
    return VXCORE_ERR_NULL_POINTER;
  }

  auto *ctx = reinterpret_cast<vxcore::VxCoreContext *>(context);
  if (!ctx->buffer_manager) {
    return VXCORE_ERR_NOT_INITIALIZED;
  }

  try {
    nlohmann::json json = nlohmann::json::parse(content_json);
    if (!json.contains("content") || !json["content"].is_string()) {
      ctx->last_error = "Invalid content JSON: missing 'content' field";
      return VXCORE_ERR_JSON_PARSE;
    }

    std::string encoded = json["content"].get<std::string>();
    std::vector<uint8_t> data = vxcore::Base64Decode(encoded);
    if (data.empty() && !encoded.empty()) {
      ctx->last_error = "Invalid base64 content";
      return VXCORE_ERR_JSON_PARSE;
    }

    VxCoreError err = ctx->buffer_manager->SetBufferContent(id, data.data(), data.size());
    if (err != VXCORE_OK) {
      ctx->last_error = "Failed to set buffer content";
    }
    return err;
  } catch (const nlohmann::json::exception &e) {
    ctx->last_error = std::string("JSON error: ") + e.what();
    return VXCORE_ERR_JSON_PARSE;
  } catch (const std::exception &e) {
    ctx->last_error = std::string("Exception: ") + e.what();
    return VXCORE_ERR_UNKNOWN;
  }
}

VXCORE_API VxCoreError vxcore_buffer_get_content_raw(VxCoreContextHandle context, const char *id,
                                                     const void **out_data, size_t *out_size) {
  if (!context || !id || !out_data || !out_size) {
    return VXCORE_ERR_NULL_POINTER;
  }

  auto *ctx = reinterpret_cast<vxcore::VxCoreContext *>(context);
  if (!ctx->buffer_manager) {
    return VXCORE_ERR_NOT_INITIALIZED;
  }

  try {
    VxCoreError err = ctx->buffer_manager->GetBufferContent(id, out_data, out_size);
    if (err != VXCORE_OK) {
      ctx->last_error = "Failed to get buffer content";
    }
    return err;
  } catch (const std::exception &e) {
    ctx->last_error = std::string("Exception: ") + e.what();
    return VXCORE_ERR_UNKNOWN;
  }
}

VXCORE_API VxCoreError vxcore_buffer_set_content_raw(VxCoreContextHandle context, const char *id,
                                                     const void *data, size_t size) {
  if (!context || !id || !data) {
    return VXCORE_ERR_NULL_POINTER;
  }

  auto *ctx = reinterpret_cast<vxcore::VxCoreContext *>(context);
  if (!ctx->buffer_manager) {
    return VXCORE_ERR_NOT_INITIALIZED;
  }

  try {
    VxCoreError err = ctx->buffer_manager->SetBufferContent(id, data, size);
    if (err != VXCORE_OK) {
      ctx->last_error = "Failed to set buffer content";
    }
    return err;
  } catch (const std::exception &e) {
    ctx->last_error = std::string("Exception: ") + e.what();
    return VXCORE_ERR_UNKNOWN;
  }
}

VXCORE_API VxCoreError vxcore_buffer_get_state(VxCoreContextHandle context, const char *id,
                                               VxCoreBufferState *out_state) {
  if (!context || !id || !out_state) {
    return VXCORE_ERR_NULL_POINTER;
  }

  auto *ctx = reinterpret_cast<vxcore::VxCoreContext *>(context);
  if (!ctx->buffer_manager) {
    return VXCORE_ERR_NOT_INITIALIZED;
  }

  try {
    auto *buffer = ctx->buffer_manager->GetBuffer(id);
    if (!buffer) {
      ctx->last_error = "Buffer not found";
      return VXCORE_ERR_BUFFER_NOT_FOUND;
    }

    *out_state = buffer->GetState();
    return VXCORE_OK;
  } catch (const std::exception &e) {
    ctx->last_error = std::string("Exception: ") + e.what();
    return VXCORE_ERR_UNKNOWN;
  }
}

VXCORE_API VxCoreError vxcore_buffer_is_modified(VxCoreContextHandle context, const char *id,
                                                 int *out_modified) {
  if (!context || !id || !out_modified) {
    return VXCORE_ERR_NULL_POINTER;
  }

  auto *ctx = reinterpret_cast<vxcore::VxCoreContext *>(context);
  if (!ctx->buffer_manager) {
    return VXCORE_ERR_NOT_INITIALIZED;
  }

  try {
    auto *buffer = ctx->buffer_manager->GetBuffer(id);
    if (!buffer) {
      ctx->last_error = "Buffer not found";
      return VXCORE_ERR_BUFFER_NOT_FOUND;
    }

    *out_modified = buffer->IsModified() ? 1 : 0;
    return VXCORE_OK;
  } catch (const std::exception &e) {
    ctx->last_error = std::string("Exception: ") + e.what();
    return VXCORE_ERR_UNKNOWN;
  }
}

VXCORE_API VxCoreError vxcore_buffer_get_revision(VxCoreContextHandle context, const char *id,
                                                   int *out_revision) {
  if (!context || !id || !out_revision) {
    return VXCORE_ERR_NULL_POINTER;
  }

  auto *ctx = reinterpret_cast<vxcore::VxCoreContext *>(context);
  if (!ctx->buffer_manager) {
    return VXCORE_ERR_NOT_INITIALIZED;
  }

  try {
    auto *buffer = ctx->buffer_manager->GetBuffer(id);
    if (!buffer) {
      ctx->last_error = "Buffer not found";
      return VXCORE_ERR_BUFFER_NOT_FOUND;
    }

    *out_revision = buffer->GetRevision();
    return VXCORE_OK;
  } catch (const std::exception &e) {
    ctx->last_error = std::string("Exception: ") + e.what();
    return VXCORE_ERR_UNKNOWN;
  }
}

// ============ Buffer Backup Operations ============

VXCORE_API VxCoreError vxcore_buffer_write_backup(VxCoreContextHandle context, const char *id) {
  if (!context || !id) {
    return VXCORE_ERR_NULL_POINTER;
  }

  auto *ctx = reinterpret_cast<vxcore::VxCoreContext *>(context);
  if (!ctx->buffer_manager) {
    return VXCORE_ERR_NOT_INITIALIZED;
  }

  try {
    return ctx->buffer_manager->WriteBackup(id);
  } catch (const std::exception &e) {
    ctx->last_error = std::string("Exception: ") + e.what();
    return VXCORE_ERR_UNKNOWN;
  }
}

VXCORE_API VxCoreError vxcore_buffer_has_backup(VxCoreContextHandle context, const char *id,
                                                int *out_has_backup) {
  if (!context || !id || !out_has_backup) {
    return VXCORE_ERR_NULL_POINTER;
  }

  auto *ctx = reinterpret_cast<vxcore::VxCoreContext *>(context);
  if (!ctx->buffer_manager) {
    return VXCORE_ERR_NOT_INITIALIZED;
  }

  try {
    bool has_backup = false;
    VxCoreError err = ctx->buffer_manager->HasBackup(id, has_backup);
    if (err != VXCORE_OK) {
      ctx->last_error = "Failed to check backup";
      return err;
    }

    *out_has_backup = has_backup ? 1 : 0;
    return VXCORE_OK;
  } catch (const std::exception &e) {
    ctx->last_error = std::string("Exception: ") + e.what();
    return VXCORE_ERR_UNKNOWN;
  }
}

VXCORE_API VxCoreError vxcore_buffer_recover_backup(VxCoreContextHandle context, const char *id) {
  if (!context || !id) {
    return VXCORE_ERR_NULL_POINTER;
  }

  auto *ctx = reinterpret_cast<vxcore::VxCoreContext *>(context);
  if (!ctx->buffer_manager) {
    return VXCORE_ERR_NOT_INITIALIZED;
  }

  try {
    return ctx->buffer_manager->RecoverBackup(id);
  } catch (const std::exception &e) {
    ctx->last_error = std::string("Exception: ") + e.what();
    return VXCORE_ERR_UNKNOWN;
  }
}

VXCORE_API VxCoreError vxcore_buffer_discard_backup(VxCoreContextHandle context, const char *id) {
  if (!context || !id) {
    return VXCORE_ERR_NULL_POINTER;
  }

  auto *ctx = reinterpret_cast<vxcore::VxCoreContext *>(context);
  if (!ctx->buffer_manager) {
    return VXCORE_ERR_NOT_INITIALIZED;
  }

  try {
    return ctx->buffer_manager->DiscardBackup(id);
  } catch (const std::exception &e) {
    ctx->last_error = std::string("Exception: ") + e.what();
    return VXCORE_ERR_UNKNOWN;
  }
}

VXCORE_API VxCoreError vxcore_buffer_get_backup_path(VxCoreContextHandle context, const char *id,
                                                     char **out_path) {
  if (!context || !id || !out_path) {
    return VXCORE_ERR_NULL_POINTER;
  }

  auto *ctx = reinterpret_cast<vxcore::VxCoreContext *>(context);
  if (!ctx->buffer_manager) {
    return VXCORE_ERR_NOT_INITIALIZED;
  }

  try {
    std::string path;
    VxCoreError err = ctx->buffer_manager->GetBackupPath(id, path);
    if (err != VXCORE_OK) {
      ctx->last_error = "Failed to get backup path";
      return err;
    }

    *out_path = vxcore_strdup(path.c_str());
    return VXCORE_OK;
  } catch (const std::exception &e) {
    ctx->last_error = std::string("Exception: ") + e.what();
    return VXCORE_ERR_UNKNOWN;
  }
}

// ============ Buffer Asset Operations (Filesystem Only) ============

VXCORE_API VxCoreError vxcore_buffer_insert_asset_raw(VxCoreContextHandle context,
                                                      const char *buffer_id, const char *asset_name,
                                                      const void *data, size_t data_size,
                                                      char **out_relative_path) {
  if (!context || !buffer_id || !asset_name || !data || !out_relative_path) {
    return VXCORE_ERR_NULL_POINTER;
  }

  auto *ctx = reinterpret_cast<vxcore::VxCoreContext *>(context);
  if (!ctx->buffer_manager) {
    return VXCORE_ERR_NOT_INITIALIZED;
  }

  try {
    auto *provider = ctx->buffer_manager->GetProvider(buffer_id);
    if (!provider) {
      ctx->last_error = "Buffer provider not available (unsupported notebook type)";
      return VXCORE_ERR_UNSUPPORTED;
    }

    std::vector<uint8_t> data_vec(static_cast<const uint8_t *>(data),
                                  static_cast<const uint8_t *>(data) + data_size);
    std::string relative_path;
    VxCoreError err = provider->InsertAssetRaw(asset_name, data_vec, relative_path);
    if (err != VXCORE_OK) {
      ctx->last_error = "Failed to insert asset";
      return err;
    }

    *out_relative_path = vxcore_strdup(relative_path.c_str());
    return VXCORE_OK;
  } catch (const std::exception &e) {
    ctx->last_error = std::string("Exception: ") + e.what();
    return VXCORE_ERR_UNKNOWN;
  }
}

VXCORE_API VxCoreError vxcore_buffer_insert_asset(VxCoreContextHandle context,
                                                  const char *buffer_id, const char *source_path,
                                                  char **out_relative_path) {
  if (!context || !buffer_id || !source_path || !out_relative_path) {
    return VXCORE_ERR_NULL_POINTER;
  }

  auto *ctx = reinterpret_cast<vxcore::VxCoreContext *>(context);
  if (!ctx->buffer_manager) {
    return VXCORE_ERR_NOT_INITIALIZED;
  }

  try {
    auto *provider = ctx->buffer_manager->GetProvider(buffer_id);
    if (!provider) {
      ctx->last_error = "Buffer provider not available (unsupported notebook type)";
      return VXCORE_ERR_UNSUPPORTED;
    }

    std::string relative_path;
    VxCoreError err = provider->InsertAsset(source_path, relative_path);
    if (err != VXCORE_OK) {
      ctx->last_error = "Failed to insert asset";
      return err;
    }

    *out_relative_path = vxcore_strdup(relative_path.c_str());
    return VXCORE_OK;
  } catch (const std::exception &e) {
    ctx->last_error = std::string("Exception: ") + e.what();
    return VXCORE_ERR_UNKNOWN;
  }
}

VXCORE_API VxCoreError vxcore_buffer_delete_asset(VxCoreContextHandle context,
                                                  const char *buffer_id,
                                                  const char *relative_path) {
  if (!context || !buffer_id || !relative_path) {
    return VXCORE_ERR_NULL_POINTER;
  }

  auto *ctx = reinterpret_cast<vxcore::VxCoreContext *>(context);
  if (!ctx->buffer_manager) {
    return VXCORE_ERR_NOT_INITIALIZED;
  }

  try {
    auto *provider = ctx->buffer_manager->GetProvider(buffer_id);
    if (!provider) {
      ctx->last_error = "Buffer provider not available (unsupported notebook type)";
      return VXCORE_ERR_UNSUPPORTED;
    }

    VxCoreError err = provider->DeleteAsset(relative_path);
    if (err != VXCORE_OK) {
      ctx->last_error = "Failed to delete asset";
    }
    return err;
  } catch (const std::exception &e) {
    ctx->last_error = std::string("Exception: ") + e.what();
    return VXCORE_ERR_UNKNOWN;
  }
}

VXCORE_API VxCoreError vxcore_buffer_get_assets_folder(VxCoreContextHandle context,
                                                       const char *buffer_id, char **out_path) {
  if (!context || !buffer_id || !out_path) {
    return VXCORE_ERR_NULL_POINTER;
  }

  auto *ctx = reinterpret_cast<vxcore::VxCoreContext *>(context);
  if (!ctx->buffer_manager) {
    return VXCORE_ERR_NOT_INITIALIZED;
  }

  try {
    auto *provider = ctx->buffer_manager->GetProvider(buffer_id);
    if (!provider) {
      ctx->last_error = "Buffer provider not available (unsupported notebook type)";
      return VXCORE_ERR_UNSUPPORTED;
    }

    std::string path;
    VxCoreError err = provider->GetAssetsFolder(path);
    if (err != VXCORE_OK) {
      ctx->last_error = "Failed to get assets folder";
      return err;
    }

    *out_path = vxcore_strdup(path.c_str());
    return VXCORE_OK;
  } catch (const std::exception &e) {
    ctx->last_error = std::string("Exception: ") + e.what();
    return VXCORE_ERR_UNKNOWN;
  }
}

// ============ Buffer Attachment Operations (Filesystem + Metadata) ============

VXCORE_API VxCoreError vxcore_buffer_insert_attachment(VxCoreContextHandle context,
                                                       const char *buffer_id,
                                                       const char *source_path,
                                                       char **out_filename) {
  if (!context || !buffer_id || !source_path || !out_filename) {
    return VXCORE_ERR_NULL_POINTER;
  }

  auto *ctx = reinterpret_cast<vxcore::VxCoreContext *>(context);
  if (!ctx->buffer_manager) {
    return VXCORE_ERR_NOT_INITIALIZED;
  }

  try {
    auto *provider = ctx->buffer_manager->GetProvider(buffer_id);
    if (!provider) {
      ctx->last_error = "Buffer provider not available (unsupported notebook type)";
      return VXCORE_ERR_UNSUPPORTED;
    }

    std::string filename;
    VxCoreError err = provider->InsertAttachment(source_path, filename);
    if (err != VXCORE_OK) {
      ctx->last_error = "Failed to insert attachment";
      return err;
    }

    *out_filename = vxcore_strdup(filename.c_str());
    return VXCORE_OK;
  } catch (const std::exception &e) {
    ctx->last_error = std::string("Exception: ") + e.what();
    return VXCORE_ERR_UNKNOWN;
  }
}

VXCORE_API VxCoreError vxcore_buffer_delete_attachment(VxCoreContextHandle context,
                                                       const char *buffer_id,
                                                       const char *filename) {
  if (!context || !buffer_id || !filename) {
    return VXCORE_ERR_NULL_POINTER;
  }

  auto *ctx = reinterpret_cast<vxcore::VxCoreContext *>(context);
  if (!ctx->buffer_manager) {
    return VXCORE_ERR_NOT_INITIALIZED;
  }

  try {
    auto *provider = ctx->buffer_manager->GetProvider(buffer_id);
    if (!provider) {
      ctx->last_error = "Buffer provider not available (unsupported notebook type)";
      return VXCORE_ERR_UNSUPPORTED;
    }

    VxCoreError err = provider->DeleteAttachment(filename);
    if (err != VXCORE_OK) {
      ctx->last_error = "Failed to delete attachment";
    }
    return err;
  } catch (const std::exception &e) {
    ctx->last_error = std::string("Exception: ") + e.what();
    return VXCORE_ERR_UNKNOWN;
  }
}

VXCORE_API VxCoreError vxcore_buffer_rename_attachment(VxCoreContextHandle context,
                                                       const char *buffer_id,
                                                       const char *old_filename,
                                                       const char *new_filename,
                                                       char **out_new_filename) {
  if (!context || !buffer_id || !old_filename || !new_filename || !out_new_filename) {
    return VXCORE_ERR_NULL_POINTER;
  }

  auto *ctx = reinterpret_cast<vxcore::VxCoreContext *>(context);
  if (!ctx->buffer_manager) {
    return VXCORE_ERR_NOT_INITIALIZED;
  }

  try {
    auto *provider = ctx->buffer_manager->GetProvider(buffer_id);
    if (!provider) {
      ctx->last_error = "Buffer provider not available (unsupported notebook type)";
      return VXCORE_ERR_UNSUPPORTED;
    }

    std::string actual_new_filename;
    VxCoreError err = provider->RenameAttachment(old_filename, new_filename, actual_new_filename);
    if (err != VXCORE_OK) {
      ctx->last_error = "Failed to rename attachment";
      return err;
    }

    *out_new_filename = vxcore_strdup(actual_new_filename.c_str());
    return VXCORE_OK;
  } catch (const std::exception &e) {
    ctx->last_error = std::string("Exception: ") + e.what();
    return VXCORE_ERR_UNKNOWN;
  }
}

VXCORE_API VxCoreError vxcore_buffer_list_attachments(VxCoreContextHandle context,
                                                      const char *buffer_id,
                                                      char **out_attachments_json) {
  if (!context || !buffer_id || !out_attachments_json) {
    return VXCORE_ERR_NULL_POINTER;
  }

  auto *ctx = reinterpret_cast<vxcore::VxCoreContext *>(context);
  if (!ctx->buffer_manager) {
    return VXCORE_ERR_NOT_INITIALIZED;
  }

  try {
    auto *provider = ctx->buffer_manager->GetProvider(buffer_id);
    if (!provider) {
      ctx->last_error = "Buffer provider not available (unsupported notebook type)";
      return VXCORE_ERR_UNSUPPORTED;
    }

    std::vector<std::string> filenames;
    VxCoreError err = provider->ListAttachments(filenames);
    if (err != VXCORE_OK) {
      ctx->last_error = "Failed to list attachments";
      return err;
    }

    nlohmann::json json_array = nlohmann::json::array();
    for (const auto &filename : filenames) {
      json_array.push_back(filename);
    }

    std::string json_str = json_array.dump();
    *out_attachments_json = vxcore_strdup(json_str.c_str());
    return VXCORE_OK;
  } catch (const nlohmann::json::exception &e) {
    ctx->last_error = std::string("JSON error: ") + e.what();
    return VXCORE_ERR_JSON_SERIALIZE;
  } catch (const std::exception &e) {
    ctx->last_error = std::string("Exception: ") + e.what();
    return VXCORE_ERR_UNKNOWN;
  }
}

VXCORE_API VxCoreError vxcore_buffer_get_attachments_folder(VxCoreContextHandle context,
                                                            const char *buffer_id,
                                                            char **out_path) {
  if (!context || !buffer_id || !out_path) {
    return VXCORE_ERR_NULL_POINTER;
  }

  auto *ctx = reinterpret_cast<vxcore::VxCoreContext *>(context);
  if (!ctx->buffer_manager) {
    return VXCORE_ERR_NOT_INITIALIZED;
  }

  try {
    auto *provider = ctx->buffer_manager->GetProvider(buffer_id);
    if (!provider) {
      ctx->last_error = "Buffer provider not available (unsupported notebook type)";
      return VXCORE_ERR_UNSUPPORTED;
    }

    std::string path;
    VxCoreError err = provider->GetAttachmentsFolder(path);
    if (err != VXCORE_OK) {
      ctx->last_error = "Failed to get attachments folder";
      return err;
    }

    *out_path = vxcore_strdup(path.c_str());
    return VXCORE_OK;
  } catch (const std::exception &e) {
    ctx->last_error = std::string("Exception: ") + e.what();
    return VXCORE_ERR_UNKNOWN;
  }
}
