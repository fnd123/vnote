#include <stdlib.h>

#include <nlohmann/json.hpp>

#include "api/api_utils.h"
#include "core/context.h"
#include "core/snippet_manager.h"
#include "vxcore/vxcore.h"

VXCORE_API VxCoreError vxcore_snippet_get_folder_path(VxCoreContextHandle context,
                                                      char **out_path) {
  if (!context || !out_path) {
    return VXCORE_ERR_NULL_POINTER;
  }

  *out_path = nullptr;

  auto *ctx = reinterpret_cast<vxcore::VxCoreContext *>(context);
  if (!ctx->snippet_manager) {
    return VXCORE_ERR_NOT_INITIALIZED;
  }

  try {
    std::string path;
    VxCoreError err = ctx->snippet_manager->GetSnippetFolderPath(path);
    if (err != VXCORE_OK) return err;
    *out_path = vxcore_strdup(path.c_str());
    if (!*out_path) return VXCORE_ERR_OUT_OF_MEMORY;
    return VXCORE_OK;
  } catch (const std::exception &e) {
    ctx->last_error = std::string("Exception: ") + e.what();
    return VXCORE_ERR_UNKNOWN;
  }
}

VXCORE_API VxCoreError vxcore_snippet_list(VxCoreContextHandle context, char **out_json) {
  if (!context || !out_json) {
    return VXCORE_ERR_NULL_POINTER;
  }

  *out_json = nullptr;

  auto *ctx = reinterpret_cast<vxcore::VxCoreContext *>(context);
  if (!ctx->snippet_manager) {
    return VXCORE_ERR_NOT_INITIALIZED;
  }

  try {
    std::vector<vxcore::SnippetData> snippets;
    VxCoreError err = ctx->snippet_manager->ListSnippets(snippets);
    if (err != VXCORE_OK) return err;

    nlohmann::json json_array = nlohmann::json::array();
    for (const auto &s : snippets) {
      nlohmann::json obj;
      obj["name"] = s.name;
      obj["type"] = (s.type == vxcore::SnippetType::kDynamic) ? "dynamic" : "text";
      obj["description"] = s.description;
      obj["isBuiltin"] = s.is_builtin;
      json_array.push_back(obj);
    }

    std::string json_str = json_array.dump();
    *out_json = vxcore_strdup(json_str.c_str());
    if (!*out_json) return VXCORE_ERR_OUT_OF_MEMORY;
    return VXCORE_OK;
  } catch (const nlohmann::json::exception &e) {
    ctx->last_error = std::string("JSON error: ") + e.what();
    return VXCORE_ERR_JSON_SERIALIZE;
  } catch (const std::exception &e) {
    ctx->last_error = std::string("Exception: ") + e.what();
    return VXCORE_ERR_UNKNOWN;
  }
}

VXCORE_API VxCoreError vxcore_snippet_get(VxCoreContextHandle context, const char *name,
                                          char **out_json) {
  if (!context || !name || !out_json) {
    return VXCORE_ERR_NULL_POINTER;
  }

  *out_json = nullptr;

  auto *ctx = reinterpret_cast<vxcore::VxCoreContext *>(context);
  if (!ctx->snippet_manager) {
    return VXCORE_ERR_NOT_INITIALIZED;
  }

  try {
    vxcore::SnippetData data;
    VxCoreError err = ctx->snippet_manager->GetSnippet(name, data);
    if (err != VXCORE_OK) return err;

    nlohmann::json obj;
    obj["name"] = data.name;
    obj["type"] = (data.type == vxcore::SnippetType::kDynamic) ? "dynamic" : "text";
    obj["description"] = data.description;
    obj["content"] = data.content;
    obj["cursorMark"] = data.cursor_mark;
    obj["selectionMark"] = data.selection_mark;
    obj["indentAsFirstLine"] = data.indent_as_first_line;
    obj["isBuiltin"] = data.is_builtin;

    std::string json_str = obj.dump();
    *out_json = vxcore_strdup(json_str.c_str());
    if (!*out_json) return VXCORE_ERR_OUT_OF_MEMORY;
    return VXCORE_OK;
  } catch (const nlohmann::json::exception &e) {
    ctx->last_error = std::string("JSON error: ") + e.what();
    return VXCORE_ERR_JSON_SERIALIZE;
  } catch (const std::exception &e) {
    ctx->last_error = std::string("Exception: ") + e.what();
    return VXCORE_ERR_UNKNOWN;
  }
}

VXCORE_API VxCoreError vxcore_snippet_create(VxCoreContextHandle context, const char *name,
                                             const char *content_json) {
  if (!context || !name || !content_json) {
    return VXCORE_ERR_NULL_POINTER;
  }

  auto *ctx = reinterpret_cast<vxcore::VxCoreContext *>(context);
  if (!ctx->snippet_manager) {
    return VXCORE_ERR_NOT_INITIALIZED;
  }

  try {
    return ctx->snippet_manager->CreateSnippet(name, content_json);
  } catch (const std::exception &e) {
    ctx->last_error = std::string("Exception: ") + e.what();
    return VXCORE_ERR_UNKNOWN;
  }
}

VXCORE_API VxCoreError vxcore_snippet_delete(VxCoreContextHandle context, const char *name) {
  if (!context || !name) {
    return VXCORE_ERR_NULL_POINTER;
  }

  auto *ctx = reinterpret_cast<vxcore::VxCoreContext *>(context);
  if (!ctx->snippet_manager) {
    return VXCORE_ERR_NOT_INITIALIZED;
  }

  try {
    return ctx->snippet_manager->DeleteSnippet(name);
  } catch (const std::exception &e) {
    ctx->last_error = std::string("Exception: ") + e.what();
    return VXCORE_ERR_UNKNOWN;
  }
}

VXCORE_API VxCoreError vxcore_snippet_rename(VxCoreContextHandle context, const char *old_name,
                                             const char *new_name) {
  if (!context || !old_name || !new_name) {
    return VXCORE_ERR_NULL_POINTER;
  }

  auto *ctx = reinterpret_cast<vxcore::VxCoreContext *>(context);
  if (!ctx->snippet_manager) {
    return VXCORE_ERR_NOT_INITIALIZED;
  }

  try {
    return ctx->snippet_manager->RenameSnippet(old_name, new_name);
  } catch (const std::exception &e) {
    ctx->last_error = std::string("Exception: ") + e.what();
    return VXCORE_ERR_UNKNOWN;
  }
}

VXCORE_API VxCoreError vxcore_snippet_update(VxCoreContextHandle context, const char *name,
                                             const char *content_json) {
  if (!context || !name || !content_json) {
    return VXCORE_ERR_NULL_POINTER;
  }

  auto *ctx = reinterpret_cast<vxcore::VxCoreContext *>(context);
  if (!ctx->snippet_manager) {
    return VXCORE_ERR_NOT_INITIALIZED;
  }

  try {
    return ctx->snippet_manager->UpdateSnippet(name, content_json);
  } catch (const std::exception &e) {
    ctx->last_error = std::string("Exception: ") + e.what();
    return VXCORE_ERR_UNKNOWN;
  }
}

VXCORE_API VxCoreError vxcore_snippet_apply(VxCoreContextHandle context, const char *name,
                                            const char *selected_text, const char *indentation,
                                            const char *overrides_json, char **out_json) {
  if (!context || !name || !out_json) {
    return VXCORE_ERR_NULL_POINTER;
  }

  *out_json = nullptr;

  auto *ctx = reinterpret_cast<vxcore::VxCoreContext *>(context);
  if (!ctx->snippet_manager) {
    return VXCORE_ERR_NOT_INITIALIZED;
  }

  try {
    // Parse overrides JSON (optional — null or empty string → empty map).
    vxcore::OverrideMap overrides;
    if (overrides_json && overrides_json[0] != '\0') {
      auto j = nlohmann::json::parse(overrides_json);
      if (j.is_object()) {
        for (auto it = j.begin(); it != j.end(); ++it) {
          overrides[it.key()] = it.value().get<std::string>();
        }
      }
    }

    std::string sel = selected_text ? selected_text : "";
    std::string indent = indentation ? indentation : "";

    vxcore::ApplyResult result = ctx->snippet_manager->ApplySnippet(name, sel, indent, overrides);

    nlohmann::json obj;
    obj["text"] = result.text;
    obj["cursorOffset"] = result.cursor_offset;

    std::string json_str = obj.dump();
    *out_json = vxcore_strdup(json_str.c_str());
    if (!*out_json) return VXCORE_ERR_OUT_OF_MEMORY;
    return VXCORE_OK;
  } catch (const nlohmann::json::exception &e) {
    ctx->last_error = std::string("JSON error: ") + e.what();
    return VXCORE_ERR_JSON_SERIALIZE;
  } catch (const std::exception &e) {
    ctx->last_error = std::string("Exception: ") + e.what();
    return VXCORE_ERR_UNKNOWN;
  }
}
