#include <stdlib.h>
#include <string.h>

#include <nlohmann/json.hpp>

#include "api/api_utils.h"
#include "core/config_manager.h"
#include "core/context.h"
#include "core/filetype_config.h"
#include "vxcore/vxcore.h"

VXCORE_API VxCoreError vxcore_filetype_list(VxCoreContextHandle context, char **out_json) {
  if (!context || !out_json) {
    return VXCORE_ERR_NULL_POINTER;
  }

  auto *ctx = reinterpret_cast<vxcore::VxCoreContext *>(context);
  if (!ctx->config_manager) {
    return VXCORE_ERR_NOT_INITIALIZED;
  }

  try {
    const vxcore::FileTypesConfig &file_types = ctx->config_manager->GetConfig().file_types;
    nlohmann::json types_array = nlohmann::json::array();

    for (const auto &entry : file_types.types) {
      types_array.push_back(entry.ToJson());
    }

    std::string json_str = types_array.dump(2);
    *out_json = vxcore_strdup(json_str.c_str());
    if (!*out_json) {
      return VXCORE_ERR_OUT_OF_MEMORY;
    }
    return VXCORE_OK;
  } catch (const nlohmann::json::exception &) {
    ctx->last_error = "JSON serialization failed for file types";
    return VXCORE_ERR_JSON_SERIALIZE;
  } catch (...) {
    ctx->last_error = "Unknown error listing file types";
    return VXCORE_ERR_UNKNOWN;
  }
}

VXCORE_API VxCoreError vxcore_filetype_get_by_suffix(VxCoreContextHandle context,
                                                     const char *suffix, char **out_json) {
  if (!context || !suffix || !out_json) {
    return VXCORE_ERR_NULL_POINTER;
  }

  auto *ctx = reinterpret_cast<vxcore::VxCoreContext *>(context);
  if (!ctx->config_manager) {
    return VXCORE_ERR_NOT_INITIALIZED;
  }

  try {
    const vxcore::FileTypesConfig &file_types = ctx->config_manager->GetConfig().file_types;
    const vxcore::FileTypeEntry *entry = file_types.GetBySuffix(suffix);

    // GetBySuffix always returns a valid entry (Others if not found)
    if (!entry) {
      ctx->last_error = "File type lookup failed";
      return VXCORE_ERR_UNKNOWN;
    }

    nlohmann::json json = entry->ToJson();
    std::string json_str = json.dump(2);
    *out_json = vxcore_strdup(json_str.c_str());
    if (!*out_json) {
      return VXCORE_ERR_OUT_OF_MEMORY;
    }
    return VXCORE_OK;
  } catch (const nlohmann::json::exception &) {
    ctx->last_error = "JSON serialization failed for file type";
    return VXCORE_ERR_JSON_SERIALIZE;
  } catch (...) {
    ctx->last_error = "Unknown error getting file type by suffix";
    return VXCORE_ERR_UNKNOWN;
  }
}

VXCORE_API VxCoreError vxcore_filetype_get_by_name(VxCoreContextHandle context, const char *name,
                                                   char **out_json) {
  if (!context || !name || !out_json) {
    return VXCORE_ERR_NULL_POINTER;
  }

  auto *ctx = reinterpret_cast<vxcore::VxCoreContext *>(context);
  if (!ctx->config_manager) {
    return VXCORE_ERR_NOT_INITIALIZED;
  }

  try {
    const vxcore::FileTypesConfig &file_types = ctx->config_manager->GetConfig().file_types;
    const vxcore::FileTypeEntry *entry = file_types.GetByName(name);

    if (!entry) {
      ctx->last_error = "File type not found: " + std::string(name);
      return VXCORE_ERR_NOT_FOUND;
    }

    nlohmann::json json = entry->ToJson();
    std::string json_str = json.dump(2);
    *out_json = vxcore_strdup(json_str.c_str());
    if (!*out_json) {
      return VXCORE_ERR_OUT_OF_MEMORY;
    }
    return VXCORE_OK;
  } catch (const nlohmann::json::exception &) {
    ctx->last_error = "JSON serialization failed for file type";
    return VXCORE_ERR_JSON_SERIALIZE;
  } catch (...) {
    ctx->last_error = "Unknown error getting file type by name";
    return VXCORE_ERR_UNKNOWN;
  }
}

VXCORE_API VxCoreError vxcore_filetype_set(VxCoreContextHandle context, const char *filetype_json) {
  if (!context || !filetype_json) {
    return VXCORE_ERR_NULL_POINTER;
  }

  auto *ctx = reinterpret_cast<vxcore::VxCoreContext *>(context);
  if (!ctx->config_manager) {
    return VXCORE_ERR_NOT_INITIALIZED;
  }

  try {
    // 1. Parse JSON array
    nlohmann::json json_array = nlohmann::json::parse(filetype_json);
    if (!json_array.is_array()) {
      ctx->last_error = "filetype_json must be a JSON array";
      return VXCORE_ERR_INVALID_PARAM;
    }

    // 2. Parse entries
    std::vector<vxcore::FileTypeEntry> new_types;
    for (const auto &entry_json : json_array) {
      new_types.push_back(vxcore::FileTypeEntry::FromJson(entry_json));
    }

    // 3. Validate and apply via SetTypes
    vxcore::SetTypesResult result =
        ctx->config_manager->GetConfig().file_types.SetTypes(std::move(new_types));
    if (!result.success) {
      ctx->last_error = result.error;
      return VXCORE_ERR_INVALID_PARAM;
    }

    // 4. Persist to disk
    VxCoreError err = ctx->config_manager->SaveConfig();
    if (err != VXCORE_OK) {
      ctx->last_error = "Failed to save config";
      return err;
    }

    return VXCORE_OK;

  } catch (const nlohmann::json::parse_error &e) {
    ctx->last_error = std::string("JSON parse error: ") + e.what();
    return VXCORE_ERR_JSON_PARSE;
  } catch (const nlohmann::json::exception &e) {
    ctx->last_error = std::string("JSON error: ") + e.what();
    return VXCORE_ERR_JSON_SERIALIZE;
  } catch (const std::exception &e) {
    ctx->last_error = std::string("Error: ") + e.what();
    return VXCORE_ERR_UNKNOWN;
  } catch (...) {
    ctx->last_error = "Unknown error setting file types";
    return VXCORE_ERR_UNKNOWN;
  }
}
