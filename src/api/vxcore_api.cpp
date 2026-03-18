#include <stdlib.h>

#include "api_utils.h"
#include "core/buffer_manager.h"
#include "core/config_manager.h"
#include "core/context.h"
#include "core/notebook_manager.h"
#include "core/workspace_manager.h"
#include "platform/path_provider.h"
#include "utils/logger.h"
#include "vxcore/vxcore.h"
#include "vxcore/vxcore_log.h"

VXCORE_API VxCoreVersion vxcore_get_version(void) {
  VxCoreVersion version = {0, 1, 0};
  return version;
}

VXCORE_API const char *vxcore_get_version_string(void) { return "0.1.0"; }

VXCORE_API void vxcore_set_test_mode(int enabled) {
  vxcore::ConfigManager::SetTestMode(enabled != 0);
}

VXCORE_API void vxcore_clear_test_directory(void) { vxcore::ConfigManager::ClearTestDirectory(); }

VXCORE_API void vxcore_set_app_info(const char *org_name, const char *app_name) {
  if (!org_name || !app_name) {
    return;
  }
  vxcore::ConfigManager::SetAppInfo(org_name, app_name);
}

VXCORE_API void vxcore_get_execution_file_path(char **out_path) {
  if (!out_path) {
    return;
  }
  try {
    std::filesystem::path exe_path = vxcore::PathProvider::GetExecutionFilePath();
    *out_path = vxcore_strdup(exe_path.string().c_str());
  } catch (...) {
    *out_path = nullptr;
  }
}

VXCORE_API void vxcore_get_execution_folder_path(char **out_path) {
  if (!out_path) {
    return;
  }
  try {
    std::filesystem::path exe_path = vxcore::PathProvider::GetExecutionFolderPath();
    *out_path = vxcore_strdup(exe_path.string().c_str());
  } catch (...) {
    *out_path = nullptr;
  }
}

VXCORE_API const char *vxcore_error_message(VxCoreError error) {
  switch (error) {
    case VXCORE_OK:
      return "Success";
    case VXCORE_ERR_INVALID_PARAM:
      return "Invalid parameter";
    case VXCORE_ERR_NULL_POINTER:
      return "Null pointer";
    case VXCORE_ERR_OUT_OF_MEMORY:
      return "Out of memory";
    case VXCORE_ERR_NOT_FOUND:
      return "Not found";
    case VXCORE_ERR_ALREADY_EXISTS:
      return "Already exists";
    case VXCORE_ERR_IO:
      return "I/O error";
    case VXCORE_ERR_DATABASE:
      return "Database error";
    case VXCORE_ERR_JSON_PARSE:
      return "JSON parse error";
    case VXCORE_ERR_JSON_SERIALIZE:
      return "JSON serialize error";
    case VXCORE_ERR_INVALID_STATE:
      return "Invalid state";
    case VXCORE_ERR_NOT_INITIALIZED:
      return "Not initialized";
    case VXCORE_ERR_ALREADY_INITIALIZED:
      return "Already initialized";
    case VXCORE_ERR_PERMISSION_DENIED:
      return "Permission denied";
    case VXCORE_ERR_UNSUPPORTED:
      return "Unsupported operation";
    default:
      return "Unknown error";
  }
}

VXCORE_API VxCoreError vxcore_context_create(const char *config_json,
                                             VxCoreContextHandle *out_context) {
  (void)config_json;
  if (!out_context) {
    return VXCORE_ERR_NULL_POINTER;
  }

  try {
    auto *ctx = new vxcore::VxCoreContext();
    ctx->config_manager = std::make_unique<vxcore::ConfigManager>();

    VxCoreError err = ctx->config_manager->LoadConfigs();
    if (err != VXCORE_OK) {
      delete ctx;
      return err;
    }

    ctx->notebook_manager = std::make_unique<vxcore::NotebookManager>(ctx->config_manager.get());
    ctx->buffer_manager = std::make_unique<vxcore::BufferManager>(ctx->config_manager.get(),
                                                                  ctx->notebook_manager.get());
    ctx->workspace_manager = std::make_unique<vxcore::WorkspaceManager>(ctx->config_manager.get(),
                                                                        ctx->buffer_manager.get());

    *out_context = reinterpret_cast<VxCoreContextHandle>(ctx);
    return VXCORE_OK;
  } catch (...) {
    return VXCORE_ERR_OUT_OF_MEMORY;
  }
}

VXCORE_API void vxcore_context_destroy(VxCoreContextHandle context) {
  if (context) {
    auto *ctx = reinterpret_cast<vxcore::VxCoreContext *>(context);
    delete ctx;
  }
}

VXCORE_API VxCoreError vxcore_prepare_shutdown(VxCoreContextHandle context) {
  if (!context) {
    return VXCORE_ERR_NULL_POINTER;
  }

  auto *ctx = reinterpret_cast<vxcore::VxCoreContext *>(context);

  // Idempotency guard: no-op if already shut down
  if (ctx->shutdown_called) {
    return VXCORE_OK;
  }

  try {
    // Update in-memory session config (no disk write yet)
    if (ctx->buffer_manager) {
      ctx->buffer_manager->UpdateSessionBuffers();
      ctx->buffer_manager->SetShutdownCalled(true);
    }

    if (ctx->workspace_manager) {
      ctx->workspace_manager->UpdateSessionWorkspaces();
      ctx->workspace_manager->SetShutdownCalled(true);
    }

    // Single atomic write to disk
    if (ctx->config_manager) {
      ctx->config_manager->SaveSessionConfig();
    }

    ctx->shutdown_called = true;
    VXCORE_LOG_INFO("Shutdown complete: session state saved");
    return VXCORE_OK;
  } catch (const std::exception &e) {
    VXCORE_LOG_ERROR("Shutdown failed: %s", e.what());
    ctx->last_error = std::string("Shutdown failed: ") + e.what();
    return VXCORE_ERR_UNKNOWN;
  }
}

VXCORE_API VxCoreError vxcore_cancel_shutdown(VxCoreContextHandle context) {
  if (!context) {
    return VXCORE_ERR_NULL_POINTER;
  }

  auto *ctx = reinterpret_cast<vxcore::VxCoreContext *>(context);

  // No-op if not in shutdown state.
  if (!ctx->shutdown_called) {
    return VXCORE_OK;
  }

  ctx->shutdown_called = false;

  if (ctx->buffer_manager) {
    ctx->buffer_manager->SetShutdownCalled(false);
  }

  if (ctx->workspace_manager) {
    ctx->workspace_manager->SetShutdownCalled(false);
  }

  VXCORE_LOG_INFO("Shutdown cancelled: normal operation resumed");
  return VXCORE_OK;
}

VXCORE_API VxCoreError vxcore_context_get_last_error(VxCoreContextHandle context,
                                                     const char **out_message) {
  if (!context || !out_message) {
    return VXCORE_ERR_NULL_POINTER;
  }
  auto *ctx = reinterpret_cast<vxcore::VxCoreContext *>(context);
  if (ctx->last_error.empty()) {
    *out_message = "No error";
  } else {
    *out_message = ctx->last_error.c_str();
  }
  return VXCORE_OK;
}

VXCORE_API VxCoreError vxcore_context_get_data_path(VxCoreContextHandle context,
                                                    VxCoreDataLocation location, char **out_path) {
  if (!context || !out_path) {
    return VXCORE_ERR_NULL_POINTER;
  }

  auto *ctx = reinterpret_cast<vxcore::VxCoreContext *>(context);
  if (!ctx->config_manager) {
    return VXCORE_ERR_NOT_INITIALIZED;
  }

  try {
    std::string path = ctx->config_manager->GetDataPath(location);
    *out_path = vxcore_strdup(path.c_str());
    return VXCORE_OK;
  } catch (...) {
    return VXCORE_ERR_UNKNOWN;
  }
}

VXCORE_API VxCoreError vxcore_context_get_config_path(VxCoreContextHandle context,
                                                      char **out_path) {
  if (!context || !out_path) {
    return VXCORE_ERR_NULL_POINTER;
  }

  auto *ctx = reinterpret_cast<vxcore::VxCoreContext *>(context);
  if (!ctx->config_manager) {
    return VXCORE_ERR_NOT_INITIALIZED;
  }

  try {
    std::string path = ctx->config_manager->GetConfigPath();
    *out_path = vxcore_strdup(path.c_str());
    return VXCORE_OK;
  } catch (...) {
    return VXCORE_ERR_UNKNOWN;
  }
}

VXCORE_API VxCoreError vxcore_context_get_session_config_path(VxCoreContextHandle context,
                                                              char **out_path) {
  if (!context || !out_path) {
    return VXCORE_ERR_NULL_POINTER;
  }

  auto *ctx = reinterpret_cast<vxcore::VxCoreContext *>(context);
  if (!ctx->config_manager) {
    return VXCORE_ERR_NOT_INITIALIZED;
  }

  try {
    std::string path = ctx->config_manager->GetSessionConfigPath();
    *out_path = vxcore_strdup(path.c_str());
    return VXCORE_OK;
  } catch (...) {
    return VXCORE_ERR_UNKNOWN;
  }
}

VXCORE_API VxCoreError vxcore_context_get_config(VxCoreContextHandle context, char **out_json) {
  if (!context || !out_json) {
    return VXCORE_ERR_NULL_POINTER;
  }

  auto *ctx = reinterpret_cast<vxcore::VxCoreContext *>(context);
  if (!ctx->config_manager) {
    return VXCORE_ERR_NOT_INITIALIZED;
  }

  try {
    nlohmann::json json = ctx->config_manager->GetConfig().ToJson();
    std::string json_str = json.dump(2);
    *out_json = vxcore_strdup(json_str.c_str());
    return VXCORE_OK;
  } catch (const nlohmann::json::exception &) {
    return VXCORE_ERR_JSON_SERIALIZE;
  } catch (...) {
    return VXCORE_ERR_UNKNOWN;
  }
}

VXCORE_API VxCoreError vxcore_context_update_config(VxCoreContextHandle context,
                                                     const char *config_json) {
  if (!context || !config_json) {
    return VXCORE_ERR_NULL_POINTER;
  }

  auto *ctx = reinterpret_cast<vxcore::VxCoreContext *>(context);
  if (!ctx->config_manager) {
    return VXCORE_ERR_NOT_INITIALIZED;
  }

  try {
    auto json = nlohmann::json::parse(config_json);
    auto &config = ctx->config_manager->GetConfig();

    if (json.contains("recoverLastSession") && json["recoverLastSession"].is_boolean()) {
      config.recover_last_session = json["recoverLastSession"].get<bool>();
    }
    // Add other top-level config fields here as needed.

    return ctx->config_manager->SaveConfig();
  } catch (const nlohmann::json::parse_error &e) {
    ctx->last_error = std::string("JSON parse error: ") + e.what();
    return VXCORE_ERR_JSON_PARSE;
  } catch (...) {
    ctx->last_error = "Unknown error updating config";
    return VXCORE_ERR_UNKNOWN;
  }
}

VXCORE_API VxCoreError vxcore_context_get_session_config(VxCoreContextHandle context,
                                                         char **out_json) {
  if (!context || !out_json) {
    return VXCORE_ERR_NULL_POINTER;
  }

  auto *ctx = reinterpret_cast<vxcore::VxCoreContext *>(context);
  if (!ctx->config_manager) {
    return VXCORE_ERR_NOT_INITIALIZED;
  }

  try {
    nlohmann::json json = ctx->config_manager->GetSessionConfig().ToJson();
    std::string json_str = json.dump(2);
    *out_json = vxcore_strdup(json_str.c_str());
    return VXCORE_OK;
  } catch (const nlohmann::json::exception &) {
    return VXCORE_ERR_JSON_SERIALIZE;
  } catch (...) {
    return VXCORE_ERR_UNKNOWN;
  }
}

VXCORE_API VxCoreError vxcore_context_get_config_by_name(VxCoreContextHandle context,
                                                         VxCoreDataLocation location,
                                                         const char *base_name, char **out_json) {
  if (!context || !base_name || !out_json) {
    return VXCORE_ERR_NULL_POINTER;
  }

  auto *ctx = reinterpret_cast<vxcore::VxCoreContext *>(context);
  if (!ctx->config_manager) {
    return VXCORE_ERR_NOT_INITIALIZED;
  }

  try {
    std::string content;
    VxCoreError err = ctx->config_manager->LoadConfigByName(location, base_name, content);
    if (err != VXCORE_OK) {
      return err;
    }
    *out_json = vxcore_strdup(content.c_str());
    return VXCORE_OK;
  } catch (...) {
    return VXCORE_ERR_UNKNOWN;
  }
}

VXCORE_API VxCoreError vxcore_context_get_config_by_name_with_defaults(VxCoreContextHandle context,
                                                                       VxCoreDataLocation location,
                                                                       const char *base_name,
                                                                       const char *default_json,
                                                                       char **out_json) {
  if (!context || !base_name || !default_json || !out_json) {
    return VXCORE_ERR_NULL_POINTER;
  }

  auto *ctx = reinterpret_cast<vxcore::VxCoreContext *>(context);
  if (!ctx->config_manager) {
    return VXCORE_ERR_NOT_INITIALIZED;
  }

  try {
    std::string merged;
    VxCoreError err = ctx->config_manager->LoadConfigByNameWithDefaults(location, base_name,
                                                                        default_json, merged);
    if (err != VXCORE_OK) {
      return err;
    }
    *out_json = vxcore_strdup(merged.c_str());
    return VXCORE_OK;
  } catch (...) {
    return VXCORE_ERR_UNKNOWN;
  }
}

VXCORE_API VxCoreError vxcore_context_update_config_by_name(VxCoreContextHandle context,
                                                            VxCoreDataLocation location,
                                                            const char *base_name,
                                                            const char *json) {
  if (!context || !base_name || !json) {
    return VXCORE_ERR_NULL_POINTER;
  }

  auto *ctx = reinterpret_cast<vxcore::VxCoreContext *>(context);
  if (!ctx->config_manager) {
    return VXCORE_ERR_NOT_INITIALIZED;
  }

  try {
    return ctx->config_manager->SaveConfigByName(location, base_name, json);
  } catch (...) {
    return VXCORE_ERR_UNKNOWN;
  }
}

VXCORE_API void vxcore_string_free(char *str) {
  if (str) {
    free(str);
  }
}

VXCORE_API VxCoreError vxcore_log_set_level(VxCoreLogLevel level) {
  vxcore::LogLevel cpp_level;
  switch (level) {
    case VXCORE_LOG_LEVEL_TRACE:
      cpp_level = vxcore::LogLevel::kTrace;
      break;
    case VXCORE_LOG_LEVEL_DEBUG:
      cpp_level = vxcore::LogLevel::kDebug;
      break;
    case VXCORE_LOG_LEVEL_INFO:
      cpp_level = vxcore::LogLevel::kInfo;
      break;
    case VXCORE_LOG_LEVEL_WARN:
      cpp_level = vxcore::LogLevel::kWarn;
      break;
    case VXCORE_LOG_LEVEL_ERROR:
      cpp_level = vxcore::LogLevel::kError;
      break;
    case VXCORE_LOG_LEVEL_FATAL:
      cpp_level = vxcore::LogLevel::kFatal;
      break;
    case VXCORE_LOG_LEVEL_OFF:
      cpp_level = vxcore::LogLevel::kOff;
      break;
    default:
      return VXCORE_ERR_INVALID_PARAM;
  }
  vxcore::Logger::GetInstance().SetLevel(cpp_level);
  return VXCORE_OK;
}

VXCORE_API VxCoreError vxcore_log_set_file(const char *path) {
  if (!path) {
    return VXCORE_ERR_NULL_POINTER;
  }
  try {
    vxcore::Logger::GetInstance().SetLogFile(path);
    return VXCORE_OK;
  } catch (...) {
    return VXCORE_ERR_IO;
  }
}

VXCORE_API VxCoreError vxcore_log_enable_console(int enable) {
  vxcore::Logger::GetInstance().EnableConsole(enable != 0);
  return VXCORE_OK;
}
