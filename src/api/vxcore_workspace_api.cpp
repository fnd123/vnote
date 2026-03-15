#include <stdlib.h>
#include <string.h>

#include <nlohmann/json.hpp>

#include "api/api_utils.h"
#include "core/context.h"
#include "core/workspace_manager.h"
#include "vxcore/vxcore.h"

VXCORE_API VxCoreError vxcore_workspace_create(VxCoreContextHandle context, const char *name,
                                               char **out_id) {
  if (!context || !name || !out_id) {
    return VXCORE_ERR_NULL_POINTER;
  }

  auto *ctx = reinterpret_cast<vxcore::VxCoreContext *>(context);
  if (!ctx->workspace_manager) {
    return VXCORE_ERR_NOT_INITIALIZED;
  }

  try {
    std::string id = ctx->workspace_manager->CreateWorkspace(name);
    *out_id = vxcore_strdup(id.c_str());
    return VXCORE_OK;
  } catch (const std::exception &e) {
    ctx->last_error = std::string("Exception: ") + e.what();
    return VXCORE_ERR_UNKNOWN;
  }
}

VXCORE_API VxCoreError vxcore_workspace_delete(VxCoreContextHandle context, const char *id) {
  if (!context || !id) {
    return VXCORE_ERR_NULL_POINTER;
  }

  auto *ctx = reinterpret_cast<vxcore::VxCoreContext *>(context);
  if (!ctx->workspace_manager) {
    return VXCORE_ERR_NOT_INITIALIZED;
  }

  try {
    bool success = ctx->workspace_manager->DeleteWorkspace(id);
    if (!success) {
      ctx->last_error = "Workspace not found";
      return VXCORE_ERR_WORKSPACE_NOT_FOUND;
    }
    return VXCORE_OK;
  } catch (const std::exception &e) {
    ctx->last_error = std::string("Exception: ") + e.what();
    return VXCORE_ERR_UNKNOWN;
  }
}

VXCORE_API VxCoreError vxcore_workspace_get(VxCoreContextHandle context, const char *id,
                                            char **out_json) {
  if (!context || !id || !out_json) {
    return VXCORE_ERR_NULL_POINTER;
  }

  auto *ctx = reinterpret_cast<vxcore::VxCoreContext *>(context);
  if (!ctx->workspace_manager) {
    return VXCORE_ERR_NOT_INITIALIZED;
  }

  try {
    auto *workspace = ctx->workspace_manager->GetWorkspace(id);
    if (!workspace) {
      ctx->last_error = "Workspace not found";
      return VXCORE_ERR_WORKSPACE_NOT_FOUND;
    }

    nlohmann::json json = workspace->ToJson();
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

VXCORE_API VxCoreError vxcore_workspace_list(VxCoreContextHandle context, char **out_json) {
  if (!context || !out_json) {
    return VXCORE_ERR_NULL_POINTER;
  }

  auto *ctx = reinterpret_cast<vxcore::VxCoreContext *>(context);
  if (!ctx->workspace_manager) {
    return VXCORE_ERR_NOT_INITIALIZED;
  }

  try {
    auto workspaces = ctx->workspace_manager->ListWorkspaces();
    nlohmann::json json = nlohmann::json::array();
    for (const auto &ws : workspaces) {
      json.push_back(ws.ToJson());
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

VXCORE_API VxCoreError vxcore_workspace_rename(VxCoreContextHandle context, const char *id,
                                               const char *name) {
  if (!context || !id || !name) {
    return VXCORE_ERR_NULL_POINTER;
  }

  auto *ctx = reinterpret_cast<vxcore::VxCoreContext *>(context);
  if (!ctx->workspace_manager) {
    return VXCORE_ERR_NOT_INITIALIZED;
  }

  try {
    bool success = ctx->workspace_manager->RenameWorkspace(id, name);
    if (!success) {
      ctx->last_error = "Workspace not found";
      return VXCORE_ERR_WORKSPACE_NOT_FOUND;
    }
    return VXCORE_OK;
  } catch (const std::exception &e) {
    ctx->last_error = std::string("Exception: ") + e.what();
    return VXCORE_ERR_UNKNOWN;
  }
}

VXCORE_API VxCoreError vxcore_workspace_get_current(VxCoreContextHandle context, char **out_id) {
  if (!context || !out_id) {
    return VXCORE_ERR_NULL_POINTER;
  }

  auto *ctx = reinterpret_cast<vxcore::VxCoreContext *>(context);
  if (!ctx->workspace_manager) {
    return VXCORE_ERR_NOT_INITIALIZED;
  }

  try {
    std::string id = ctx->workspace_manager->GetCurrentWorkspaceId();
    *out_id = vxcore_strdup(id.c_str());
    return VXCORE_OK;
  } catch (const std::exception &e) {
    ctx->last_error = std::string("Exception: ") + e.what();
    return VXCORE_ERR_UNKNOWN;
  }
}

VXCORE_API VxCoreError vxcore_workspace_set_current(VxCoreContextHandle context, const char *id) {
  if (!context || !id) {
    return VXCORE_ERR_NULL_POINTER;
  }

  auto *ctx = reinterpret_cast<vxcore::VxCoreContext *>(context);
  if (!ctx->workspace_manager) {
    return VXCORE_ERR_NOT_INITIALIZED;
  }

  try {
    bool success = ctx->workspace_manager->SetCurrentWorkspaceId(id);
    if (!success) {
      ctx->last_error = "Workspace not found";
      return VXCORE_ERR_WORKSPACE_NOT_FOUND;
    }
    return VXCORE_OK;
  } catch (const std::exception &e) {
    ctx->last_error = std::string("Exception: ") + e.what();
    return VXCORE_ERR_UNKNOWN;
  }
}

VXCORE_API VxCoreError vxcore_workspace_add_buffer(VxCoreContextHandle context,
                                                   const char *workspace_id,
                                                   const char *buffer_id) {
  if (!context || !workspace_id || !buffer_id) {
    return VXCORE_ERR_NULL_POINTER;
  }

  auto *ctx = reinterpret_cast<vxcore::VxCoreContext *>(context);
  if (!ctx->workspace_manager) {
    return VXCORE_ERR_NOT_INITIALIZED;
  }

  try {
    bool success = ctx->workspace_manager->AddBufferToWorkspace(workspace_id, buffer_id);
    if (!success) {
      ctx->last_error = "Workspace not found or buffer already added";
      return VXCORE_ERR_WORKSPACE_NOT_FOUND;
    }
    return VXCORE_OK;
  } catch (const std::exception &e) {
    ctx->last_error = std::string("Exception: ") + e.what();
    return VXCORE_ERR_UNKNOWN;
  }
}

VXCORE_API VxCoreError vxcore_workspace_remove_buffer(VxCoreContextHandle context,
                                                      const char *workspace_id,
                                                      const char *buffer_id) {
  if (!context || !workspace_id || !buffer_id) {
    return VXCORE_ERR_NULL_POINTER;
  }

  auto *ctx = reinterpret_cast<vxcore::VxCoreContext *>(context);
  if (!ctx->workspace_manager) {
    return VXCORE_ERR_NOT_INITIALIZED;
  }

  try {
    bool success = ctx->workspace_manager->RemoveBufferFromWorkspace(workspace_id, buffer_id);
    if (!success) {
      ctx->last_error = "Workspace not found or buffer not in workspace";
      return VXCORE_ERR_WORKSPACE_NOT_FOUND;
    }
    return VXCORE_OK;
  } catch (const std::exception &e) {
    ctx->last_error = std::string("Exception: ") + e.what();
    return VXCORE_ERR_UNKNOWN;
  }
}

VXCORE_API VxCoreError vxcore_workspace_set_current_buffer(VxCoreContextHandle context,
                                                           const char *workspace_id,
                                                           const char *buffer_id) {
  if (!context || !workspace_id) {
    return VXCORE_ERR_NULL_POINTER;
  }
  // buffer_id can be NULL to clear current buffer

  auto *ctx = reinterpret_cast<vxcore::VxCoreContext *>(context);
  if (!ctx->workspace_manager) {
    return VXCORE_ERR_NOT_INITIALIZED;
  }

  try {
    std::string buf_id = buffer_id ? buffer_id : "";
    bool success = ctx->workspace_manager->SetCurrentBufferInWorkspace(workspace_id, buf_id);
    if (!success) {
      ctx->last_error = "Workspace not found";
      return VXCORE_ERR_WORKSPACE_NOT_FOUND;
    }
    return VXCORE_OK;
  } catch (const std::exception &e) {
    ctx->last_error = std::string("Exception: ") + e.what();
    return VXCORE_ERR_UNKNOWN;
  }
}

VXCORE_API VxCoreError vxcore_workspace_set_buffer_order(VxCoreContextHandle context,
                                                         const char *workspace_id,
                                                         const char *buffer_ids_json) {
  if (!context || !workspace_id || !buffer_ids_json) {
    return VXCORE_ERR_NULL_POINTER;
  }

  auto *ctx = reinterpret_cast<vxcore::VxCoreContext *>(context);
  if (!ctx->workspace_manager) {
    return VXCORE_ERR_NOT_INITIALIZED;
  }

  try {
    auto json = nlohmann::json::parse(buffer_ids_json);
    if (!json.is_array()) {
      ctx->last_error = "buffer_ids_json must be a JSON array";
      return VXCORE_ERR_JSON_PARSE;
    }

    std::vector<std::string> ids;
    ids.reserve(json.size());
    for (const auto &elem : json) {
      if (elem.is_string()) {
        ids.push_back(elem.get<std::string>());
      }
    }

    bool success = ctx->workspace_manager->SetBufferOrder(workspace_id, ids);
    if (!success) {
      ctx->last_error = "Workspace not found";
      return VXCORE_ERR_WORKSPACE_NOT_FOUND;
    }
    return VXCORE_OK;
  } catch (const nlohmann::json::exception &e) {
    ctx->last_error = std::string("JSON error: ") + e.what();
    return VXCORE_ERR_JSON_PARSE;
  } catch (const std::exception &e) {
    ctx->last_error = std::string("Exception: ") + e.what();
    return VXCORE_ERR_UNKNOWN;
  }
}
