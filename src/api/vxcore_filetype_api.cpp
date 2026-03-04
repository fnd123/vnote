#include <stdlib.h>
#include <string.h>

#include <nlohmann/json.hpp>
#include <set>

#include "api/api_utils.h"
#include "core/config_manager.h"
#include "core/context.h"
#include "core/vxcore_config.h"
#include "utils/string_utils.h"
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

    // 2. Parse and validate entries
    std::vector<vxcore::FileTypeEntry> new_types;
    std::set<std::string> seen_names;

    for (size_t i = 0; i < json_array.size(); ++i) {
      const auto &entry_json = json_array[i];

      // Parse entry
      vxcore::FileTypeEntry entry = vxcore::FileTypeEntry::FromJson(entry_json);

      // Validate: name must not be empty
      if (entry.name.empty()) {
        ctx->last_error = "Entry at index " + std::to_string(i) + " has empty name";
        return VXCORE_ERR_INVALID_PARAM;
      }

      // Validate: name must be unique (case-insensitive)
      std::string lower_name = vxcore::ToLowerString(entry.name);
      if (seen_names.count(lower_name) > 0) {
        ctx->last_error = "Duplicate name: " + entry.name;
        return VXCORE_ERR_INVALID_PARAM;
      }
      seen_names.insert(lower_name);

      new_types.push_back(std::move(entry));
    }

    // 3. Build new config with updated file_types
    vxcore::VxCoreConfig new_config = ctx->config_manager->GetConfig();
    new_config.file_types.types = std::move(new_types);

    // 4. Serialize and persist
    std::string config_json = new_config.ToJson().dump(2);
    VxCoreError err = ctx->config_manager->SaveConfigByName(VXCORE_DATA_APP, "vxcore", config_json);
    if (err != VXCORE_OK) {
      ctx->last_error = "Failed to save config";
      return err;
    }

    // 5. Reload to update in-memory state
    err = ctx->config_manager->LoadConfigs();
    if (err != VXCORE_OK) {
      ctx->last_error = "Failed to reload config after save";
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
